// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef BIDIRECTIONAL_MAP_H
#define BIDIRECTIONAL_MAP_H

#include <set>
#include <vector>

/// Small utility to compare pointers by the values they point to.
template<typename T>
class PointerValueCompare
{
public:
	bool operator()(const T* lhs, const T* rhs) const { return *lhs < *rhs; }
};

/**
 * @brief Allows going from values to keys as well.
 *
 * NOTE Jul 10, 2007: <pvl> not truly generic but could be made if needed.
 * Uses two std::sets to index pairs of values.  Supports a subset of STL-like
 * operations, however everything's capitalized (e.g. 'Iterator' instead of
 * 'iterator'), partly to indicate that no specific effort has been made
 * to make it really STL-conformant.
 */
template<typename T0, typename T1>
class BidirectionalMap
{
	typedef std::set<const T0*, PointerValueCompare<T0>> FwdMap;
	typedef std::set<const T1*, PointerValueCompare<T1>> BwdMap;

	FwdMap mFwd;
	BwdMap mBwd;

	void Swap(BidirectionalMap&);
public:
	typedef std::pair<T0, T1> DataRecord;

	BidirectionalMap();
	BidirectionalMap(const BidirectionalMap&);
	~BidirectionalMap();

	BidirectionalMap& operator=(const BidirectionalMap&);

	void              Insert(const T1&, const T0&);
	void              Insert(const T0&, const T1&);

	void              Erase(const T0&);
	void              Erase(const T1&);

	void              Clear();

	class Iterator;
	Iterator  Find(const T0&) const;
	Iterator  Find(const T1&) const;

	const T1& operator[](const T0&) const;
	const T0& operator[](const T1&) const;

	int       Size() const;
	bool      Empty() const;

	// TODO Jun 28, 2007: make the GetKeys function a template is possible:
	// bimap.GetKeys<float> ();
	// Using terms "primary" and "secondary" implies asymetry between the two
	// sets of values where there's essentially none (except for fairly minor
	// implementation one).
	//	template <typename T>
	//	std::vector <T> GetKeys () const;

	// use the swap idiom:
	// bimap.GetKeys().swap (your_key_vec);
	std::vector<T0> GetPrimaryKeys() const;
	std::vector<T1> GetSecondaryKeys() const;

	class Iterator
	{
		typename FwdMap::const_iterator mPrimKeysIter;
	public:
		Iterator(const typename FwdMap::const_iterator& prim_keys_iter);

		Iterator&         operator++();
		const DataRecord* operator*() const;
		bool              operator==(const Iterator&) const;
		bool              operator!=(const Iterator&) const;
	};

	Iterator Begin() const;
	Iterator End() const;
};

template<typename T0, typename T1>
inline BidirectionalMap<T0, T1>::BidirectionalMap()
{
}

template<typename T0, typename T1>
inline BidirectionalMap<T0, T1>::BidirectionalMap(const BidirectionalMap& rhs)
{
	typename FwdMap::const_iterator it = rhs.mFwd.begin();
	typename FwdMap::const_iterator end = rhs.mFwd.end();
	for (; it != end; ++it)
	{
		const DataRecord* data = reinterpret_cast<const DataRecord*>(*it);
		Insert(data->first, data->second);
	}
}

template<typename T0, typename T1>
inline BidirectionalMap<T0, T1>::~BidirectionalMap()
{
	Clear();
}

template<typename T0, typename T1>
inline BidirectionalMap<T0, T1>& BidirectionalMap<T0, T1 >::operator=(const BidirectionalMap& rhs)
{
	BidirectionalMap tmp(rhs);
	Swap(tmp);
	return *this;
}

template<typename T0, typename T1>
inline void BidirectionalMap<T0, T1 >::Swap(BidirectionalMap& rhs)
{
	mFwd.swap(rhs.mFwd);
	mBwd.swap(rhs.mBwd);
}

template<typename T0, typename T1>
inline void BidirectionalMap<T0, T1 >::Insert(const T1& f, const T0& i)
{
	DataRecord* new_val = new DataRecord(i, f);
	std::pair<typename FwdMap::iterator, bool> fwd_result = mFwd.insert(&new_val->first);
	assert(fwd_result.second);
	std::pair<typename BwdMap::iterator, bool> bwd_result = mBwd.insert(&new_val->second);
	assert(bwd_result.second);
}

template<typename T0, typename T1>
inline void BidirectionalMap<T0, T1 >::Insert(const T0& i, const T1& f)
{
	Insert(f, i);
}

template<typename T0, typename T1>
inline void BidirectionalMap<T0, T1 >::Erase(const T0& i)
{
	typename FwdMap::iterator fwd_it = mFwd.find(&i);
	assert(fwd_it != mFwd.end());
	const DataRecord* to_be_deleted = reinterpret_cast<const DataRecord*>(*fwd_it);

	typename BwdMap::iterator bwd_it = mBwd.find(&to_be_deleted->second);
	assert(bwd_it != mBwd.end());

	mFwd.erase(fwd_it);
	mBwd.erase(bwd_it);

	delete to_be_deleted;
}

