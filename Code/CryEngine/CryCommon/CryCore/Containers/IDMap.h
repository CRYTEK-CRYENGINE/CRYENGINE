// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __IDMap_h__
#define __IDMap_h__

#pragma once

#include <type_traits>

template<typename IDType, typename ValueType, typename IndexType = unsigned short, typename CounterType = unsigned short>
class id_map
{
public:
	typedef ValueType                                         value_type;
	typedef IDType                                            id_type;
	typedef IndexType                                         index_type;
	typedef CounterType                                       counter_type;
	typedef id_map<IDType, ValueType, IndexType, CounterType> this_type;

	id_map(size_t size);
	~id_map();

	void                    clear();

	id_type                 insert(const value_type& value);
	void                    insert(id_type id, const value_type& value);
	void                    erase(id_type id);
	void                    swap(this_type& other);
	size_t                  grow(size_t amount);

	ILINE size_t            size() const;
	ILINE size_t            capacity() const;

	ILINE bool              empty() const;
	ILINE bool              full() const;

	ILINE bool              validate(id_type id) const;
	ILINE bool              free(id_type id) const;

	ILINE bool              index_free(index_type index) const;
	ILINE id_type           get_index_id(index_type index) const;
	ILINE const value_type& get_index(index_type index) const;
	ILINE value_type&       get_index(index_type index);

	ILINE const value_type& get(id_type id) const;
	ILINE value_type&       get(id_type id);

	ILINE index_type        get_index_for_id(id_type id);

	ILINE const value_type& operator[](id_type id) const;
	ILINE value_type&       operator[](id_type id);

	static constexpr size_t max_capacity() { return std::numeric_limits<index_type>::max(); }

protected:
	enum { index_bits = sizeof(index_type) * 8, };
	enum { counter_bits = sizeof(counter_type) * 8, };
	enum { counter_free_bit = 1 << (counter_bits - 1), };
	enum { counter_count_mask = counter_free_bit - 1 };

	ILINE id_type id_(index_type index, counter_type counter) const
	{
		return (counter << index_bits) | (index + 1);
	}

	ILINE counter_type counter_(id_type id) const
	{
		return id >> index_bits;
	}

	ILINE index_type index_(id_type id) const
	{
		return (id & ((1 << index_bits) - 1)) - 1;
	}

	ILINE bool free_(counter_type counter) const
	{
		return (counter & counter_free_bit) != 0;
	}

	ILINE void construct_(void* buffer, const value_type& value) const
	{
		new(buffer) value_type(value);
	}

	ILINE void destruct_(void* buffer) const
	{
		static_cast<value_type*>(buffer)->~value_type();
	}

	struct Container
	{
		Container()
			: counter(counter_free_bit)
		{
		}

		typename std::aligned_storage<sizeof(value_type), alignof(value_type)>::type value;
		counter_type                                                                 counter;
	};

	typedef std::vector<index_type> Frees;
	Frees  frees_;
	size_t freesBegin_; //!< Circular queue.
	size_t freesEnd_;
	size_t freesLen_;

	typedef std::vector<Container> Values;
	Values values_;

private:

	id_map(const id_map& otherMap);
	id_map& operator=(const id_map& otherMap);

};

template<typename IDType, typename ValueType, typename IndexType, typename CounterType>
id_map<IDType, ValueType, IndexType, CounterType>::id_map(size_t size)
	: values_(size)
	, frees_(size)
{
	CRY_ASSERT(size <= max_capacity());
	
	for (size_t i = 0; i < size; ++i)
		frees_[i] = static_cast<index_type>(size - i - 1);
	freesBegin_ = 0;
	freesEnd_ = 0;
	freesLen_ = size;
}

template<typename IDType, typename ValueType, typename IndexType, typename CounterType>
id_map<IDType, ValueType, IndexType, CounterType>::~id_map()
{
	clear();
}

template<typename IDType, typename ValueType, typename IndexType, typename CounterType>
void id_map<IDType, ValueType, IndexType, CounterType >::clear()
{
	frees_.clear();

	size_t count = values_.size();

	for (size_t i = 0; i < count; ++i)
	{
		Container& container = values_[i];
		if (!free_(container.counter))
			destruct_(&container.value);
		frees_.push_back(static_cast<index_type>(count - i - 1));
	}

	freesBegin_ = 0;
	freesEnd_ = 0;
	freesLen_ = frees_.size();

	values_.clear();
	values_.resize(count);
}

