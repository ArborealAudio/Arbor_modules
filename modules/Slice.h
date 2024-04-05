// Slice.h

#pragma once
#include <stddef.h>

// A modern type of data buffer.

template <typename T>
struct Slice {
	T *data;
	size_t len;

	Slice() = default;

	Slice(T type[], size_t size) : data(type), len(size) {}
};
