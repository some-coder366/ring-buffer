#pragma once
#include "span.h"
#include "slice.h"
#include "StackAllocator.h"
#include <cassert>
#include <type_traits>
#include <vector>
#include <algorithm>

#define PROVIDE_CONTAINER_TYPES(T) \
	using value_type = typename T::value_type;\
	using pointer = typename T::pointer;\
	using const_pointer = typename T::const_pointer;\
	using reference = typename T::reference;\
	using const_reference = typename T::const_reference; \
	using size_type = typename T::size_type; \
	using difference_type = typename T::difference_type

namespace LIB_NAMESPACE
{
	template <class T, class Allocator>
	class ring_buffer;

	template <class T, class Allocator, bool is_const>
	class ring_buffer_iterator
	{
		using rbf_type = std::conditional_t<is_const, const ring_buffer<T, Allocator>, ring_buffer<T, Allocator>>;
		using rbf_ptr_type = rbf_type *;
		rbf_ptr_type rbf_ptr;
		mutable T *ptr;

	public:

		PROVIDE_CONTAINER_TYPES(rbf_type);
		using iterator_category = std::random_access_iterator_tag;

		ring_buffer_iterator() noexcept : rbf_ptr{ nullptr }, ptr{ nullptr } {}

		ring_buffer_iterator(rbf_ptr_type ring_buffer_ptr, pointer ptr) noexcept : rbf_ptr{ ring_buffer_ptr }, ptr{ ptr } {}

		std::conditional_t<is_const, const_reference, reference> operator*() const noexcept;

		std::conditional_t<is_const, const_pointer, pointer> operator->() const noexcept { return &(this->operator*()); }

		ring_buffer_iterator& operator++() noexcept;

		ring_buffer_iterator& operator--() noexcept;

		ring_buffer_iterator operator++(int) noexcept
		{
			auto saved_it{ *this };
			++(*this);
			return saved_it;
		}

		ring_buffer_iterator operator--(int) noexcept
		{
			auto saved_it{ *this };
			--(*this);
			return saved_it;
		}

		ring_buffer_iterator& operator +=(difference_type n) noexcept;

		ring_buffer_iterator& operator -=(difference_type n) noexcept;

		friend ring_buffer_iterator operator+(const ring_buffer_iterator& iter, difference_type n) noexcept
		{
			ring_buffer_iterator it{ iter };
			it += n;
			return it;
		}

		friend ring_buffer_iterator operator+(difference_type n, const ring_buffer_iterator& iter) noexcept
		{
			ring_buffer_iterator it{ iter };
			it += n;
			return it;
		}

		ring_buffer_iterator operator-(difference_type n) const noexcept { return (*this + (-n)); }

		difference_type operator-(const ring_buffer_iterator& other) const noexcept;

		template <bool const_iter = is_const, std::enable_if_t<!const_iter, bool> = true>
		reference operator[](difference_type n) const noexcept { return *(*this + n); }

		template <bool const_iter = is_const, std::enable_if_t<const_iter, bool> = true>
		const_reference operator[](difference_type n) const noexcept { return *(*this + n); }

		friend constexpr bool operator==(const ring_buffer_iterator& lhs, const ring_buffer_iterator& rhs) noexcept { return lhs.ptr == rhs.ptr; }

		friend constexpr bool operator!=(const ring_buffer_iterator& lhs, const ring_buffer_iterator& rhs) noexcept { return lhs.ptr != rhs.ptr; }

	};

	template <class T, class Allocator = std::allocator<T>>
	class ring_buffer : private Allocator
	{
		using alloc_traits = typename std::allocator_traits<Allocator>;

	public:

		using allocator_type = Allocator;
		using value_type = typename alloc_traits::value_type;
		using pointer = typename alloc_traits::pointer;
		using const_pointer = typename alloc_traits::const_pointer;
		using reference = T & ;
		using const_reference = const T&;
		using size_type = typename alloc_traits::size_type;
		using difference_type = typename alloc_traits::difference_type;

		using iterator = ring_buffer_iterator<value_type, Allocator, false>;
		using const_iterator = ring_buffer_iterator < value_type, Allocator, true>;
		using reverse_iterator = std::reverse_iterator<iterator>;
		using const_reverse_iterator = std::reverse_iterator<const_iterator>;

		ring_buffer() noexcept : buffer_start{ nullptr } {}

		explicit ring_buffer(const allocator_type& alloc) : allocator_type(alloc), buffer_start{ nullptr } { }

		ring_buffer(size_type capacity_size) : buffer_start{ AllocMem(capacity_size) },
			buffer_end{ buffer_start + capacity_size }, front_ptr{ buffer_start }, back_ptr{ buffer_start }, count{ 0 }
		{
			assert(buffer_start != nullptr && "the ring buffer couldn't allocate memory");
		}

