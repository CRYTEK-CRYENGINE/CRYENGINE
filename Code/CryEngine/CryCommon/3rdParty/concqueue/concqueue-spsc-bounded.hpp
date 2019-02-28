// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Anyone is free to copy, modify, publish, use, compile, sell, or
// distribute this software, either in source code form or as a compiled
// binary, for any purpose, commercial or non-commercial, and by any
// means.

// software to the public domain. We make this dedication for the benefit
// of the public at large and to the detriment of our heirs and
// successors. We intend this dedication to be an overt act of
// relinquishment in perpetuity of all present and future rights to this

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

// For more information, please refer to <http://unlicense.org/>

// Note:
// A combination of the algorithms described by the circular buffers
// documentation found in the Linux kernel, and the bounded MPMC queue
// by Dmitry Vyukov[1]. Implemented in pure C++11. Should work across
// most CPU architectures.
//
// [1] http://www.1024cores.net/home/lock-free-algorithms/queues/bounded-mpmc-queue

#ifndef __SPSC_BOUNDED_QUEUE_INCLUDED__
#define __SPSC_BOUNDED_QUEUE_INCLUDED__

#include <atomic>
#include <assert.h>

namespace concqueue
{

template<typename T>
class spsc_bounded_queue_t
{
public:

	spsc_bounded_queue_t(size_t size)
		: _size(size)
		, _mask(size - 1)
		, _buffer(reinterpret_cast<T*>(new aligned_t[_size + 1])) // need one extra element for a guard
		, _head(0)
		, _tail(0)
	{
		// make sure it's a power of 2
		assert((_size != 0) && ((_size & (~_size + 1)) == _size));
	}

	~spsc_bounded_queue_t()
	{
		delete[] reinterpret_cast<aligned_t*>(_buffer);
	}

	bool enqueue(T& input)
	{
		const size_t head = _head.load(std::memory_order_relaxed);

		if (((_tail.load(std::memory_order_acquire) - (head + 1)) & _mask) >= 1)
		{
			// new aligned_t is equivalent to malloc(...), see std::aligned_storage
			// placement-new constructor for specified T must be called
			T* node = &_buffer[head & _mask];
			new(node) T();

			// set the data
			*node = input;
			_head.store(head + 1, std::memory_order_release);

			return true;
		}

		return false;
	}

	bool dequeue(T& output)
	{
		const size_t tail = _tail.load(std::memory_order_relaxed);

		if (((_head.load(std::memory_order_acquire) - tail) & _mask) >= 1)
		{
			T* node = & _buffer[_tail & _mask];

			// set the output
			output = *node;
			// destruct the copied T
			node->~T();

			_tail.store(tail + 1, std::memory_order_release);

			return true;
		}

		return false;
	}

private:

	typedef typename std::aligned_storage<sizeof(T), std::alignment_of<T>::value>::type aligned_t;
	typedef char                                                                        cache_line_pad_t[64];

	cache_line_pad_t    _pad0;
	const size_t        _size;
	const size_t        _mask;
	T* const            _buffer;

	cache_line_pad_t    _pad1;
	std::atomic<size_t> _head;

	cache_line_pad_t    _pad2;
	std::atomic<size_t> _tail;

	spsc_bounded_queue_t(const spsc_bounded_queue_t&) {}
	void operator=(const spsc_bounded_queue_t&) {}
};

}

#endif
