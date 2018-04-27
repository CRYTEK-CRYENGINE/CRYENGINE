// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   VectorSet.h
//  Version:     v1.00
//  Created:     25/9/2006 by MichaelS.
//  Compilers:   Visual Studio.NET 2005
//  Description: std::set replacement implemented using sorted vector.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef __SORTEDSET_H__
#define __SORTEDSET_H__

//! --------------------------------------------------------------------------
//! VectorSet
//! Usage Notes:
//! This class is designed to be an (almost, see below) drop-in replacement
//! for std::set. It features an almost identical interface, but it is
//! implemented using a sorted vector rather than a tree. This is in most
//! cases more efficient, as there is less dynamic memory allocation and
//! pointer dereferencing.
//! *************************************************************************
//! PLEASE NOTE: There is one vital difference between std::set and VectorSet
//! that you will need to note before trying to replace std::set. Since
//! VectorSet is implemented using a vector, iterators can and will be
//! invalidated by many operations, such as insertions and deletions, and
//! due to sorting potentially even normal lookups. Please Please PLEASE make
//! sure that you are not storing any iterators to this class.
//! *************************************************************************
//! The class varies from the std::set API in that two of the erase methods
//! methods are not of void return type but return an iterator - this is
//! required in practice because they invalidate iterators, as noted above.
//! * iterator erase(iterator where);
//! * iterator erase(iterator first, iterator last);
//! It also adds operator[] to the API.
//! Performance Notes:
//! This class uses the empty base optimization hack to allow comparison
//! predicate objects that have no state to take up no space in the object.
//! As a result the size of the overall VectorMap instance is the same as
//! that of the std::vector it uses to store the elements.
//! In addition to the normal map interface, this class provides the
//! following members that can be used to manage memory requirements:
//! * void reserve(size_type count);
//! Allocate enough space for count elements (see vector::reserve()).
//! * size_type capacity() const;
//! Report how many elements can be stored without reallocating (see
//! vector::capacity()).
//! * void resize(size_type new_size, const_reference x=key_type());
//! see vector::resize()).
//! --------------------------------------------------------------------------
template<typename K, typename T = std::less<K>, typename A = std::allocator<K>>
class VectorSet : private T // Empty base optimization
{
public:
	typedef K                                               key_type;
	typedef A                                               allocator_type;
	typedef T                                               key_compare;
	typedef T                                               value_compare;
	typedef std::vector<key_type, allocator_type>           container_type;
	typedef typename container_type::iterator               iterator;
	typedef typename container_type::const_iterator         const_iterator;
	typedef typename container_type::reverse_iterator       reverse_iterator;
	typedef typename container_type::const_reverse_iterator const_reverse_iterator;
	typedef typename container_type::size_type              size_type;
	typedef typename container_type::difference_type        difference_type;
	typedef key_type&                                       reference;
	typedef const key_type&                                 const_reference;
	typedef key_type*                                       pointer;
	typedef const key_type*                                 const_pointer;

