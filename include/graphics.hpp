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
#include <nonstd/scope.hpp>

#include <math.hpp>

namespace gfx
{
using window = std::unique_ptr<SDL_Window, void(*)(SDL_Window *)>;
using renderer = std::unique_ptr<SDL_Renderer, void(*)(SDL_Renderer *)>;

struct SDL_stuff
{
    std::array<nonstd::scope_exit<void(*)()>, 3> exit_guards;
    gfx::window window;
    gfx::renderer renderer;
};

[[nodiscard]]
auto setup_SDL(int screen_width, int screen_height) -> SDL_stuff;

void draw_arrow(
    gfx::renderer & renderer,
    int x, int y,
    std::array<double, 2> const & v, std::array<int, 3> color = {0xFF, 0xCC, 0xFF}
) noexcept;

template <std::ranges::forward_range Rope>
    // requires std::same_as<std::ranges::range_value_t<Rope>, std::array<double, 2>>
void render(gfx::renderer const & renderer, Rope const & rope, double l0, double scale)
{
    constexpr auto red = math::vector<uint8_t, 3>{0xff, 0, 0};
    constexpr auto blue = math::vector<uint8_t, 3>{0, 0, 0xff};
    constexpr auto c0 = (red + blue) / 2;
    constexpr auto c1 = (red - blue) / 2;
    constexpr auto max = +0.2;
    constexpr auto min = -max;

    auto const size = std::ssize(rope);

    for (auto i = 0; i < size; ++i) {
        auto length = 0.;
        if (i != 0) {
            length += math::norm(rope[i - 1] - rope[i]) - l0 * scale;
        }
        if (i + 1 < size) {
            length += math::norm(rope[i] - rope[i + 1]) - l0 * scale;
        }

        length = std::clamp(length, min, max);
        auto c = (c0 + std::pow(length / max, 2) * c1);

        SDL_SetRenderDrawColor(renderer.get(), c[0], c[1], c[2], 0xff);
        if (i != 0) {
            SDL_RenderDrawLine(renderer.get(), rope[i - 1][0], rope[i - 1][1], rope[i][0], rope[i][1]);
        }
        auto rect = SDL_Rect(rope[i][0] - 1, rope[i][1] - 1, 3, 3);
        SDL_RenderFillRect(renderer.get(), &rect);
    }
}

} // namespace gfx

#endif /* GRAPHICS_HPP */
