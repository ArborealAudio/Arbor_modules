/**
 * FastMath.h
 * Fast math functions for faster math
*/
#pragma once

template <typename T>
inline T fast_tanh(T x)
{
    T x2 = x * x;
    T a = x * (135135.0 + x2 * (17325.0 + x2 * (378.0 + x2)));
    T b = 135135.0 + x2 * (62370.0 + x2 * (3150.0 + x2 * 28.0));
    return a / b;
}

template <typename T>
#if USE_SIMD
inline T tanh(T x)
{
    return xsimd::tanh(x);
}
#else
inline T tanh(T x)
{
    return std::tanh(x);
}
#endif

template <typename T>
#if USE_SIMD
inline T abs(T x)
{
    return xsimd::abs(x);
}
#else
inline T abs(T x)
{
    return std::abs(x);
}
#endif