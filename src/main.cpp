/**
 * @author      : rbrugo (brugo.riccardo@gmail.com)
 * @file        : main
 * @created     : Friday Jun 21, 2024 22:08:08 CEST
 * @description : 
 */

#ifndef NO_GRAPHICS
#include <SDL_keycode.h>
#include "graphics.hpp"
#include <GL/gl.h>

#include <imgui.h>
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include <implot.h>
#else
struct SDL_Event {};
#endif
#include <math.hpp>
#include <mp-units/math.h>
#include <thread>
#include <expected>
#include <structopt/app.hpp>

#include "simulation.hpp"
#include <mp-units/systems/si/chrono.h>

#include <expression.hpp>

auto forces_ui(sym::settings & settings) -> gfx::forces_ui_fn
{
    return gfx::forces_ui_fn{settings};
}

auto rope_editor_ui(sym::settings const & settings,
                    auto & rope, auto & metadata, ph::duration & time,
                    std::string_view x_opt, std::string_view y_opt
) -> gfx::rope_editor_fn {
    return gfx::rope_editor_fn{settings, rope, metadata, time, x_opt, y_opt};
}

auto data_ui(sym::settings const & settings, auto const & rope, ph::time t, int steps)
    -> gfx::data_ui_fn
{
    return gfx::data_ui_fn{settings, rope, t, steps};
}
struct options
{
    std::optional<int> n = sym::constants::n;
    std::optional<double> k = sym::constants::k.numerical_value_in(ph::N/ph::m);
    std::optional<double> E = sym::constants::E.numerical_value_in(ph::GPa);
    std::optional<double> b = sym::constants::b.numerical_value_in(ph::N*ph::s/ph::m);
    std::optional<double> c = sym::constants::c.numerical_value_in(ph::N*ph::s/ph::m);
    std::optional<double> total_length = sym::constants::total_length.numerical_value_in(ph::m);
    std::optional<double> diameter = sym::constants::diameter.numerical_value_in(ph::mm);
    std::optional<double> linear_density = sym::constants::linear_density.numerical_value_in(ph::kg/ph::m);
    std::optional<double> dt = sym::constants::dt.numerical_value_in(ph::s);
    std::optional<double> fps = sym::constants::fps.numerical_value_in(ph::Hz);
    std::optional<double> duration = sym::constants::t1.numerical_value_in(ph::s);
    std::optional<bool> pause = false;
    std::optional<std::string> x = "t";
    std::optional<std::string> y = "0";
};
STRUCTOPT(options, n, k, E, b, c, total_length, diameter, linear_density, dt, fps, duration, pause, x, y);

