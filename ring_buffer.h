#pragma once
#include <utility>
#include <optional>
#include "span.h"
#include <memory>

#define PROVIDE_CONTAINER_TYPES(T) \
	using value_type = T;\
	using pointer = T * ;\
	using const_pointer = const T *;\
	using reference = T & ;\
	using const_reference = const T &; \
	using size_type = std::size_t; \
	using difference_type = std::ptrdiff_t
  
struct no_init_t {};
constexpr no_init_t no_init;

class ring_buffer_index
	{
		size_t index;

	public:

		ring_buffer_index() : index{0} {}

		ring_buffer_index(size_t pos) : index{pos} {}

		ring_buffer_index& operator++()
		{
			++index;
			return *this;
		}

		ring_buffer_index& operator--()
		{
			--index;
			return *this;
		}

		ring_buffer_index operator+(size_t pos) const { return ring_buffer_index{index + pos}; }

		size_t operator-(ring_buffer_index other) const { return index - other.index; }

		bool operator==(ring_buffer_index other) const { return index == other.index; }

		bool operator!=(ring_buffer_index other) const { return index != other.index; }

		void operator+=(size_t times) { index += times; }

		void operator-=(size_t times) { index -= times; }

		void reset() { index = 0; }

		size_t as_index(size_t N) const
		{
			size_t pos = index;
			pos %= N;
			return pos;
		}
		
	};

	template <class T>
	class uninitialized_array
	{
		std::unique_ptr<unsigned char> raw_buffer;

	public:

		uninitialized_array() noexcept = default;

		uninitialized_array(size_t N) : raw_buffer{ new unsigned char[sizeof(T) * N] } {}

		uninitialized_array(uninitialized_array&& other) noexcept = default;

		uninitialized_array& operator=(uninitialized_array&& other) noexcept = default;

		void copy(const uninitialized_array& other, size_t N)
		{
			raw_buffer.reset();
			if (other.raw_buffer && N)
			{
				raw_buffer.reset(new unsigned char[sizeof(T) * N]);
				std::uninitialized_copy(other.ptr(), other.ptr() + N, ptr());
			}
		}

		void resize(size_t N)
		{
			raw_buffer.reset( new unsigned char[sizeof(T) * N] );
		}

		T * ptr() noexcept { return reinterpret_cast<T*>(raw_buffer.get()); }

		const T * ptr() const noexcept { return reinterpret_cast<const T*>(raw_buffer.get()); }

		T& operator[](size_t pos) noexcept
		{
			return ptr()[pos];
		}

	};

	template <class T, bool reverse = false, bool const_iter = false>
	class ring_buffer_iterator
	{
		T *ptr;
		ring_buffer_index index;
		size_t N;
	
	public:

		PROVIDE_CONTAINER_TYPES(T);
		
		using iterator_category = std::random_access_iterator_tag;

		ring_buffer_iterator(T *ptr, ring_buffer_index index, size_t N) : ptr{ ptr }, index{ index }, N{ N } {}
		
		ring_buffer_iterator& operator++()
		{
			if constexpr (!reverse)
				++index;
			else
				--index;
			return *this;
		}

		ring_buffer_iterator& operator+=(size_t n) { index += n; return *this; }

		ring_buffer_iterator& operator-=(size_t n) { return operator+=(-n); }

		template <bool citer = const_iter, std::enable_if_t<!citer, bool> = true> 
		reference operator*() const
		{
			return ptr[index.as_index(N)];
		}

		template <bool citer = const_iter, std::enable_if_t<citer, bool> = true>
		const_reference operator*() const
		{
			return ptr[index.as_index(N)];
		}

		template <bool citer = const_iter, std::enable_if_t<!citer, bool> = true>
		reference operator[](difference_type n) { return *(*this + n); }

		const_reference operator[](difference_type n) const { return *(*this + n); }

		bool operator!=(ring_buffer_iterator other) const { return index != other.index; }

		bool operator==(ring_buffer_iterator other) const { return index == other.index; }

		friend ring_buffer_iterator operator+(const ring_buffer_iterator& iter, size_t times) { return ring_buffer_iterator{iter.ptr, iter.index + times, iter.N}; }

		friend ring_buffer_iterator operator+(size_t times, const ring_buffer_iterator& iter) { return ring_buffer_iterator{ iter.ptr, iter.index + times, iter.N }; }

		friend ring_buffer_iterator operator-(const ring_buffer_iterator& lhs, size_t times) { return ring_buffer_iterator{lhs.ptr, lhs.index - times, lhs.N}; }

		friend difference_type operator-(const ring_buffer_iterator& lhs, const ring_buffer_iterator& rhs) { return static_cast<difference_type>(lhs.index - rhs.index); }

	};

	template <class T>
	class ring_buffer
	{
		size_t N;
		mutable uninitialized_array<T> raw_buffer;
		ring_buffer_index read_pos, write_pos;

		T& read_ptr() const
		{
			return raw_buffer[read_pos.as_index(N)];
		}

		T& write_ptr() const
		{
			return raw_buffer[write_pos.as_index(N)];
		}

		bool will_remain_linearized(size_t num)
		{
			ring_buffer_index last_elem = write_pos + num - 1;
			return last_elem.as_index(N) >= read_pos.as_index(N);
		}

	public:

		PROVIDE_CONTAINER_TYPES(T);

		using iterator = ring_buffer_iterator<T>;
		using const_iterator = ring_buffer_iterator<T, false, true>;
		using reverse_iterator = ring_buffer_iterator<T, true>;
		using const_reverse_iterator = const ring_buffer_iterator<T, true, true>;

		/*
		contrtuctors and assignment operators
		*/

		ring_buffer() : N{0} {}

		ring_buffer(size_t size) : N{ size }, raw_buffer { size } {}

		ring_buffer(ring_buffer&&) noexcept = default;

		ring_buffer(const ring_buffer& other)
		{
			clear();
			N = other.N;
			read_pos = other.read_pos;
			write_pos = other.write_pos;
			raw_buffer.copy(other.raw_buffer, N);
		}

		ring_buffer(std::initializer_list<value_type> init) : ring_buffer(init.size())
		{
			std::uninitialized_copy(init.begin(), init.end(), raw_buffer.ptr());
		}

		template <class InputIterator>
		ring_buffer(InputIterator first, InputIterator last) : ring_buffer(static_cast<size_type>(std::distance(first, last)))
		{
			std::uninitialized_copy(first, last, raw_buffer.ptr());
			write_pos += capacity();
		}

		ring_buffer& operator=(ring_buffer&& other) noexcept
		{
			clear();
			N = other.N;
			read_pos = other.read_pos;
			write_pos = other.write_pos;
			raw_buffer = std::move(other.raw_buffer);
			return *this;

		}

		ring_buffer& operator=(const ring_buffer& other)
		{
			clear();
			N = other.N;
			read_pos = other.read_pos;
			write_pos = other.write_pos;
			raw_buffer.copy(other.raw_buffer, N);
			return *this;
		}

		~ring_buffer() { clear(); }

		/*
		addition methods
		*/

		/*
		add at the back of the buffer , this is usually used rather than add at the front
		*/

		void push_back_without_checks(const value_type& value)
		{
			emplace_back_without_checks(value);
		}

		void push_back_without_checks(value_type&& value)
		{
			emplace_back_without_checks(std::move(value));
		}

		bool try_push_back(const value_type& value)
		{
			return try_emplace_back(value);
		}

		bool try_push_back(value_type&& value)
		{
			return try_emplace_back(std::move(value));
		}

		void push_back(const value_type& value)
		{
			emplace_back(value);
		}

		void push_back(value_type&& value)
		{
			emplace_back(std::move(value));
		}

		template <class ...Args>
		void emplace_back_without_checks(Args&& ... args)
		{
			new(&write_ptr()) T(std::forward<Args>(args)...);
			++write_pos;
		}

		template <class ...Args>
		bool try_emplace_back(Args&& ... args)
		{
			if (full())
				return false;
			emplace_back_without_checks(std::forward<Args>(args)...);
			return true;
		}

		template <class ...Args>
		void emplace_back(Args&& ... args)
		{
			if (full())
				pop_front();
			emplace_back_without_checks(std::forward<Args>(args)...);
		}

		template <class InputIterator>
		void insert_back(InputIterator first, InputIterator last)
		{
			size_t num = static_cast<size_t>(std::distance(first, last));
			if (will_remain_linearized(num))
			{
				std::uninitialized_copy(first, last, &write_ptr());
				write_pos += num;
			}
			else
				std::copy(first, last, std::back_inserter(*this));
		}

		/*
		add at the front of the buffer
		*/

		void push_front_without_checks(const value_type& value)
		{
			emplace_front_without_checks(value);
		}

		void push_front_without_checks(value_type&& value)
		{
			emplace_front_without_checks(value);
		}

		bool try_push_front(const value_type& value)
		{
			return try_emplace_front(value);
		}

		bool try_push_front(value_type&& value)
		{
			return try_emplace_front(value);
		}

		void push_front(const value_type& value)
		{
			emplace_front(value);
		}

		void push_front(value_type&& value)
		{
			emplace_front(value);
		}

		template <class ... Args>
		void emplace_front_without_checks(Args&& ... args)
		{
			--read_pos;
			new (&read_ptr()) T(std::forward<Args>(args)...);
		}

		template <class ... Args>
		bool try_emplace_front(Args&& ... args)
		{
			if (full())
				return false;
			emplace_front_without_checks(std::forward<Args>(args)...);
		}

		template <class ... Args>
		void emplace_front(Args&& ... args)
		{
			if (full())
				pop_back();
			emplace_front_without_checks(std::forward<Args>(args)...);
		}

		template <class InputIterator>
		void insert_front(InputIterator first, InputIterator last)
		{
			std::copy(first, last, std::front_inserter(*this));
		}

		/*
		extraction methods
		*/

		/*
		extract from the front of the buffer
		used with back insertion to make a FIFO queue
		*/

		void pop_front(value_type& value)
		{
			auto& elem = read_ptr();
			value = std::move(elem);
			elem.~T();
			++read_pos;
		}

		bool try_pop_front(value_type& value)
		{
			if (empty())
				return false;
			pop_front(value);
			return true;
		}

		void pop_front()
		{
			read_ptr().~T();
			++read_pos;
		}

		bool try_pop_front()
		{
			if (empty())
				return false;
			pop_front();
			return true;
		}

		// dumps num of first elements into dest where dest points to initialized memory
		template <class OutputIterator>
		void pop_front(OutputIterator dest, size_t num)
		{
			move_from_front(dest, num);
			if constexpr (std::is_pod_v<value_type>)
				read_pos += num;
			else
			{
				while (num--)
					pop_front();
			}
		}

		// dumps num of first elements into dest where dest points to uninitialized memory
		template <class OutputIterator>
		void pop_front(OutputIterator dest, size_t num, no_init_t)
		{
			move_from_front(dest, num, no_init);
			if constexpr (std::is_pod_v<value_type>)
				read_pos += num;
			else
			{
				while (num--)
					pop_front();
			}
		}

		template <class OutputIterator>
		bool try_pop_front(OutputIterator dest, size_t num)
		{
			if (num > size())
				return false;
			pop_front(dest, num);
			return true;
		}

		template <class OutputIterator>
		bool try_pop_front(OutputIterator dest, size_t num, no_init_t)
		{
			if (num > size())
				return false;
			pop_front(dest, num, no_init);
			return true;
		}


		/*
		extract from the back of the buffer
		used with back insertion to make a LIFO queue
		*/

		void pop_back(value_type& value)
		{
			--write_pos;
			auto& elem = write_ptr();
			value = std::move(elem);
			elem.~T();
		}

		bool try_pop_back(value_type& value)
		{
			if (empty())
				return false;
			pop_back(value);
			return true;
		}

		void pop_back()
		{
			--write_pos;
			write_ptr().~T();
		}

		bool try_pop_back()
		{
			if (empty())
				return false;
			pop_back();
			return true;
		}

		template <class OutputIterator>
		void pop_back(OutputIterator dest, size_t num)
		{
			move_from_back(dest, num);
			if constexpr (std::is_pod_v<value_type>)
				write_pos -= num;
			else
			{
				while (num--)
					pop_back();
			}
		}
		
		template <class OutputIterator>
		void pop_back(OutputIterator dest, size_t num, no_init_t)
		{
			move_from_back(dest, num, no_init);
			if constexpr (std::is_pod_v<value_type>)
				write_pos -= num;
			else
			{
				while (num--)
					pop_back();
			}
		}

		template <class OutputIterator>
		bool try_pop_back(OutputIterator dest, size_t num)
		{
			if (size() < num)
				return false;
			pop_back(dest, num);
			return true;
		}

		template <class OutputIterator>
		bool try_pop_back(OutputIterator dest, size_t num, no_init_t)
		{
			if (size() < num)
				return false;
			pop_back(dest, num, no_init);
			return true;
		}

		/*
		accesors
		*/

		reference front() { return read_ptr(); }

		reference back()
		{
			auto pos = write_pos;
			--pos;
			return raw_buffer[pos.as_index(N)];
		}

		const_reference front() const { return read_ptr(); }

		const_reference back() const
		{
			auto pos = write_pos;
			--pos;
			return raw_buffer[pos.as_index(N)];
		}

		reference operator[](size_type pos) noexcept
		{
			auto index = read_pos + pos;
			return raw_buffer[index.as_index(N)];
		}

		const_reference operator[](size_type pos) const noexcept
		{
			auto index = read_pos + pos;
			return raw_buffer[index.as_index(N)];
		}

		/*
		^ ==> read pointer
		> ==> write pointer
		data starts from read pointer to write pointer

		1 - linearized (from empty to full) : there is one array starting from read pointer to write pointer
		--------------------------------------------------------------------------
		| ^ | 1 | 2 | 3 | 4 | 5 | > |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |
		--------------------------------------------------------------------------

		2 - not linearized :
		the first array is from read pointer until the end of the buffer (last index N-1)
		the second array is from the start of the buffer until the write pointer
		--------------------------------------------------------------------------------------
		| 6 | 7 | 8 | > |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  | ^ | 1 | 2 | 3 | 4 | 5 |  
		--------------------------------------------------------------------------------------
		*/

		std::span<value_type> array_one() noexcept
		{
			pointer read_pointer = &read_ptr();
			if (is_linearized()) // or empty
				return std::span<value_type>{read_pointer, read_pointer + size()};
			else
				return std::span<value_type>{read_pointer, raw_buffer.ptr() - read_pointer + size()};
		}

		std::span<const value_type> array_one() const noexcept
		{
			pointer read_pointer = &read_ptr();
			if (is_linearized()) // or empty
				return std::span<const value_type>{read_pointer, read_pointer + size()};
			else
				return std::span<const value_type>{read_pointer, raw_buffer.ptr() - read_pointer + size()};
		}

		std::span<value_type> array_two() noexcept
		{
			if (is_linearized())
				return {};
			else
			{
				return std::span<value_type>{raw_buffer.ptr(), &write_ptr()};
			}
		}

		std::span<const value_type> array_two() const noexcept
		{
			if (is_linearized())
				return {};
			else
			{
				return std::span<const value_type>{raw_buffer.ptr(), &write_ptr()};
			}
		}

		void copy_from_front(value_type& value) const
		{
			value = front();
		}

		value_type copy_from_front() const
		{
			value_type value;
			copy_from_front(value);
			return value;
		}

		void move_from_front(value_type& value)
		{
			value = std::move(front());
		}

		value_type move_from_front()
		{
			value_type value;
			move_from_front(value);
			return value;
		}

		template <class OutputIterator>
		void copy_from_front(OutputIterator dest, size_t num) const
		{
			if (is_linearized())
			{
				pointer begin_iter = &read_ptr();
				pointer end_iter = begin_iter + num;
				std::copy(begin_iter, end_iter, dest);
			}
			else
			{
				auto begin_iter = begin();
				auto end_iter = begin_iter + num;
				std::copy(begin_iter, end_iter, dest);
			}
		}

		template <class OutputIterator>
		void copy_from_front(OutputIterator dest, size_t num, no_init_t) const
		{
			if (is_linearized())
			{
				pointer begin_iter = &read_ptr();
				pointer end_iter = begin_iter + num;
				std::uninitialized_copy(begin_iter, end_iter, dest);
			}
			else
			{
				auto begin_iter = begin();
				auto end_iter = begin_iter + num;
				std::uninitialized_copy(begin_iter, end_iter, dest);
			}
		}

		template <class OutputIterator>
		void move_from_front(OutputIterator dest, size_t num)
		{
			if (is_linearized())
			{
				pointer begin_iter = &read_ptr();
				pointer end_iter = begin_iter + num;
				std::copy(std::make_move_iterator(begin_iter), std::make_move_iterator(end_iter), dest);
			}
			else
			{
				auto begin_iter = begin();
				auto end_iter = begin_iter + num;
				std::copy(std::make_move_iterator(begin_iter), std::make_move_iterator(end_iter), dest);
			}
		}

		template <class OutputIterator>
		void move_from_front(OutputIterator dest, size_t num, no_init_t)
		{
			if (is_linearized())
			{
				pointer begin_iter = &read_ptr();
				pointer end_iter = begin_iter + num;
				std::uninitialized_move(begin_iter, end_iter, dest);
			}
			else
			{
				auto begin_iter = begin();
				auto end_iter = begin_iter + num;
				std::uninitialized_move(begin_iter, end_iter, dest);
			}
		}

		void copy_from_back(value_type& value) const
		{
			value = back();
		}

		value_type copy_from_back() const
		{
			value_type value;
			copy_from_back(value);
			return value;
		}

		void move_from_back(value_type& value)
		{
			value = std::move(back());
		}

		value_type move_from_back()
		{
			value_type value;
			move_from_back(value);
			return value;
		}

		template <class OutputIterator>
		void copy_from_back(OutputIterator dest, size_t num) const
		{
			if (is_linearized())
			{
				pointer end_iter = &write_ptr();
				pointer first = end_iter - num;
				std::copy(first, end_iter, dest);
			}
			else
			{
				auto end_iter = end();
				auto first = end_iter - num;
				std::copy(first, end_iter, dest);
			}
		}

		template <class OutputIterator>
		void copy_from_back(OutputIterator dest, size_t num, no_init_t) const
		{
			if (is_linearized())
			{
				pointer end_iter = &write_ptr();
				pointer first = end_iter - num;
				std::uninitialized_copy(first, end_iter, dest);
			}
			else
			{
				auto end_iter = end();
				auto first = end_iter - num;
				std::uninitialized_copy(first, end_iter, dest);
			}
		}

		template <class OutputIterator>
		void move_from_back(OutputIterator dest, size_t num)
		{
			if (is_linearized())
			{
				pointer end_iter = &write_ptr();
				pointer first = end_iter - num;
				std::copy(std::make_move_iterator(first), std::make_move_iterator(end_iter), dest);
			}
			else
			{
				auto end_iter = end();
				auto first = end_iter - num;
				std::copy(std::make_move_iterator(first), std::make_move_iterator(end_iter), dest);
			}
		}

		template <class OutputIterator>
		void move_from_back(OutputIterator dest, size_t num, no_init_t)
		{
			if (is_linearized())
			{
				pointer end_iter = &write_ptr();
				pointer first = end_iter - num;
				std::uninitialized_move(first, end_iter, dest);
			}
			else
			{
				auto end_iter = end();
				auto first = end_iter - num;
				std::uninitialized_move(first, end_iter, dest);
			}
		}

		/*
		range methods
		*/

		iterator begin() noexcept { return iterator{raw_buffer.ptr(), read_pos, N}; }

		iterator end() noexcept { return iterator{raw_buffer.ptr(), write_pos, N}; }

		const_iterator begin() const noexcept { return const_iterator{raw_buffer.ptr(), read_pos, N}; }

		const_iterator end() const noexcept { return const_iterator{raw_buffer.ptr(), write_pos, N}; }

		const_iterator cbegin() const noexcept { return begin(); }

		const_iterator cend() const noexcept { return end(); }

		reverse_iterator rbegin() noexcept { return reverse_iterator{ raw_buffer.ptr(), write_pos - 1, N }; }

		reverse_iterator rend() noexcept { return reverse_iterator{ raw_buffer.ptr(), read_pos - 1, N }; }

		const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator{ raw_buffer.ptr(), write_pos - 1, N }; }

		const_reverse_iterator rend() const noexcept { return const_reverse_iterator{ raw_buffer.ptr(), read_pos - 1, N }; }

		const_reverse_iterator crbegin() const noexcept { return rbegin(); }

		const_reverse_iterator crend() const noexcept { return rend(); }


		/*
		eraser, linearization, resize and some info
		*/

		void clear()
		{
			if constexpr (std::is_pod_v<value_type>)
			{
				read_pos.reset();
				write_pos.reset();
			}
			else
			{
				while (!empty())
					pop_front();
			}
		}

		pointer linearize()
		{
			ring_buffer rbf(N);
			auto first_array = array_one();
			rbf.insert_back(std::make_move_iterator(first_array.begin()), std::make_move_iterator(first_array.end()));
			auto second_array = array_two();
			rbf.insert_back(std::make_move_iterator(second_array.begin()), std::make_move_iterator(second_array.end()));
			this->operator=(std::move(rbf));
			return raw_buffer.ptr();
		}

		template <class OutputIterator>
		void linearize(OutputIterator dest) const
		{
			auto first_array = array_one();
			std::copy(first_array.begin(), first_array.end(), dest);
			auto second_array = array_two();
			std::copy(second_array.begin(), second_array.end(), dest + first_array.size());
		}

		void set_capacity(size_type Num)
		{
			clear();
			N = Num;
			raw_buffer.resize(Num);
		}

		size_t capacity() const noexcept { return N; }

		size_t size() const noexcept { return write_pos - read_pos; }

		size_t available_size() const noexcept { return capacity() - size(); }

		size_t reserve() const noexcept { return available_size(); }

		bool full() const noexcept { return size() == capacity(); }

		bool empty() const noexcept { return !size(); }

		bool is_linearized() const noexcept 
		{
			ring_buffer_index last_pos = write_pos - 1;
			return last_pos.as_index(N) >= read_pos.as_index(N);
		}

		/*
		aliases to use the ring buffer as FIFO
		*/

		template <class ...Args>
		void emplace(Args&& ... args)
		{
			emplace_back(std::forward<Args>(args)...);
		}

		void push(const value_type& value)
		{
			push_back(value);
		}

		void push(value_type&& value)
		{
			push_back(std::move(value));
		}

		template <class InputIterator>
		void insert(InputIterator first, InputIterator last)
		{
			insert_back(first, last);
		}

		void pop(value_type& value)
		{
			pop_front(value);
		}

		bool try_pop(value_type& value)
		{
			return try_pop_front(value);
		}

		void pop()
		{
			pop_front();
		}

		bool try_pop()
		{
			return try_pop_front();
		}

		template <class OutputIt>
		void pop(OutputIt dest, size_t num)
		{
			pop_front(dest, num);
		}

		template <class OutputIt>
		void pop(OutputIt dest, size_t num, no_init_t)
		{
			pop_front(dest, num, no_init);
		}

		template <class OutputIt>
		bool try_pop(OutputIt dest, size_t num)
		{
			return try_pop_front(dest, num);
		}

		template <class OutputIt>
		bool try_pop(OutputIt dest, size_t num, no_init_t)
		{
			return try_pop_front(dest, num, no_init);
		}

	};
