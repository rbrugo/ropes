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

namespace sym {

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

    auto const elastic = elastic_force(current, prev) + elastic_force(current, next);
    auto const gravitational = gravitational_force(current);

    // auto const damping = - b * current.v;
    auto const damping = internal_damping(current, prev) + internal_damping(current, next)
                       + external_damping(current, prev) + external_damping(current, next);

    // TODO: bending stiffness (I can't bend a rope 180°, it tries to straighten)

    return (elastic + gravitational + damping) * (1. / current.m);
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
    auto const g = (1. * mp_units::si::standard_gravity).in(ph::m / ph::s2);
    fmt::print("Number of points (n):             {}\n", n);
    fmt::print("Elastic constant (k):             {}\n", k);
    fmt::print("External damping coefficient (b): {}\n", b);
    fmt::print("internal damping coefficient (c): {}\n", c);
    fmt::print("Total length:                     {}\n", total_length);
    fmt::print("Linear density:                   {}\n", linear_density);
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
