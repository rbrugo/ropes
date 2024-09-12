/**
 * @author      : rbrugo (brugo.riccardo@gmail.com)
 * @file        : vector
 * @created     : Thursday Sep 12, 2024 18:56:35 CEST
 * @description : generic mathematical vector implementation
 * */

#ifndef MATH_VECTOR_HPP
#define MATH_VECTOR_HPP

#include <type_traits>
#include <functional>
#include <algorithm>
#include <ranges>
#include <array>
#include <cmath>

#include "values.hpp"

#define MATH_FWD(x) std::forward<decltype(x)>(x)

namespace math
{

struct arithmetic
{
    template <typename Self, typename Rhs>
        requires requires(Self && self, Rhs && rhs) { { auto{MATH_FWD(self)} += MATH_FWD(rhs) }; }
    constexpr auto operator+(this Self && self, Rhs && rhs)
    {
        constexpr auto same_type = std::same_as<std::remove_cvref_t<Self>, std::remove_cvref_t<Rhs>>;
        if constexpr (std::is_rvalue_reference_v<Rhs> and same_type) {
            return MATH_FWD(rhs) += MATH_FWD(self);
        } else if constexpr (std::is_rvalue_reference_v<Self> and same_type) {
            return MATH_FWD(self) += rhs;
        } else {
            return auto{MATH_FWD(self)} += MATH_FWD(rhs);
        }
    }

    template <typename Self, typename Rhs = Self>
        // requires requires(Self const & self, Rhs const & rhs) { { auto{self} -= rhs }; }
    constexpr auto operator-(this Self const & self, Rhs const & rhs)
    {
        return auto{self} -= rhs;
    }

    template <typename Self, typename Rhs>
        requires requires(Self && self, Rhs && rhs) { { auto{MATH_FWD(self)} *= MATH_FWD(rhs) }; }
    constexpr auto operator*(this Self && self, Rhs && rhs)
    {
        return auto{MATH_FWD(self)} *= rhs;
    }

    template <typename Self, typename Rhs>
        requires requires(Self && self, Rhs && rhs) { { auto{MATH_FWD(self)} /= MATH_FWD(rhs) }; }
    constexpr auto operator/(this Self && self, Rhs && rhs)
    {
        return auto{MATH_FWD(self)} /= rhs;
    }

    template <typename Self>
    constexpr auto operator-(this Self const & self) noexcept
    {
        return self * -1;
    }

    constexpr bool operator==(arithmetic const &) const noexcept = default;

protected:
    constexpr arithmetic() = default;
};

template <typename T, std::size_t N>
struct vector : std::array<T, N>, public arithmetic
{
    constexpr vector() = default;

    template <typename... Ts>
        requires ((std::constructible_from<T, Ts> && ...) and sizeof...(Ts) == N)
    constexpr vector(Ts && ...ts) noexcept : vector::array{{static_cast<T>(ts)...}} {}  // NOLINT

    template <typename T2>
    constexpr
    auto operator+=(vector<T2, N> const & rhs) noexcept -> decltype(auto)
    { std::ranges::transform(*this, rhs, this->begin(), std::plus<>{}); return *this; }


    template <typename T2>
    constexpr
    auto operator-=(vector<T2, N> const & rhs) noexcept -> decltype(auto)
    { std::ranges::transform(*this, rhs, this->begin(), std::minus<>{}); return *this; }


    template <typename Scalar>
        requires std::constructible_from<T, std::remove_cvref_t<decltype(std::declval<T>() * std::declval<Scalar>())>>
    constexpr
    auto operator*=(Scalar const & s) noexcept -> decltype(auto)
    {
        std::ranges::transform(*this, this->begin(), [&s](auto const & n) { return n * s; });
        return *this;
    }

