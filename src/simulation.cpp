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
    auto const b = settings.damping;

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

    const auto elastic = elastic_force(current, prev) + elastic_force(current, next);
    const auto gravitational = gravitational_force(current);
    const auto damping = - b * current.v;
    // fmt::print("Elastic: {}\nGravitational: {}\nDamping: {}\n", elastic, gravitational, damping);

    return (elastic + gravitational + damping) * (1. / current.m);
}

auto integrate(
    sym::settings const & settings,
    std::span<ph::state const> const states,
    ph::time t,
    ph::duration dt
) -> std::vector<ph::state>
{
    // TODO: is it ok to do this from 0 to N, and not from 1 to N?
    auto const n = std::ssize(states);
    auto const d0 = ph::derivative{ph::velocity::zero(), ph::acceleration::zero()};

    auto as = std::views::transform(std::views::iota(0, n), [settings, states, t, d0](auto i) {
        return evaluate(settings, states, std::views::repeat(d0), i, t, 0 * ph::s);
    }) | std::ranges::to<std::vector>();
    auto bs = std::views::transform(std::views::iota(0, n), [settings, states, t, dt, &as](auto i) {
        return evaluate(settings, states, as, i, t, dt * 0.5);
    }) | std::ranges::to<std::vector>();
    auto cs = std::views::transform(std::views::iota(0, n), [settings, states, t, dt, &bs](auto i) {
        return evaluate(settings, states, bs, i, t, dt * 0.5);
    }) | std::ranges::to<std::vector>();
    auto ds = std::views::transform(std::views::iota(0, n), [settings, states, t, dt, &cs](auto i) {
        return evaluate(settings, states, cs, i, t, dt);
    }) | std::ranges::to<std::vector>();

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
    auto const & [n, k, b, total_length, segment_length, linear_density, segment_mass, t0, t1, dt] = settings;
    fmt::print("Number of points (n):    {}\n", n);
    fmt::print("Elastic constant (k):    {}\n", k);
    fmt::print("Damping coefficient (b): {}\n", b);
    fmt::print("Total length:            {}\n", total_length);
    fmt::print("Linear density:          {}\n", linear_density);
    fmt::print("Segment length:          {}\n", segment_length);
    fmt::print("Segment mass:            {}\n", segment_mass);
    fmt::print("\n");
    fmt::print("Standard gravity:        {}\n", (1. * mp_units::si::standard_gravity).in(ph::m / ph::s2));
    fmt::print("Segment weight:          {}\n", (segment_mass * mp_units::si::standard_gravity).in(ph::N));
    fmt::print("\n");
}