int main(int argc, char * argv[]) try  // NOLINT
{
    auto options = structopt::app("ropes").parse<::options>(argc, argv);

    auto settings = sym::settings{
        options.n.value(),
        options.k.value() * ph::N / ph::m,
        options.E.value() * ph::GPa,
        options.b.value() * ph::N * ph::s / ph::m,
        options.c.value() * ph::N * ph::s / ph::m,
        options.total_length.value() * ph::m,
        options.diameter.value() * ph::mm,
        options.linear_density.value() * ph::kg / ph::m,
        options.dt.value() * ph::s,
        options.fps.value() * ph::Hz,
        options.duration.value() * ph::s
    };
    constexpr auto get_metadata = true;

    dump_settings(settings);

#ifndef NO_GRAPHICS
    constexpr auto screen_width = 800;
    constexpr auto screen_height = 600;
    auto sdl_context = gfx::setup_SDL(screen_width, screen_height);
    auto & window = sdl_context.window;

    auto clear_screen = [] static {
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    };
    auto update_screen = [&window] {
        SDL_GL_SwapWindow(window.get());
    };
#endif

    auto x_expr = brun::expr::parse_expression(options.x.value(), "t");
    auto y_expr = brun::expr::parse_expression(options.y.value(), "t");
    auto fn = [x=x_expr.value()['t'], y=y_expr.value()['t']](auto t) { return ph::vector<>{x(t), -y(t)}; };
    auto rope = sym::construct_rope(settings, fn);
    auto metadata = std::vector<ph::metadata>{};

    auto [quit, pause, step] = std::array{false, *options.pause, false};

    auto config = gfx::screen_config {
        .screen_size = {0, 0},
        .scale = 10.0,
        .offset = {0, -480}  // 400, 15
    };
    auto steps = 0;

    // Dragging section data
    // struct mouse : math::vector<double, 2> {
    //     [[nodiscard]] auto x() -> double & { return (*this)[0]; }
    //     [[nodiscard]] auto y() -> double & { return (*this)[1]; }
    //     bool clicking = false;
    // } mouse;
    // struct dragged_info {
    //     ssize_t index;
    //     bool was_fixed;
    // };
    // auto dragged = std::optional<dragged_info>{std::nullopt};
    auto manually_fixed = std::vector<ssize_t>{};  manually_fixed.reserve(5);

    auto const ΔT = 1. / settings.fps;
    auto const δt = settings.dt;

    using clock_t = std::chrono::system_clock;
    using duration_t = decltype(to_chrono_duration(ΔT));
    auto begin = std::chrono::time_point<clock_t, duration_t>(clock_t::now());
    for (auto [t, event] = std::tuple{settings.t0, SDL_Event{}}; t < settings.t1 and not quit;) {
#ifndef NO_GRAPHICS
        // clear the screen
        clear_screen();

        // poll events
        while (SDL_PollEvent(&event) != 0) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            switch (event.type) {
            case SDL_QUIT:
                quit = true;
                break;
            case SDL_KEYDOWN:
                if (ImGui::GetIO().WantCaptureKeyboard) {
                    continue;
                }
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
                case SDLK_r:
                    sym::reset(settings, rope, metadata, t);
                    if ((event.key.keysym.mod & KMOD_SHIFT) != 0) {
                        pause = true;
                    }
                    break;
                case SDLK_PLUS:
                case SDLK_KP_PLUS:
                    config.scale += 0.1;
                    break;
                case SDLK_MINUS:
                case SDLK_KP_MINUS:
                    config.scale -= 0.1;
                    break;
                case SDLK_UP:
                    config.offset[1] -= 5;
                    break;
                case SDLK_DOWN:
                    config.offset[1] += 5;
                    break;
                case SDLK_LEFT:
                    config.offset[0] -= 5;
                    break;
                case SDLK_RIGHT:
                    config.offset[0] += 5;
                    break;
                }
                break;
            case SDL_MOUSEWHEEL:
                if (ImGui::GetIO().WantCaptureMouse) {
                    continue;
                }
                if (event.wheel.y > 0) {
                    config.scale += 0.5;
                } else if (event.wheel.y < 0) {
                    config.scale -= 0.5;
                }
                // player.hook.length = std::clamp<physics::scalar>(player.hook.length, 5, 400);
                break;
            // case SDL_MOUSEMOTION:
            //     mouse.x() = event.motion.x;
            //     mouse.y() = event.motion.y;
            //     if (dragged.has_value()) {
            //         rope[dragged->index].x = gfx::map_from_screen(config)(mouse);
            //     }
            //     break;
            // case SDL_MOUSEBUTTONUP: {
            //     if (ImGui::GetIO().WantCaptureMouse) {
            //         break;
            //     }
            //     switch (event.button.button) {
            //         case SDL_BUTTON_LEFT: {
            //             if (dragged.has_value()) {
            //                 rope[dragged->index].fixed = dragged->was_fixed;
            //                 dragged = std::nullopt;
            //             }
            //         }
            //     }
            //     break;
            // }
            // case SDL_MOUSEBUTTONDOWN: {
            //     if (ImGui::GetIO().WantCaptureMouse) {
            //         break;
            //     }
            //     auto distance = [x0 = gfx::map_from_screen(config)(mouse)](ph::position x) {
            //         return math::squared_norm(x - x0);
            //     };
            //     switch (event.button.button) {
            //         // left click to move a point
            //         case SDL_BUTTON_LEFT: {
            //             auto distances = rope
            //                            | std::views::transform(&ph::state::x)
            //                            | std::views::transform(distance)
            //                            ;
            //             mouse.clicking = true;
            //             // find the nearest point
            //             auto nearest = std::ranges::min_element(distances).base().base();
            //             auto idx = std::ranges::distance(rope.begin(), nearest);
            //             dragged = dragged_info{ .index = idx, .was_fixed = rope[idx].fixed };
            //             // fix the point
            //             rope[idx].fixed = true;
            //             rope[idx].v = ph::velocity::zero();
            //             break;
            //         }
            //         // right click to statically fix a point,
            //         case SDL_BUTTON_RIGHT: {
            //             auto fixed = manually_fixed
            //                        | std::views::transform([&](auto i) { return rope[i]; })
            //                        | std::views::transform(&ph::state::x)
            //                        | std::views::transform(distance)
            //                        ;
            //             auto nearest_locked = std::ranges::min_element(fixed);
            //             if (not manually_fixed.empty() and (*nearest_locked).numerical_value_in(ph::m2) * config.scale <= 100*100) {
            //                 auto it = nearest_locked.base().base().base().base();
            //                 auto idx = *it;
            //                 rope[idx].fixed = false;
            //                 std::ranges::iter_swap(manually_fixed.end() - 1, it);
            //                 manually_fixed.pop_back();
            //             } else {
            //                 auto distances = rope
            //                     | std::views::filter(std::not_fn(&ph::state::fixed))
            //                     | std::views::transform(&ph::state::x)
            //                     | std::views::transform(distance)
            //                     ;
            //                 auto nearest = std::ranges::min_element(distances).base().base().base();
            //                 if (nearest == rope.end()) {
            //                     break;
            //                 }
            //                 auto idx = std::ranges::distance(rope.begin(), nearest);
            //                 manually_fixed.push_back(idx);
            //                 rope[idx].fixed = true;
            //                 rope[idx].v = ph::velocity::zero();
            //             }
            //             break;
            //         }
            //     }
            // }
            default:
                break;
            }
        }