		ring_buffer(size_type capacity_size, const allocator_type& alloc) : allocator_type(alloc),
			buffer_start{ AllocMem(capacity_size) }, buffer_end{ buffer_start + capacity_size }, front_ptr{ buffer_start }, back_ptr{ buffer_start }, count{ 0 }
		{
			assert(buffer_start != nullptr && "the ring buffer couldn't allocate memory");
		}

		ring_buffer(size_type capacity_size_and_count, const value_type& value) :
			buffer_start{ AllocMem(capacity_size_and_count) }, buffer_end{ buffer_start + capacity_size_and_count },
			front_ptr{ buffer_start }, back_ptr{ buffer_start }, count{ capacity_size_and_count }
		{
			assert(capacity_size_and_count > 0 && "invalid capacity");
			assert(buffer_start != nullptr && "the ring buffer couldn't allocate memory");
			std::uninitialized_fill(front_ptr, front_ptr + size(), value);
		}

		ring_buffer(size_type capacity_size_and_count, const value_type& value, const allocator_type& alloc) : allocator_type(alloc),
			buffer_start{ AllocMem(capacity_size_and_count) }, buffer_end{ buffer_start + capacity_size_and_count },
			front_ptr{ buffer_start }, back_ptr{ buffer_end }, count{ capacity_size_and_count }
		{
			assert(capacity_size_and_count > 0 && "invalid capacity");
			assert(buffer_start != nullptr && "the ring buffer couldn't allocate memory");
			std::uninitialized_fill(front_ptr, front_ptr + size(), value);
		}

		ring_buffer(size_type capacity_size, size_type buffer_size, const value_type& value) : buffer_start{ AllocMem(capacity_size) },
			buffer_end{ buffer_start + capacity_size }, front_ptr{ buffer_start }, back_ptr{ buffer_start + buffer_size }, count{ buffer_size }
		{
			assert(capacity_size >= buffer_size && "ring buffer size can't be greater than capacity");
			assert(buffer_start != nullptr && "the ring buffer couldn't allocate memory");
			if (back_ptr == buffer_end)
				back_ptr = buffer_start;
			std::uninitialized_fill(front_ptr, front_ptr + size(), value);
		}

		ring_buffer(size_type capacity_size, size_type buffer_size, const value_type& value, const allocator_type& alloc) : allocator_type(alloc),
			buffer_start{ AllocMem(capacity_size) },
			buffer_end{ buffer_start + capacity_size }, front_ptr{ buffer_start }, back_ptr{ buffer_start + buffer_size }, count{ buffer_size }
		{
			assert(capacity_size >= buffer_size && "ring buffer size can't be greater than capacity");
			assert(buffer_start != nullptr && "the ring buffer couldn't allocate memory");
			std::uninitialized_fill(front_ptr, front_ptr + size(), value);
		}

		template <class InputIterator>
		ring_buffer(InputIterator first, InputIterator last) : ring_buffer(udistance(first, last))
		{
			std::uninitialized_copy(first, last, front_ptr);
			count = capacity();
		}

		template <class InputIterator>
		ring_buffer(InputIterator first, InputIterator last, const allocator_type& alloc) : ring_buffer(udistance(first, last), alloc)
		{
			std::uninitialized_copy(first, last, front_ptr);
			count = capacity();
		}

		ring_buffer(const ring_buffer& other) : count{ other.count }, buffer_start{ nullptr }
		{
			if (other.buffer_start)
			{
				size_type capacity_size = other.capacity();
				buffer_start = AllocMem(capacity_size);
				buffer_end = buffer_start + capacity_size;
				front_ptr = buffer_start + (other.front_ptr - other.buffer_start);
				back_ptr = buffer_start + (other.back_ptr - other.buffer_start);

				if (is_linearized())
					std::uninitialized_copy(other.front_ptr, other.front_ptr + size(), front_ptr);
				else
				{
					auto array1 = get_array<1, true>(false);
					auto array2 = get_array<2, true>(false);
					std::uninitialized_copy(array1.begin(), array1.end(), front_ptr);
					std::uninitialized_copy(array2.begin(), array2.end(), buffer_start);
				}
			}
		}

		ring_buffer(ring_buffer&& other) noexcept : buffer_start{ other.buffer_start }, buffer_end{ other.buffer_end }, front_ptr{ other.front_ptr }
			, back_ptr{ other.back_ptr }, count{ other.count }
		{
			other.buffer_start = nullptr;
		}

		~ring_buffer()
		{
			if (buffer_start)
			{
				DestroyAll();
				FreeMem(buffer_start);
			}
		}

		ring_buffer& operator=(const ring_buffer& other)
		{
			this->~ring_buffer();
			new (this) ring_buffer(other);
			return *this;
		}

		ring_buffer& operator=(ring_buffer&& other) noexcept
		{
			this->~ring_buffer();
			new (this) ring_buffer(std::move(other));
			return *this;
		}

		/*
		emplace, push and pop
		for a FIFO order use emplace_back/push_back with pop_front
		for a LIFP order use emplace_back/push_back with pop_back
		*/

