#pragma once
#include "LibBase.h"
#include <iterator>

namespace LIB_NAMESPACE
{
	template <class T>
	struct slice_struct
	{
		T begin_iter;
		T end_iter;

		slice_struct(T _begin_iter, T _end_iter) : begin_iter{ _begin_iter }, end_iter{ _end_iter } {}

		T begin() { return begin_iter; }

		T end() { return end_iter; }

	};

	// iterate from begin iterator to end iterator

	template <class T>
	auto slice(T begin_iter, T end_iter)
	{
		return slice_struct{ begin_iter, end_iter };
	}

	
	// iterate from begin to begin + toIterate
	template <class T>
	auto slice(T&& Container, std::size_t toIterate)
	{
		auto&& begin_iter = std::begin(Container);
		return slice_struct(begin_iter, begin_iter + toIterate);
	}

	// iterate from (begin + from) to (begin + toIterate)
	template <class T>
	auto slice(T&& Container, std::size_t from, std::size_t toIterate)
	{
		auto begin_iter = std::begin(Container);
		return slice_struct(begin_iter + from, begin_iter + toIterate);
	}

};
