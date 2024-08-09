/**
 * @author      : rbrugo (brugo.riccardo@gmail.com)
 * @file        : simulation
 * @created     : Saturday Jul 06, 2024 23:11:40 CEST
 * @description : 
 */

#include "simulation.hpp"
#include <fmt/ranges.h>
#include <mp-units/format.h>
#include <mp-units/ext/format.h>
#include <mp-units/math.h>
#include <numbers>

namespace sym {

template <typename T>
[[nodiscard]] constexpr
auto radius_given_three_points(math::vector<T, 2> p1, math::vector<T, 2> p2, math::vector<T, 2> p3)
    -> std::optional<T>
{
    // Circumrcircle formula

    auto const Δr12 = math::norm(p1 - p2);
    auto const Δr23 = math::norm(p2 - p3);
    auto const Δr13 = math::norm(p1 - p3);

    // Shoelace formula
    auto const A = (
            p1[0] * (p2[1] - p3[1]) +
            p2[0] * (p3[1] - p1[1]) +
            p3[0] * (p1[1] - p2[1])
    ) / 2.;

    if (A == A * 0) {
        return std::nullopt;
    }

    return Δr12 * Δr23 * Δr13 / (4 * A);
}

auto acceleration(
    sym::settings const & settings,
    ph::state const & current,
    ph::state const * const prev,
    ph::state const * const next,
    [[maybe_unused]] ph::time const t
) -> ph::acceleration
{
    if (current.fixed) {
        return ph::acceleration::zero();
    }

    auto const segment_length = settings.segment_length;
    auto const k = settings.elastic_constant;
    auto const b = settings.external_damping;
    auto const c = settings.internal_damping;

    static auto distance = [segment_length](auto const & p, auto const & q) {
        auto const delta = p - q;
        auto const norm = math::norm(delta);
        if (abs(norm) < 0.0001 * ph::m) {
            return ph::position{0 * ph::m, 0 * ph::m};
        }
        return delta - segment_length * delta * (1. / norm);
    };

    static auto elastic_force = [k](ph::state const & curr, ph::state const * const other) {
        if (not other) {
            return ph::force{0 * ph::N, 0 * ph::N};
        }
        return - k * distance(curr.x, other->x);
    };

    static constexpr auto gravitational_force = [](ph::state const & curr) static {
        if (curr.fixed) {
            return ph::force::zero();
        }
        return ph::force{0 * ph::N, (curr.m * mp_units::si::standard_gravity).in(ph::N)};
    };

    static auto internal_damping = [c](ph::state const & curr, ph::state const * const other) {
        if (not other) {
            return ph::force::zero();
        }
        auto Δx = curr.x - other->x;
        auto direction = math::unit(Δx);
        auto radial_velocity = ((curr.v - other->v) * direction) * direction;

        return - c * radial_velocity;
    };

    static auto external_damping = [b](ph::state const & curr, ph::state const * const other) {
        if (not other) {
            return ph::force::zero();
        }
        auto Δx = curr.x - other->x;
        auto direction = math::unit(Δx);
        auto tg = decltype(direction){direction[1], -direction[0]};
        auto tangential_velocity = ((curr.v - other->v) * tg) * tg;
        return - b * tangential_velocity;
    };

    /** Bending stiffness / Flexural rigidity **/
    // Second moment of area: I = ∫y²dA = π(r₁⁴ - r₀⁴) / 4 for a hollow circular cross-section
    // Bending stiffness: E·I
    // Curvature: κ = 1 / R
    // Bending moment: M = EIκ
    // Direction: perpendicular to the tangent
    // >> To compute the force:
    // Δx1 = b - a
    // Δx2 = c - b
    // Δs = (|Δx1| + |Δx2|) / 2
    // |F| = M / Δs
    // t = (Δx1 + Δx2) / |Δx1 + Δx2|
    // dir = (t[1], -t[0])
    // F = |F| * dir
    static auto bending_stiffness_force = [](ph::state const * const prev, ph::state const & current, ph::state const * const next) -> ph::force {
        if (not next or not prev) {
            return ph::force::zero();
        }
        auto const radius = radius_given_three_points(prev->x, current.x, next->x);
        if (not radius) {
            return ph::force::zero();
        }
        auto const κ = 1. / *radius;
        auto const r = 0.6 * ph::cm;  // rope section radius
        auto const I = std::numbers::pi * r * r * r * r / 4;  // second moment of area
        auto const E = 1. * ph::GPa;  // Young modulus
        auto const bending_moment = E * I * κ;

        auto const Δx1 = prev->x - current.x;
        auto const Δx2 = current.x - next->x;

        auto const nΔx1 = math::norm(Δx1);
        auto const nΔx2 = math::norm(Δx2);

        auto modulus = 2 * bending_moment / (nΔx1 + nΔx2);  // using the full arc length

        auto const t = math::unit(Δx1 + Δx2);  // using the weighted direction

        return modulus * math::vector{-t[1], t[0]};
    };

    auto const elastic = elastic_force(current, prev) + elastic_force(current, next);
    auto const gravitational = gravitational_force(current);

    auto const damping = internal_damping(current, prev) + internal_damping(current, next)
                       + external_damping(current, prev) + external_damping(current, next);

    auto const bending_stiffness = bending_stiffness_force(prev, current, next);

    return (elastic + gravitational + damping + bending_stiffness) * (1. / current.m);
}

auto integrate(
    sym::settings const & settings,
    std::span<ph::state const> const states,
    ph::time t,
    ph::duration dt
) -> std::vector<ph::state>
{
    auto const n = std::ssize(states);
    auto const d0 = ph::derivative{ph::velocity::zero(), ph::acceleration::zero()};

    auto do_evaluate = [&settings, states, t, dt] (auto && derivatives, double time_scale) {
        auto ds = std::views::all(std::forward<decltype(derivatives)>(derivatives));
        return [&settings, states, t, dt, ds, time_scale] (auto i) {
            return evaluate(settings, states, ds, i, t, dt * time_scale);
        };
    };

    auto as = std::views::transform(std::views::iota(0, n), do_evaluate(std::views::repeat(d0), 0.))
            | std::ranges::to<std::vector>();
    auto bs = std::views::transform(std::views::iota(0, n), do_evaluate(as, 0.5))
            | std::ranges::to<std::vector>();
    auto cs = std::views::transform(std::views::iota(0, n), do_evaluate(bs, 0.5))
            | std::ranges::to<std::vector>();
    auto ds = std::views::transform(std::views::iota(0, n), do_evaluate(cs, 1.0))
            | std::ranges::to<std::vector>();

    auto evolve = [dt](auto && curr, auto a, auto b, auto c, auto d) {
        auto dxdt = 1./6 * (a.dx + 2 * (b.dx + c.dx) + d.dx);
        auto dvdt = 1./6 * (a.dv + 2 * (b.dv + c.dv) + d.dv);

        return ph::state{curr.x + dxdt * dt, curr.v + dvdt * dt, curr.m, curr.fixed};
    };

    return std::views::zip(states, as, bs, cs, ds)
         | std::views::transform([&](auto && tp) { return std::apply(evolve, FWD(tp)); })
         | std::ranges::to<std::vector<ph::state>>()
         ;
}

}  // namespace sym