		/*
		emplace_back and push_back

		----------------------------------------------------------
		| =>1 | 2 | 3 | 4 | 5 | => |  |  |  |  |  |  |  |  |  |  |
		---^--------------------^---------------------------------
		   ^                    ^
		  front				   back

		-----------------------------------------------------------
		| =>1 | 2 | 3 | 4 | 5 | 6 | => |  |  |  |  |  |  |  |  |  |
		---^------------------------^------------------------------
		   ^					    ^
		  front				      new back
		*/

		template <class ... Args>
		void emplace_back_without_checks(Args&& ... args)
		{
			assert(buffer_start != nullptr && "void ring_buffer<T>::emplace_back_without_checks(Args&& ... args) the ring buffer has no capacity");
			assert(!full() && "void ring_buffer<T>::emplace_back_without_checks(Args&& ... args) can't append to a full ring buffer without overwriting");
#ifdef __cpp_exceptions
			try
			{
				pointer current_back_ptr = back_ptr;
				increment_back();
				Construct(current_back_ptr, std::forward<Args>(args)...);
			}
			catch (...)
			{
				decrement_back();
				throw;
			}
#else
			Construct(back_ptr, std::forward<Args>(args)...);
			increment_back();
#endif // __cpp_exceptions
			}

		template <class ... Args>
		void emplace_back(Args&& ... args)
		{
			if (full())
				pop_front();
			emplace_back_without_checks(std::forward<Args>(args)...);
		}

		template <class ... Args>
		bool try_emplace_back(Args&& ... args)
		{
			if (full())
				return false;
			emplace_back_without_checks(std::forward<Args>(args)...);
			return true;
		}

		void push_back_without_checks(value_type&& value) { emplace_back_without_checks(std::move_if_noexcept(value)); }

		void push_back_without_checks(const value_type& value) { emplace_back_without_checks(value); }

		void push_back(value_type&& value) { emplace_back(std::move_if_noexcept(value)); }

		void push_back(const value_type& value) { emplace_back(value); }

		bool try_push_back(value_type&& value) { return try_emplace_back(std::move_if_noexcept(value)); }

		bool try_push_back(const value_type& value) { return try_emplace_back(value); }

		template <class InputIter>
		void insert_back(InputIter first, InputIter last)
		{
			assert(buffer_start != nullptr && "void ring_buffer<T>::emplace_back_without_checks(Args&& ... args) the ring buffer has no capacity");

			size_type range_size = udistance(first, last);
			bool linearized = is_linearized();
			size_type available_size = reserve();
			size_type capacity_size = capacity();

			if (available_size >= range_size)
			{
				count += range_size;

				if (front_ptr == buffer_start)
				{
					back_ptr = std::uninitialized_copy(first, last, back_ptr);
					if (back_ptr == buffer_end)
						back_ptr = buffer_start;
				}
				else if (back_ptr == buffer_start || !linearized)
				{
					back_ptr = std::uninitialized_copy(first, last, back_ptr);
				}
				else
				{
					auto end1 = first + std::min(range_size, (buffer_end - back_ptr));
					back_ptr = std::uninitialized_copy(first, end1, back_ptr);
					if (end1 != last)
						back_ptr = std::uninitialized_copy(end1, last, buffer_start);
					else if (back_ptr == buffer_end)
						back_ptr = buffer_start;
				}
			}

			else if (range_size < capacity_size)
			{
				count = capacity_size;

				// fill the reverse then overwrite starting from front ptr

				if (front_ptr == buffer_start)
				{
					/*
					----------------------------------------------------
					| >>1 | 2 | 3 | 4 | 5 | => |  |  |  |  |  |  |  |  |
					----------------------------------------------------

					*/

					std::destroy(front_ptr, front_ptr + range_size - available_size);
					auto end1 = first + available_size;
					std::uninitialized_copy(first, end1, back_ptr);
					front_ptr = back_ptr = std::uninitialized_copy(end1, last, buffer_start);
				}
				
				else if (back_ptr == buffer_start)
				{
					/*
					----------------------------------------------------
					| => |  |  |  |  |  |  |  |  | >>1 | 2 | 3 | 4 | 5 |
					----------------------------------------------------
					*/

					std::destroy(front_ptr, front_ptr + range_size - available_size);
					front_ptr = back_ptr = std::uninitialized_copy(first, last, back_ptr);

				}

				else if (!linearized)
				{
					/*
					----------------------------------------
					| 4 | 5 | => |  |  |  |  | >>1 | 2 | 3 |
					----------------------------------------
					*/

					size_type array1_size = buffer_end - front_ptr;
					range_size -= available_size;
					size_type to_copy = std::min(range_size, array1_size);
					range_size -= to_copy;

					std::destroy(front_ptr, front_ptr + to_copy);
					auto end1 = first + available_size + to_copy;
					back_ptr = front_ptr = std::uninitialized_copy(first, end1, back_ptr);

					if (range_size)
					{
						std::destroy(buffer_start, buffer_start + range_size);
						back_ptr = front_ptr = std::uninitialized_copy(end1, last, buffer_start);
					}

					else if (back_ptr == buffer_end)
						back_ptr = front_ptr = buffer_start;
				}
				
				else
				{
					/*
					-------------------------------------------------
					|  |  |  | >>1 | 2 | 3 | 4 | 5 | => |  |  |  |  |
					-------------------------------------------------
					*/

					range_size -= available_size;
					std::destroy(front_ptr, front_ptr + range_size);

					auto end1 = first + (buffer_end - back_ptr);
					std::uninitialized_copy(first, end1, back_ptr);
					front_ptr = back_ptr = std::uninitialized_copy(end1, last, buffer_start);
				}
			}

			else
			{
				count = capacity_size;
				DestroyAll();
				first += range_size - capacity_size;
				std::uninitialized_copy(first, last, buffer_start);
				front_ptr = back_ptr = buffer_start;
			}

		}

