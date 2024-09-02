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
#include <ranges>
#include <expression.hpp>

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
    [[maybe_unused]] ph::time const t,
    ph::metadata * metadata
) -> ph::acceleration
{
    if (current.fixed) {
        return ph::acceleration::zero();
    }

    constexpr static auto zero = ph::force::zero();
    auto const & enabled = settings.enabled;

    auto const segment_length = settings.segment_length;
    auto const k = settings.elastic_constant;
    auto const E = settings.young_modulus;
    auto const b = settings.external_damping;
    auto const c = settings.internal_damping;
    auto const r = settings.diameter / 2;

    static auto distance = [segment_length](auto const & p, auto const & q) {
        auto const delta = p - q;
        auto const norm = math::norm(delta);
        if (abs(norm) < 0.0001 * ph::m) {
            return ph::position{0 * ph::m, 0 * ph::m};
        }
        return delta - segment_length * delta * (1. / norm);
    };

    auto const elastic_force = [k](ph::state const & curr, ph::state const * const other) {
        if (not other) {
            return zero;
        }
        return - k * distance(curr.x, other->x);
    };

    static constexpr auto gravitational_force = [](ph::state const & curr) static {
        if (curr.fixed) {
            return zero;
        }
        return ph::force{0 * ph::N, (curr.m * mp_units::si::standard_gravity).in(ph::N)};
    };

    auto internal_damping = [c](ph::state const & curr, ph::state const * const other) {
        if (not other) {
            return zero;
        }
        auto Δx = curr.x - other->x;
        auto direction = math::unit(Δx);
        auto radial_velocity = ((curr.v - other->v) * direction) * direction;

        return - c * radial_velocity;
    };

    auto external_damping = [b](ph::state const & curr, ph::state const * const other) {
        if (not other) {
            return zero;
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
    // FIXME: does not work correctly - it tries to curl the rope
    auto bending_stiffness_force = [E,r](ph::state const * const prv, ph::state const & curr, ph::state const * const nxt) -> ph::force {
        if (not nxt or not prv) {
            return zero;
        }
        auto const radius = radius_given_three_points(prv->x, curr.x, nxt->x);
        if (not radius) {
            return zero;
        }
        auto const κ = 1. / *radius;
        auto const I = std::numbers::pi * r * r * r * r / 4;  // second moment of area
        auto const bending_moment = E * I * κ;

        auto const Δx1 = prv->x - curr.x;
        auto const Δx2 = curr.x - nxt->x;

        auto const nΔx1 = math::norm(Δx1);
        auto const nΔx2 = math::norm(Δx2);

        auto const modulus = 2 * bending_moment / (nΔx1 + nΔx2);  // using the full arc length

        auto const tg = math::unit(Δx1 + Δx2);  // using the weighted direction
        auto const normal = math::vector{-tg[1], tg[0]};  // Perpendicular direction

        // Determine the sign by checking the relative position of nxt and prv
        auto const sign = (math::cosine(normal, Δx1) >= 0 * mp_units::one
                       and math::cosine(normal, Δx2) >= 0 * mp_units::one)
                         ? 1. : -1; 

        return sign * modulus * normal;
    };

    auto const elastic = enabled.elastic
                       ? elastic_force(current, prev) + elastic_force(current, next)
                       : zero;
    auto const gravitational = enabled.gravity ? gravitational_force(current) : zero;

    auto const int_damping = enabled.internal_damping
                           ? internal_damping(current, prev) + internal_damping(current, next)
                           : zero;
    auto const ext_damping = enabled.external_damping
                           ? external_damping(current, prev) + external_damping(current, next)
                           : zero;
    auto const damping = int_damping + ext_damping;

    auto const bending_stiffness = enabled.flexural_rigidity
                                 ? bending_stiffness_force(prev, current, next)
                                 : zero;
    auto const total_force = elastic + gravitational + damping + bending_stiffness;
    if (metadata != nullptr) {
        *metadata = {
            .elastic = elastic,
            .gravitational = gravitational,
            .internal_damping = int_damping,
            .external_damping = ext_damping,
            .bending_stiffness = bending_stiffness,
            .total = total_force
        };
    }
    return total_force * (1. / current.m);
}

auto integrate(
    sym::settings const & settings,
    std::span<ph::state const> const states,
    ph::time t,
    ph::duration dt,
    bool save
) -> ph::simulation_data
{
    auto const n = std::ssize(states);
    auto const d0 = ph::derivative{ph::velocity::zero(), ph::acceleration::zero()};

    using meta_t = std::vector<ph::metadata>;
    auto metadata = meta_t{};
    if (save) {
        metadata.resize(states.size());
    }
    auto meta_ptr = save ? std::addressof(metadata) : nullptr;

    auto do_evaluate = [&settings, states, t, dt] (auto && derivatives, double time_scale, meta_t * ptr = nullptr) {
        auto ds = std::views::all(std::forward<decltype(derivatives)>(derivatives));
        return [&settings, states, t, dt, ds, time_scale, ptr] (auto i) {
            return evaluate(settings, states, ds, i, t, dt * time_scale, ptr ? &(*ptr)[i] : nullptr);
        };
    };

    auto as = std::views::transform(std::views::iota(0, n), do_evaluate(std::views::repeat(d0), 0.))
            | std::ranges::to<std::vector>();
    auto bs = std::views::transform(std::views::iota(0, n), do_evaluate(as, 0.5))
            | std::ranges::to<std::vector>();
    auto cs = std::views::transform(std::views::iota(0, n), do_evaluate(bs, 0.5))
            | std::ranges::to<std::vector>();
    auto ds = std::views::transform(std::views::iota(0, n), do_evaluate(cs, 1.0, meta_ptr))
            | std::ranges::to<std::vector>();

    auto evolve = [dt](auto && curr, auto a, auto b, auto c, auto d) {
        auto dxdt = 1./6 * (a.dx + 2 * (b.dx + c.dx) + d.dx);
        auto dvdt = 1./6 * (a.dv + 2 * (b.dv + c.dv) + d.dv);

        return ph::state{curr.x + dxdt * dt, curr.v + dvdt * dt, curr.m, curr.fixed};
    };

    return {
        std::views::zip(states, as, bs, cs, ds)
        | std::views::transform([&](auto && tp) { return std::apply(evolve, FWD(tp)); })
        | std::ranges::to<std::vector<ph::state>>(),
        std::move(metadata)
    };
}

auto construct_rope(
    sym::settings const & settings, std::function<ph::vector<>(double)> const & f
) -> std::vector<ph::state>
{
    auto total_length = settings.total_length.numerical_value_in(ph::m);
    auto n_points = settings.number_of_points;
    auto points = settings.equalize_distance
        ? equidistant_points_along_function(f, n_points, total_length)
        : points_along_function(f, n_points, total_length);

    auto at_idx = [&points](int idx) { return points.at(idx); };
    auto mkstate = [&](int idx) {
        return ph::state{
            .x = at_idx(idx) * ph::m,
                .v = ph::velocity::zero(),
                .m = settings.segment_mass,
                .fixed = idx == 0
        };
    };
    return std::views::iota(0, settings.number_of_points)
        | std::views::transform(mkstate)
        | std::ranges::to<std::vector>();
}

auto points_along_function(
    std::function<ph::vector<>(double)> const & fn, ssize_t n_points,
    std::optional<double> total_len
) -> std::vector<math::vector<double, 2>>
{
    auto distance = [](auto p) {
        auto [a, b] = p;
        return math::norm(a - b);
    };
    auto pts = std::views::iota(0, n_points)
        | std::views::transform([=](int n) { return double(n) / (n_points - 1); })
        | std::views::transform(fn)
        | std::ranges::to<std::vector>();
    if (total_len.has_value()) {
        auto const arc_lengths = pts | std::views::adjacent<2> | std::views::transform(distance);
        auto const total_arc_length = std::ranges::fold_left(arc_lengths, 0., std::plus{});
        auto const ratio = *total_len / total_arc_length;
        auto const mul = [ratio](auto x) { return x * ratio; };
        std::ranges::transform(pts, pts.begin(), mul);
    }

    return pts;
}

auto equidistant_points_along_function(
    std::function<ph::vector<>(double)> const & fn, ssize_t n_points,
    std::optional<double> total_len
) -> std::vector<math::vector<double, 2>>
{
    auto plot_points = std::views::iota(0, n_points)
        | std::views::transform([=](int n) { return float(n) / (n_points - 1); })
        | std::views::transform(fn)
        | std::ranges::to<std::vector>();
    auto distance = [](auto p) {
        auto [a, b] = p;
        return std::min(math::norm(a - b), 1e9);
    };
    auto arc_lengths = plot_points
        | std::views::adjacent<2>
        | std::views::transform(distance)
    ;
    auto cumulative_arc_lengths = std::vector<double>(plot_points.size(), 0);
    std::partial_sum(arc_lengths.begin(), arc_lengths.end(), cumulative_arc_lengths.begin() + 1);

    if (std::isinf(cumulative_arc_lengths.back())) {
        fmt::print("Runtime error - infinite rope length:\n");
        fmt::print("{}\n", cumulative_arc_lengths);
        std::exit(1);
    }

    if (total_len.has_value()) {
        auto const len = *total_len;
        auto const ratio = len / cumulative_arc_lengths.back();
        auto fn_ = [ratio](auto x) { return x * ratio; };
        std::ranges::transform(cumulative_arc_lengths, cumulative_arc_lengths.begin(), fn_);
        std::ranges::transform(plot_points, plot_points.begin(), fn_);
    }

    auto pt_dst = std::views::zip(plot_points, cumulative_arc_lengths);
    auto it = std::ranges::begin(pt_dst);
    auto const pt_end = std::ranges::cend(pt_dst);
    auto const Δl = cumulative_arc_lengths.back() / (n_points - 1);
    auto current_arc = Δl;
    auto equidistant_points = std::vector<math::vector<double, 2>>{plot_points[0]};
    while (true) {
        auto pt = equidistant_points.back();
        // trovo P_next (it)
        while (it != pt_end and std::get<1>(*it) < current_arc) { ++it; }
        if (it == pt_end) {
            break;  // TODO: break or what?
        }
        auto const [p_next, l_next] = *it;
        auto new_pt = pt + (p_next - pt) * Δl / (l_next - current_arc + Δl);
        equidistant_points.push_back(new_pt);
        current_arc += Δl;
    }
    if (std::ssize(equidistant_points) != n_points) {
        if (n_points - equidistant_points.size() > 1) {
            fmt::print("ERROR - different number of points: : {} instead of {}\n", equidistant_points.size(), n_points);
        }
        equidistant_points.push_back(plot_points.back());
    }
    return equidistant_points;
}


void reset(
    sym::settings & settings,
    std::vector<ph::state> & rope, std::vector<ph::metadata> & metadata,
    ph::duration & t
)
{
    using maybe_expression = std::expected<brun::expr::expression, std::string>;
    auto eval = [](auto & arr) -> maybe_expression {
        auto ptr = arr.data();
        return brun::expr::parse_expression(std::string_view{ptr, std::strlen(ptr)}, "t");
    };
    auto x_expr = eval(settings.x_formula);
    auto y_expr = eval(settings.y_formula);
    auto const fn = [x=(x_expr.value())['t'],y=(y_expr.value())['t']] (auto n) {
        return math::vector<double, 2>{x(n), -y(n)};
    };
    rope = sym::construct_rope(settings, fn);
    metadata.clear();
    t = settings.t0;
}

}  // namespace sym

void dump_settings(sym::settings const & settings) noexcept {
    auto const & [
        n, k, E, b, c,
        total_length, diameter, segment_length, linear_density, segment_mass,
        t0, t1, dt, fps,
        x_formula, y_formula, equalize_distance,
        enabled
    ] = settings;
    auto const g = (1. * mp_units::si::standard_gravity).in(ph::N / ph::kg);
    fmt::print("Number of points (n):             {}\n", n);
    fmt::print("Starting formulas:                x(t) = {}\n", x_formula);
    fmt::print("                                  y(t) = {}\n", y_formula);
    fmt::print("Elastic constant (k):             {}\n", k);
    fmt::print("Young modulus (E):                {}\n", E);
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
    fmt::print("Forces enabled:\n");
    fmt::print("Gravity:                          {}\n", enabled.gravity);
    fmt::print("Elastic:                          {}\n", enabled.elastic);
    fmt::print("Internal damping:                 {}\n", enabled.internal_damping);
    fmt::print("External damping:                 {}\n", enabled.external_damping);
    fmt::print("Flexural rigidity:                {}\n", enabled.flexural_rigidity);
    fmt::print("\n");
}
