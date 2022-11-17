/**
 * FastMath.h
 * Fast math functions for faster math
*/

template <typename T>
T fast_tanh(T x)
{
    T x2 = x * x;
    T a = x * (135135.0 + x2 * (17325.0 + x2 * (378.0 + x2)));
    T b = 135135.0 + x2 * (62370.0 + x2 * (3150.0 + x2 * 28.0));
    return a / b;
}