		/*
		pop front : removes elements from the begin of the buffer then advance the the front pointer
		before the pop the front ptr points to the front object
		after the pop the front ptr moves towards the back ptr to point to the next object which becomes the new front object
		=====>
		 front                                back
		   ^                                   ^
		   |                                   |
		------------------------------------------------------------------------------
		| >1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | < |  |  |  |  |  |  |  |  |  |  |  |  |
		------------------------------------------------------------------------------
		------>
		   new front                        back
			  ^                               ^
			  |                               |
		--------------------------------------------------------------------------
		|  | >2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | < |  |  |  |  |  |  |  |  |  |  |  |
		--------------------------------------------------------------------------
		*/

		void pop_front(value_type& value)
		{
			assert(buffer_start != nullptr && "void ring_buffer<T>::pop_front(value_type& value) the ring buffer has no capacity");
			assert(!empty() && "void ring_buffer<T>::pop_front(value_type& value) the ring buffer is empty");
#ifdef __cpp_exceptions
			try
			{
				pointer current_front = front_ptr;
				increment_front();
				value = std::move_if_noexcept(*current_front);
				current_front->~T();
			}
			catch (...)
			{
				decrement_front();
				throw;
			}
#else
			value = std::move_if_noexcept(*front_ptr);
			front_ptr->~T();
			increment_front();
#endif // __cpp_exceptions
		}

		void pop_front() noexcept
		{
			assert(buffer_start != nullptr && "void ring_buffer<T>::pop_front() the ring buffer has no capacity");
			assert(!empty() && "void ring_buffer<T>::pop_front() the ring buffer is empty");

			front_ptr->~T();
			increment_front();
		}

		bool try_pop_front(value_type& value)
		{
			if (empty())
				return false;
			pop_front(value);
			return true;
		}

		bool try_pop_front() noexcept
		{
			if (empty())
				return false;
			pop_front();
			return true;
		}

		template <class OutputIter>
		OutputIter pop_front(OutputIter first, OutputIter last)
		{
			return pop_front(first, udistance(first, last));
		}

		template <class OutputIter>
		OutputIter pop_front(OutputIter dest_first, size_type num)
		{
			num = std::min(num, size());
			if (!num)
				return dest_first;

			count -= num;
			OutputIter ret;

			if (is_linearized())
			{
				ret = std::copy(std::make_move_iterator(front_ptr), std::make_move_iterator(front_ptr + num), dest_first);
				std::destroy(front_ptr, front_ptr + num);
				front_ptr += num;
				if (front_ptr == buffer_end)
					front_ptr = buffer_start;
			}

			else
			{
				auto array1 = get_array<1, false>(false);
				size_type num1 = std::min(num, array1.size());
				ret = std::copy(std::make_move_iterator(array1.begin()), std::make_move_iterator(array1.begin() + num1), dest_first);
				std::destroy(array1.begin(), array1.begin() + num1);

				front_ptr += num1;
				if (front_ptr == buffer_end)
					front_ptr = buffer_start;

				size_type rem = num - num1;
				if (rem)
				{
					auto array2 = get_array<2, false>(false);
					ret = std::copy(std::make_move_iterator(array2.begin()), std::make_move_iterator(array2.begin() + rem), ret);
					std::destroy(array2.begin(), array2.begin() + rem);
					front_ptr += rem;
				}
			}

			return ret;
		}

		/*
		pop back : removes elements from the end of the buffer then decrement the back pointer
		before the pop the back pointer points to the place past the last element (the place in which a new element would be inserted at the back)
		after the pop the back pointer points to the place of the popped element

		------------------------------------------------------------------
		| =>1 | 2 | 3 | 4 | 5 | 6 | 7 | => |  |  |  |  |  |  |  |  |  |  |
		--^-----------------------------^---------------------------------
		  ^                             ^
		 front                         back
						<------------------
		-----------------------------------------------------------------
		| =>1 | 2 | 3 | 4 | 5 | 6 | => |  |  |  |  |  |  |  |  |  |  |  |
		--^-------------------------^------------------------------------
		  ^                         ^
		 front                     new back
		*/

