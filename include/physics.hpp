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

struct metadata
{
    ph::force elastic;
    ph::force gravitational;
    ph::force internal_damping;
    ph::force external_damping;
    ph::force bending_stiffness;
    ph::force total;
};

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

struct simulation_data
{
    std::vector<ph::state> state;
    std::vector<ph::metadata> metadata;
};

constexpr inline struct numerical_value_in_fn
{
    template <typename Unit>
    struct numerical_value_proxy
    {
        Unit unit;

        explicit constexpr numerical_value_proxy(Unit unit) : unit{unit} {}

        /**
         * @brief Computes the numerical value of a quantity in the given unit
         *
         * @param x the quantity
         * @return the numerical value of the quantity
         */
        template <typename T>
        constexpr auto operator()(T && x) const noexcept
        { return std::forward<T>(x).numerical_value_in(unit); }
    };

    /**
     * @brief Generates a proxy callable which gets the numerical value
     * of a quantity as in the required unit
     *
     * @param unit the units to get the numerical value in
     * @return the proxy object
     */
    static constexpr auto operator()(auto unit) {
        return numerical_value_proxy{unit};
    }
    /**
     * @brief Computes the numerical value of a quantity in a given unit
     *
     * @param unit the unit to get the numerical value in
     * @param value the quantity
     * @return the numerical value of the quantity
     */
    static constexpr auto operator()(auto unit, auto && value) {
        return numerical_value_proxy{unit}(std::forward<decltype(value)>(value));
    }
} numerical_value_in;

}  // namespace ph

template <> struct fmt::formatter<ph::state> : fmt::formatter<std::string_view>
{
    static constexpr
    auto format(ph::state const & s, format_context & ctx) {
        auto const & [x, v, m, f] = s;
        return fmt::format_to(ctx.out(), "x: {}, v: {}, m: {}, fixed: {}", x, v, m, f);
    }
};

template <> struct fmt::formatter<ph::derivative> : fmt::formatter<std::string_view>
{
    static constexpr
    auto format(ph::derivative const & s, format_context & ctx) {
        auto const & [dx, dv] = s;
        return fmt::format_to(ctx.out(), "δx: {}, δv: {}", dx, dv);
    }
};

#endif /* PHYSICS_HPP */
