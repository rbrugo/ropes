/**
 * @author      : Riccardo Brugo (brugo.riccardo@gmail.com)
 * @file        : math
 * @created     : Sunday Aug 27, 2023 19:04:51 CEST
 * @description : 
 * */

#ifndef ROPES_MATH_HPP
#define ROPES_MATH_HPP

#include <concepts>
#define USE_ARITHMETIC

#include <cmath>
#include <array>
#include <ranges>
#include <algorithm>
// #include <brun/callables/arithmetic.hpp>

#define FWD(x) static_cast<decltype(x) &&>(x)

namespace math
{

template <typename T, typename ...Ts> struct first_type { using type = T; };

struct arithmetic
{
    template <typename Self, typename Rhs>
        requires requires(Self && self, Rhs && rhs) { { auto{FWD(self)} += FWD(rhs) }; }
    constexpr auto operator+(this Self && self, Rhs && rhs)
    {
        constexpr auto same_type = std::is_same_v<std::remove_cvref_t<Self>, std::remove_cvref_t<Rhs>>;
        if constexpr (std::is_rvalue_reference_v<Rhs> and same_type) {
            return FWD(rhs) += FWD(self);
        } else {
            return auto{FWD(self)} += FWD(rhs);
        }
    }

    template <typename Self, typename Rhs = Self>
        // requires requires(Self const & self, Rhs const & rhs) { { auto{self} -= rhs }; }
    constexpr auto operator-(this Self const & self, Rhs const & rhs)
    {
        return auto{self} -= rhs;
    }

    template <typename Self, typename Rhs>
        requires requires(Self && self, Rhs && rhs) { { auto{FWD(self)} *= FWD(rhs) }; }
    constexpr auto operator*(this Self && self, Rhs && rhs)
    {
        return auto{FWD(self)} *= rhs;
    }

    template <typename Self, typename Rhs>
        requires requires(Self && self, Rhs && rhs) { { auto{FWD(self)} /= FWD(rhs) }; }
    constexpr auto operator/(this Self && self, Rhs && rhs)
    {
        return auto{FWD(self)} /= rhs;
    }

    constexpr bool operator==(arithmetic const &) const noexcept = default;

protected:
    constexpr arithmetic() = default;
};

template <typename T, std::size_t N>
struct vector : std::array<T, N>, public arithmetic
{
    constexpr vector() : vector::array{} {}

    template <typename... Ts>
        requires ((std::constructible_from<T, Ts> && ...) and sizeof...(Ts) == N)
    constexpr vector(Ts && ...ts) : std::array<T, N>{{static_cast<T>(ts)...}} {}

    template <typename T2>
    constexpr
    auto operator+=(vector<T2, N> const & rhs) noexcept -> decltype(auto)
    { std::ranges::transform(*this, rhs, this->begin(), std::plus<>{}); return *this; }

#ifndef USE_ARITHMETIC
    constexpr auto operator+(vector rhs) const noexcept { return rhs += *this; }
    template <typename T2> requires std::same_as<std::common_type_t<T, T2>, T2>
    constexpr
    auto operator+(vector<T2, N> rhs) const noexcept { return rhs += *this; }
    template <typename T2> requires std::same_as<std::common_type_t<T, T2>, T>
    constexpr
    auto operator+(vector<T2, N> const & rhs) const noexcept { return auto{*this} += rhs; }
#else
    using arithmetic::operator+;
#endif  // USE_ARITHMETIC

    template <typename T2>
    constexpr
    auto operator-=(vector<T2, N> const & rhs) noexcept -> decltype(auto)
    { std::ranges::transform(*this, rhs, this->begin(), std::minus<>{}); return *this; }

#ifndef USE_ARITHMETIC
    constexpr auto operator-(vector const & rhs) const noexcept { return auto{*this} -= rhs; }
    template <typename T2> requires std::same_as<std::common_type_t<T, T2>, T2>
    constexpr
    auto operator-(vector<T2, N> rhs) const noexcept { return (-rhs) += *this; }
    template <typename T2> requires std::same_as<std::common_type_t<T, T2>, T>
    constexpr
    auto operator-(vector<T2, N> const & rhs) const noexcept { return auto{*this} -= rhs; }
#else
    using arithmetic::operator-;
#endif  // USE_ARITHMETIC