		void pop_back(value_type& value)
		{
			assert(buffer_start != nullptr && "void ring_buffer<T>::pop_back(value_type& value) the ring buffer has no capacity");
			assert(!empty() && "void ring_buffer<T>::pop_back(value_type& value) the ring buffer is empty");

#ifdef __cpp_exceptions
			try
			{
				decrement_back();
				value = std::move_if_noexcept(*back_ptr);
				back_ptr->~T();
			}
			catch (...)
			{
				increment_back();
				throw;
			}
#else
			decrement_back();
			value = std::move_if_noexcept(*back_ptr);
			back_ptr->~T();
#endif // __cpp_exceptions
			}

		void pop_back() noexcept
		{
			assert(buffer_start != nullptr && "void ring_buffer<T>::pop_back() the ring buffer has no capacity");
			assert(!empty() && "void ring_buffer<T>::pop_back() the ring buffer is empty");

			decrement_back();
			back_ptr->~T();
		}

		bool try_pop_back(value_type& value)
		{
			if (empty())
				return false;
			pop_back(value);
			return true;
		}

		bool try_pop_back() noexcept
		{
			if (empty())
				return false;
			pop_back();
			return true;
		}

		template <class OutputIter>
		OutputIter pop_back(OutputIter first, OutputIter last)
		{
			return pop_back(first, udistance(first, last));
		}

		template <class OutputIter>
		OutputIter pop_back(OutputIter dest_first, size_type num)
		{
			num = std::min(num, size());
			if (!num)
				return dest_first;

			count -= num;
			OutputIter ret = dest_first;

			if (is_linearized())
			{
				decrement(back_ptr);
				pointer end_copy_ptr = back_ptr;
				back_ptr -= num;
				ret = std::copy(std::make_move_iterator(back_ptr), std::make_move_iterator(end_copy_ptr), dest_first);
				std::destroy(back_ptr, end_copy_ptr);
			}

			else
			{
				auto array2 = get_array<2, false>(false);
				size_type num1 = std::min(num, array2.size());
				size_type rem = num - num1;
				if (rem)
				{
					auto array1 = get_array<1, false>(false);
					auto start_copy_ptr = std::prev(array1.end(), rem);
					ret = std::copy(std::make_move_iterator(start_copy_ptr), std::make_move_iterator(array1.end()), dest_first);
					std::destroy(start_copy_ptr, array1.end());
				}
				auto start_copy_ptr = std::prev(array2.end(), num1);
				ret = std::copy(std::make_move_iterator(start_copy_ptr), std::make_move_iterator(array2.end()), ret);
				std::destroy(start_copy_ptr, array2.end());
				decrement_by(back_ptr, num);
			}

			return ret;
		}

		/*
		ranges
		*/

		iterator begin() { return iterator{ this, !empty() ? front_ptr : nullptr }; }

		const_iterator begin() const { return const_iterator{ this, !empty() ? front_ptr : nullptr }; }

		const_iterator cbegin() const { return begin(); }

		iterator end() { return iterator{ this, nullptr }; }

		const_iterator end() const { return const_iterator{ this, nullptr }; }

		const_iterator cend() const { return end(); }

		reverse_iterator rbegin() { return reverse_iterator{ end() }; }

		const_reverse_iterator rbegin() const { return const_reverse_iterator{ end() }; }

		const_reverse_iterator crbegin() const { return rbegin(); }

		reverse_iterator rend() { return reverse_iterator{ begin() }; }

		const_reverse_iterator rend() const { return const_reverse_iterator{ begin() }; }

		const_reverse_iterator crend() const { return rend(); }

		/*
			^ ==> front pointer
			> ==> back pointer
			data starts from front pointer to back pointer

			1 - linearized : there is one array starting from front pointer to back pointer
			--------------------------------------------------------------------------
			| =>1 | 2 | 3 | 4 | 5 | => |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |
			---^--------------------^-------------------------------------------------
			   ^                    ^
			   front               back

			2 - not linearized :
			the first array is from front pointer until the end of the buffer (last index N-1)
			the second array is from the start of the buffer until the back pointer
			----------------------------------------------------------------------------------------
			| 6 | 7 | 8 | => |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  | =>1 | 2 | 3 | 4 | 5 |
			--------------^-----------------------------------------------------^-------------------
						  ^                                                     ^
						  ^                                                     ^
						 back                                                  front
			*/

		template <size_t OneOrTwo, bool const_array>
		std::span<std::conditional_t<const_array, const value_type, value_type>> get_array(bool is_buffer_linearized) const noexcept
		{
			static_assert((OneOrTwo == 1 || OneOrTwo == 2), " wrong array !!!");
			assert(buffer_start != nullptr && "the ring buffer has no capacity");

			if constexpr (OneOrTwo == 1)
			{
				if (is_buffer_linearized)
					return { front_ptr, front_ptr + size() };
				else
					return { front_ptr, udistance(front_ptr, buffer_end) };
			}

			else
			{
				if (is_buffer_linearized)
					return {};
				else
					return { buffer_start, back_ptr };
			}
		}