void dump_settings(sym::settings const & settings) noexcept {
    auto const & [n, k, b, c, total_length, segment_length, linear_density, segment_mass, t0, t1, dt, fps] = settings;
    auto const g = (1. * mp_units::si::standard_gravity).in(ph::N / ph::kg);
    fmt::print("Number of points (n):             {}\n", n);
    fmt::print("Elastic constant (k):             {}\n", k);
    fmt::print("External damping coefficient (b): {}\n", b);
    fmt::print("internal damping coefficient (c): {}\n", c);
    fmt::print("Linear density:                   {}\n", linear_density);
    fmt::print("Total length:                     {}\n", total_length);
    fmt::print("Total mass:                       {}\n", total_length * linear_density);
    fmt::print("Segment length:                   {}\n", segment_length);
    fmt::print("Segment mass:                     {}\n", segment_mass);
    fmt::print("\n");
    fmt::print("Standard gravity:                 {}\n", g);
    fmt::print("Segment weight:                   {}\n", segment_mass * g);
    fmt::print("\n");
    fmt::print("Initial time point:               {}\n", t0);
    fmt::print("Final time point:                 {}\n", t1);
    fmt::print("Simulation time-step:             {}\n", dt);
    fmt::print("Frames per second:                {}\n", fps);
    fmt::print("Steps per frame:                  {}\n", dt * fps);
    fmt::print("\n");
}
