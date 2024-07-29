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
    std::optional<double> c = sym::constants::c.numerical_value_in(ph::N*ph::s/ph::m);
    std::optional<double> total_length = sym::constants::total_length.numerical_value_in(ph::m);
    std::optional<double> linear_density = sym::constants::linear_density.numerical_value_in(ph::kg/ph::m);
    std::optional<double> dt = sym::constants::dt.numerical_value_in(ph::s);
    std::optional<double> duration = sym::constants::t1.numerical_value_in(ph::s);
};
STRUCTOPT(options, n, k, b, c, total_length, linear_density, dt, duration);

int main(int argc, char * argv[]) try
{
    auto options = structopt::app("ropes").parse<::options>(argc, argv);
    // auto const n_points = argc > 1 ? std::stoi(argv[1]) : n;
    // dump_settings(n_points);

    auto const settings = sym::settings{
        options.n.value(),
        options.k.value() * ph::N / ph::m,
        options.b.value() * ph::N * ph::s / ph::m,
        options.c.value() * ph::N * ph::s / ph::m,
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
    auto scale = 5.0;
    auto offset = ph::vector<double>{400, 15};
    auto map_to_screen = [&scale, &offset](ph::position v) -> ph::vector<double> {
        return v.transform([](auto x) { return x.numerical_value_in(ph::m); }) * scale + offset;
    };
    auto map_from_screen = [&scale, &offset](ph::vector<> x) -> ph::position {
        return ((x - offset) / scale) * ph::m;
    };


    // Dragging section data
    struct mouse : math::vector<double, 2> {
        double & x = (*this)[0];
        double & y = (*this)[1];
        bool clicking = false;
    } mouse;
    struct dragged_info {
        ssize_t index;
        bool was_fixed;
    };
    auto dragged = std::optional<dragged_info>{std::nullopt};
    auto manually_fixed = std::vector<ssize_t>{};
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
                case SDLK_UP:
                    offset[1] -= 5;
                    break;
                case SDLK_DOWN:
                    offset[1] += 5;
                    break;
                case SDLK_LEFT:
                    offset[0] -= 5;
                    break;
                case SDLK_RIGHT:
                    offset[0] += 5;
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
            case SDL_MOUSEMOTION:
                mouse.x = event.motion.x;
                mouse.y = event.motion.y;
                if (dragged.has_value()) {
                    rope[dragged->index].x = map_from_screen(mouse);
                }
                break;
            case SDL_MOUSEBUTTONUP: {
                switch (event.button.button) {
                    case SDL_BUTTON_LEFT: {
                        if (dragged.has_value()) {
                            rope[dragged->index].fixed = dragged->was_fixed;
                            dragged = std::nullopt;
                        }
                    }
                }
                break;
            }
            case SDL_MOUSEBUTTONDOWN: {
                auto distance = [x0 = map_from_screen(mouse)](ph::position x) {
                    return math::squared_norm(x - x0);
                };
                switch (event.button.button) {
                    // left click to move a point
                    case SDL_BUTTON_LEFT: {
                        auto distances = rope
                                       | std::views::transform(&ph::state::x)
                                       | std::views::transform(distance)
                                       ;
                        mouse.clicking = true;
                        // find the nearest point
                        auto nearest = std::ranges::min_element(distances).base().base();
                        auto idx = std::ranges::distance(rope.begin(), nearest);
                        dragged = dragged_info{ .index = idx, .was_fixed = rope[idx].fixed };
                        // fix the point
                        rope[idx].fixed = true;
                        rope[idx].v = ph::velocity::zero();
                        break;
                    }
                    // right click to statically fix a point,
                    case SDL_BUTTON_RIGHT: {
                        auto fixed = manually_fixed
                                   | std::views::transform([&](auto i) { return rope[i]; })
                                   | std::views::transform(&ph::state::x)
                                   | std::views::transform(distance)
                                   ;
                        auto nearest_locked = std::ranges::min_element(fixed);
                        if (not manually_fixed.empty() and (*nearest_locked).numerical_value_in(ph::m2) * scale <= 100*100) {
                            auto it = nearest_locked.base().base().base().base();
                            auto idx = *it;
                            rope[idx].fixed = false;
                            std::ranges::iter_swap(manually_fixed.end() - 1, it);
                            manually_fixed.pop_back();
                        } else {
                            auto distances = rope
                                | std::views::filter(std::not_fn(&ph::state::fixed))
                                | std::views::transform(&ph::state::x)
                                | std::views::transform(distance)
                                ;
                            auto nearest = std::ranges::min_element(distances).base().base().base();
                            if (nearest == rope.end()) {
                                break;
                            }
                            auto idx = std::ranges::distance(rope.begin(), nearest);
                            manually_fixed.push_back(idx);
                            rope[idx].fixed = true;
                            rope[idx].v = ph::velocity::zero();
                        }
                        break;
                    }
                }
            }
            default:
                break;
            }
        }

        if (not pause or step) {
            step = false;
            // draw rope
            auto const points = rope
                              | std::views::transform(&ph::state::x)
                              | std::views::transform(map_to_screen)
                              ;
            gfx::render(renderer, points, settings.segment_length.numerical_value_in(ph::m), scale);

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
        // fmt::print("  >>>  Waiting for {}\n", now + std::chrono::milliseconds{value_cast<int>(settings.dt * 1000)} - std::chrono::steady_clock::now());
        std::this_thread::sleep_until(now + std::chrono::milliseconds{value_cast<int>(settings.dt * 1000)});

        // TODO: move graphics on another thread!
    }
} catch (structopt::exception const & e) {
    fmt::print("{}\n", e.what());
    fmt::print("{}\n", e.help());
}

// cmake --build build --preset conan-release && time build/build/Release/ropes -n=100 --dt=0.008 --duration=25