		std::span<value_type> array_one() noexcept
		{
			return get_array<1, false>(is_linearized());
		}

		std::span<const value_type> array_one() const noexcept
		{
			return get_array<1, true>(is_linearized());
		}

		std::span<value_type> array_two() noexcept
		{
			return get_array<2, false>(is_linearized());
		}

		std::span<const value_type> array_two() const noexcept
		{
			return get_array<2, true>(is_linearized());
		}


		/*
		accessors
		*/

		reference front() noexcept
		{
			assert(buffer_start != nullptr && "reference ring_buffer<T>::front() noexcept : the ring buffer has no capacity");
			assert(!empty() && "reference ring_buffer<T>::front() noexcept : the ring buffer is empty()");

			return *front_ptr;
		}

		const_reference front() const noexcept
		{
			assert(buffer_start != nullptr && "const_reference ring_buffer<T>::front() const noexcept : the ring buffer has no capacity");
			assert(!empty() && "const_reference ring_buffer<T>::front() const noexcept : the ring buffer is empty()");

			return *front_ptr;
		}

		reference back()
		{
			assert(buffer_start != nullptr && "reference ring_buffer<T>::back() the ring buffer has no capacity");
			assert(!empty() && "reference ring_buffer<T>::back() : the ring buffer is empty()");

			pointer last_elem = back_ptr;
			decrement(last_elem);
			return *last_elem;
		}

		const_reference back() const noexcept
		{
			assert(buffer_start != nullptr && "const_reference ring_buffer<T>::back() const noexcept : the ring buffer has no capacity");
			assert(!empty() && "const_reference ring_buffer<T>::back() const noexcept : the ring buffer is empty()");

			pointer last_elem = back_ptr;
			decrement(last_elem);
			return *last_elem;
		}

		reference operator[](size_type index) noexcept
		{
			assert(buffer_start != nullptr && "reference ring_buffer<T>::operator[](size_type index) noexcept : the ring buffer has no capacity");
			assert(index >= 0 && index < size() && "reference ring_buffer<T>::operator[](size_type index) noexcept : index out of range");

			pointer ptr = front_ptr;
			increment_by(ptr, index);
			return *ptr;
		}

		const_reference operator[](size_type index) const
		{
			assert(buffer_start != nullptr && "const_reference ring_buffer<T>::operator[](size_type index) const the ring buffer has no capacity");
			assert(index >= 0 && index < size() && "const_reference ring_buffer<T>::operator[](size_type index) const noexcept : index out of range");

			pointer ptr = front_ptr;
			increment_by(ptr, index);
			return *ptr;
		}

		/*
		modifiers
		*/


		pointer linearize()
		{

			assert(buffer_start != nullptr && "the ring buffer has no capacity");

			if (empty())
				return nullptr;

			else if (is_linearized())
				return front_ptr;

			auto array1 = array_one();
			auto array2 = array_two();

			auto it1 = array1.begin();
			auto it2 = array2.begin();
			auto end1 = array1.end();
			auto end2 = array2.end();

			if (array1.size() > array2.size())
			{
				// initizalize the uninitialized memory
				auto free_mem = reserve();
				for (auto& uninit : slice(end2, end2 + free_mem))
					new (&uninit) T();

				// shift the free mem to the most back of the buff
				std::rotate(end2, it1, end1);

				// uninitizalize the initialized  free memory
				for (auto& inited : slice(buffer_end - free_mem, buffer_end))
					inited.~T();

				// swap array2 with first elements of array1

				auto array2_size = array2.size();
				auto rit1 = buffer_end - array2_size;

				while (1)
				{
					std::swap_ranges(it2, end2, rit1);
					rit1 -= array2_size;
					if (rit1 < it1)
					{
						rit1 += array2_size;
						break;
					}
				}

				// shift the front to the most front of the buffer
				std::rotate(it2, it1, rit1);

			}

			else if (array2.size() > array1.size())
			{

				// swap array1 with first elements of array2

				auto array1_size = array1.size();
				while (array1_size >= static_cast<size_t>(std::distance(it2, array2.end())))
					it2 = std::swap_ranges(it1, end1, it2);

				// initizalize the uninitialized memory
				auto free_mem = reserve();
				for (auto& uninit : slice(end2, end2 + free_mem))
					new (&uninit) T();

				// shift the back and previously uninitialzed memeory to the most back
				std::rotate(it2, it1, array1.end());

				// uninitizalize the initialized  free memory
				for (auto& inited : slice(end2, end2 + free_mem))
					inited.~T();

			}

			front_ptr = buffer_start;
			back_ptr = front_ptr + size();

			return buffer_start;
		}

		void swap(ring_buffer& other) noexcept
		{
			using std::swap;
			swap(buffer_start, other.buffer_start);
			swap(buffer_end, other.buffer_end);
			swap(front_ptr, other.front_ptr);
			swap(back_ptr, other.back_ptr);
			swap(count, other.count);
		}

