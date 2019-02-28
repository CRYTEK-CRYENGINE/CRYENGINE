// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __FIXED_ALLOCATOR_H
#define __FIXED_ALLOCATOR_H

#pragma once

#include <type_traits>
#include <CryCore/StlUtils.h>

template<size_t ObjSize, size_t ObjAlign, size_t ChunkObjCount>
struct FixedAllocator
{
	FixedAllocator()
		: allocChunk_(0)
		, emptyChunk_(0)
	{
	}

	~FixedAllocator()
	{
		free_memory();
	}

	enum { chunk_object_count = ChunkObjCount };
	enum { object_size = ObjSize };
	enum { object_align = ObjAlign };

	void*  alloc();
	void   dealloc(void* p);

	size_t get_chunk_count() const;
	size_t get_total_memory() const;

	void   swap(FixedAllocator& other);
	void   free_unused_memory();
	void   free_memory();
private:
	template<typename BlockIndexType = uint32>
	struct Chunk
	{
		typedef BlockIndexType block_index_type;
		enum { object_memory_size = stl::static_max<object_size, sizeof(block_index_type)>::value };
		enum { object_memory_alignment = stl::static_max<object_size, alignof(block_index_type)>::value };

		inline void init(block_index_type maxObjCount)
		{
			assert(maxObjCount <= (1ul << ((sizeof(block_index_type) * 8) - 1)));

			typedef typename std::aligned_storage<object_memory_alignment, object_memory_alignment>::type AlignedType;
			const size_t dataSize = maxObjCount * object_memory_size / sizeof(AlignedType);
			assert(dataSize * sizeof(AlignedType) == object_memory_size * maxObjCount);

			data_ = (uchar*)(new AlignedType[dataSize]);
			firstFree_ = 0;
			freeCount_ = maxObjCount;

			uchar* p = data_;
			for (size_t i = 0; i < maxObjCount; p += object_memory_size)
				*(block_index_type*)p = ++i;
		}

		inline void* alloc()
		{
			assert(data_);

			if (!freeCount_)
				return 0;

			uchar* mem = data_ + firstFree_ * object_memory_size;
			firstFree_ = *(block_index_type*)mem;
			--freeCount_;

			return mem;
		}

		inline void dealloc(void* p)
		{
			assert(p >= data_);
			assert(((uchar*)p - data_) % object_memory_size == 0);
			*static_cast<block_index_type*>(p) = firstFree_;
			firstFree_ = static_cast<block_index_type>(((uchar*)p - data_) / object_memory_size);
			assert(firstFree_ == ((uchar*)p - data_) / object_memory_size);
			++freeCount_;
		}

		inline void free_memory()
		{
			delete[] data_;
		}

		inline bool full() const
		{
			return freeCount_ == 0;
		}

		inline block_index_type free_count() const
		{
			return freeCount_;
		}

		inline bool contains(void* p, block_index_type maxObjCount) const
		{
			return (p >= data_) && (p <= data_ + (maxObjCount - 1) * object_memory_size);
		}

	private:
		uchar*           data_;
		block_index_type firstFree_;
		block_index_type freeCount_;
	};

	typedef Chunk<> MemoryChunk;
	MemoryChunk* find_chunk_containing(void* p);

	MemoryChunk* allocChunk_;
	MemoryChunk* emptyChunk_;

	typedef std::vector<MemoryChunk> MemoryChunks;
	MemoryChunks chunks_;
};

template<size_t ObjSize, size_t ObjAlign, size_t ChunkObjCount>
void* FixedAllocator<ObjSize, ObjAlign, ChunkObjCount >::alloc()
{
	if (allocChunk_ && !allocChunk_->full())
	{
		if (emptyChunk_ == allocChunk_)
			emptyChunk_ = 0;

		return allocChunk_->alloc();
	}

	typename MemoryChunks::iterator it = chunks_.begin();

	for (;; ++it)
	{
		if (it == chunks_.end())
		{
			chunks_.resize(chunks_.size() + 1);
			allocChunk_ = &chunks_.back();
			allocChunk_->init(chunk_object_count);

			return allocChunk_->alloc();
		}

		if (!it->full())
		{
			allocChunk_ = &(*it);

			if (emptyChunk_ == allocChunk_)
				emptyChunk_ = 0;

			return allocChunk_->alloc();
		}
	}
}