template<typename IDType, typename ValueType, typename IndexType, typename CounterType>
typename id_map<IDType, ValueType, IndexType, CounterType>::id_type
id_map<IDType, ValueType, IndexType, CounterType >::insert(const value_type& value)
{
	assert(!frees_.empty()); // capacity exceeded

	if (freesLen_)
	{
		index_type index = frees_[freesBegin_];
		freesBegin_ = (freesBegin_ + 1) % values_.size();
		--freesLen_;

		Container& container = values_[index];
		counter_type counter = container.counter;
		construct_(&container.value, value);
		assert(free_(counter));

		counter = (counter & counter_count_mask) + 1;
		counter |= ((unsigned)counter >> (counter_bits - 1));
		counter &= counter_count_mask;
		assert(counter != 0);

		container.counter = counter;
		return id_(index, counter);
	}

	return 0;
}

template<typename IDType, typename ValueType, typename IndexType, typename CounterType>
void id_map<IDType, ValueType, IndexType, CounterType >::insert(id_type id, const value_type& value)
{
	index_type index = index_(id);
	assert(index < values_.size());

	if (index < values_.size())
	{
		counter_type counter = counter_(id);

		Container& container = values_[index];
		construct_(&container.value, value);
		container.counter = counter;

		typename Frees::iterator it = std::find(frees_.begin(), frees_.end(), index);
		assert(it != frees_.end());
		std::swap(*it, frees_[freesBegin_]);
		freesBegin_ = (freesBegin_ + 1) % values_.size();
		--freesLen_;
	}
}

template<typename IDType, typename ValueType, typename IndexType, typename CounterType>
void id_map<IDType, ValueType, IndexType, CounterType >::erase(id_type id)
{
	index_type index = index_(id);
	assert(index < values_.size());

	if (index < values_.size())
	{
		counter_type counter = counter_(id);

		Container& container = values_[index];

		if (container.counter == counter)
		{
			frees_[freesEnd_] = index;
			freesEnd_ = (freesEnd_ + 1) % values_.size();
			++freesLen_;
			destruct_(&container.value);
			container.counter |= counter_free_bit;
		}
	}
}

template<typename IDType, typename ValueType, typename IndexType, typename CounterType>
void id_map<IDType, ValueType, IndexType, CounterType >::swap(this_type& other)
{
	using std::swap;

	values_.swap(other.values_);
	frees_.swap(other.frees_);
	swap(freesBegin_, other.freesBegin_);
	swap(freesEnd_, other.freesEnd_);
	swap(freesLen_, other.freesLen_);
}

template<typename IDType, typename ValueType, typename IndexType, typename CounterType>
size_t id_map<IDType, ValueType, IndexType, CounterType >::grow(size_t amount)
{
	const size_t size = values_.size();
	amount = min(amount, max_capacity() - size);

	if (amount)
	{
		index_type first = static_cast<index_type>(size);

		if (freesBegin_ <= freesEnd_ && size != freesLen_)
		{
			frees_.resize(size + amount);

			for (size_t i = freesEnd_; i < size; ++i)
			{
				frees_[i + amount] = frees_[i];
			}
			const size_t newEnd = freesEnd_ + amount;
			for (size_t i = freesEnd_; i < newEnd; ++i)
			{
				frees_[i] = first++;
			}
			freesEnd_ = newEnd;
		}
		else
		{
			frees_.reserve(size + amount);
			for (size_t i = 0; i < amount; ++i)
				frees_.push_back(first++);
		}
		freesLen_ += amount;

		Values grown(size + amount);

		for (size_t i = 0; i < size; ++i)
		{
			Container& container = values_[i];
			Container& grownContainer = grown[i];

			counter_type counter = container.counter;
			grownContainer.counter = counter;

			if (!free_(counter))
			{
				construct_(&grownContainer.value, *(value_type*)&container.value);
				destruct_(&container.value);
				container.counter = 0;
			}
		}

		grown.swap(values_);
	}
	return amount;
}