template<typename T0, typename T1>
inline void BidirectionalMap<T0, T1 >::Erase(const T1& f)
{
	typename BwdMap::const_iterator bwd_it = mBwd.find(&f);
	assert(bwd_it != mBwd.end());
	const DataRecord* to_be_deleted = reinterpret_cast<const DataRecord*>((char*)*bwd_it - offsetof(DataRecord, second));

	typename FwdMap::const_iterator fwd_it = mFwd.find(&to_be_deleted->first);
	assert(fwd_it != mFwd.end());

	mFwd.erase(fwd_it);
	mBwd.erase(bwd_it);

	delete to_be_deleted;
}

template<typename T0, typename T1>
inline void BidirectionalMap<T0, T1 >::Clear()
{
	typename FwdMap::iterator fwd_it = mFwd.begin();
	typename FwdMap::iterator fwd_end = mFwd.end();
	for (; fwd_it != fwd_end; ++fwd_it)
	{
		delete reinterpret_cast<const DataRecord*>(*fwd_it);
	}
	mFwd.clear();
	mBwd.clear();
}

template<typename T0, typename T1>
inline typename BidirectionalMap<T0, T1>::Iterator
BidirectionalMap<T0, T1 >::Find(const T0& i) const
{
	return Iterator(mFwd.find(&i));
}

template<typename T0, typename T1>
inline typename BidirectionalMap<T0, T1>::Iterator
BidirectionalMap<T0, T1 >::Find(const T1& f) const
{
	typename BwdMap::const_iterator it = mBwd.find(&f);
	if (it == mBwd.end())
		return mFwd.end();
	const DataRecord* value = reinterpret_cast<const std::pair<T0, T1>*>((char*)*it - offsetof(DataRecord, second));
	return Iterator(mFwd.find(&value->first));
}

template<typename T0, typename T1>
inline const T1& BidirectionalMap<T0, T1 >::operator[](const T0& i) const
{
	return (*Find(i))->second;
}

template<typename T0, typename T1>
inline const T0& BidirectionalMap<T0, T1 >::operator[](const T1& f) const
{
	return (*Find(f))->first;
}

template<typename T0, typename T1>
inline int BidirectionalMap<T0, T1 >::Size() const
{
	assert(mFwd.size() == mBwd.size());
	return mFwd.size();
}

template<typename T0, typename T1>
inline bool BidirectionalMap<T0, T1 >::Empty() const
{
	assert(mFwd.size() == mBwd.size());
	return mFwd.empty();
}

#if 0
template<typename T0, typename T1>
template<typename T>
inline std::vector<T> BidirectionalMap<T0, T1 >::GetKeys() const
{
	std::vector<T> result;
	typename FwdMap::const_iterator it = mFwd.begin();
	typename FwdMap::const_iterator end = mFwd.end();
	for (; it != end; ++it)
		result.push_back(**it);
	return result;
}
#endif

template<typename T0, typename T1>
inline std::vector<T0> BidirectionalMap<T0, T1 >::GetPrimaryKeys() const
{
	std::vector<T0> result;
	result.reserve(mFwd.size());

	typename FwdMap::const_iterator it = mFwd.begin();
	typename FwdMap::const_iterator end = mFwd.end();
	for (; it != end; ++it)
		result.push_back(**it);
	return result;
}

template<typename T0, typename T1>
inline std::vector<T1> BidirectionalMap<T0, T1 >::GetSecondaryKeys() const
{
	std::vector<T1> result;
	result.reserve(mBwd.size());

#if 0
	typename BwdMap::const_iterator it = mBwd.begin();
	typename BwdMap::const_iterator end = mBwd.end();
	for (; it != end; ++it)
		result.push_back(**it);
#endif
	Iterator it = Begin();
	Iterator end = End();
	for (; it != end; ++it)
		result.push_back((*it)->second);
	return result;
}

template<typename T0, typename T1>
inline BidirectionalMap<T0, T1>::Iterator::Iterator(const typename FwdMap::const_iterator& prim_keys_iter) :
	mPrimKeysIter(prim_keys_iter)
{
}

template<typename T0, typename T1>
inline typename BidirectionalMap<T0, T1>::Iterator&
BidirectionalMap<T0, T1 >::Iterator::operator++()
{
	++mPrimKeysIter;
	return *this;
}

template<typename T0, typename T1>
inline const typename BidirectionalMap<T0, T1>::DataRecord*
BidirectionalMap<T0, T1 >::Iterator::operator*() const
{
	return reinterpret_cast<const DataRecord*>(*mPrimKeysIter);
}

template<typename T0, typename T1>
inline bool BidirectionalMap<T0, T1 >::Iterator::operator==(const Iterator& rhs) const
{
	return mPrimKeysIter == rhs.mPrimKeysIter;
}

template<typename T0, typename T1>
inline bool BidirectionalMap<T0, T1 >::Iterator::operator!=(const Iterator& rhs) const
{
	return !(mPrimKeysIter == rhs.mPrimKeysIter);
}

template<typename T0, typename T1>
inline typename BidirectionalMap<T0, T1>::Iterator
BidirectionalMap<T0, T1 >::Begin() const
{
	return Iterator(mFwd.begin());
}

template<typename T0, typename T1>
inline typename BidirectionalMap<T0, T1>::Iterator
BidirectionalMap<T0, T1 >::End() const
{
	return Iterator(mFwd.end());
}

#endif
