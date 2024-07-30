/**
 * @author      : rbrugo (brugo.riccardo@gmail.com)
 * @file        : main
 * @created     : Friday Jun 21, 2024 22:08:08 CEST
 * @description : 
 */

#ifndef NO_GRAPHICS
#include <SDL_keycode.h>
#include "graphics.hpp"
#else
struct SDL_Event {};
#endif
#include <math.hpp>
#include <thread>
#include <structopt/app.hpp>

#include "simulation.hpp"
#include <mp-units/systems/si/chrono.h>

struct options
{
    std::optional<int> n = sym::constants::n;
    std::optional<double> k = sym::constants::k.numerical_value_in(ph::N/ph::m);
    std::optional<double> b = sym::constants::b.numerical_value_in(ph::N*ph::s/ph::m);
    std::optional<double> c = sym::constants::c.numerical_value_in(ph::N*ph::s/ph::m);
    std::optional<double> total_length = sym::constants::total_length.numerical_value_in(ph::m);
    std::optional<double> linear_density = sym::constants::linear_density.numerical_value_in(ph::kg/ph::m);
    std::optional<double> dt = sym::constants::dt.numerical_value_in(ph::s);
    std::optional<double> fps = sym::constants::fps.numerical_value_in(ph::Hz);
    std::optional<double> duration = sym::constants::t1.numerical_value_in(ph::s);
};
STRUCTOPT(options, n, k, b, c, total_length, linear_density, dt, fps, duration);

int main(int argc, char * argv[]) try
{
    auto options = structopt::app("ropes").parse<::options>(argc, argv);

    auto const settings = sym::settings{
        options.n.value(),
        options.k.value() * ph::N / ph::m,
        options.b.value() * ph::N * ph::s / ph::m,
        options.c.value() * ph::N * ph::s / ph::m,
        options.total_length.value() * ph::m,
        options.linear_density.value() * ph::kg / ph::m,
        options.dt.value() * ph::s,
        options.fps.value() * ph::Hz,
        options.duration.value() * ph::s
    };

    dump_settings(settings);

#ifndef NO_GRAPHICS
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
#endif

    auto make_state = [&settings](int idx) {
        return ph::state{
            .x = ph::position{settings.segment_length * idx, 0. * ph::m},
            .v = ph::velocity::zero(),
            .m = settings.segment_mass,
            .fixed = idx == 0
        };
    };

    auto rope = std::views::iota(0, settings.number_of_points)
              | std::views::transform(make_state)
              | std::ranges::to<std::vector>();
              ;

    auto [quit, pause, step] = std::array{false, false, false};
    auto scale = 5.0;
    auto offset = ph::vector<double>{400, 15};
    auto map_to_screen = [&scale, &offset](ph::position v) -> ph::vector<double> {
        return v.transform([](auto x) static { return x.numerical_value_in(ph::m); }) * scale + offset;
    };
    auto map_from_screen = [&scale, &offset](ph::vector<> const & x) -> ph::position {
        return ((x - offset) / scale) * ph::m;
    };


    // Dragging section data
    struct mouse : math::vector<double, 2> {
        [[nodiscard]] auto x() -> double & { return (*this)[0]; }
        [[nodiscard]] auto y() -> double & { return (*this)[1]; }
        bool clicking = false;
    } mouse;
    struct dragged_info {
        ssize_t index;
        bool was_fixed;
    };
    auto dragged = std::optional<dragged_info>{std::nullopt};
    auto manually_fixed = std::vector<ssize_t>{};  manually_fixed.reserve(5);

    auto const ΔT = 1. / settings.fps;
    auto const δt = settings.dt;
    for (auto [t, event] = std::tuple{settings.t0, SDL_Event{}}; t < settings.t1 and not quit;) {
        // take time
#ifndef NO_GRAPHICS
        auto const now = std::chrono::steady_clock::now();
        auto const end = now + to_chrono_duration(ΔT);

        // clear the screen
        clear_screen();

        // poll events
        while (SDL_PollEvent(&event) != 0) {
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
                mouse.x() = event.motion.x;
                mouse.y() = event.motion.y;
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
#endif

        if (not pause or step) {
            step = false;
            // draw rope
#ifndef NO_GRAPHICS
            auto const points = rope
                              | std::views::transform(&ph::state::x)
                              | std::views::transform(map_to_screen)
                              ;
            gfx::render(renderer, points, settings.segment_length.numerical_value_in(ph::m), scale);

            // redraw
            update_screen();
#endif

            // update the simulation
            auto Δt = ph::duration::zero();
            for (; Δt < ΔT; Δt += δt) {
                rope = sym::integrate(settings, rope, t + Δt, δt);
            }
            t += Δt;
        }

#ifndef NO_GRAPHICS
        // wait
        std::this_thread::sleep_until(end);
#endif
    }
#ifdef NO_GRAPHICS
    fmt::print("{}\n", rope.back());  // avoid optimizing away the computation
#endif
} catch (structopt::exception const & e) {
    fmt::print("{}\n", e.what());
    fmt::print("{}\n", e.help());
}

// cmake --build build --preset conan-release && time build/build/Release/ropes -n=200 --dt=0.001 --duration=25
