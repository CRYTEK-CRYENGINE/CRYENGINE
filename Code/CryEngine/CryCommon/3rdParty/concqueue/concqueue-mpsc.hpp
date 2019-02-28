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

// C++ implementation of Dmitry Vyukov's non-intrusive lock free unbound MPSC queue
// http://www.1024cores.net/home/lock-free-algorithms/queues/non-intrusive-mpsc-node-based-queue

#ifndef __MPSC_BOUNDED_QUEUE_INCLUDED__
#define __MPSC_BOUNDED_QUEUE_INCLUDED__

#include <atomic>
#include <assert.h>

namespace concqueue
{

template<typename T>
class mpsc_queue_t
{
public:

	mpsc_queue_t()
		: _head(reinterpret_cast<buffer_node_t*>(new buffer_node_aligned_t))
		, _tail(_head.load(std::memory_order_relaxed))
	{
		buffer_node_t* front = _head.load(std::memory_order_relaxed);
		front->next.store(nullptr, std::memory_order_relaxed);
	}

	~mpsc_queue_t()
	{
		T output;
		while (this->dequeue(output)) {}
		buffer_node_t* front = _head.load(std::memory_order_relaxed);
		delete reinterpret_cast<buffer_node_aligned_t*>(front);
	}

	bool enqueue(const T& input)
	{
		// new buffer_node_aligned_t is equivalent to malloc(...), see std::aligned_storage
		// placement-new constructor for specified T must be called
		buffer_node_t* node = reinterpret_cast<buffer_node_t*>(new buffer_node_aligned_t);
		new(&node->data) T();

		// set the data
		node->data = input;
		node->next.store(nullptr, std::memory_order_relaxed);

		buffer_node_t* prev_head = _head.exchange(node, std::memory_order_acq_rel);
		prev_head->next.store(node, std::memory_order_release);
		return true;
	}

	bool dequeue(T& output)
	{
		buffer_node_t* tail = _tail.load(std::memory_order_relaxed);
		buffer_node_t* next = tail->next.load(std::memory_order_acquire);

		if (next == nullptr)
		{
			return false;
		}

		// set the output
		output = next->data;
		// destruct the copied T
		next->data.~T();

		_tail.store(next, std::memory_order_release);
		delete reinterpret_cast<buffer_node_aligned_t*>(tail);

		return true;
	}

	bool available()
	{
		buffer_node_t* tail = _tail.load(std::memory_order_relaxed);
		buffer_node_t* next = tail->next.load(std::memory_order_acquire);

		return next != nullptr;
	}

private:

	struct buffer_node_t
	{
		T                           data;
		std::atomic<buffer_node_t*> next;
	};

	typedef typename std::aligned_storage<sizeof(buffer_node_t), std::alignment_of<buffer_node_t>::value>::type buffer_node_aligned_t;

	std::atomic<buffer_node_t*> _head;
	std::atomic<buffer_node_t*> _tail;

	mpsc_queue_t(const mpsc_queue_t&) {}
	void operator=(const mpsc_queue_t&) {}
};

}

#endif