#endif

            // draw rope
#ifndef NO_GRAPHICS
        SDL_GetWindowSize(window.get(), &config.screen_size[0], &config.screen_size[1]);  // NOLINT

        // TODO:
        // - `gfx::render` must render rope and metadata
        // - the metadata to render must be selectable from UI (eventually strided)
        // - make a table with metadata relative to a bunch of selected points
        auto const points = rope | std::views::transform(&ph::state::x);
        gfx::render(points, settings.segment_length, config);

        if (get_metadata) {
            auto as_numbers = [](auto const & meta) {
                return meta.total.transform([](auto const & f) { return f.numerical_value_in(ph::N); });
            };
            auto sizes = metadata
                       | std::views::transform(as_numbers)
                       | std::views::transform([](auto x) { return math::vector{x[0], -x[1]}; });
            auto pairs = std::views::zip(points, sizes) | std::views::stride(5);
            for (auto const & [from, size] : pairs) {
                gfx::draw_arrow(from, size, config);
            }
        }


        /** IMGUI **/
        auto const dragging = ImGui::IsMouseDragging(ImGuiMouseButton_Left);
        SDL_GL_SetSwapInterval(dragging ? 0 : 1);
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();


        gfx::draw_window("Data", data_ui(settings, rope, t, steps));
        gfx::draw_window("Forces", forces_ui(settings));
        gfx::draw_window("Rope",  rope_editor_ui(settings, rope, metadata, t, *options.x, *options.y));

        // ImGui::ShowDemoWindow();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());


        // redraw
        update_screen();
#endif

        if (not pause or step) {
            auto const now = std::chrono::system_clock::now();
            while (now - begin >= to_chrono_duration(ΔT)) {
                begin += to_chrono_duration(ΔT);
                step = false;
                // update the simulation
                auto Δt = ph::duration::zero();
                steps = 0;
                for (; Δt < ΔT; Δt += δt) {
                    auto res = sym::integrate(settings, rope, t + Δt, δt, get_metadata);
                    rope = std::move(res.state);
                    metadata = std::move(res.metadata);
                    ++steps;
                }
                t += Δt;
            }
        }

#ifndef NO_GRAPHICS
        // TODO: does it still make sense to set fps?
        // wait
        // if (settings.fps < 60 * ph::Hz) {
        //     std::this_thread::sleep_until(end);
        // }
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