	VectorSet();
	explicit VectorSet(const key_compare& comp);
	explicit VectorSet(const key_compare& comp, const allocator_type& allocator);
	VectorSet(const VectorSet<K, T, A>& right);
	template<class InputIterator> VectorSet(InputIterator first, InputIterator last);
	template<class InputIterator> VectorSet(InputIterator first, InputIterator last, const key_compare& comp);
	template<class InputIterator> VectorSet(InputIterator first, InputIterator last, const key_compare& comp, const allocator_type& allocator);
	void                                      SwapElementsWithVector(container_type& elementVector);
	const_iterator                            begin() const;
	iterator                                  begin();
	size_type                                 capacity() const;
	void                                      resize(size_type __new_size, const_reference __x = key_type());
	void                                      clear();
	size_type                                 count(const_reference key) const;
	bool                                      empty() const;
	const_iterator                            end() const;
	iterator                                  end();
	std::pair<const_iterator, const_iterator> equal_range(const_reference key) const;
	std::pair<iterator, iterator>             equal_range(const_reference key);
	iterator                                  erase(iterator where);                //!< See documentation above.
	iterator                                  erase(iterator first, iterator last); //!< See documentation above.
	size_t                                    erase(const_reference key);
	iterator                                  find(const_reference key);
	const_iterator                            find(const_reference key) const;
	allocator_type                            get_allocator() const;
	std::pair<iterator, bool>                 insert(const_reference value);
	iterator                                  insert(iterator _Where, const_reference value);
	template<class InputIterator> void        insert(InputIterator first, InputIterator last);
	key_compare                               key_comp() const;
	const_iterator                            lower_bound(const_reference key) const;
	iterator                                  lower_bound(const_reference key);
	size_type                                 max_size() const;
	const_reverse_iterator                    rbegin() const;
	reverse_iterator                          rbegin();
	const_reverse_iterator                    rend() const;
	reverse_iterator                          rend();
	void                                      reserve(size_type count);
	size_type                                 size() const;
	void                                      swap(VectorSet& right);
	const_iterator                            upper_bound(const_reference key) const;
	iterator                                  upper_bound(const_reference key);
	value_compare                             value_comp() const;
	reference                                 operator[](int index);       //!< See documentation above.
	const_reference                           operator[](int index) const; //!< See documentation above.

	template<typename Sizer>
	void GetMemoryUsage(Sizer* pSizer) const
	{
		pSizer->AddObject(m_entries);
	}
private:
	container_type m_entries;
};

template<typename K, typename T, typename A>
VectorSet<K, T, A>::VectorSet()
{
}

template<typename K, typename T, typename A>
VectorSet<K, T, A>::VectorSet(const key_compare& comp)
	: key_compare(comp)
{
}

template<typename K, typename T, typename A>
VectorSet<K, T, A>::VectorSet(const key_compare& comp, const allocator_type& allocator)
	: key_compare(comp),
	m_entries(allocator)
{
}

template<typename K, typename T, typename A>
VectorSet<K, T, A>::VectorSet(const VectorSet<K, T, A>& right)
	: key_compare(right),
	m_entries(right.m_entries)
{
}

template<typename K, typename T, typename A>
template<class InputIterator> VectorSet<K, T, A>::VectorSet(InputIterator first, InputIterator last)
{
	for (; first != last; ++first)
		m_entries.push_back(*first);
	std::sort(m_entries.begin(), m_entries.end(), static_cast<key_compare>(*this));
}

template<typename K, typename T, typename A>
template<class InputIterator> VectorSet<K, T, A>::VectorSet(InputIterator first, InputIterator last, const key_compare& comp)
	: key_compare(comp)
{
	for (; first != last; ++first)
		m_entries.push_back(*first);
	std::sort(m_entries.begin(), m_entries.end(), static_cast<key_compare>(*this));
}

template<typename K, typename T, typename A>
template<class InputIterator> VectorSet<K, T, A>::VectorSet(InputIterator first, InputIterator last, const key_compare& comp, const allocator_type& allocator)
	: key_compare(comp),
	m_entries(allocator)
{
	for (; first != last; ++first)
		m_entries.push_back(*first);
	std::sort(m_entries.begin(), m_entries.end(), static_cast<key_compare>(*this));
}

template<typename K, typename T, typename A>
void VectorSet<K, T, A >::SwapElementsWithVector(container_type& elementVector)
{
	m_entries.swap(elementVector);
	std::sort(m_entries.begin(), m_entries.end(), static_cast<key_compare>(*this));
}

template<typename K, typename T, typename A>
typename VectorSet<K, T, A>::const_iterator VectorSet<K, T, A >::begin() const
{
	return m_entries.begin();
}

template<typename K, typename T, typename A>
typename VectorSet<K, T, A>::iterator VectorSet<K, T, A >::begin()
{
	return m_entries.begin();
}

