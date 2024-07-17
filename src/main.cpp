/**
 * @author      : rbrugo (brugo.riccardo@gmail.com)
 * @file        : main
 * @created     : Friday Jun 21, 2024 22:08:08 CEST
 * @description : 
 */

#include <SDL_keycode.h>
#include <fmt/ranges.h>
#include <fmt/chrono.h>
#include <math.hpp>
#include <thread>
#include <structopt/app.hpp>

#include "simulation.hpp"

#include "graphics.hpp"

struct options
{
    std::optional<int> n = sym::constants::n;
    std::optional<double> k = sym::constants::k.numerical_value_in(ph::N/ph::m);
    std::optional<double> b = sym::constants::b.numerical_value_in(ph::N*ph::s/ph::m);
    std::optional<double> total_length = sym::constants::total_length.numerical_value_in(ph::m);
    std::optional<double> linear_density = sym::constants::linear_density.numerical_value_in(ph::kg/ph::m);
    std::optional<double> dt = sym::constants::dt.numerical_value_in(ph::s);
    std::optional<double> duration = sym::constants::t1.numerical_value_in(ph::s);
};
STRUCTOPT(options, n, k, b, total_length, linear_density, dt, duration);

int main(int argc, char * argv[]) try
{
    auto options = structopt::app("ropes").parse<::options>(argc, argv);
    // auto const n_points = argc > 1 ? std::stoi(argv[1]) : n;
    // dump_settings(n_points);

    auto const settings = sym::settings{
        options.n.value(),
        options.k.value() * ph::N / ph::m,
        options.b.value() * ph::N * ph::s / ph::m,
        options.total_length.value() * ph::m,
        options.linear_density.value() * ph::kg / ph::m,
        options.dt.value() * ph::s,
        options.duration.value() * ph::s
    };

    dump_settings(settings);

    auto sdl_context = gfx::setup_SDL(800, 600);
    auto & renderer = sdl_context.renderer;

    [[maybe_unused]]
    auto clear_screen = [&] {
        SDL_SetRenderDrawColor(renderer.get(), 0xFF, 0xFF, 0xFF, 0xFF);
        SDL_RenderClear(renderer.get());
    };
    [[maybe_unused]]
    auto update_screen = [&] {
        SDL_RenderPresent(renderer.get());
    };

    auto make_state = [&settings](int i) {
        return ph::state{
            // .x = ph::position{400 * ph::m + settings.segment_length * i, 10. * ph::m},
            .x = ph::position{settings.segment_length * i, 0. * ph::m},
            .v = ph::velocity::zero(),
            .m = settings.segment_mass,
            .fixed = i == 0
        };
    };

    auto rope = std::views::iota(0, settings.number_of_points)
              | std::views::transform(make_state)
              | std::ranges::to<std::vector>();
              ;

    auto quit = false;
    auto pause = false;
    auto step = false;
    auto scale = 1.0;
    for (auto [t, event] = std::tuple{settings.t0, SDL_Event{}}; t < settings.t1 and not quit;) {
        // take time
        auto now = std::chrono::steady_clock::now();
        // clear the screen
        clear_screen();

        // poll events
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT:
                quit = true;
                break;
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                case SDLK_q:
                    quit = true;
                    break;
                case SDLK_p:
                    pause = ! pause;
                    break;
                case SDLK_s:
                    pause = true;
                    step = true;
                    break;
                case SDLK_PLUS:
                case SDLK_KP_PLUS:
                    scale += 0.1;
                    break;
                case SDLK_MINUS:
                case SDLK_KP_MINUS:
                    scale -= 0.1;
                    break;
                }
                break;
            case SDL_MOUSEWHEEL:
                // if (event.wheel.y > 0) {
                //     player.hook.length -= 5;
                // } else if (event.wheel.y < 0) {
                //     player.hook.length += 5;
                // }
                // player.hook.length = std::clamp<physics::scalar>(player.hook.length, 5, 400);
                break;
            default:
                break;
            }
        }

        if (not pause or step) {
            step = false;
            // draw rope
            auto const points = rope
                              | std::views::transform(&ph::state::x)
                              | std::views::transform([scale](auto x) {
                                    return ph::vector<double>{
                                        x[0].numerical_value_in(ph::m) * scale + 400,
                                        x[1].numerical_value_in(ph::m) * scale + 10
                                    };
                              });
            gfx::render(renderer, points);
            // fmt::print("{}\n", points);

            // redraw
            update_screen();

            // update the player
            constexpr auto steps = 4;
            auto const ddt = settings.dt / steps;
            for (auto i = steps; i > 0; --i) {
                // rope = simulate(std::move(rope), ddt);
                rope = sym::integrate(settings, rope, t, ddt);
                t += ddt;
            }
        }

        // wait
        fmt::print("  >>>  Waiting for {}\n", now + std::chrono::milliseconds{value_cast<int>(settings.dt * 1000)} - std::chrono::steady_clock::now());
        std::this_thread::sleep_until(now + std::chrono::milliseconds{value_cast<int>(settings.dt * 1000)});
    }
} catch (structopt::exception const & e) {
    fmt::print("{}\n", e.what());
    fmt::print("{}\n", e.help());
}
