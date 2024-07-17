/**
 * @author      : rbrugo (brugo.riccardo@gmail.com)
 * @file        : graphics
 * @created     : Saturday Jul 06, 2024 19:26:31 CEST
 * @description : 
 */

#include <fmt/core.h>
#include <nonstd/scope.hpp>
#include "graphics.hpp"
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

namespace gfx
{

[[nodiscard]]
auto setup_SDL(int screen_width, int screen_height) -> SDL_stuff
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fmt::print("SDL could not initialize! SDL_Error: {}\n", SDL_GetError());
        std::exit(1);
    }
    auto _ = nonstd::make_scope_exit(SDL_Quit);
    if (not (IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        fmt::print("SDL_image could not initialize! SDL_imag error: {}\n", IMG_GetError());
        std::exit(1);
    }
    auto _2 = nonstd::make_scope_exit(IMG_Quit);
    if (TTF_Init() == -1) {
        fmt::print("SDL_ttf could not initialize! SDL_ttf error: {}\n", TTF_GetError());
        std::exit(1);
    }
    auto _3 = nonstd::make_scope_exit(TTF_Quit);

    auto window = std::unique_ptr<SDL_Window, void(*)(SDL_Window *)>{
        SDL_CreateWindow("Ropes",
                         SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                         screen_width, screen_height, SDL_WINDOW_SHOWN),
        SDL_DestroyWindow
    };

    if (not window) {
        fmt::print("Window could not be created! SDL_Error: {}\n", SDL_GetError());
        std::exit(1);
    }

    auto renderer = std::unique_ptr<SDL_Renderer, void(*)(SDL_Renderer *)>{
        SDL_CreateRenderer(window.get(), -1, SDL_RENDERER_ACCELERATED),
        SDL_DestroyRenderer
    };
    if (not renderer) {
        fmt::print("Renderer could not be created! SDL_Error: {}\n", SDL_GetError());
        std::exit(1);
    }

    // return std::tuple{std::move(_), std::move(_2), std::move(_3), std::move(window), std::move(renderer)};
    return {
        { std::move(_), std::move(_2), std::move(_3) },
        std::move(window), std::move(renderer)
    };
}

void draw_arrow(
    gfx::renderer & renderer,
    int x, int y,
    std::array<double, 2> const & v, std::array<int, 3> color
) noexcept {
    SDL_SetRenderDrawColor(renderer.get(), color[0], color[1], color[2], 0xFF);
    SDL_RenderDrawLine(renderer.get(), x, y, x + v[0], y + v[1]);
    auto const rect = SDL_Rect(x + v[0] - 4, y + v[1] - 4, 8, 8);
    SDL_RenderFillRect(renderer.get(), &rect);
}

}  // namespace gfx
