/**
 * @author      : rbrugo (brugo.riccardo@gmail.com)
 * @file        : values
 * @created     : Thursday Sep 12, 2024 18:48:43 CEST
 * @description : 
 * */

#ifndef MATH_VALUES_HPP
#define MATH_VALUES_HPP

#include <concepts>

namespace math
{

namespace detail
{
template <typename T>
consteval auto zero()
{
    if constexpr (requires { { T::zero() } -> std::convertible_to<T>; }) {
        return T::zero();
    } else if constexpr (std::constructible_from<T, double>) {
        return T{0};
    } else if constexpr (std::is_default_constructible<T>::value){
        return T{} - T{};
    } else {
        static_assert(false, "can't find a 'zero' value for type T");
    }
}

template <typename T>
consteval auto one()
{
    if constexpr (requires { { T::one() } -> std::convertible_to<T>; }) {
        return T::one();
    } else if constexpr (std::constructible_from<T, double>) {
        return T{1};
    } else if constexpr (std::is_default_constructible<T>::value and std::convertible_to<T, decltype(T{} / T{})>) {
        return T{} / T{};
    } else {
        static_assert(false, "can't find a 'one' value for type T");
    }
}
}  // namespace detail

constexpr inline struct zero_t
{
    template <typename T>
    explicit consteval operator T() const requires (not std::same_as<T, void>) { return detail::zero<T>(); }
} zero;
constexpr inline struct one_t
{
    template <typename T>
    explicit consteval operator T() const requires (not std::same_as<T, void>) { return detail::one<T>(); }
} one;

#define MATH_DEFINE_OP(op, type) \
template <typename T> \
constexpr auto operator op(T const & t, [[maybe_unused]] type##_t _) { return t op detail::type<T>(); }
#define MATH_DEFINE_OPS_FOR(type) \
MATH_DEFINE_OP(==, type) \
MATH_DEFINE_OP(!=, type) \
MATH_DEFINE_OP(<=, type) \
MATH_DEFINE_OP(<, type)  \
MATH_DEFINE_OP(>=, type) \
MATH_DEFINE_OP(>, type)


MATH_DEFINE_OPS_FOR(zero)
MATH_DEFINE_OPS_FOR(one)

#undef MATH_DEFINE_OPS_FOR
#undef MATH_DEFINE_OP


} // namespace math

#endif /* MATH_VALUES_HPP */
