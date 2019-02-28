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

// non-intrusive lock free unbounded SPSC queue
// the algorithm was taken from the blog post below, and converted to C++11
// http://cbloomrants.blogspot.com/2009/02/02-26-09-low-level-threading-part-51.html

#ifndef __SPSC_QUEUE_INCLUDED__
#define __SPSC_QUEUE_INCLUDED__

#include <atomic>

namespace concqueue
{

template<typename T>
class spsc_queue_t
{
public:

	spsc_queue_t()
		: _head(reinterpret_cast<node_t*>(new node_aligned_t))
		, _tail(_head)
	{
		_head->next = nullptr;
	}

	~spsc_queue_t()
	{
		T output;
		while (this->dequeue(output)) {}
		delete reinterpret_cast<node_aligned_t*>(_head);
	}

	bool enqueue(const T& input)
	{
		// new node_aligned_t is equivalent to malloc(...), see std::aligned_storage
		// placement-new constructor for specified T must be called
		node_t* node = reinterpret_cast<node_t*>(new node_aligned_t);
		new(&node->data) T();

		// set the data
		node->data = input;
		node->next = nullptr;

		std::atomic_thread_fence(std::memory_order_acq_rel);
		_head->next = node;
		_head = node;
		return true;
	}

	bool dequeue(T& output)
	{
		std::atomic_thread_fence(std::memory_order_consume);
		if (!_tail->next)
		{
			return false;
		}

		// set the output
		output = _tail->next->data;
		// destruct the copied T
		_tail->next->data.~T();

		std::atomic_thread_fence(std::memory_order_acq_rel);
		_back = _tail;
		_tail = _back->next;

		delete reinterpret_cast<node_aligned_t*>(_back);
		return true;
	}

private:

	struct node_t
	{
		node_t* next;
		T       data;
	};

	typedef typename std::aligned_storage<sizeof(node_t), std::alignment_of<node_t>::value>::type node_aligned_t;

	node_t* _head;
	char    _cache_line[64];
	node_t* _tail;
	node_t* _back;

	spsc_queue_t(const spsc_queue_t&) {}
	void operator=(const spsc_queue_t&) {}
};

}

#endif
