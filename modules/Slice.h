// Slice.h

#pragma once
#include <stddef.h>

// A modern type of data buffer.
// The slice does not own the memory, so it must outlive the slice
struct Slice {
	void *data;
	size_t len;
};

// T = data, S = size
#define to_slice(T, S) Slice{(T), (S)}
