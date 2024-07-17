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
    // std::array<double, 2> const & v, std::array<int, 3> color = {0xFF, 0x00, 0x00}
) noexcept;

template <std::ranges::forward_range Rope>
    // requires std::same_as<std::ranges::range_value_t<Rope>, std::array<double, 2>>
void render(gfx::renderer const & renderer, Rope const & rope)
{
    SDL_SetRenderDrawColor(renderer.get(), 0x00, 0xFF, 0xFF, 0xFF);
    auto const pairs = rope
                     | std::views::adjacent<2>;
    for (auto const & [a, b] : pairs) {
        SDL_RenderDrawLine(renderer.get(), a[0], a[1], b[0], b[1]);
        auto rect = SDL_Rect(a[0] - 2, a[1] - 2, 4, 4);
        SDL_RenderFillRect(renderer.get(), &rect);
    }
}

} // namespace gfx

#endif /* GRAPHICS_HPP */