		void clear()
		{
			assert(buffer_start != nullptr && "the ring buffer has no capacity");
			DestroyAll();
			front_ptr = back_ptr = buffer_start;
			count = 0;
		}

		void set_capacity(size_type capacity_size)
		{
			this->~ring_buffer();
			new (this) ring_buffer(capacity_size);
		}

		void resize(size_type new_size, const value_type& value = value_type())
		{
			auto current_cap = buffer_start ? capacity() : 0;

			if (new_size > current_cap)
			{
				size_type oldsize = size();
				auto temp_buffer = std::move(*this);
				set_capacity(new_size);
				
				bool linearized = is_linearized();
				auto array1 = temp_buffer.get_array<1, false>(linearized);
				auto array2 = temp_buffer.get_array<2, false>(linearized);
				back_ptr = std::uninitialized_copy(std::make_move_iterator(array1.begin()), std::make_move_iterator(array1.end()), buffer_start);
				back_ptr = std::uninitialized_copy(std::make_move_iterator(array2.begin()), std::make_move_iterator(array2.end()), back_ptr);

				std::uninitialized_fill(back_ptr, buffer_end, value);
				back_ptr = buffer_start;
				count = new_size;
			}

			else if (new_size < current_cap)
			{
				if (size() > new_size)
				{
					while (size() > new_size)
						pop_back();
				}
				else
				{
					while (size() < new_size)
						emplace_back_without_checks(value);
				}
			}

			else
			{
				while (!full())
					emplace_back_without_checks(T());
			}
		}

		/*
		info
		*/

		constexpr size_type size() const noexcept { assert(buffer_start != nullptr && "the ring buffer has no capacity"); return count; }

		constexpr size_type capacity() const noexcept { assert(buffer_start != nullptr && "the ring buffer has no capacity"); return udistance(buffer_start, buffer_end); }

		constexpr size_type reserve() const noexcept { return capacity() - size(); }

		constexpr bool empty() const noexcept { return !size(); }

		constexpr bool full() const noexcept { return size() == capacity(); }

		constexpr bool is_linearized() const noexcept { assert(buffer_start != nullptr && "the ring buffer has no capacity"); return back_ptr > front_ptr || back_ptr == buffer_start; }

		Allocator& get_allocator() noexcept
		{
			return static_cast<Allocator&>(*this);
		}

		const Allocator& get_allocator() const noexcept
		{
			return static_cast<const Allocator&>(*this);
		}

		friend bool operator==(const ring_buffer& lhs, const ring_buffer& rhs)
		{
			auto first1 = rhs.begin();
			auto last1 = rhs.end();
			auto first2 = lhs.begin();

			while (first1 != last1)
			{
				if (*first1++ != *first2++)
					return false;
			}

			return true;
		}

		friend bool operator!=(const ring_buffer& lhs, const ring_buffer& rhs)
		{
			return !(lhs == rhs);
		}

	private:

		pointer buffer_start;
		pointer buffer_end;

		pointer front_ptr;
		pointer back_ptr;

		size_type count;

		template <typename, typename, bool> friend class ring_buffer_iterator;

		void increment(pointer& ptr) const
		{
			if (++ptr == buffer_end)
				ptr = buffer_start;
		}

		void increment_by(pointer& ptr, difference_type n) const
		{
			ptr = buffer_start + ((buffer_end - ptr + n) % capacity());
		}

		void decrement(pointer& ptr) const
		{
			if (ptr == buffer_start)
				ptr = buffer_end;
			--ptr;
		}

		void decrement_by(pointer& ptr, difference_type n) const { increment_by(ptr, -n); }

		// push_back
		void increment_back() noexcept
		{
			if (++back_ptr == buffer_end)
				back_ptr = buffer_start;
			++count;
		}

		// pop_front
		void increment_front() noexcept
		{
			if (++front_ptr == buffer_end)
				front_ptr = buffer_start;
			--count;
		}

		// pop_back
		void decrement_back() noexcept
		{
			if (back_ptr == buffer_start)
				back_ptr = buffer_end;
			--back_ptr;
			--count;
		}

		// push_front
		void decrement_front() noexcept
		{
			if (front_ptr == buffer_start)
				front_ptr = buffer_end;
			--front_ptr;
			++count;
		}

		template <class Iter>
		size_type udistance(Iter first, Iter last) const
		{
			return static_cast<size_type>(std::distance(first, last));
		}

		pointer AllocMem(size_type n)
		{
			return alloc_traits::allocate(get_allocator(), n);;
		}

		void FreeMem(pointer Block)
		{
			alloc_traits::deallocate(get_allocator(), Block, 1);
		}

		template <class ... Args>
		void Construct(pointer Ptr, Args && ... args)
		{
			alloc_traits::construct(get_allocator(), Ptr, std::forward<Args>(args)...);
		}

