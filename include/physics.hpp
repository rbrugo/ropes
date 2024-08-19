/**
 * @author      : rbrugo (brugo.riccardo@gmail.com)
 * @file        : physics
 * @created     : Tuesday Aug 06, 2024 16:43:05 CEST
 * @description : 
 * */

#ifndef PHYSICS_HPP
#define PHYSICS_HPP

#include <fmt/ranges.h>
#include <mp-units/systems/si.h>
#include <mp-units/ext/format.h>
#include <mp-units/format.h>

#include <math.hpp>

template<class T, auto N>
requires mp_units::is_scalar<T>
inline constexpr bool mp_units::is_vector<math::vector<T, N>> = true;

template <typename T, auto N, mp_units::Reference R>
constexpr mp_units::Quantity auto operator *(math::vector<T, N> const & v, [[maybe_unused]] R _) noexcept
{ return mp_units::quantity{v, R{}}; }

using namespace mp_units;

namespace ph
{
using namespace mp_units::si::unit_symbols;

template <typename T = double>
using vector = math::vector<T, 2>;

using mass = quantity<kg>;
using duration = quantity<s>;
using time = quantity<s>;
using length = quantity<m>;
using diameter = quantity<mm>;
using speed = quantity<m / s>;
using magnitude_of_acceleration = quantity<m / s2>;

using stiffness = quantity<N / m>;
using compressive_stiffness = quantity<GPa>;
using damping_coefficient = quantity<N * s / m>;
using linear_density = quantity<kg / m>;

using position = vector<length>;
using velocity = vector<speed>;
using acceleration = vector<magnitude_of_acceleration>;
using force = vector<quantity<N>>;
using energy = quantity<J>;

using framerate = quantity<Hz>;

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

#endif /* PHYSICS_HPP */

