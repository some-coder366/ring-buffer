#pragma once
#include "LibBase.h"

#define PROVIDE_ALLOCATOR_TYPES(T) \
	using value_type = T;\
	using pointer = T * ;\
	using const_pointer = const T *;\
	using reference = T & ;\
	using const_reference = const T &; \
	using size_type = std::size_t; \
	using difference_type = std::ptrdiff_t

namespace LIB_NAMESPACE
{

	template <class T, std::size_t N>
	struct StackAllocator
	{
		PROVIDE_ALLOCATOR_TYPES(T);

		pointer allocate(size_type n) noexcept
		{
			return reinterpret_cast<pointer>(StackArray);
		}

		void deallocate(pointer unused_ptr, size_type unused_size) noexcept {}

		unsigned char StackArray[sizeof(value_type) * N];

	};

};