template<typename K, typename T, typename A>
typename VectorSet<K, T, A>::size_type VectorSet<K, T, A >::capacity() const
{
	return m_entries.capacity();
}

template<typename K, typename T, typename A>
void VectorSet<K, T, A >::clear()
{
	m_entries.clear();
}

template<typename K, typename T, typename A>
void VectorSet<K, T, A >::resize(size_type __new_size, const_reference __x)
{
	m_entries.resize(__new_size, __x);
}

template<typename K, typename T, typename A>
typename VectorSet<K, T, A>::size_type VectorSet<K, T, A >::count(const_reference key) const
{
	return size_type(std::binary_search(m_entries.begin(), m_entries.end(), key, static_cast<key_compare>(*this)));
}

template<typename K, typename T, typename A>
bool VectorSet<K, T, A >::empty() const
{
	return m_entries.empty();
}

template<typename K, typename T, typename A>
typename VectorSet<K, T, A>::const_iterator VectorSet<K, T, A >::end() const
{
	return m_entries.end();
}

template<typename K, typename T, typename A>
typename VectorSet<K, T, A>::iterator VectorSet<K, T, A >::end()
{
	return m_entries.end();
}

template<typename K, typename T, typename A>
std::pair<typename VectorSet<K, T, A>::const_iterator, typename VectorSet<K, T, A>::const_iterator> VectorSet<K, T, A >::equal_range(const_reference key) const
{
	const_iterator lower = lower_bound(key);
	if (lower != m_entries.end() && key_compare::operator()(key, *lower))
		lower = m_entries.end();
	const_iterator upper = lower;
	if (upper != m_entries.end())
		++upper;
	return std::make_pair(lower, upper);
}

template<typename K, typename T, typename A>
std::pair<typename VectorSet<K, T, A>::iterator, typename VectorSet<K, T, A>::iterator> VectorSet<K, T, A >::equal_range(const_reference key)
{
	iterator lower = lower_bound(key);
	if (lower != m_entries.end() && key_compare::operator()(key, *lower))
		lower = m_entries.end();
	iterator upper = lower;
	if (upper != m_entries.end())
		++upper;
	return std::make_pair(lower, upper);
}

template<typename K, typename T, typename A>
typename VectorSet<K, T, A>::iterator VectorSet<K, T, A >::erase(iterator where)
{
	return m_entries.erase(where);
}

template<typename K, typename T, typename A>
typename VectorSet<K, T, A>::iterator VectorSet<K, T, A >::erase(iterator first, iterator last)
{
	return m_entries.erase(first, last);
}

template<typename K, typename T, typename A>
size_t VectorSet<K, T, A >::erase(const_reference key)
{
	iterator where = find(key);

	if (where != m_entries.end())
	{
		// Note erasing entries does not invalidate the sort - no need to trigger a re-sort.
		m_entries.erase(where);
		return 1;
	}

	return 0;
}

template<typename K, typename T, typename A>
typename VectorSet<K, T, A>::iterator VectorSet<K, T, A >::find(const_reference key)
{
	iterator it = lower_bound(key);
	if (it != m_entries.end() && key_compare::operator()(key, *it))
		it = m_entries.end();
	return it;
}

template<typename K, typename T, typename A>
typename VectorSet<K, T, A>::const_iterator VectorSet<K, T, A >::find(const_reference key) const
{
	const_iterator it = lower_bound(key);
	if (it != m_entries.end() && key_compare::operator()(key, *it))
		it = m_entries.end();
	return it;
}

template<typename K, typename T, typename A>
typename VectorSet<K, T, A>::allocator_type VectorSet<K, T, A >::get_allocator() const
{
	return m_entries.get_allocator();
}

template<typename K, typename T, typename A>
std::pair<typename VectorSet<K, T, A>::iterator, bool> VectorSet<K, T, A >::insert(const_reference value)
{
	iterator it = lower_bound(value);
	bool insertionMade = false;
	if (it == m_entries.end() || key_compare::operator()(value, (*it)))
		it = m_entries.insert(it, value), insertionMade = true;
	return std::make_pair(it, insertionMade);
}

