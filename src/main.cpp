/**
 * @author      : rbrugo (brugo.riccardo@gmail.com)
 * @file        : main
 * @created     : Friday Jun 21, 2024 22:08:08 CEST
 * @description : 
 */

#include <math.hpp>
#include <mp-units/systems/isq/space_and_time.h>
#include <mp-units/systems/si/si.h>
#include <mp-units/math.h>
#include <mp-units/systems/si/constants.h>

#include <null.hpp>

template<class T, auto N>
requires mp_units::is_scalar<T>
inline constexpr bool mp_units::is_vector<math::vector<T, N>> = true;

template <typename T, auto N, mp_units::Reference R>
constexpr mp_units::Quantity auto operator *(math::vector<T, N> const & v, R) noexcept
{ return mp_units::quantity{v, R{}}; }

using namespace mp_units;

namespace ph
{
    using namespace mp_units::si::unit_symbols;

    template <typename T>
    using vector = math::vector<T, 2>;

    using mass = quantity<kg>;
    using duration = quantity<s>;
    using time = quantity<s>;
    using length = quantity<m>;
    using speed = quantity<m / s>;
    using magnitude_of_acceleration = quantity<m / s2>;

    using position = vector<length>;
    using velocity = vector<speed>;
    using acceleration = vector<magnitude_of_acceleration>;
    using force = vector<quantity<N>>;
}  // namespace ph

constexpr auto n = 26;
constexpr auto k = 3.29e3 * si::newton / si::metre;
constexpr auto b = 5e-1 * si::newton * si::second / si::metre;
constexpr auto total_length = 13.l * si::metre;
constexpr auto linear_density = 0.085 * si::kilogram / si::metre;  // kg/m
constexpr auto segment_length = total_length / n;
constexpr QuantityOf<isq::mass> auto segment_mass = segment_length * linear_density;

constexpr auto t0 = 0. * si::second;
// constexpr auto t0 = quantity_point(0 * si::second);
constexpr auto t1 = 10. * si::second;
constexpr auto dt = ph::duration{0.1 * si::second};

struct state
{
    ph::position x;
    ph::velocity v;
    ph::mass m;
};

struct derivative
{
    ph::velocity dx;
    ph::acceleration dv;
};

auto acceleration(state const & current,
                  state const * const prev,
                  state const * const next,
                  [[maybe_unused]] ph::time const t,
                  [[maybe_unused]] ph::duration const dt
                  ) -> ph::acceleration
{
    static constexpr auto distance = [](auto const & p, auto const & q) static {
        auto const delta = p - q;
        auto const norm = math::norm(delta);
        if (abs(norm) < 0.0001 * ph::m) {
            return ph::position{0 * ph::m, 0 * ph::m};
        }
        return delta - segment_length * delta * (1. / norm);
    };

    static constexpr auto elastic_force = [](state const & curr, state const * const other) static {
        if (not other) {
            return ph::force{0 * ph::N, 0 * ph::N};
        }
        return - k * distance(curr.x, other->x);
    };

    const auto elastic = elastic_force(current, prev) + elastic_force(current, next);
    const auto gravitational = ph::force{0 * ph::N, (current.m * mp_units::si::standard_gravity).in(ph::N)};
    const auto damping = - b * current.v;

    return (elastic + gravitational + damping) * (1. / current.m);
}

template <std::ranges::random_access_range Derivatives>
    requires std::convertible_to<
        std::ranges::range_reference_t<Derivatives>, derivative const &
    >
auto evaluate(
    std::span<state const> const states,
    Derivatives && derivatives,
    int idx,
    ph::time t,
    ph::duration dt
) -> derivative
{
    auto const & curr = states[idx];
    auto const & d_curr = derivatives[idx];
    auto const current = state{curr.x + d_curr.dx * dt, curr.v + d_curr.dv * dt, curr.m};

    auto prev = std::optional<state>{std::nullopt};
    if (idx >= 0) {
        auto const s = states[idx - 1];
        auto const d = derivatives[idx - 1];
        prev = state{s.x + d.dx * dt, s.v + d.dv * dt, s.m};
    }

    auto next = std::optional<state>{std::nullopt};
    if (idx + 1 < std::ssize(states)) {
        auto const s = states[idx + 1];
        auto const d = derivatives[idx + 1];
        next = state{s.x + d.dx * dt, s.v + d.dv * dt, s.m};
    }

    return derivative{
        current.v,
        acceleration(current, prev ? &*prev : nullptr, next ? &*next : nullptr, t, dt)
    };
}

auto integrate(std::span<state const> const states, ph::time t, ph::duration dt)
{
    // TODO: is it ok to do this from 0 to N, and not from 1 to N?
    auto const n = std::ssize(states);
    auto const d0 = derivative{ph::velocity::zero(), ph::acceleration::zero()};

    // TODO: I should consider avoiding creating 4 different vectors, since I can
    // recycle the space and save some allocations
    auto as = std::views::transform(std::views::iota(0, n), [states, t, d0](auto i) {
        return evaluate(states, std::views::repeat(d0), i, t, 0 * ph::s);
    }) | std::ranges::to<std::vector>();
    auto bs = std::views::transform(std::views::iota(0, n), [states, t, dt, &as](auto i) {
        return evaluate(states, as, i, t, dt * 0.5);
    }) | std::ranges::to<std::vector>();
    auto cs = std::views::transform(std::views::iota(0, n), [states, t, dt, &bs](auto i) {
        return evaluate(states, bs, i, t, dt * 0.5);
    }) | std::ranges::to<std::vector>();
    auto ds = std::views::transform(std::views::iota(0, n), [states, t, dt, &cs](auto i) {
        return evaluate(states, cs, i, t, dt);
    }) | std::ranges::to<std::vector>();

    auto evolve = [dt](auto && curr, auto a, auto b, auto c, auto d) {
        auto dxdt = 1./6 * (a.dx + 2 * (b.dx + c.dx) + d.dx);
        auto dvdt = 1./6 * (a.dv + 2 * (b.dv + c.dv) + d.dv);

        return state{curr.x + dxdt * dt, curr.v + dvdt * dt, curr.m};
    };

    return std::views::zip(states, as, bs, cs, ds)
         | std::views::transform([&](auto && tp) { return std::apply(evolve, FWD(tp)); })
         | std::ranges::to<std::vector<state>>()
         ;
}

// int main()
// {
//
// }
