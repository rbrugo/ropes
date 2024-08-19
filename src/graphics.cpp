// NOLINTBEGIN(concurrency-mt-unsafe)
/**
 * @author      : rbrugo (brugo.riccardo@gmail.com)
 * @file        : graphics
 * @created     : Saturday Jul 06, 2024 19:26:31 CEST
 * @description : 
 */

#include <SDL_video.h>
#include <fmt/core.h>
#include <nonstd/scope.hpp>
#include "graphics.hpp"
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <experimental/scope>

#include <imgui.h>
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

#include <simulation.hpp>

namespace gfx
{

[[nodiscard]]
auto setup_imgui(auto const & glsl_version, auto const & window, auto const & gl_context)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    // auto & io = ImGui::GetIO();
    ImGui::StyleColorsDark();

    ImGui_ImplSDL2_InitForOpenGL(window.get(), gl_context.get());
    ImGui_ImplOpenGL3_Init(glsl_version);
}

[[nodiscard]]
auto setup_SDL(int screen_width, int screen_height) -> SDL_stuff
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fmt::print("SDL could not initialize! SDL_Error: {}\n", SDL_GetError());
        std::exit(1);
    }
    auto _1 = nonstd::make_scope_exit(SDL_Quit);
    if (TTF_Init() == -1) {
        fmt::print("SDL_ttf could not initialize! SDL_ttf error: {}\n", TTF_GetError());
        std::exit(1);
    }
    auto _2 = nonstd::make_scope_exit(TTF_Quit);

    // GL 3.0 + GLSL 130
    constexpr auto glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    auto window_flags = static_cast<SDL_WindowFlags>(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);  // NOLINT

    auto window = std::unique_ptr<SDL_Window, void(*)(SDL_Window *)>{
        SDL_CreateWindow("Ropes",
                         SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                         screen_width, screen_height, window_flags/*SDL_WINDOW_SHOWN*/),
        SDL_DestroyWindow
    };

    if (not window) {
        fmt::print("Window could not be created! SDL_Error: {}\n", SDL_GetError());
        std::exit(1);
    }

    auto gl_context = std::unique_ptr<void, void(*)(void *)>{
            SDL_GL_CreateContext(window.get()), SDL_GL_DeleteContext
    };
    if (not gl_context) {
        fmt::print("Could not create OpenGL context! Error: {}\n", SDL_GetError());
        std::exit(1);
    }
    SDL_GL_MakeCurrent(window.get(), gl_context.get());
    SDL_GL_SetSwapInterval(1);

    setup_imgui(glsl_version, window, gl_context);

    return {
        { std::move(_1), std::move(_2) },
        std::move(window), std::move(gl_context), nonstd::scope_exit{+[] static {
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplSDL2_Shutdown();
            ImGui::DestroyContext();
        }}
    };
}

void forces_ui_fn::operator()() const noexcept
{
#define MAYBE_ENABLED(cond, fn) /* NOLINT */ \
    if (not (cond)) { \
        ImGui::BeginDisabled(); \
        (fn); \
        ImGui::EndDisabled(); \
    } else { \
        (fn); \
    }
#define REF(arg, unit) &settings->arg.numerical_value_ref_in(unit)  // NOLINT

    auto & enable = settings->enabled;

    gfx::tree_node("Elastic force", [&] {
        constexpr auto min = 0.;
        constexpr auto max = 10'000.;
        ImGui::Checkbox("Enable force", &enable.elastic);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.5f);
        MAYBE_ENABLED(
            enable.elastic,
            ImGui::SliderScalar("Elastic constant (k)", ImGuiDataType_Double, REF(elastic_constant, ph::N / ph::m), &min, &max, "%.2lf N/m")
        );
        if (ImGui::Button("Reset")) {
            settings->elastic_constant = sym::constants::k; // FIXME: real value is set on startup
        }
    });
    gfx::tree_node("Gravity", [&] {
        ImGui::Checkbox("Enable force", &enable.gravity);
    });
    gfx::tree_node("External damping", [&] {
        constexpr auto min = 0.;
        constexpr auto max = 1.;
        ImGui::Checkbox("Enable force", &enable.external_damping);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.5f);
        MAYBE_ENABLED(
            enable.external_damping,
            ImGui::SliderScalar("Internal damping (b)", ImGuiDataType_Double, REF(external_damping, ph::N * ph::s / ph::m), &min, &max, "%.2lf N·s/m")
        );
        if (ImGui::Button("Reset")) {
            settings->external_damping = sym::constants::b; // FIXME: real value is set on startup
        }
    });
    gfx::tree_node("Internal damping", [&] {
        constexpr auto min = 0.;
        constexpr auto max = 1.;
        ImGui::Checkbox("Enable force", &enable.internal_damping);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.5f);
        MAYBE_ENABLED(
            enable.internal_damping,
            ImGui::SliderScalar("Internal damping (c)", ImGuiDataType_Double, REF(internal_damping, ph::N * ph::s / ph::m), &min, &max, "%.2lf N·s/m")
        );
        if (ImGui::Button("Reset")) {
            settings->internal_damping = sym::constants::c; // FIXME: real value is set on startup
        }
    });
    gfx::tree_node("Flexural rigidity", [&] {
        constexpr auto E_min = 0.;
        constexpr auto E_max = 10.;
        constexpr auto r_min = 0.4;
        constexpr auto r_max = 18.;
        ImGui::Checkbox("Enable force", &enable.flexural_rigidity);
        ImGui::SetNextItemWidth(100);
        MAYBE_ENABLED(
            enable.flexural_rigidity,
            ImGui::SliderScalar("Young modulus (E)", ImGuiDataType_Double, REF(young_modulus, ph::GPa), &E_min, &E_max, "%.2lf GPa")
        );

        auto windowWidth = ImGui::GetWindowSize().x;
        auto buttonWidth = ImGui::CalcTextSize("Reset").x + ImGui::GetStyle().FramePadding.x * 2;

        ImGui::SameLine();
        ImGui::SetCursorPosX(windowWidth - buttonWidth - 2 * ImGui::GetStyle().ItemSpacing.x);
        if (ImGui::Button("Reset")) {
            settings->young_modulus = sym::constants::E; // FIXME: real value is set on startup
        }
        ImGui::SetNextItemWidth(100);
        MAYBE_ENABLED(
            enable.flexural_rigidity,
            ImGui::SliderScalar("Rope diameter (mm)", ImGuiDataType_Double, REF(diameter, ph::mm), &r_min, &r_max, "%.2lf mm")
        );
        ImGui::SameLine();
        ImGui::SetCursorPosX(windowWidth - buttonWidth - 2 * ImGui::GetStyle().ItemSpacing.x);
        if (ImGui::Button("Reset")) {
            settings->diameter = sym::constants::diameter; // FIXME: real value is set on startup
        }
    });

    #undef REF
    #undef MAYBE_ENABLED
}


// void draw_arrow(
//     gfx::renderer & renderer,
//     int x, int y,
//     std::array<double, 2> const & v, std::array<int, 3> color
// ) noexcept {
//     SDL_SetRenderDrawColor(renderer.get(), color[0], color[1], color[2], 0xFF);
//     SDL_RenderDrawLine(renderer.get(), x, y, x + v[0], y + v[1]);
//     auto const rect = SDL_Rect(x + v[0] - 4, y + v[1] - 4, 8, 8);
//     SDL_RenderFillRect(renderer.get(), &rect);
// }

}  // namespace gfx
// NOLINTEND(concurrency-mt-unsafe)
