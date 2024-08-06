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
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <nonstd/scope.hpp>

#include <math.hpp>
#include <physics.hpp>

namespace gfx
{
using window = std::unique_ptr<SDL_Window, void(*)(SDL_Window *)>;
using renderer = std::unique_ptr<SDL_Renderer, void(*)(SDL_Renderer *)>;
using gl_context = std::unique_ptr<void, void(*)(void *)>;

struct SDL_stuff
{
    std::array<nonstd::scope_exit<void(*)()>, 2> exit_guards;
    gfx::window window;
    gfx::gl_context gl_context;
};

struct screen_config
{
    math::vector<int, 2> screen_size;
    double scale{};
    math::vector<double, 2> offset;
};

[[nodiscard]]
auto setup_SDL(int screen_width, int screen_height) -> SDL_stuff;

void draw_arrow(
    gfx::renderer & renderer,
    int x, int y,
    std::array<double, 2> const & v, std::array<int, 3> color = {0xFF, 0xCC, 0xFF}
) noexcept;


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
void draw_square(std::array<double, 2> const & p, math::vector<int, 2> screen_size)
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

template <std::ranges::forward_range Rope>
void render(Rope const & rope, double l0, screen_config const & config)
{
    auto const points = rope | std::views::transform(map_to_screen(config));
    constexpr auto red = math::vector<uint8_t, 3>{0xff, 0, 0};
    constexpr auto blue = math::vector<uint8_t, 3>{0, 0, 0xff};
    constexpr auto c0 = (red + blue) / 2;
    constexpr auto c1 = (red - blue) / 2;
    constexpr auto max = +0.2;
    constexpr auto min = -max;

    auto const size = std::ssize(points);

    for (auto i = 0; i < size; ++i) {
        auto length = 0.;
        if (i != 0) {
            length += math::norm(points[i - 1] - points[i]) - l0 * config.scale;
        }
        if (i + 1 < size) {
            length += math::norm(points[i] - points[i + 1]) - l0 * config.scale;
        }

        length = std::clamp(length, min, max);
        auto c = (c0 + std::pow(length / max, 2) * c1);

        glColor3f(c[0] / 255.0f, c[1] / 255.0f, c[2] / 255.0f);  // NOLINT
        if (i != 0) {
            glBegin(GL_LINES);
            glVertex2f(points[i - 1][0], points[i - 1][1]);
            glVertex2f(points[i][0], points[i][1]);
            glEnd();
        }
        draw_square(points[i], config.screen_size);
    }
}

} // namespace gfx

#endif /* GRAPHICS_HPP */
