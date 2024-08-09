/**
 * @author      : rbrugo (brugo.riccardo@gmail.com)
 * @file        : simulation
 * @created     : Saturday Jul 06, 2024 23:12:31 CEST
 * @description : 
 * */

#ifndef SIMULATION_HPP
#define SIMULATION_HPP

#include <physics.hpp>

namespace sym
{
// TODO: replace stiffness k with Young modulus E; introduce section area
// in that case: k = E * A / l₀ (A cross section, l₀ rest length of the segment)
// NOTE: this is ideal, meaning the k computed is ~10 times smaller than the realistic one
// TODO: introduce temperature for graphics and, maybe, to change k or E (if significant)
struct settings
{
    int number_of_points;
    ph::stiffness elastic_constant;
    ph::damping_coefficient external_damping;
    ph::damping_coefficient internal_damping;
    ph::length total_length;
    ph::length segment_length;
    ph::linear_density linear_density;
    ph::mass segment_mass;

    ph::time t0;
    ph::time t1;
    ph::duration dt;
    ph::framerate fps;

    settings(
        int n_points,
        ph::stiffness k,
        ph::damping_coefficient b,  // external / tangential  // NOLINT
        ph::damping_coefficient c,  // internal / radial
        ph::length total_length,
        ph::linear_density linear_density,
        ph::duration dt,
        ph::framerate framerate,
        ph::duration duration
    ) :
        number_of_points{n_points},
        elastic_constant{k},
        external_damping{b},
        internal_damping{c},
        total_length{total_length},
        segment_length{total_length / (n_points - 1)},
        linear_density{linear_density},
        segment_mass{linear_density * segment_length},
        t0{0 * ph::s},
        t1{t0 + duration},
        dt{dt},
        fps{framerate}
    { }
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
constexpr auto b = 2e-1 * si::newton * si::second / si::metre;
constexpr auto c = 5e-1 * si::newton * si::second / si::metre;  // range 0.5 - 2
constexpr auto total_length = 70. * si::metre;
constexpr auto linear_density = 0.085 * si::kilogram / si::metre;
constexpr auto segment_length = total_length / (n - 1);
constexpr QuantityOf<isq::mass> auto segment_mass = segment_length * linear_density;
constexpr QuantityOf<isq::mass> auto fixed_point_mass = 1e10 * si::kilogram;

constexpr auto t0 = 0. * si::second;
constexpr auto t1 = 10. * si::second;
constexpr auto dt = ph::duration{0.1 * si::second};
constexpr auto fps = 60 * si::hertz;
}  // namespace constants

} // namespace sym


void dump_settings(sym::settings const &) noexcept;

#endif /* SIMULATION_HPP */
