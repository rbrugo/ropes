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
#else
struct SDL_Event {};
#endif
#include <math.hpp>
#include <mp-units/math.h>
#include <thread>
#include <structopt/app.hpp>

#include "simulation.hpp"
#include <mp-units/systems/si/chrono.h>

template <typename Fn, typename ...Ts>
void for_each(std::tuple<Ts...> const & arg, Fn const & fn) {
    auto impl = [&arg, &fn]<int I>(this auto & self, std::integral_constant<int, I>) {
        if constexpr (I < sizeof...(Ts)) {
            fn(std::get<I>(arg));
            self(std::integral_constant<int, I+1>{});
        }
    };
    impl(std::integral_constant<int, 0>{});
}

auto forces_ui(sym::settings & settings) -> gfx::forces_ui_fn
{
    return gfx::forces_ui_fn{settings};
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
};
STRUCTOPT(options, n, k, E, b, c, total_length, diameter, linear_density, dt, fps, duration);

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

    auto const linear_disposition = [] (double t) {
        return ph::vector<double>{t, 0};
    };

    auto make_state = [&settings](auto & fn) {
        return [&fn,&settings](int idx) {
            auto const n = settings.number_of_points;
            return ph::state{
                .x = fn(idx * 1. / n) * n * settings.segment_length,
                .v = ph::velocity::zero(),
                .m = settings.segment_mass,
                .fixed = idx == 0
            };
        };
    };

    auto rope = std::views::iota(0, settings.number_of_points)
              | std::views::transform(make_state(linear_disposition))
              | std::ranges::to<std::vector>();
              ;

    auto [quit, pause, step] = std::array{false, false, false};

    auto config = gfx::screen_config {
        .screen_size = {0, 0},
        .scale = 10.0,
        .offset = {0, -480}  // 400, 15
    };
    auto steps = 0;

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
            ImGui_ImplSDL2_ProcessEvent(&event);
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
                    rope[dragged->index].x = gfx::map_from_screen(config)(mouse);
                }
                break;
            case SDL_MOUSEBUTTONUP: {
                if (ImGui::GetIO().WantCaptureMouse) {
                    break;
                }
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
                if (ImGui::GetIO().WantCaptureMouse) {
                    break;
                }
                auto distance = [x0 = gfx::map_from_screen(config)(mouse)](ph::position x) {
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
                        if (not manually_fixed.empty() and (*nearest_locked).numerical_value_in(ph::m2) * config.scale <= 100*100) {
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

            // draw rope
#ifndef NO_GRAPHICS
        SDL_GetWindowSize(window.get(), &config.screen_size[0], &config.screen_size[1]);  // NOLINT

        auto const points = rope | std::views::transform(&ph::state::x);
        gfx::render(points, settings.segment_length, config);


        /** IMGUI **/
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();


        gfx::draw_window("Data", [&] {
            auto const points = rope
                              | std::views::transform(&ph::state::x)
                              ;
            constexpr auto distance = [](auto && segment) {
                auto [x, y] = segment;
                return math::norm(x - y);
            };
            auto const framerate = 1. * ImGui::GetIO().Framerate * ph::Hz;
            auto energy = [l=settings.segment_length,k=settings.elastic_constant](auto && segment) {
                static auto elongation = [l](auto const & p, auto const & q) {
                    auto const delta = p - q;
                    auto const norm = math::norm(delta);
                    if (abs(norm) < 0.0001 * ph::m) {
                        return ph::position{0 * ph::m, 0 * ph::m};
                    }
                    return delta - l * delta * (1. / norm);
                };
                auto [a, b] = segment;
                auto d = elongation(a.x, b.x);
                mp_units::QuantityOf<isq::energy> auto kinetic = b.m * math::squared_norm(b.v) / 2;
                mp_units::QuantityOf<isq::energy> auto elastic = k * d * d / 2;
                mp_units::QuantityOf<isq::energy> auto gravitational = - (b.m * mp_units::si::standard_gravity * b.x[1]).in(ph::J);
                return math::vector<ph::energy, 2>{kinetic, elastic + gravitational};
            };
            auto total_len = std::ranges::fold_left(
                std::views::adjacent<2>(points) |
                std::views::transform(distance),
                0. * ph::m,
                std::plus{}
            );
            auto [kinetic_energy, potential_energy] = std::ranges::fold_left(
                std::views::adjacent<2>(rope) |
                std::views::transform(energy),
                math::vector<ph::energy, 2>::zero(),
                std::plus{}
            );

            constexpr auto table_flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg;
            auto const args = std::tuple{
                std::tuple{"Time", t},
                std::tuple{"Framerate", framerate},
                std::tuple{"Steps per frame", steps * mp_units::one},
                std::tuple{"Length", total_len},
                std::tuple{"Kinetic energy", kinetic_energy},
                std::tuple{"Potential energy", potential_energy},
                std::tuple{"Total energy", (kinetic_energy + potential_energy)}
            };

            auto print_line = [](auto const & name_value) static {
                auto const [name, value] = name_value;
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%s", name);  // NOLINT
                ImGui::TableNextColumn();
                auto unit = value.unit;
                auto amount = value.numerical_value_in(unit);
                if constexpr (std::same_as<decltype(amount), int>) {
                    ImGui::Text("%+10d %s", amount, fmt::format("{}", unit).c_str());  // NOLINT(*-vararg)
                } else {
                    ImGui::Text("%+10.3f %s", amount, fmt::format("{}", unit).c_str());  // NOLINT(*-vararg)
                }
            };

            constexpr auto columns = std::tuple_size_v<std::tuple_element_t<0, decltype(args)>>;
            if (ImGui::BeginTable("Some data", columns, table_flags)) {
                for_each(args, print_line);
                ImGui::EndTable();
            }
        });

        gfx::draw_window("Forces", forces_ui(settings));

        ImGui::ShowDemoWindow();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());


        // redraw
        update_screen();
#endif

        if (not pause or step) {
            step = false;
            // update the simulation
            auto Δt = ph::duration::zero();
            steps = 0;
            for (; Δt < ΔT; Δt += δt) {
                rope = sym::integrate(settings, rope, t + Δt, δt);
                ++steps;
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
