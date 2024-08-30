/**
 * @author      : rbrugo (brugo.riccardo@gmail.com)
 * @file        : parser
 * @created     : Wednesday Aug 14, 2024 17:59:49 CEST
 * @description : 
 * */

#ifndef BRUN_EXPR_EXPRESSION_HPP
#define BRUN_EXPR_EXPRESSION_HPP

#include <fmt/core.h>
#include <memory>
#include <variant>
#include <expected>
#include <functional>
#include <string_view>

#include <fmt/std.h>

namespace brun::expr
{

class expression;

auto parse_expression(std::string_view expr, std::string_view param_names) noexcept -> std::expected<expression, std::string>;

using nothing  = std::monostate;
using const_t  = double;
using unary_f  = std::function<const_t(const_t)>;
using binary_f = std::function<const_t(const_t, const_t)>;
using param_t  = char;

using variant_t = std::variant<nothing, const_t, param_t, unary_f, binary_f>;

struct parameter
{
    param_t name;
    const_t value;
};

struct node
{
    template <typename T>
    requires std::constructible_from<variant_t, T &&>
    explicit node(T && src) : content{std::forward<T>(src)} {}

    variant_t content;
    std::unique_ptr<node> left;
    std::unique_ptr<node> right;
};

class expression
{
    std::unique_ptr<node> _head;

public:
    explicit expression(std::string_view source, std::string_view const param_names);

    [[nodiscard]] auto eval(std::optional<parameter> const & param = std::nullopt) const -> const_t;

    auto operator[](param_t p) const & {
        return [this, p] (const_t value) { return this->eval({{p, value}}); };
    }
    auto operator[](param_t p) && {
        return [e=std::move(*this), p] (const_t value) { return e.eval({{p, value}}); };
    }

    explicit operator bool() const { return _head != nullptr; }
};

} // namespace brun::expr

template <>
struct fmt::formatter<std::function<double(double)>> : fmt::formatter<double>
{
    auto format([[maybe_unused]] auto && _, auto & ctx) const {
        return fmt::format_to(ctx.out(), "<unary-fn>");
    }
};

template <>
struct fmt::formatter<std::function<double(double, double)>> : fmt::formatter<double>
{
    auto format([[maybe_unused]] auto && _, auto & ctx) const {
        return fmt::format_to(ctx.out(), "<binary-fn>");
    }
};

template <typename T>
struct fmt::formatter<std::unique_ptr<T>> : fmt::formatter<T>
{
    static
    auto format(std::unique_ptr<T> const & n, fmt::format_context & ctx) {
        if (not n) {
            return fmt::format_to(ctx.out(), "null");
        }
        return fmt::formatter<T>::format(*n, ctx);
    }
};

template <>
struct fmt::formatter<brun::expr::node> : fmt::formatter<double>
{
    static
    auto format([[maybe_unused]] brun::expr::node const & n, fmt::format_context & ctx)
        -> fmt::format_context::iterator
    {
        return fmt::format_to(ctx.out(), "{} .l({}) .r({})", n.content, n.left, n.right);
    }
};

#endif /* BRUN_EXPR_EXPRESSION_HPP */
