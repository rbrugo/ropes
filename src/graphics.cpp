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
#include <SDL2/SDL_ttf.h>
#include <experimental/scope>

#include <imgui.h>
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include <implot.h>

#include <mp-units/math.h>

#include <simulation.hpp>
#include <expression.hpp>

// NOLINTBEGIN(concurrency-mt-unsafe)
namespace gfx
{

[[nodiscard]]
auto setup_imgui(auto const & glsl_version, auto const & window, auto const & gl_context)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
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
            ImPlot::DestroyContext();
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

    gfx::tree_node("Elastic force", enable.elastic, [&] {
        constexpr auto min = 0.;
        constexpr auto max = 10'000.;
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.5f);
        MAYBE_ENABLED(
            enable.elastic,
            ImGui::SliderScalar("Elastic constant (k)", ImGuiDataType_Double, REF(elastic_constant, ph::N / ph::m), &min, &max, "%.2lf N/m")
        );
        if (ImGui::Button("Reset")) {
            settings->elastic_constant = initial_settings->elastic_constant;
        }
    });
    gfx::tree_node("Gravity", enable.gravity, [] {});
    gfx::tree_node("External damping", enable.external_damping, [&] {
        constexpr auto min = 0.;
        constexpr auto max = 1.;
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.5f);
        MAYBE_ENABLED(
            enable.external_damping,
            ImGui::SliderScalar("External damping (b)", ImGuiDataType_Double, REF(external_damping, ph::N * ph::s / ph::m), &min, &max, "%.2lf N·s/m")
        );
        if (ImGui::Button("Reset")) {
            settings->external_damping = initial_settings->external_damping;
        }
    });
    gfx::tree_node("Internal damping", enable.internal_damping, [&] {
        constexpr auto min = 0.;
        constexpr auto max = 1.;
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.5f);
        MAYBE_ENABLED(
            enable.internal_damping,
            ImGui::SliderScalar("Internal damping (c)", ImGuiDataType_Double, REF(internal_damping, ph::N * ph::s / ph::m), &min, &max, "%.2lf N·s/m")
        );
        if (ImGui::Button("Reset")) {
            settings->internal_damping = initial_settings->internal_damping;
        }
    });
    gfx::tree_node("Flexural rigidity", enable.flexural_rigidity, [&] {
        constexpr auto E_min = 0.;
        constexpr auto E_max = 10.;
        constexpr auto r_min = 0.4;
        constexpr auto r_max = 18.;
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
            settings->young_modulus = initial_settings->young_modulus;
        }
        ImGui::SetNextItemWidth(100);
        MAYBE_ENABLED(
            enable.flexural_rigidity,
            ImGui::SliderScalar("Rope diameter (mm)", ImGuiDataType_Double, REF(diameter, ph::mm), &r_min, &r_max, "%.2lf mm")
        );
        ImGui::SameLine();
        ImGui::SetCursorPosX(windowWidth - buttonWidth - 2 * ImGui::GetStyle().ItemSpacing.x);
        if (ImGui::Button("Reset")) {
            settings->diameter = initial_settings->diameter;
        }
    });

    #undef REF
    #undef MAYBE_ENABLED
}

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


void data_ui_fn::operator()() const noexcept
{
    constexpr auto distance = [](auto && segment) {
        auto [x, y] = segment;
        return math::norm(x - y);
    };
    auto const framerate = 1. * ImGui::GetIO().Framerate * ph::Hz;
    auto energy = [l=settings->segment_length,k=settings->elastic_constant](auto && segment) {
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
            *rope | std::views::transform(&ph::state::x) |
            std::views::adjacent<2> |
            std::views::transform(distance),
            0. * ph::m,
            std::plus{}
            );
    auto [kinetic_energy, potential_energy] = std::ranges::fold_left(
            std::views::adjacent<2>(*rope) |
            std::views::transform(energy),
            math::vector<ph::energy, 2>::zero(),
            std::plus{}
            );

    constexpr auto table_flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg;
    auto const args = std::tuple{
        std::tuple{"Time", t},
        std::tuple{"Framerate", framerate},
        std::tuple{"Steps per frame", steps * mp_units::one},
        // std::tuple{"Offset", screen_cfg->offset * mp_units::one},
        // std::tuple{"Scale", screen_cfg->scale * mp_units::one},
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
        auto [amount, unit] = [&] {
            if constexpr (requires { { value.unit }; }) {
                auto unit = value.unit;
                return std::pair{ value.numerical_value_in(unit), unit };
            } else if constexpr (requires { { value[0].unit }; }) {
                auto unit = value[0].unit;
                return std::pair{ value.transform(ph::numerical_value_in(unit)), unit };
            } else {
                return std::pair{ value, "" };
            }
        }();
        using amount_t = std::remove_cvref_t<decltype(amount)>;
        if constexpr (math::concepts::vector<amount_t>) {
            static_assert(amount_t::size() == 2, "implemented for 2D vectors");
            using amount_value_t = typename amount_t::value_type;
            auto partial = fmt::format("{:.2f} {:.2f}", amount[0], amount[1]);
            while (partial.size() < 10) { partial.push_back(' '); }
            ImGui::Text("%s %s", partial.c_str(), fmt::to_string(unit).c_str());  // NOLINT(*-vararg)
        }
        else if constexpr (std::same_as<decltype(amount), int>) {
            ImGui::Text("%+10d %s", amount, fmt::to_string(unit).c_str());  // NOLINT(*-vararg)
        } else {
            ImGui::Text("%+10.3f %s", amount, fmt::to_string(unit).c_str());  // NOLINT(*-vararg)
        }
    };

    constexpr auto columns = std::tuple_size_v<std::tuple_element_t<0, decltype(args)>>;
    if (ImGui::BeginTable("Some data", columns, table_flags)) {
        for_each(args, print_line);
        ImGui::EndTable();
    }
}