    template <typename Scalar>
        requires requires(vector const & v, Scalar const & s) { { v[0] * s }; }
    constexpr
    auto operator*(Scalar const & s) const noexcept
    {
        using scalar = std::remove_cvref_t<decltype((*this)[0] * s)>;
        auto result = math::vector<scalar, N>{};
        std::ranges::transform(*this, result.begin(), [s](auto const & elem) { return elem * s; });
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

    template <typename Scalar>
        requires (std::floating_point<Scalar> or std::integral<Scalar>)
             and std::constructible_from<T, std::remove_cvref_t<decltype(std::declval<T>() / std::declval<Scalar>())>>
    constexpr
    auto operator/=(Scalar const & s) noexcept -> decltype(auto)
    {
        std::ranges::transform(*this, this->begin(), [&s](auto const & n) { return n / s; });
        return *this;
    }

    template <typename Scalar>
        requires requires(vector const & v, Scalar const & s) { { v[0] / s }; }
    constexpr
    auto operator/(Scalar const & s) const noexcept
    {
        using scalar = std::remove_cvref_t<decltype((*this)[0] / s)>;
        auto result = math::vector<scalar, N>{};
        std::ranges::transform(*this, result.begin(), [&s](auto const & elem) { return elem / s; });
        return result;
    }


    using arithmetic::operator+;
    using arithmetic::operator-;
    using arithmetic::operator*;
    using arithmetic::operator/;

    [[nodiscard]] static constexpr auto zero() noexcept { return vector{}; }
    [[nodiscard]] static constexpr auto one() noexcept {
        return []<size_t ...I>(std::index_sequence<I...>) static {
            return vector{((void)I, static_cast<T>(math::one))...};
        }(std::make_index_sequence<N>{});
    }

    constexpr bool operator==(vector const &) const noexcept = default;

    template <typename T2>
        requires std::constructible_from<T2, T>
    [[nodiscard]]
    constexpr operator vector<T2, N>() const noexcept(std::is_nothrow_constructible_v<T2, T>)  // NOLINT
    {
        return [this]<size_t ...I>(std::index_sequence<I...>) {
            return vector<T2, N>{T2(std::get<I>(*this))...};
        }(std::make_index_sequence<N>{});
    }

    template <typename Fn>
        requires std::invocable<Fn, T>
    [[nodiscard]]
    constexpr auto transform(Fn && fn) const
        -> vector<std::invoke_result_t<Fn, T>, N>
    {
        using R = std::invoke_result_t<Fn, T>;
        return [this, fn=MATH_FWD(fn)]<size_t ...I>(std::index_sequence<I...>) {
            return vector<R, N>{fn(std::get<I>(*this))...};
        }(std::make_index_sequence<N>{});
    }

    static constexpr auto size() { return N; }
};

template <typename ...Ts>
    requires (std::constructible_from<typename std::common_type<Ts...>::type, Ts> and ...)
vector(Ts ...) -> vector<typename std::common_type<Ts...>::type, sizeof...(Ts)>;

namespace traits
{
template <typename T>
struct is_vector: std::false_type {};
template <typename T, std::size_t N>
struct is_vector<math::vector<T, N>> : std::true_type {};
}  // namespace traits

namespace concepts
{
template <typename T>
concept vector = traits::is_vector<T>::value;
}  // namespace concepts

// Operations over a vector

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

