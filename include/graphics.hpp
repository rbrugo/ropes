/**
 * @author      : rbrugo (brugo.riccardo@gmail.com)
 * @file        : graphics
 * @created     : Saturday Jul 06, 2024 21:59:31 CEST
 * @description : 
 * */

#ifndef GRAPHICS_HPP
#define GRAPHICS_HPP

#include <ranges>
#include <memory>
#include <nonstd/scope.hpp>

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <imgui.h>

#include <math.hpp>
#include <physics.hpp>

namespace sym { struct settings; }

namespace gfx
{
using window = std::unique_ptr<SDL_Window, void(*)(SDL_Window *)>;
using renderer = std::unique_ptr<SDL_Renderer, void(*)(SDL_Renderer *)>;
using gl_context = std::unique_ptr<void, void(*)(void *)>;
using imgui_guard = nonstd::scope_exit<void (*)()>;

struct SDL_stuff
{
    std::array<nonstd::scope_exit<void(*)()>, 2> exit_guards;
    gfx::window window;
    gfx::gl_context gl_context;
    gfx::imgui_guard imgui_guard;
};

struct screen_config
{
    math::vector<int, 2> screen_size;
    double scale{};
    math::vector<double, 2> offset;
};

struct arrow_settings {
    std::string name;
    std::function<ph::vector<>(ph::metadata const &)> value;
    float scale;
    math::vector<float, 3> color;
    bool enabled;

    template <typename Fn, typename T>
    arrow_settings(
        std::string name, Fn && fn_to_value, float scale, math::vector<T, 3> color, bool enabled = false
    ) :
        name{std::move(name)},
        value{std::forward<Fn>(fn_to_value)},
        scale{scale},
        color{[&] {
            if constexpr (std::is_integral_v<T>) {
                return math::vector_cast<float>(color) / 255;
            } else {
                return color;
            }
        }()},
        enabled{enabled}
    {}

    template <typename Fn, typename T>
    arrow_settings(
        std::string name, Fn && fn_to_value, math::vector<T, 3> color, bool enabled = false
    ) : arrow_settings{std::move(name), std::forward<Fn>(fn_to_value), 1.f, std::move(color), enabled}
    {}
};

struct forces_ui_fn {
    sym::settings * settings;
    sym::settings const * initial_settings;
    explicit forces_ui_fn(sym::settings & settings, sym::settings const & initial) :
        settings{std::addressof(settings)},
        initial_settings{std::addressof(initial)}
    {};
    void operator()() const noexcept;
};

struct data_ui_fn {
    sym::settings const * settings;
    std::vector<ph::state> const * rope;
    ph::time t;
    int steps;

    explicit data_ui_fn(
        sym::settings const & s,
        std::vector<ph::state> const & rope,
        ph::time t,
        int steps
    ) : settings{std::addressof(s)}, rope{std::addressof(rope)}, t{t}, steps{steps}
    {}
    void operator()() const noexcept;
};

struct rope_editor_fn {
    sym::settings * settings;
    std::vector<ph::state> * rope;
    std::vector<ph::metadata> * metadata;
    ph::duration * t;

    explicit rope_editor_fn(
        sym::settings & settings, auto & rope, auto & metadata, ph::duration & time
    ) :
        settings{std::addressof(settings)},
        rope{std::addressof(rope)},
        metadata{std::addressof(metadata)},
        t{std::addressof(time)}
    {}

    void operator()() noexcept;
};

struct arrows_ui {
    std::variant<std::monostate, uint32_t, float> stride = uint32_t{10};
    std::vector<arrow_settings> arrows;

    explicit arrows_ui();