template<size_t ObjSize, size_t ObjAlign, size_t ChunkObjCount>
void FixedAllocator<ObjSize, ObjAlign, ChunkObjCount >::dealloc(void* p)
{
	assert(allocChunk_);

	MemoryChunk* chunk = (allocChunk_ && allocChunk_->contains(p, chunk_object_count)) ?
	                     allocChunk_ : find_chunk_containing(p);

	assert(chunk);

	chunk->dealloc(p);

	if (chunk->free_count() == chunk_object_count)
	{
		if (emptyChunk_)
		{
			assert(emptyChunk_ != chunk);
			emptyChunk_->free_memory();

			if (allocChunk_ == emptyChunk_)
				allocChunk_ = chunk;

			std::swap(*emptyChunk_, chunks_.back());
			if (&chunks_.back() == allocChunk_)
				allocChunk_ = emptyChunk_;
			if (&chunks_.back() == chunk)
				chunk = emptyChunk_;

			chunks_.pop_back();
		}

		emptyChunk_ = chunk;
	}
}

template<size_t ObjSize, size_t ObjAlign, size_t ChunkObjCount>
size_t FixedAllocator<ObjSize, ObjAlign, ChunkObjCount >::get_chunk_count() const
{
	return chunks_.size();
}

template<size_t ObjSize, size_t ObjAlign, size_t ChunkObjCount>
size_t FixedAllocator<ObjSize, ObjAlign, ChunkObjCount >::get_total_memory() const
{
	return chunks_.size() * (sizeof(MemoryChunk) + chunk_object_count * object_size);
}

template<size_t ObjSize, size_t ObjAlign, size_t ChunkObjCount>
void FixedAllocator<ObjSize, ObjAlign, ChunkObjCount >::swap(FixedAllocator& other)
{
	chunks_.swap(other.chunks_);
	std::swap(allocChunk_, other.allocChunk_);
	std::swap(emptyChunk_, other.emptyChunk_);
}

template<size_t ObjSize, size_t ObjAlign, size_t ChunkObjCount>
void FixedAllocator<ObjSize, ObjAlign, ChunkObjCount >::free_unused_memory()
{
	size_t i = 0;
	size_t size = chunks_.size();

	for (; i < size; )
	{
		MemoryChunk& chunk = chunks_[i];

		if (chunk.free_count() == chunk_object_count)
		{
			chunk.free_memory();
			if (allocChunk_ == &chunk)
				allocChunk_ = 0;
			else if (allocChunk_ == &chunks_.back())
				allocChunk_ = &chunk;

			std::swap(chunk, chunks_.back());

			chunks_.pop_back();
			--size;
			continue;
		}
		++i;
	}

	emptyChunk_ = 0;
}

template<size_t ObjSize, size_t ObjAlign, size_t ChunkObjCount>
void FixedAllocator<ObjSize, ObjAlign, ChunkObjCount >::free_memory()
{
	typename MemoryChunks::iterator it = chunks_.begin();
	typename MemoryChunks::iterator end = chunks_.end();

	for (; it != end; ++it)
		it->free_memory();

	MemoryChunks().swap(chunks_);
}

template<size_t ObjSize, size_t ObjAlign, size_t ChunkObjCount>
typename FixedAllocator<ObjSize, ObjAlign, ChunkObjCount>::MemoryChunk *
FixedAllocator<ObjSize, ObjAlign, ChunkObjCount>::find_chunk_containing(void* p)
{
	if (!chunks_.empty())
	{
		MemoryChunk* lo = allocChunk_;
		MemoryChunk* hi = allocChunk_ + 1;

		const MemoryChunk* loBound = &chunks_.front();
		const MemoryChunk* hiBound = &chunks_.back() + 1;

		if (hi == hiBound)
			hi = 0;

		for (;; )
		{
			if (lo)
			{
				if (lo->contains(p, chunk_object_count))
					return lo;

				if (lo == loBound)
				{
					lo = 0;
					if (!hi)
						break;
				}
				else
					--lo;
			}

			if (hi)
			{
				if (hi->contains(p, chunk_object_count))
					return hi;

				if (++hi == hiBound)
				{
					hi = 0;
					if (!lo)
						break;
				}
			}
		}
	}

	return 0;
}

template<typename Ty, size_t ChunkObjectCount = 1024, size_t ObjAlign = alignof(Ty)>
//template<typename Ty, size_t ChunkObjectCount = 1024, size_t ObjAlign = alignment_of<Ty>>
struct TypeFixedAllocator
	: public FixedAllocator<sizeof(Ty), ObjAlign, ChunkObjectCount>
{
};

#endif  // #ifndef __FIXED_ALLOCATOR_H