    template <typename Scalar>
        requires std::constructible_from<T, std::remove_cvref_t<decltype(std::declval<T>() * std::declval<Scalar>())>>
    constexpr
    auto operator*=(Scalar const & s) noexcept -> decltype(auto) {
        std::ranges::transform(*this, this->begin(), [&s](auto const & n) { return n * s; });
        return *this;
    }

    template <typename Scalar>
        requires requires(vector const & v, Scalar const & s) { { v[0] * s }; }
    constexpr
    auto operator*(Scalar const & s) const noexcept {
        using scalar = std::remove_cvref_t<decltype((*this)[0] * s)>;
        auto result = math::vector<scalar, N>{};
        std::ranges::transform(*this, result.begin(), [s](auto && elem) { return elem * s; });
        return result;
    }

    template <typename T2>
        requires requires(vector const & v, T2 const & t) { { v[0] * t }; }
    constexpr
    auto operator*(vector<T2, N> const & rhs) const noexcept
    {
        using scalar = std::remove_cvref_t<decltype((*this)[0] * rhs[0])>;
        return std::ranges::fold_left(
            std::views::zip_transform(std::multiplies<>{}, *this, rhs), scalar{}, std::plus<>{});
    }

#ifndef USE_ARITHMETIC
    template <typename Scalar>
    constexpr
    auto operator*(Scalar const & s) const noexcept -> vector { return auto{*this} *= s; }
#else
    using arithmetic::operator*;
#endif  // USE_ARITHMETIC

    template <typename Scalar>
        requires (std::floating_point<Scalar> or std::integral<Scalar>)
             and std::constructible_from<T, std::remove_cvref_t<decltype(std::declval<T>() / std::declval<Scalar>())>>
    constexpr
    auto operator/=(Scalar const & s) noexcept -> decltype(auto) {
        std::ranges::transform(*this, this->begin(), [&s](auto const & n) { return n / s; });
        return *this;
    }

#ifndef USE_ARITHMETIC
    template <typename Scalar>
        requires std::floating_point<Scalar> or std::integral<Scalar>
    constexpr
    auto operator/(Scalar const & s) const noexcept { return auto{*this} /= s; }
#else
    using arithmetic::operator/;
#endif  // USE_ARITHMETIC

    constexpr auto operator-() const noexcept { return auto{*this} *= -1; }

    static constexpr auto zero() noexcept { return vector{}; }

    constexpr bool operator==(vector const &) const = default;

    template <typename T2>
        requires std::constructible_from<T2, T>
    constexpr operator vector<T2, N>() const noexcept(std::is_nothrow_constructible_v<T2, T>)
    {
        return [this]<size_t ...I>(std::index_sequence<I...>) {
            return vector<T2, N>{T2(std::get<I>(*this))...};
        }(std::make_index_sequence<N>{});
    }