    template <typename T, std::size_t N>
    [[nodiscard]] static constexpr
    auto squared(vector<T, N> const & v) noexcept {
        return squared_norm(v);
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
    auto operator()(vector<T, N> const & v1, vector<U, N> const & v2) noexcept
    {
        using std::sqrt;
        return (v1 * v2) / sqrt(squared_norm(v1) * squared_norm(v2));
    }

    template <typename T, std::size_t N, typename U = T>
    [[nodiscard]] static constexpr
    auto squared(vector<T, N> const & v1, vector<U, N> const & v2) noexcept
    {
        auto tmp = v1 * v2;
        return tmp * tmp / (squared_norm(v1) * squared_norm(v2));
    }
} cosine;

// sine
constexpr inline struct sine_fn {
    template <typename T, typename U = T>
    [[nodiscard]] static constexpr
    auto operator()(vector<T, 2> const & v1, vector<U, 2> const & v2) noexcept
    {
        using std::sqrt;
        return (v1[0] * v2[1] - v1[1] * v2[0]) / sqrt(squared_norm(v1) * squared_norm(v2));
    }

    template <typename T, typename U = T>
    [[nodiscard]] static constexpr
    auto squared(vector<T, 2> const & v1, vector<U, 2> const & v2) noexcept
    {
        auto tmp = (v1[0] * v2[1] - v1[1] * v2[0]);
        return tmp * tmp / (squared_norm(v1) * squared_norm(v2));
    }
} sine;

// hadamard product
constexpr inline struct hadamard_product_fn {
    template <typename T, typename U = T, std::size_t N>
    [[nodiscard]] static constexpr
    auto operator()(vector<T, N> const & v1, vector<U, N> const & v2) noexcept
    {
        using R = decltype(std::declval<T const &>() * std::declval<U const &>());
        return [&v1, &v2]<size_t ...I>(std::index_sequence<I...>) {
            return vector<R, N>{R(std::get<I>(v1) * std::get<I>(v2))...};
        }(std::make_index_sequence<N>{});
    }
} hadamard_product;

// hadamard division
constexpr inline struct hadamard_division_fn {
    template <typename T, typename U = T, std::size_t N>
    [[nodiscard]] static constexpr
    auto operator()(vector<T, N> const & v1, vector<U, N> const & v2) noexcept
    {
        using R = decltype(std::declval<T const &>() / std::declval<U const &>());
        return [&v1, &v2]<size_t ...I>(std::index_sequence<I...>) {
            return vector<R, N>{R(std::get<I>(v1) / std::get<I>(v2))...};
        }(std::make_index_sequence<N>{});
    }
} hadamard_division;

// hadamard inverse
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

// dot product
constexpr inline struct dot_fn {
    template <typename T, typename U, std::size_t N>
    [[nodiscard]] static constexpr
    auto operator()(vector<T, N> const & v, vector<U, N> const & u) noexcept
    {
        return v * u;
    }
} dot;

// 2d and 3d cross product
constexpr inline struct cross_fn {
    template <typename T, typename U>
    [[nodiscard]] static constexpr
    auto operator()(vector<T, 2> const & v, vector<U, 2> const & u) noexcept
    {
        return v[0] * u[1] - v[1] * u[0];
    }

    template <typename T, typename U>
    [[nodiscard]] static constexpr
    auto operator()(vector<T, 3> const & v, vector<U, 3> const & u) noexcept
    {
        using scalar = std::remove_cvref_t<decltype(v[0] * u[0])>;
        return vector<scalar, 3>{
            v[1] * u[2] - v[2] * u[1],
            v[2] * u[0] - v[0] * u[2],
            v[0] * u[1] - v[1] * u[0]
        };
    }

    template <typename T, typename U, std::size_t N>
        requires (N != 2 and N != 3)
    [[nodiscard]] static constexpr
    auto operator()(vector<T, N> const & v, vector<U, N> const & u) noexcept = delete;
} cross;

} // namespace math

template <typename T, std::size_t N>
struct std::tuple_size<math::vector<T, N>> : public std::tuple_size<std::array<T, N>> {};  // NOLINT
template <typename T, std::size_t N, std::size_t I>
struct std::tuple_element<I, math::vector<T, N>> : public std::tuple_element<I, std::array<T, N>> {};  // NOLINT


#define MATH_COMPILE_TIME_TESTS
#ifdef MATH_COMPILE_TIME_TESTS
static_assert(-math::vector<int, 2>{1, 2} == math::vector<int, 2>{-1, -2});
static_assert(math::vector<int, 2>::zero() == math::vector<int, 2>{0, 0});
static_assert(math::vector<int, 2>::one() == math::vector<int, 2>{1, 1});
static_assert(math::vector<int, 2>{1, 2} + math::vector<int, 2>{3, 4} == math::vector<int, 2>{4, 6});
consteval auto convert(math::vector<int, 2> x) -> math::vector<double, 2> { return x; }
#endif  // MATH_COMPILE_TIME_TESTS

#undef MATH_FWD

#endif /* MATH_VECTOR_HPP */
