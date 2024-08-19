/**
 * @author      : rbrugo (brugo.riccardo@gmail.com)
 * @file        : parser
 * @created     : Wednesday Aug 14, 2024 20:59:06 CEST
 * @description : 
 */

#include <expression.hpp>
#include <numbers>
#include <ranges>
#include <cmath>

namespace brun::expr
{

auto parse_expression(std::string_view expr, std::string_view param_names) noexcept
    -> std::expected<expression, std::string>
{
    try {
        return brun::expr::expression{expr, param_names};
    } catch (std::logic_error const & exc) {
        return std::unexpected(exc.what());
    }
};

namespace detail
{
template <class... Args> struct overload : Args... { using Args::operator()...; };
template <class... Args> overload(Args...) -> overload<Args...>;

constexpr auto sign_priority(char x) noexcept
{
    constexpr auto operator_lower = std::string_view{"+-"};
    constexpr auto operator_higher = std::string_view{"*/"};
    constexpr auto power = '^';
    constexpr auto function = std::string_view{"sctelav"};
    if (operator_lower.contains(x)) { return 0; }
    if (operator_higher.contains(x)) { return 1; }
    if (power == x) { return 2; }
    if (function.contains(static_cast<char>(std::tolower(static_cast<int>(x))))) { return 3; }
    return -1;  // '%'
}

constexpr bool stronger_sign(char A, char B) noexcept
{
    return sign_priority(A) > sign_priority(B);
}

constexpr bool is_binary_f(char ch)
{
    return std::string_view{"+-*/^%"}.contains(ch);
}

}  // namespace detail


namespace function
{

constexpr inline auto plus        = [](const_t const & a, const_t const & b) { return a + b; };
constexpr inline auto minus       = [](const_t const & a, const_t const & b) { return a - b; };
constexpr inline auto multiplies  = [](const_t const & a, const_t const & b) { return a * b; };
constexpr inline auto divides     = [](const_t const & a, const_t const & b) { return a / b; };
constexpr inline auto modulus     = [](const_t const & a, const_t const & b) { return static_cast<long>(a) % static_cast<long>(b); };
constexpr inline auto pow         = [](const_t const & a, const_t const & b) { return std::pow(a,b); };

constexpr inline auto square      = [](const_t const & a) { return a * a; };
constexpr inline auto cube        = [](const_t const & a) { return a * a * a; };
constexpr inline auto sin         = [](const_t const & a) { return std::sin(a); };
constexpr inline auto cos         = [](const_t const & a) { return std::cos(a); };
constexpr inline auto tan         = [](const_t const & a) { return std::tan(a); };
constexpr inline auto asin        = [](const_t const & a) { return std::asin(a); };
constexpr inline auto acos        = [](const_t const & a) { return std::acos(a); };
constexpr inline auto atan        = [](const_t const & a) { return std::atan(a); };
constexpr inline auto exp         = [](const_t const & a) { return std::exp(a); };
constexpr inline auto ln          = [](const_t const & a) { return std::log(a); };
constexpr inline auto abs         = [](const_t const & a) { return std::abs(a); };
constexpr inline auto sqrt        = [](const_t const & a) { return std::sqrt(a); };
constexpr inline auto cbrt        = [](const_t const & a) { return std::cbrt(a); };
constexpr inline auto unary_minus = [](const_t const & a) { return -a; };
//sinh
//cosh
//tanh
//asinh
//acosh
//atanh
//ceil
//floor
//log10
}  // namespace function

inline
auto sign_to_binary(char ch) -> binary_f
{
    switch (ch) {
        case '+': return function::plus;
        case '-': return function::minus;
        case '*': return function::multiplies;
        case '/': return function::divides;
        case '^': return function::pow;
        case '%': return function::modulus;
        default:
            throw std::logic_error{fmt::format("Found bad operator with no correspective function: {}", ch)};
    }
}
inline
auto sign_to_unary(char ch) -> unary_f
{
    switch (ch) {
        case 's': return function::sin;
        case 'c': return function::cos;
        case 't': return function::tan;
        case 'S': return function::asin;
        case 'C': return function::acos;
        case 'T': return function::atan;
        case 'l': return function::ln;
        case 'e': return function::exp;
        case '|': return function::abs;
        case 'v': return function::sqrt;
        case 'V': return function::cbrt;
        case 'n': return function::unary_minus;
        default:
            throw std::logic_error{fmt::format("Found bad operator with no correspective function: {}", ch)};
    }
}

namespace parser
{
constexpr
auto match_real(char const * const begin, char const * const end) -> std::pair<std::optional<const_t>, int>
{
    auto result = const_t{};
    auto [ptr, ec] = std::from_chars(begin, end, result);

    if (ec == std::errc()) {
        return {result, ptr - begin};
    }
    return {std::nullopt, 0};
}

constexpr
auto match_function(std::string_view const str) -> std::optional<std::pair<char, size_t>>
{
    if (str[0] == '-' and (std::isdigit(str[1]) == 0)) {
        return std::pair{'n', 1};
    }
    for (std::string_view const word : {
        "sin", "cos", "tan", "asin", "acos", "atan", "ln", "log", "exp", "abs", "sqrt", "cbrt"
    }) {
        if (str.starts_with(word)) {
            auto size = word.size();
            // return str.substr(size);
            char operation = str[0];
            switch (str[0]) {
                case 'a':
                    if (str[1] == 'b') { operation = '|'; }
                    else { operation = static_cast<char>(std::toupper(str[1])); }
                    break;
                case 's':
                    if (str[1] == 'q' ) { operation = 'v'; }
                    else { operation = 's'; }
                    break;
                case 'c':
                    if (str[1] == 'b' ) { operation = 'V'; }
                    else { operation = 'c'; }
                    break;
                default:
                    operation = str[0];
            }
            return std::pair{operation, size};
        }
    }
    return std::nullopt;
}

constexpr
auto match_pi(std::string_view const str) -> std::optional<std::size_t>
{
    for (std::string_view pi : { "pi", "PI", "Pi", "π"}) {
        if (str.starts_with(pi)) {
            return { pi.size() };
        }
    }
    return std::nullopt;
}
}  // namespace parser

template <std::string_view const &...Strs>
struct join {
    static constexpr auto impl() noexcept
    {
        constexpr std::size_t len = (Strs.size() + ... + 0);
        std::array<char, len + 1> arr{};
        auto append = [i = 0, &arr](auto const & s) mutable {
            for (auto c : s) { arr[i++] = c; }  // NOLINT
        };
        (append(Strs), ...);
        arr[len] = 0;
        return arr;
    }
    static constexpr auto arr = impl();
    static constexpr auto value = std::string_view{arr.data(), arr.size() - 1};
};
// EXPRESSION
// maps 123(...) to 123*(...) and (...)(...) to (...)*(...)
inline
auto preparse(std::string_view src_) noexcept -> std::string
{
    auto src = src_
             | std::views::filter([](char c) { return std::isspace(c) == 0; })
             | std::ranges::to<std::string>();
    auto result = std::string{};
    result.reserve(src.size());

    static constexpr auto spaces = std::string_view{" \t\n"};
    static constexpr auto parens = std::string_view{"("};
    static constexpr auto ops    = std::string_view{"+-"};
    static constexpr auto chars = join<spaces, parens, ops>::value;

    auto const idx = static_cast<int>(src[0] == '(');


    auto i = src.find_first_of(chars, src[0] == '(' ? 1 : 0);
    auto from = 0l;
    for (; i < std::string::npos; i = src.find_first_of(chars, from + 1)) {
        result.append(src.substr(from, i - from));
        auto c1 = src[i];
        if (ops.contains(c1)) {
            while (ops.contains(src[i + 1])) {
                c1 = c1 == src[++i] ? '+' : '-';
            }
            src[i] = c1;
        } else if (src.at(i - 1) == ')' or static_cast<bool>(std::isdigit(src[i - 1]))) {
            result.append("*(");
            i += 1;
        }
        from = static_cast<decltype(from)>(i);
    }
    result.append(src.substr(from, i - from));
    return result;
}

auto parse_impl(std::string_view line, std::string_view param_names) -> std::vector<variant_t>  // NOLINT(misc-no-recursion)
{
    auto buffer = std::vector<variant_t>{};
    auto sign_buffer = std::string{};

    // unary minus is mapped to binary minus
    if  (detail::is_binary_f(line[0]))  {
        buffer.emplace_back(0.);
    }

    while (not line.empty()) {
        auto begin = line.cbegin();
        auto end   = line.cend();

        // Match a function
        if (auto match = parser::match_function(line); match.has_value()) {
            auto [fn, len] = *match;
            line.remove_prefix(len);
            sign_buffer.push_back(fn);
        }
        // Match an operator
        else if (detail::is_binary_f(*begin)) {
            auto operation = *begin;
            while (not sign_buffer.empty() and not detail::stronger_sign(operation, sign_buffer.back())) {
                if (detail::is_binary_f(sign_buffer.back())) {
                    buffer.emplace_back(sign_to_binary(sign_buffer.back()));
                }
                else {
                    buffer.emplace_back(sign_to_unary(sign_buffer.back()));
                }

                sign_buffer.pop_back();
            }
            sign_buffer.push_back(operation);
            line.remove_prefix(1);
        }
        // Match a number
        else if (auto [value, len] = parser::match_real(begin, end); value.has_value()) {
            line.remove_prefix(len);
            buffer.emplace_back(*value);
        }
        // Match an open parenthesis
        else if (*begin == '(') {
            auto counter  = 1;
            auto index = 1z;

            while (index < line.size()) {
                if (line.at(index) == '(') {
                    ++counter;
                }
                else if (line.at(index) == ')') {
                    --counter;
                }
                if (counter == 0) {
                    break;
                }
                ++index;
            }

            if (index == line.size()) {
                throw std::logic_error{"Unterminated parenthesis"};
            }
            if (index > 1) {
                for (auto & symbol : parse_impl(line.substr(1, index - 1), param_names)) {
                    buffer.emplace_back(std::move(symbol));
                }
            }
            line.remove_prefix(index + 1);
        }
        // Match a closed parenthesis - error
        else if (*begin == ')') {
            throw std::logic_error{"Closed parentheses without an opening correspective"};
        }
        // Match a space
        else if (static_cast<bool>(std::isspace(*begin))) {
            line.remove_prefix(1);
        }
        // Match π
        else if (auto pi_size = parser::match_pi(line); pi_size.has_value()) {
            buffer.emplace_back(std::numbers::pi_v<const_t>);
            line.remove_prefix(*pi_size);
        }
        // Match e
        else if (*begin == 'e') {
            buffer.emplace_back(std::numbers::e_v<const_t>);
            line.remove_prefix(1);
            continue;
        }
        // Match a parameter
        else if (param_names.contains(*begin)) {
            buffer.emplace_back(param_t{*begin});
            line.remove_prefix(1);
        }
        // Match something else
        else {

            // TODO: assert this is right
            throw std::invalid_argument{
                fmt::format("Unexpected token in parsing: {}", line)
            };
        }
    }

    for (auto const op : std::views::reverse(sign_buffer)) {
        if (detail::is_binary_f(op)) {
            buffer.emplace_back(sign_to_binary(op));
        } else {
            buffer.emplace_back(sign_to_unary(op));
        }
    }

    if ( buffer.empty() ) {
        buffer.emplace_back( const_t{0} );
    }
    return buffer;
}

auto parse(std::string_view src, std::string_view param_names) -> std::vector<variant_t>
{
    if (src.empty()) {
        auto v = variant_t{const_t{0}};
        auto res = std::vector<variant_t>{};
        res.push_back(std::move(v));
        return res;
    }
    return parse_impl(preparse(src), param_names);
}

auto build_impl(std::string_view src, std::string_view param_names) -> std::unique_ptr<node>
{
    // fmt::print("Parsing\n");  // DEBUG
    auto symbols = parse(src, param_names);
    // fmt::print("Parsed\n");  // DEBUG
    // fmt::print("Result: {}\n", symbols);
    auto it = symbols.crbegin();
    auto end = symbols.crend();

    auto head = std::make_unique<node>(*it);
    if (std::holds_alternative<const_t>(*it)) {
        if (symbols.size() > 1) {
            throw std::logic_error{"Bad parsing or semantics"};
        }
        return head;  // TODO: head or a specialized class?
    }
    else if (std::holds_alternative<unary_f>(*it) or std::holds_alternative<binary_f>(*it)) {
        if (symbols.size() == 1) {
            throw std::logic_error{"Function or operator without arguments"};
        }
    }
    ++it;
    // fmt::print("Head is: {}\n", *head);  // DEBUG

    auto stack = std::vector<decltype(head.get())>{head.get()};
    while (it != end) {
        // fmt::print("Symbol {}/{}\n", it - symbols.crbegin(), end - symbols.crbegin());  // DEBUG
        auto const & top = stack.back();
        // fmt::print("Symbol: {}\n", *top);  // DEBUG
        if (top->left and top->right) {
            stack.pop_back();
            continue;
        }
        auto * ptr = &(not top->left ? top->left : top->right);
        *ptr = std::make_unique<node>(*it);

        if (std::holds_alternative<unary_f>(*it)) {
            stack.push_back(ptr->get());
            stack.back()->right = std::make_unique<node>(nothing{});
        }
        else if (std::holds_alternative<binary_f>(*it)) {
            stack.push_back(ptr->get());
        }
        ++it;
    }

    return head;
}

constexpr
bool evalutable(std::unique_ptr<node> const & node)  // NOLINT(misc-no-recursion)
{
    if (not node) {
        return false;
    }
    if (std::holds_alternative<const_t>(node->content)) {
        return true;
    }
    if (std::holds_alternative<binary_f>(node->content)) {
        return evalutable(node->left) && evalutable(node->right);
    }
    if (std::holds_alternative<unary_f>(node->content)) {
        return evalutable(node->left);
    }

    return false;
}

[[nodiscard]] inline
auto eval_impl(std::unique_ptr<node> const & head, std::optional<parameter> const & param = std::nullopt)
    -> const_t
{
    return std::visit(
            detail::overload{
                [ ](const_t const & value) static {
                    return value;
                },
                [&](param_t const & p) {  // TODO: should not appear, right?
                    if (not param.has_value()) {
                        throw std::logic_error{"Found parameter in parameter-less evaluation"};
                    }
                    if (param->name != p) {
                        throw std::logic_error{"Wrong parameter name"};
                    }
                    return param->value;
                },
                [&](unary_f & unary) {
                    return unary(eval_impl(head->left, param));
                },
                [&](binary_f & binary) {
                    return binary(eval_impl(head->right, param), eval_impl(head->left, param));
                },
                [ ](nothing) static {
                    throw std::logic_error{"Found (literally) nothing..."};
                    return const_t{0};
                }
            }, head->content
    );
}

auto expression::eval(std::optional<parameter> const & param) const
    -> const_t
{
    return eval_impl(_head, param);
}

void optimize(std::unique_ptr<node> & node)  // NOLINT(misc-no-recursion)
{
    //Optimization for binary
    if (std::holds_alternative<binary_f>(node->content)) {
        optimize(node->left);
        optimize(node->right);
        //Optimize if the content is without parameters
        if (evalutable(node)) {
            node->content = const_t{eval_impl(node)};
        }
        //Compose [f]->[g->[x,y],h->[z,w]] into [f.°h->[z,w]]->g[x,y] and then reiterate as unary
        else {
            if (std::holds_alternative<unary_f>(node->right->content))
            {
                auto left  = std::move(node->left);
                auto right = std::move(node->right);
                auto new_function =
                [
                    f = std::move(std::get<binary_f>(node->content)),
                    g = std::move(std::get<unary_f>(right->content))
                ]
                (const_t const & a, const_t const & b) mutable {
                    return f(g(a), b);
                };
                node = std::make_unique<expr::node>(std::move(new_function));
                node->left  = std::move(left);
                node->right = std::move(right->left);
            }
            if (std::holds_alternative<unary_f>(node->left->content))
            {
                auto left  = std::move(node->left);
                auto right = std::move(node->right);
                auto new_function =
                [
                    f = std::move(std::get<binary_f>(node->content)),
                    g = std::move(std::get<unary_f> (left->content))
                ]
                (const_t const & a, const_t const & b) mutable {
                    return f(g(a), b);
                };
                node = std::make_unique<expr::node>(std::move(new_function));
                node->left  = std::move(left->left); //std::move(left);
                node->right = std::move(right); //std::move(right->left);
            }
        }
    }
    //....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
    //Optimization for unary
    else if (std::holds_alternative<unary_f>(node->content)) {
        optimize(node->left);
        //Optimize if the content is without parameters
        if (evalutable(node->left)) {
            node->content = const_t{eval_impl(node)};
        } else {
            //Compose [f]->[g]->[x] into [f°g]->[x]
            if (std::holds_alternative<unary_f>(node->left->content)) {
                auto tmp = std::move(node->left);
                auto new_function =
                [
                    f = std::move(std::get<unary_f>(node->content)),
                    g = std::move(std::get<unary_f>( tmp->content))
                ]
                (const_t const & a) mutable {
                    return f(g(a));
                };
                node = std::make_unique<expr::node>(std::move(new_function));
                node->left = std::move(tmp->left);
            }
            //Compose [f]->[g]->[x,y] into [f°g]->[x,y]
            else if ( std::holds_alternative<binary_f>(node->left->content) ) {
                auto tmp = std::move(node->left);
                auto new_function = binary_f{
                    [
                        f = std::move(std::get<unary_f>(node->content)),
                        g = std::move(std::get<binary_f>( tmp->content))
                    ]
                    (const_t const & a, const_t const & b) mutable {
                        return f(g(a, b));
                    }
                };
                node = std::make_unique<expr::node>(std::move(new_function));
                node->left  = std::move(tmp->left);
                node->right = std::move(tmp->right);
            }
        }
    }
}

auto parse_and_build(std::string_view src, std::string_view const param_names) -> std::unique_ptr<node>
{
    auto result = build_impl(src, param_names);
    // fmt::print("Got result: {}\n", result);  // DEBUG
    optimize(result);
    // fmt::print("Optimized: {}\n", result);  // DEBUG
    return result;
}

expression::expression(std::string_view source, std::string_view const param_names) :
    _head{parse_and_build(source, param_names)}
{}

} // namespace brun::expr
