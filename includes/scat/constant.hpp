// Contains "Constant Time" implementations of various expressions.
//
// Constant time refers to code that does not perform data dependent branches or data dependent
//  accesses to memory. Constant Time code is typically used in cryptographic implementations to
//  mitigate side-channel attacks. In scat it is mostly used in speculative execution (spectre)
//  attacks to prevent branches from interfering with various speculation units in the processor.
//
// When an expression in this file returns something truthy, such as is_notzero, it will return
//  No bits set for false
//  All bits set for true
//
// In general you should prefer to use if_ versions of each function as they will allow you to
//  return specific values in a constant time way.
//
// We assume we're running on a 2s complement platform
#ifndef SCAT_HEADER_CONSTANT
#define SCAT_HEADER_CONSTANT

#include <cstddef>
#include <type_traits>

namespace scat::constant {

// Evaluate the expression (value != 0) without using branches
template<class T>
inline T is_notzero(T value){
    using S = std::make_signed_t<T>;
    using U = std::make_unsigned_t<T>;
    constexpr size_t shift = sizeof(T) * 8 - 1;

    // If zero, value remains zero
    // Otherwise, make value some positive value
    //  Logical shift right
    value = static_cast<T>(static_cast<U>(value) >> 1) | (value & 1);

    // If zero, MSB is zero
    // Otherwise, MSB is one
    value = -value;

    // Set all bits to same value as MSB
    //  Arithemtic shift right
    return static_cast<T>(static_cast<S>(value) >> shift);
}

// Evaluate the expression (value == 0) without using branches
template<class T>
inline T is_zero(T value){
    return ~is_notzero<T>(value);
}

// Evaluate the expression (left < right) without using branches (Unsigned)
template<
    class T,
    std::enable_if_t<std::is_unsigned_v<T>, int> = 0
>
inline T is_lessthan(T left, T right){
    using S = std::make_signed_t<T>;
    constexpr size_t shift = sizeof(T) * 8 - 1;
    return static_cast<T>(static_cast<S>(left - right) >> shift);
}

// Evaluate the expression (left < right) without using branches (Signed)
template<
    class T,
    std::enable_if_t<std::is_signed_v<T>, int> = 0
>
inline T is_lessthan(T left, T right){
    constexpr size_t shift = sizeof(T) * 8 - 1;

    // Set mask if left and right have different signs
    auto mask = (left ^ right) >> shift;

    // Handle case where left and right have different sign
    //  We only need to check if left is negative or not.
    //  If it's negative it must be smaller than positive right hand side, and will have MSB set.
    // Set all bits to same value as left MSB
    auto value = mask & (left >> shift);

    // Handle case where left and right have same sign
    value |= ~mask & ((left - right) >> shift);

    return value;
}

// Evaluate the expression (left > right) without using branches
template<class T>
inline T is_greaterthan(T left, T right){
    return is_lessthan<T>(right, left);
}

// Evaluate the expression (left <= right) without using branches
template<class T>
inline T is_lessthanequal(T left, T right){
    return ~is_lessthan<T>(right, left);
}

// Evaluate the expression (left >= right) without using branches
template<class T>
inline T is_greaterthanequal(T left, T right){
    return ~is_lessthan<T>(left, right);
}

// Evaluate the expression (left == right) without using branches
template<class T>
inline T is_equal(T left, T right){
    return is_zero<T>(left ^ right);
}

// Evaluate the expression (left != right) without using branches
template<class T>
inline T is_notequal(T left, T right){
    return is_notzero<T>(left ^ right);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

// Evaluate the expression (value == 0 ? zero : nonzero) without using branches
template<class T>
inline T if_zero(T value, T zero, T nonzero){
    value = is_notzero(value);
    return (value & nonzero) | (~value & zero);
};

// Evaluate the expression (value != 0 ? nonzero : zero) without using branches
template<class T>
inline T if_notzero(T value, T nonzero, T zero){
    value = is_notzero(value);
    return (value & nonzero) | (~value & zero);
};

// Evaluate the expression (left < right ? lt : gte) without using branches
template<class T>
inline T if_lessthan(T left, T right, T lt, T gte){
    auto value = is_lessthan(left, right);
    return (value & lt) | (~value & gte);
};

// Evaluate the expression (left > right ? gt : lte) without using branches
template<class T>
inline T if_greaterthan(T left, T right, T gt, T lte){
    return if_lessthan<T>(right, left, gt, lte);
};

// Evaluate the expression (left <= right ? lte : gt) without using branches
template<class T>
inline T if_lessthanequal(T left, T right, T lte, T gt){
    return if_lessthan<T>(right, left, gt, lte);
};

// Evaluate the expression (left >= right ? gte : lt) without using branches
template<class T>
inline T if_greaterthanequal(T left, T right, T gte, T lt){
    return if_lessthan<T>(left, right, lt, gte);
};

// Evaluate the expression (left == right ? same : different) without using branches
template<class T>
inline T if_equal(T left, T right, T same, T different){
    return if_zero<T>(left ^ right, same, different);
}

// Evaluate the expression (left != right ? different : same) without using branches
template<class T>
inline T if_notequal(T left, T right, T different, T same){
    return if_zero<T>(left ^ right, same, different);
}

} // namespace scat::constant
#endif // SCAT_HEADER_CONSTANT