    void operator()() noexcept;
};


[[nodiscard]]
auto setup_SDL(int screen_width, int screen_height) -> SDL_stuff;

[[nodiscard]] constexpr
auto map_to_screen(gfx::screen_config const & config) noexcept {
    return [&config](ph::position v) -> math::vector<double, 2> {
        auto const [width, height] = config.screen_size;
        auto const numerical = v.transform([](auto x) static { return x.numerical_value_in(ph::m); });
        auto const mapped = numerical * config.scale + config.offset;
        auto const normalized = math::hadamard_division(mapped, { width, -height });
        return normalized;
    };
}

[[nodiscard]] constexpr
auto map_from_screen(gfx::screen_config const & config) noexcept {
    return [&config](math::vector<double, 2> const & x) -> ph::position {
        auto const [width, height] = config.screen_size;
        auto const expand = math::hadamard_product(x, { width, -height });
        auto const unmapped = (expand - config.offset) / config.scale;
        auto const result = unmapped * ph::m;
        return result;
    };
}

inline
void vertex(std::array<double, 2> const & pt) noexcept { return glVertex2d(pt[0], pt[1]); }

inline
void draw_arrow(
    ph::position const & from_,
    math::vector<double, 2> const & size,
    gfx::screen_config const & config,
    std::array<int, 3> color = {0xFF, 0xCC, 0xFF}
) noexcept
{
    constexpr auto head_width = 15.f;
    constexpr auto head_height = 17.f;
    auto const remap = map_to_screen(config);
    auto const from = remap(from_);
    auto const [w, h] = static_cast<math::vector<float, 2>>(config.screen_size);
    auto const head_w = head_width / w;
    auto const head_h = head_height / h;

    if (size == math::vector{0., 0.}) {
        return;
    }

    auto const to = remap(from_ + size * ph::m);
    auto const direction = math::unit(to - from);
    auto const ortho = math::vector{-direction[1], direction[0]};

    auto const arrow_head_base = to - direction * head_h;

    auto const arrow_head_pt1 = arrow_head_base + ortho * head_w / 2;
    auto const arrow_head_pt2 = arrow_head_base - ortho * head_w / 2;

    glColor3ub(color[0], color[1], color[2]);  // NOLINT
    glBegin(GL_LINES);
    vertex(from);
    vertex(to);
    glEnd();

    glBegin(GL_TRIANGLES);
    vertex(to);
    vertex(arrow_head_pt1);
    vertex(arrow_head_pt2);
    glEnd();
}

inline
void draw_arrow(
    ph::position const & from_,
    math::vector<double, 2> const & size,
    gfx::screen_config const & config,
    math::vector<float, 3> color
) noexcept
{
    draw_arrow(from_, size, config, math::vector_cast<int>(color * 255));
}


inline
void draw_square(std::array<double, 2> const & p, math::vector<int, 2> screen_size) noexcept
{
    constexpr auto side = 3.f;
    auto const [w, h] = math::vector_cast<float>(screen_size);
    auto const x = static_cast<float>(p[0]);
    auto const y = static_cast<float>(p[1]);

    auto const wside = side / w;
    auto const hside = side / h;

    glBegin(GL_QUADS); // GL_QUADS for drawing a quadrilateral
    glVertex2f(x - wside, y - hside); // Bottom-left corner
    glVertex2f(x + wside, y - hside); // Bottom-right corner
    glVertex2f(x + wside, y + hside); // Top-right corner
    glVertex2f(x - wside, y + hside); // Top-left corner
    glEnd();
}

/**
 * @brief Draws the rope into the canvas
 *
 * @param rope the vector of positions of the rope points
 * @param l0 the rest length of the rope, used to determinate the color
 * @param config screen config
 */
template <std::ranges::forward_range Rope>
    requires std::same_as<std::ranges::range_value_t<Rope>, ph::position>
void render(Rope const & rope, ph::length l0, screen_config const & config)
{
    auto const points = rope | std::views::transform(map_to_screen(config));
    constexpr auto red = math::vector<uint8_t, 3>{0xff, 0, 0};
    constexpr auto blue = math::vector<uint8_t, 3>{0, 0, 0xff};
    constexpr auto c0 = (red + blue) / 2;
    constexpr auto c1 = (red - blue) / 2;
    auto max = 0.1 * l0;
    auto min = -max;

    auto const size = std::ssize(points);

    for (auto i = 0; i < size; ++i) {
        auto length = 0. * ph::m;
        if (i != 0) {
            length += math::norm(rope[i - 1] - rope[i]) - l0;
        }
        if (i + 1 < size) {
            length += math::norm(rope[i] - rope[i + 1]) - l0;
        }

        length = std::clamp(length, min, max);
        auto c = (c0 + std::pow((length / max).numerical_value_in(one), 2) * c1);

        glColor3ub(c[0], c[1], c[2]);  // NOLINT
        if (i != 0) {
            glBegin(GL_LINES);
            vertex(points[i - 1]);
            vertex(points[i]);
            glEnd();
        }
        draw_square(points[i], config.screen_size);
    }
}

/**
 * @brief Renders the metadata (atm forces) using the settings provided via UI
 *
 * @param rope the vector of the positions of the rope points
 * @param metadata the metadata objects
 * @param arrows_data the draw settings, from the UI
 * @param config screen config
 */
template <std::ranges::forward_range Rope>
    requires std::same_as<std::ranges::range_value_t<Rope>, ph::position>
void render(
    Rope const & rope,
    std::span<ph::metadata const> metadata,
    arrows_ui const & arrows_settings,
    screen_config const & config
) {
    if (metadata.empty()) {
        return;
    }
    auto const stride = std::visit([s=metadata.size()]<typename T>(T const & value) -> int32_t {
        if constexpr (std::same_as<T, float>) {
            return std::clamp<int32_t>(s * value, 1, s);
        } else if constexpr (std::is_integral_v<T>) {
            return static_cast<int32_t>(value);
        } else {
            return 1;
        }
    }, arrows_settings.stride);

    auto const points = rope
                      | std::views::stride(stride)
                      ;
    auto strided_metadata = metadata | std::views::stride(stride);
    auto settings = arrows_settings.arrows
                  | std::views::filter(&arrow_settings::enabled)
                  | std::views::transform([](auto & s) { return std::addressof(s); })
                  | std::ranges::to<std::vector>();
    if (settings.empty()) {
        return;
    }

    auto pairs = std::views::cartesian_product(std::views::zip(points, strided_metadata), settings);

    for (auto const & [data, settings] : pairs) {
        auto const & [from, arrow_data] = data;
        auto const & [_1, fn, scale, color, _2] = *settings;
        auto const size = fn(arrow_data) * scale;
        gfx::draw_arrow(from, size, config, color);
    }
}

template <typename Fn, typename ...Ts>
void draw_window(char const * const title, Fn && fn, Ts ...size) {
    if constexpr (sizeof...(size) == 2) {
        ImGui::SetNextWindowSize(ImVec2(size...));
    } else {
        static_assert(sizeof...(size) == 0, "Size args must be 0 or 2");
    }
    ImGui::Begin(title);
    std::forward<decltype(fn)>(fn)();
    ImGui::End();
}

template <typename Fn>
void tree_node(char const * title, bool & enabled, Fn && fn) {
    ImGui::Checkbox(fmt::format("##{}", title).c_str(), &enabled);
    ImGui::SameLine();
    if (ImGui::CollapsingHeader(title)) {
        std::forward<Fn>(fn)();
    }
}

} // namespace gfx

#endif /* GRAPHICS_HPP */