    template <typename Fn>
        requires std::invocable<Fn, T>
    constexpr auto transform(Fn && fn) const
        -> vector<std::invoke_result_t<Fn, T>, N>
    {
        using R = std::invoke_result_t<Fn, T>;
        return [this, fn=FWD(fn)]<size_t ...I>(std::index_sequence<I...>) {
            return vector<R, N>{fn(std::get<I>(*this))...};
        }(std::make_index_sequence<N>{});
    }
};

static_assert(vector<int, 2>::zero() == vector<int, 2>{0, 0});
static_assert(vector<int, 2>{1, 2} + vector<int, 2>{3, 4} == vector<int, 2>{4, 6});
consteval auto convert(vector<int, 2> x) -> vector<double, 2> { return x; }

template <typename ...Ts>
    requires (std::constructible_from<typename std::common_type<Ts...>::type, Ts> and ...)
vector(Ts ...) -> vector<typename std::common_type<Ts...>::type, sizeof...(Ts)>;


// product with a scalar
template <typename T, std::size_t N, typename Scalar>
[[nodiscard]]
constexpr auto operator*(Scalar const & s, vector<T, N> const & v) noexcept { return v * s; }

// squared norm
constexpr inline struct squared_norm_fn
{
    template <typename T, std::size_t N>
    [[nodiscard]] static constexpr
    auto operator()(vector<T, N> const & v) noexcept { return v * v; }
} squared_norm;

// norm
constexpr inline struct norm_fn
{
    template <typename T, std::size_t N>
    [[nodiscard]] static constexpr
    auto operator()(vector<T, N> const & v) noexcept {
        using std::sqrt;
        return sqrt(squared_norm(v));
    }
} norm;

// unit vector
constexpr inline struct unit_fn
{
    template <typename T, std::size_t N>
    [[nodiscard]] static constexpr
    auto operator()(vector<T, N> const & v) noexcept {
        if constexpr (requires(vector<T, N> const & x) { { x / norm(x) }; }) {
            return v / norm(v);
        } else if constexpr (requires(vector<T, N> const & x) { { x * inverse(norm(x)) }; }) {
            return v * inverse(norm(v));
        } else if constexpr (requires { { T{1.} / norm(v) } -> std::same_as<T>; }) {
            return v * (T{1.} / norm(v));
        } else {
            return v * (1. / norm(v)); 
        }
    }
} unit;

// vector_cast
template <typename To>
struct vector_cast_fn
{
    template <typename From, size_t N>
        requires std::constructible_from<To, From>
    [[nodiscard]] static constexpr
    auto operator()(vector<From, N> const & other)
    {
        auto res = vector<To, N>{};
        std::ranges::copy(other, res.begin());
        return res;
    }
};
template <typename To>
constexpr inline vector_cast_fn<To> vector_cast;

// cosine
constexpr inline struct cosine_fn {
    template <typename T, std::size_t N, typename U = T>
    [[nodiscard]] static constexpr
    auto operator()(vector<T, N> const & v1, vector<T, N> const & v2) noexcept
    { return (v1 * v2) / (norm(v1) * norm(v2)); }
} cosine;

// sine
constexpr inline struct sine_fn {
    template <typename T, typename U = T>
    [[nodiscard]] static constexpr
    auto operator()(vector<T, 2> const & v1, vector<T, 2> const & v2) noexcept
    { return (v1[0] * v2[1] - v1[1] * v2[0]) / (norm(v1) * norm(v2)); }
} sine;

constexpr inline struct hadamard_product_fn {
    template <typename T, typename U = T, std::size_t N>
    [[nodiscard]] static constexpr
    auto operator()(vector<T, N> const & v1, vector<T, N> const & v2) noexcept
    {
        using R = decltype(std::declval<T const &>() * std::declval<U const &>());
        return [&v1, &v2]<size_t ...I>(std::index_sequence<I...>) {
            return vector<R, N>{R(std::get<I>(v1) * std::get<I>(v2))...};
        }(std::make_index_sequence<N>{});
    }
} hadamard_product;

constexpr inline struct hadamard_division_fn {
    template <typename T, typename U = T, std::size_t N>
    [[nodiscard]] static constexpr
    auto operator()(vector<T, N> const & v1, vector<T, N> const & v2) noexcept
    {
        using R = decltype(std::declval<T const &>() / std::declval<U const &>());
        return [&v1, &v2]<size_t ...I>(std::index_sequence<I...>) {
            return vector<R, N>{R(std::get<I>(v1) / std::get<I>(v2))...};
        }(std::make_index_sequence<N>{});
    }
} hadamard_division;

constexpr inline struct hadamard_inverse_fn {
    template <typename T, std::size_t N>
    [[nodiscard]] static constexpr
    auto operator()(vector<T, N> const & v) noexcept
    {
        using R = decltype(T{1.} / std::declval<T const &>());
        return [&v]<size_t ...I>(std::index_sequence<I...>) {
            return vector<R, N>{R(T{1} / std::get<I>(v))...};
        }(std::make_index_sequence<N>{});
    }
} hadamard_inverse;


}  // namespace math

template <typename T, std::size_t N>
struct std::tuple_size<math::vector<T, N>> : public std::tuple_size<std::array<T, N>> {};  // NOLINT
template <typename T, std::size_t N, std::size_t I>
struct std::tuple_element<I, math::vector<T, N>> : public std::tuple_element<I, std::array<T, N>> {};  // NOLINT

// void test_math()
// {
//     [[maybe_unused]] auto x = math::vector{1, 2, 3};
//     [[maybe_unused]] auto y = math::vector{1.1, 2.2, 3, -4};
//     [[maybe_unused]] auto z = math::vector{0};
//     [[maybe_unused]] auto w = math::vector{0.};
//
//     z += w;
//     // z = z + w;
//     // z = w + z;
//     w = z + w;
//     w = w + z;
// }

#endif /* ROPES_MATH_HPP */