template<typename K, typename T, typename A>
typename VectorSet<K, T, A>::iterator VectorSet<K, T, A >::insert(iterator where, const_reference value)
{
	return insert(value);
}

template<typename K, typename T, typename A>
template<class InputIterator> void VectorSet<K, T, A >::insert(InputIterator first, InputIterator last)
{
	for (; first != last; ++first)
		insert(*first);
}

template<typename K, typename T, typename A>
typename VectorSet<K, T, A>::key_compare VectorSet<K, T, A >::key_comp() const
{
	return static_cast<key_compare>(*this);
}

template<typename K, typename T, typename A>
typename VectorSet<K, T, A>::const_iterator VectorSet<K, T, A >::lower_bound(const_reference key) const
{
	return std::lower_bound(m_entries.begin(), m_entries.end(), key, static_cast<key_compare>(*this));
}

template<typename K, typename T, typename A>
typename VectorSet<K, T, A>::iterator VectorSet<K, T, A >::lower_bound(const_reference key)
{
	return std::lower_bound(m_entries.begin(), m_entries.end(), key, static_cast<key_compare>(*this));
}

template<typename K, typename T, typename A>
typename VectorSet<K, T, A>::size_type VectorSet<K, T, A >::max_size() const
{
	return m_entries.max_size();
}

template<typename K, typename T, typename A>
typename VectorSet<K, T, A>::const_reverse_iterator VectorSet<K, T, A >::rbegin() const
{
	return m_entries.rbegin();
}

template<typename K, typename T, typename A>
typename VectorSet<K, T, A>::reverse_iterator VectorSet<K, T, A >::rbegin()
{
	return m_entries.rbegin();
}

template<typename K, typename T, typename A>
typename VectorSet<K, T, A>::const_reverse_iterator VectorSet<K, T, A >::rend() const
{
	return m_entries.rend();
}

template<typename K, typename T, typename A>
typename VectorSet<K, T, A>::reverse_iterator VectorSet<K, T, A >::rend()
{
	return m_entries.rend();
}

template<typename K, typename T, typename A>
void VectorSet<K, T, A >::reserve(size_type count)
{
	m_entries.reserve(count);
}

template<typename K, typename T, typename A>
typename VectorSet<K, T, A>::size_type VectorSet<K, T, A >::size() const
{
	return m_entries.size();
}

template<typename K, typename T, typename A>
void VectorSet<K, T, A >::swap(VectorSet& other)
{
	m_entries.swap(other.m_entries);
	std::swap(static_cast<key_compare&>(*this), static_cast<key_compare&>(other));
}

template<typename K, typename T, typename A>
typename VectorSet<K, T, A>::const_iterator VectorSet<K, T, A >::upper_bound(const_reference key) const
{
	const_iterator upper = lower_bound(key);
	if (upper != m_entries.end() && !key_compare::operator()(key, *upper))
		++upper;
	return upper;
}

template<typename K, typename T, typename A>
typename VectorSet<K, T, A>::iterator VectorSet<K, T, A >::upper_bound(const_reference key)
{
	iterator upper = lower_bound(key);
	if (upper != m_entries.end() && !key_compare::operator()(key, *upper))
		++upper;
	return upper;
}

template<typename K, typename T, typename A>
typename VectorSet<K, T, A>::value_compare VectorSet<K, T, A >::value_comp() const
{
	return static_cast<key_compare>(*this);
}

template<typename K, typename T, typename A>
typename VectorSet<K, T, A>::reference VectorSet<K, T, A >::operator[](int index)
{
	return m_entries[index];
}

template<typename K, typename T, typename A>
typename VectorSet<K, T, A>::const_reference VectorSet<K, T, A >::operator[](int index) const
{
	return m_entries[index];
}

#endif //__SORTEDSET_H__