		void DestroyAll()
		{
			if constexpr (!std::is_pod_v<value_type>)
			{
				bool linearized = is_linearized();
				auto array1 = get_array<1, false>(linearized);
				auto array2 = get_array<2, false>(linearized);
				std::destroy(array1.begin(), array1.end());
				std::destroy(array2.begin(), array2.end());
			}
		}

		};

	template <class T, std::size_t N>
	class stack_ring_buffer : public ring_buffer<T, StackAllocator<T, N>>
	{
		using base = ring_buffer<T, StackAllocator<T, N>>;
	public:

		PROVIDE_CONTAINER_TYPES(base);

		stack_ring_buffer() noexcept : base(N) {}

		stack_ring_buffer(const stack_ring_buffer& other) : base(other) {}

		stack_ring_buffer(stack_ring_buffer&& other) noexcept : stack_ring_buffer()
		{
			std::uninitialized_move(other.begin(), other.end(), std::back_inserter(*this));
		}

		template <class InputIter>
		stack_ring_buffer(InputIter first, InputIter last) : base(first, last) {}

		stack_ring_buffer& operator=(const stack_ring_buffer& other)
		{
			base::operator=(other);
			return *this;
		}

		stack_ring_buffer& operator=(stack_ring_buffer&& other) noexcept
		{
			base::~ring_buffer();
			new (this) stack_ring_buffer(std::move(other));
			return *this;
		}

		void set_capacity(size_type) = delete;

		void resize(size_type) = delete;

	};

	template <class T, class Allocator, bool is_const>
	inline auto ring_buffer_iterator<T, Allocator, is_const>::operator*() const noexcept -> std::conditional_t<is_const, const_reference, reference>
	{
		assert(ptr != nullptr && "ring_buffer_iterator::operator*() derefrencing null iterator");
#ifdef _DEBUG
		if (!rbf_ptr->full())
		{
			if (rbf_ptr->is_linearized())
				assert(ptr >= rbf_ptr->front_ptr && ptr < rbf_ptr->back_ptr && "the iterator is out of range");
			else
			{
				auto array1 = rbf_ptr->array_one();
				auto array2 = rbf_ptr->array_two();
				assert((ptr >= array1.end() && ptr < array1.end()) || (ptr >= array2.end() && ptr < array2.end()) && "the iterator is out of range");
			}
		}
#endif // _DEBUG
		return *ptr;
	}

	template <class T, class Allocator, bool is_const>
	inline ring_buffer_iterator<T, Allocator, is_const>& ring_buffer_iterator<T, Allocator, is_const>::operator++() noexcept
	{
		rbf_ptr->increment(ptr);

		if (ptr == rbf_ptr->back_ptr)
			ptr = nullptr;

		return *this;
	}

	template <class T, class Allocator, bool is_const>
	inline ring_buffer_iterator<T, Allocator, is_const>& ring_buffer_iterator<T, Allocator, is_const>::operator--() noexcept
	{
		if (ptr == nullptr) // for --end(), std::prev(end()) which is used with reverse iterator
			ptr = rbf_ptr->back_ptr;

		rbf_ptr->decrement(ptr);

		return *this;
	}

	template<class T, class Allocator, bool is_const>
	inline auto ring_buffer_iterator<T, Allocator, is_const>::operator-(const ring_buffer_iterator & other) const noexcept -> difference_type
	{
		// convert pointer to index
		auto make_linear_pointer = [&](pointer ptr) -> pointer
		{
			if (this->rbf_ptr->is_linearized())
				return ptr;
			else
			{
				if (ptr >= rbf_ptr->front_ptr && ptr < rbf_ptr->buffer_end)
					return ptr;
				else
					return ptr + (rbf_ptr->buffer_end - rbf_ptr->front_ptr);
			}
		};
		return make_linear_pointer(ptr) - make_linear_pointer(other.ptr);
	}

	template <class T, class Allocator, bool is_const>
	inline ring_buffer_iterator<T, Allocator, is_const>& ring_buffer_iterator<T, Allocator, is_const>::operator+=(typename ring_buffer_iterator::difference_type n) noexcept
	{
		rbf_ptr->increment_by(ptr, n);

		if (ptr == rbf_ptr->back_ptr)
			ptr = nullptr;

		return *this;
	}

	template <class T, class Allocator, bool is_const>
	inline ring_buffer_iterator<T, Allocator, is_const>& ring_buffer_iterator<T, Allocator, is_const>::operator-=(typename ring_buffer_iterator::difference_type n) noexcept
	{
		if (ptr == nullptr)
			ptr = rbf_ptr->back_ptr;

		rbf_ptr->decrement_by(ptr, n);

		return *this;
	}

};

namespace std
{
	template <class T, class Allocator>
	inline void swap(LIB_NAMESPACE::ring_buffer<T, Allocator>& lhs, LIB_NAMESPACE::ring_buffer<T, Allocator>& rhs) noexcept
	{
		lhs.swap(rhs);
	}
};
