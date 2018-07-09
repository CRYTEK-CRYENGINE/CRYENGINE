// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __SimpleHashLookUp_h__
#define __SimpleHashLookUp_h__

#pragma once

#include <algorithm>
#include <type_traits>
#include <CryCore/StlUtils.h>

template<size_t IntegerSize>
struct default_integer_hash
{
	inline size_t operator()(size_t key) const
	{
		key = (key + 0x7ed55d16) + (key << 12);
		key = (key ^ 0xc761c23c) ^ (key >> 19);
		key = (key + 0x165667b1) + (key << 5);
		key = (key + 0xd3a2646c) ^ (key << 9);
		key = (key + 0xfd7046c5) + (key << 3);
		key = (key ^ 0xb55a4f09) ^ (key >> 16);

		return key;
	}
};

template<>
struct default_integer_hash<8>
{
	inline uint64 operator()(uint64 key) const
	{
		key = (~key) + (key << 21);
		key = key ^ (key >> 24);
		key = (key + (key << 3)) + (key << 8);
		key = key ^ (key >> 14);
		key = (key + (key << 2)) + (key << 4);
		key = key ^ (key >> 28);
		key = key + (key << 31);

		return key;
	}
};

template<typename KeyTy, typename ValueTy, typename KeyHash = default_integer_hash<sizeof(KeyTy)>, typename IndexTy = size_t>
struct simple_hash_lookup
{
	typedef std::pair<KeyTy, ValueTy> container_type;
	typedef KeyTy                     key_type;
	typedef ValueTy                   value_type;
	typedef IndexTy                   index_type;
	typedef KeyHash                   hash_type;

	inline simple_hash_lookup(size_t _capacity = 0, size_t _bucketCount = 0);
	inline ~simple_hash_lookup();

	inline value_type* insert(const key_type& key, const value_type& value, bool* inserted = 0);
	inline value_type* find(const key_type& key) const;

	inline void        reset(size_t _capacity, size_t _bucketCount);
	inline void        clear();

	inline bool        empty() const;
	inline size_t      capacity() const;
	inline size_t      size() const;
	inline void        swap(simple_hash_lookup& other);

protected:
	enum { invalid_value_index = ~static_cast<index_type>(0) };

	inline void construct_(void* buffer, const container_type& value) const;
	inline void destruct_(void* buffer) const;

	size_t      count;
	size_t      max_count;
	size_t      bucket_count;

	enum { storage_alignment = stl::static_max<alignof(container_type), alignof(index_type)>::value, };
	typedef typename std::aligned_storage<storage_alignment, storage_alignment>::type buffer_type;

	container_type* values;
	index_type*     buckets;
	index_type*     overflow;
};

template<typename KeyTy, typename ValueTy, typename KeyHash, typename IndexTy>
simple_hash_lookup<KeyTy, ValueTy, KeyHash, IndexTy>::simple_hash_lookup(size_t _capacity, size_t _bucketCount)
	: values(0)
	, buckets(0)
	, overflow(0)
	, count(0)
	, max_count(_capacity)
	, bucket_count(_bucketCount)
{
	reset(_capacity, _bucketCount);
}

template<typename KeyTy, typename ValueTy, typename KeyHash, typename IndexTy>
simple_hash_lookup<KeyTy, ValueTy, KeyHash, IndexTy>::~simple_hash_lookup()
{
	reset(0, 0);
}

template<typename KeyTy, typename ValueTy, typename KeyHash, typename IndexTy>
void simple_hash_lookup<KeyTy, ValueTy, KeyHash, IndexTy >::reset(size_t _capacity, size_t _bucketCount)
{
	if (count)
	{
		for (size_t i = 0; i < bucket_count; ++i)
		{
			size_t index = buckets[i];

			while (index != invalid_value_index)
			{
				destruct_(&values[index]);
				index = overflow[index];
			}
		}

		count = 0;
	}

	if ((max_count != _capacity) || (bucket_count != _bucketCount))
	{
		delete[] (buffer_type*)(values);
		values = 0;

		max_count = _capacity;
		bucket_count = _bucketCount;

		if (max_count && bucket_count)
		{
			size_t totalMemory = max_count * sizeof(container_type) +
			                     (max_count + bucket_count) * sizeof(index_type);

			values = (container_type*)new buffer_type[(totalMemory / sizeof(buffer_type)) + 1];
			buckets = (index_type*)(((unsigned char*)values) + sizeof(container_type) * max_count);
			overflow = buckets + bucket_count;
		}
	}

	if (buckets)
		memset(buckets, (uint8)invalid_value_index, sizeof(index_type) * (bucket_count + max_count));
}