template<typename IDType, typename ValueType, typename IndexType, typename CounterType>
size_t id_map<IDType, ValueType, IndexType, CounterType >::size() const
{
	return values_.size() - freesLen_;
}

template<typename IDType, typename ValueType, typename IndexType, typename CounterType>
size_t id_map<IDType, ValueType, IndexType, CounterType >::capacity() const
{
	return values_.size();
}

template<typename IDType, typename ValueType, typename IndexType, typename CounterType>
bool id_map<IDType, ValueType, IndexType, CounterType >::empty() const
{
	return freesLen_ == values_.size();
}

template<typename IDType, typename ValueType, typename IndexType, typename CounterType>
bool id_map<IDType, ValueType, IndexType, CounterType >::full() const
{
	return freesLen_ == 0;
}

template<typename IDType, typename ValueType, typename IndexType, typename CounterType>
bool id_map<IDType, ValueType, IndexType, CounterType >::validate(id_type id) const
{
	index_type index = index_(id);
	counter_type counter = counter_(id);

	return (index < values_.size()) && !free_(counter) && (values_[index].counter == counter);
}

template<typename IDType, typename ValueType, typename IndexType, typename CounterType>
bool id_map<IDType, ValueType, IndexType, CounterType >::free(id_type id) const
{
	index_type index = index_(id);

	return index_free(index);
}

template<typename IDType, typename ValueType, typename IndexType, typename CounterType>
bool id_map<IDType, ValueType, IndexType, CounterType >::index_free(index_type index) const
{
	const Container& container = values_[index];

	return free_(container.counter);
}

template<typename IDType, typename ValueType, typename IndexType, typename CounterType>
typename id_map<IDType, ValueType, IndexType, CounterType>::id_type
id_map<IDType, ValueType, IndexType, CounterType >::get_index_id(index_type index) const
{
	const Container& container = values_[index];

	return id_(index, container.counter);
}

template<typename IDType, typename ValueType, typename IndexType, typename CounterType>
const typename id_map<IDType, ValueType, IndexType, CounterType>::value_type &
id_map<IDType, ValueType, IndexType, CounterType>::get_index(index_type index) const
{
	const Container& container = values_[index];

	return *reinterpret_cast<const value_type*>(&container.value);
}

template<typename IDType, typename ValueType, typename IndexType, typename CounterType>
typename id_map<IDType, ValueType, IndexType, CounterType>::value_type &
id_map<IDType, ValueType, IndexType, CounterType>::get_index(index_type index)
{
	Container& container = values_[index];

	return *reinterpret_cast<value_type*>(&container.value);
}

template<typename IDType, typename ValueType, typename IndexType, typename CounterType>
const typename id_map<IDType, ValueType, IndexType, CounterType>::value_type &
id_map<IDType, ValueType, IndexType, CounterType>::get(id_type id) const
{
	index_type index = index_(id);
	const Container& container = values_[index];

	assert(counter_(id) == container.counter);

	return *reinterpret_cast<const value_type*>(&container.value);
}

template<typename IDType, typename ValueType, typename IndexType, typename CounterType>
typename id_map<IDType, ValueType, IndexType, CounterType>::value_type &
id_map<IDType, ValueType, IndexType, CounterType>::get(id_type id)
{
	index_type index = index_(id);
	Container& container = values_[index];

	assert(counter_(id) == container.counter);

	return *reinterpret_cast<value_type*>(&container.value);
}

template<typename IDType, typename ValueType, typename IndexType, typename CounterType>
typename id_map<IDType, ValueType, IndexType, CounterType>::index_type
id_map<IDType, ValueType, IndexType, CounterType >::get_index_for_id(id_type id)
{
	return index_(id);
}

template<typename IDType, typename ValueType, typename IndexType, typename CounterType>
const typename id_map<IDType, ValueType, IndexType, CounterType>::value_type&
id_map<IDType, ValueType, IndexType, CounterType >::operator[](id_type id) const
{
	return get(id);
}

template<typename IDType, typename ValueType, typename IndexType, typename CounterType>
typename id_map<IDType, ValueType, IndexType, CounterType>::value_type&
id_map<IDType, ValueType, IndexType, CounterType >::operator[](id_type id)
{
	return get(id);
}

#endif