void rope_editor_fn::operator()() noexcept
{
    using maybe_expression = std::expected<brun::expr::expression, std::string>;
    static auto update = true;
    auto & x = settings->x_formula;
    auto & y = settings->y_formula;
    static auto x_formula = [&x] {
        auto base = std::array<char, 128>{};
        std::ranges::copy(x, base.begin());
        return base;
    }();
    static auto y_formula = [&y] {
        auto base = std::array<char, 128>{};
        std::ranges::copy(y, base.begin());
        return base;
    }();
    static auto x_expr = maybe_expression{brun::expr::expression{x, "t"}};
    static auto y_expr = maybe_expression{brun::expr::expression{y, "t"}};
    auto & equalize_distance = settings->equalize_distance;

    ImGui::InputTextWithHint("= x(t)", x.data(), x_formula.data(), x_formula.size());
    ImGui::InputTextWithHint("= y(t)", y.data(), y_formula.data(), y_formula.size());
    ImGui::Checkbox("Equalize points distance", &equalize_distance);


    auto eval = [](auto & arr) -> maybe_expression {
        auto ptr = arr.data();
        return brun::expr::parse_expression(std::string_view{ptr, std::strlen(ptr)}, "t");
    };

    auto preview = ImGui::Button("Preview");
    ImGui::SameLine();
    auto apply = ImGui::Button("Reset time and apply formulas");

    if (preview or apply) {
        update = true;
        x_expr = eval(x_formula);
        y_expr = eval(y_formula);
        if (not x_expr.has_value() or not y_expr.has_value()) {
            apply = false;
            fmt::print("Bad formula!\n");
            ImVec2 center = ImGui::GetMainViewport()->GetCenter();
            ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
            ImGui::OpenPopup("Bad formula");
        }
    }

    if (apply) {
        settings->x_formula = x_formula | std::ranges::to<std::string>();
        settings->y_formula = y_formula | std::ranges::to<std::string>();
        sym::reset(*settings, *rope, *metadata, *t);
    }

    if (auto h = ImGui::GetContentRegionAvail().x; ImPlot::BeginPlot("Equalized", ImVec2(h, h))) {
        constexpr auto stride = sizeof(math::vector<double, 2>);
        static auto equidistant_points = std::vector<math::vector<double, 2>>{};
        static auto original_xs = std::vector<double>{};
        static auto original_ys = std::vector<double>{};
        static auto equalized = std::vector<math::vector<double, 2>>{};
        static auto original = std::vector<math::vector<double, 2>>{};

        auto bad_formula = not x_expr.has_value() or not y_expr.has_value();
        if (update and not bad_formula) {
            auto fn = [x=(*x_expr)['t'],y=(*y_expr)['t']] (auto n) {
                return math::vector<double, 2>{x(n), y(n)};
            };
            equidistant_points = sym::equidistant_points_along_function(fn, 100);
            original = std::views::iota(0, 11)
                | std::views::transform([](auto i) { return double(i) / 10; })
                | std::views::transform(fn)
                | std::ranges::to<std::vector>();
            equalized = equidistant_points | std::views::stride(equidistant_points.size() / 10) | std::ranges::to<std::vector>();
        }

#define PLT(type, name, rng) \
        ImPlot::type(name, &(rng)[0][0], &(rng)[0][1], static_cast<int>((rng).size()), 0, 0, stride)
        PLT(PlotLine, "Rope line", equidistant_points);
        PLT(PlotScatter, "Points", original);
        PLT(PlotScatter, "Equalized points", equalized);
#undef PLT
        ImPlot::EndPlot();
    }


    if (ImGui::BeginPopupModal("Bad formula", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        auto bad_x = not x_expr.has_value();
        auto bad_y = not y_expr.has_value();
        if (bad_x and bad_y) {
            ImGui::Text("Both x(t) and y(t) formulas failed to compile due to errors:");  // NOLINT(*-vararg)
            ImGui::Text("x: %s", x_expr.error().c_str());  // NOLINT(*-vararg)
            ImGui::Text("y: %s", y_expr.error().c_str());  // NOLINT(*-vararg)
        } else {
            ImGui::Text("%s(t) failed to compile due to error:", bad_x ? "x" : "y");  // NOLINT(*-vararg)
            ImGui::Text("%c: %s", bad_x ? 'x' : 'y', (bad_x ? x_expr : y_expr).error().c_str());  // NOLINT(*-vararg)
        }
        if (ImGui::Button("OK", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
    }

    if (update) {
        update = false;
    }
}

#define IN_N(MEMBER) \
    [](ph::metadata const & md) { return md.MEMBER.transform(ph::numerical_value_in(ph::N)); }

arrows_ui::arrows_ui() :
    arrows{
        { "gravity", IN_N(gravitational), 1.f, math::vector{255, 127, 255} },
        { "elastic", IN_N(elastic), 1.f, math::vector{255, 255, 127} },
        { "internal damping", IN_N(internal_damping), 1.f, math::vector{127, 127, 255} },
        { "external damping", IN_N(external_damping), 1.f, math::vector{127, 255, 127} },
        { "bending stiffness", IN_N(bending_stiffness), 1.f, math::vector{127, 255, 255} },
        { "total", IN_N(total), 1.f, math::vector{255, 127, 127} },
    }
{}
#undef IN_N

void arrows_ui::operator()() noexcept
{
    constexpr auto color_edit_flags = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel;
    static auto stride_selected = static_cast<int>(stride.index());
    // TODO: do I want to re-select every time? This way I can cache the stride float value
    static auto once_every_n = int32_t{10};
    static auto once_every_fraction = float{0.1};
    constexpr auto steps = std::array{ 1, 10 };
    // Option 1: draw every point
    bool changed = false;
    changed = ImGui::RadioButton("Draw all", &stride_selected, 0) or changed;
    // Option 2: draw once every N point
    changed = ImGui::RadioButton("Draw once every##1", &stride_selected, 1) or changed;
    ImGui::SameLine();
    if (ImGui::InputScalar("points", ImGuiDataType_S32, &once_every_n, &steps[0], &steps[1], "%u")) {
        changed = true;
        stride_selected = 1;
    }
    // Option 3: draw once every X%
    changed = ImGui::RadioButton("Draw once every##2", &stride_selected, 2) or changed;
    ImGui::SameLine();
    if (ImGui::SliderFloat(
        "fraction", &once_every_fraction, 0.01f, 1.f, "%.3f", ImGuiSliderFlags_Logarithmic
    )) {
        changed = true;
        stride_selected = 2;
    }

    if (changed) {
        switch (stride_selected) {
        case 0:
            stride = std::monostate{};
            break;
        case 1:
            once_every_n = std::max(once_every_n, 1);
            stride.emplace<1>(once_every_n);
            break;
        case 2:
            once_every_fraction = std::max(once_every_fraction, 0.001f);
            stride.emplace<2>(once_every_fraction);
            break;
        default:
            std::unreachable();
        }
    }
    auto single_arrow_row = [](arrow_settings & s) {
        constexpr auto min = 0.01f;
        constexpr auto max = 100.f;
        ImGui::Checkbox(fmt::format("##CheckBoxFor{}", s.name).c_str(), &s.enabled);
        ImGui::SameLine();
        ImGui::ColorEdit3(fmt::format("##ColorFor{}", s.name).c_str(), s.color.data(), color_edit_flags);
        ImGui::SameLine();
        ImGui::TextUnformatted(s.name.c_str());
        ImGui::SameLine();

        auto available_width = ImGui::GetContentRegionAvail().x;
        auto max_width = ImGui::GetWindowSize().x - 210;
        // auto item_width = std::min(available_width, max_width);
        ImGui::SameLine(200);
        ImGui::PushItemWidth(max_width);
        ImGui::SliderScalar(fmt::format("##SliderFor{}", s.name).c_str(), ImGuiDataType_Float, &s.scale, &min, &max, "scale: %.2lf");
    };
    for (auto & arrow : arrows) {
        single_arrow_row(arrow);
    }
}

}  // namespace gfx
// NOLINTEND(concurrency-mt-unsafe)