template<typename KeyTy, typename ValueTy, typename KeyHash, typename IndexTy>
typename simple_hash_lookup<KeyTy, ValueTy, KeyHash, IndexTy>::value_type * simple_hash_lookup<KeyTy, ValueTy, KeyHash, IndexTy>::insert(
  const key_type &key, const value_type &value, bool* inserted)
{
	const size_t bucketIdx = hash_type()(key) & (bucket_count - 1);
	size_t index = buckets[bucketIdx];

	if (inserted)
		*inserted = false;

	while (index != invalid_value_index)
	{
		if (values[index].first == key)
			return &values[index].second;
		index = overflow[index];
	}

	if (count == max_count)
		return 0;

	if (inserted)
		*inserted = true;

	index = count++;

	construct_(&values[index], std::make_pair(key, value));
	overflow[index] = buckets[bucketIdx];
	buckets[bucketIdx] = index;

	return &values[index].second;
}

template<typename KeyTy, typename ValueTy, typename KeyHash, typename IndexTy>
typename simple_hash_lookup<KeyTy, ValueTy, KeyHash, IndexTy>::value_type * simple_hash_lookup<KeyTy, ValueTy, KeyHash, IndexTy>::find(const KeyTy &key) const
{
	const size_t bucketIdx = hash_type()(key) & (bucket_count - 1);
	size_t index = buckets[bucketIdx];

	while (index != invalid_value_index)
	{
		if (values[index].first == key)
			return &values[index].second;
		index = overflow[index];
	}

	return 0;
}

template<typename KeyTy, typename ValueTy, typename KeyHash, typename IndexTy>
void simple_hash_lookup<KeyTy, ValueTy, KeyHash, IndexTy >::clear()
{
	for (size_t i = 0; i < bucket_count; ++i)
	{
		size_t index = buckets[i];

		while (index != invalid_value_index)
		{
			destruct_(&values[index]);
			index = overflow[index];
		}
	}

	memset(buckets, invalid_value_index, sizeof(index_type) * (bucket_count + max_count));
	count = 0;
}

template<typename KeyTy, typename ValueTy, typename KeyHash, typename IndexTy>
bool simple_hash_lookup<KeyTy, ValueTy, KeyHash, IndexTy >::empty() const
{
	return count == 0;
}

template<typename KeyTy, typename ValueTy, typename KeyHash, typename IndexTy>
size_t simple_hash_lookup<KeyTy, ValueTy, KeyHash, IndexTy >::capacity() const
{
	return max_count;
}

template<typename KeyTy, typename ValueTy, typename KeyHash, typename IndexTy>
size_t simple_hash_lookup<KeyTy, ValueTy, KeyHash, IndexTy >::size() const
{
	return count;
}

template<typename KeyTy, typename ValueTy, typename KeyHash, typename IndexTy>
void simple_hash_lookup<KeyTy, ValueTy, KeyHash, IndexTy >::swap(simple_hash_lookup& other)
{
	std::swap(count, other.count);
	std::swap(max_count, other.max_count);
	std::swap(bucket_count, other.bucket_count);

	std::swap(values, other.values);
	std::swap(buckets, other.buckets);
	std::swap(overflow, other.overflow);
}

template<typename KeyTy, typename ValueTy, typename KeyHash, typename IndexTy>
void simple_hash_lookup<KeyTy, ValueTy, KeyHash, IndexTy >::construct_(void* buffer, const container_type& value) const
{
	new(buffer) container_type(value);
}

template<typename KeyTy, typename ValueTy, typename KeyHash, typename IndexTy>
void simple_hash_lookup<KeyTy, ValueTy, KeyHash, IndexTy >::destruct_(void* buffer) const
{
	static_cast<container_type*>(buffer)->~container_type();
}

#endif
