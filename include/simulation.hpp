/**
 * @author      : rbrugo (brugo.riccardo@gmail.com)
 * @file        : simulation
 * @created     : Saturday Jul 06, 2024 23:12:31 CEST
 * @description : 
 * */

#ifndef SIMULATION_HPP
#define SIMULATION_HPP

#include <fmt/ranges.h>
#include <fmt/std.h>
#include <mp-units/ext/format.h>
#include <mp-units/format.h>
#include <math.hpp>
#include <mp-units/systems/si.h>
#include <mp-units/systems/isq/space_and_time.h>
#include <mp-units/systems/si.h>
#include <mp-units/math.h>
#include <mp-units/systems/si/constants.h>

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

using stiffness = quantity<N / m>;
using damping_coefficient = quantity<N * s / m>;
using linear_density = quantity<kg / m>;

using position = vector<length>;
using velocity = vector<speed>;
using acceleration = vector<magnitude_of_acceleration>;
using force = vector<quantity<N>>;

struct state
{
    ph::position x;
    ph::velocity v;
    ph::mass m;
    bool fixed = false;
};

struct derivative
{
    ph::velocity dx;
    ph::acceleration dv;
};
}  // namespace ph

template <> struct fmt::formatter<ph::state> : fmt::formatter<std::string_view>
{
    auto format(ph::state const & s, format_context & ctx) const {
        auto const & [x, v, m, f] = s;
        return fmt::format_to(ctx.out(), "x: {}, v: {}, m: {}, fixed: {}", x, v, m, f);
    }
};

template <> struct fmt::formatter<ph::derivative> : fmt::formatter<std::string_view>
{
    auto format(ph::derivative const & s, format_context & ctx) const {
        auto const & [dx, dv] = s;
        return fmt::format_to(ctx.out(), "δx: {}, δv: {}", dx, dv);
    }
};

namespace sym
{
struct settings
{
    int number_of_points;
    ph::stiffness elastic_constant;
    ph::damping_coefficient damping;
    ph::length total_length;
    ph::length segment_length;
    ph::linear_density linear_density;
    ph::mass segment_mass;

    ph::time t0;
    ph::time t1;
    ph::duration dt;

    settings(
        int n_points,
        ph::stiffness k,
        ph::damping_coefficient b,
        ph::length total_length,
        ph::linear_density linear_density,
        ph::duration dt,
        ph::duration duration
    ) :
        number_of_points{n_points},
        elastic_constant{k},
        damping{b},
        total_length{total_length},
        linear_density{linear_density},
        t0{0 * ph::s},
        t1{t0 + duration},
        dt{dt}
    {
        segment_length = total_length / (number_of_points - 1);
        segment_mass = linear_density * segment_length;
    }
};


auto acceleration(
    sym::settings const & settings,
    ph::state const & current,
    ph::state const * const prev,
    ph::state const * const next,
    [[maybe_unused]] ph::time const t
) -> ph::acceleration;


template <std::ranges::random_access_range Derivatives>
    requires std::convertible_to<
        std::ranges::range_reference_t<Derivatives>, ph::derivative const &
    >
auto evaluate(
    sym::settings const & settings,
    std::span<ph::state const> const states,
    Derivatives && derivatives,
    int idx,
    [[maybe_unused]] ph::time t,
    ph::duration dt
) -> ph::derivative
{
    auto const & curr = states[idx];
    auto const & d_curr = derivatives[idx];
    auto const current = ph::state{curr.x + d_curr.dx * dt, curr.v + d_curr.dv * dt, curr.m, curr.fixed};

    auto prev = std::optional<ph::state>{std::nullopt};
    if (idx > 0) {
        auto const s = states[idx - 1];
        auto const d = derivatives[idx - 1];
        prev = ph::state{s.x + d.dx * dt, s.v + d.dv * dt, s.m, s.fixed};
    }

    auto next = std::optional<ph::state>{std::nullopt};
    if (idx + 1 < std::ssize(states)) {
        auto const s = states[idx + 1];
        auto const d = derivatives[idx + 1];
        next = ph::state{s.x + d.dx * dt, s.v + d.dv * dt, s.m, s.fixed};
    }

    return ph::derivative{
        current.v,
        sym::acceleration(settings, current, prev ? &*prev : nullptr, next ? &*next : nullptr, t)
    };
}

auto integrate(sym::settings const & settings, std::span<ph::state const> const states, ph::time t, ph::duration dt) -> std::vector<ph::state>;

namespace constants
{
constexpr auto n = 3;  // 26;
constexpr auto k = 3.29e3 * si::newton / si::metre;
constexpr auto b = 0 * 5e-1 * si::newton * si::second / si::metre;  // TODO: remove 0
constexpr auto total_length = 70. * si::metre;
constexpr auto linear_density = 0.085 * si::kilogram / si::metre;  // kg/m
constexpr auto segment_length = total_length / (n - 1);
constexpr QuantityOf<isq::mass> auto segment_mass = segment_length * linear_density;
constexpr QuantityOf<isq::mass> auto fixed_point_mass = 1e10 * si::kilogram;

constexpr auto t0 = 0. * si::second;
constexpr auto t1 = 10. * si::second;
constexpr auto dt = ph::duration{0.1 * si::second};
}  // namespace constants

} // namespace sym


void dump_settings(sym::settings const &) noexcept;

#endif /* SIMULATION_HPP */
