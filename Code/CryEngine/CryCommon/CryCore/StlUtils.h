// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryMath/Range.h>

#include <map>
#include <set>
#include <type_traits>
#include <algorithm>
#include <functional>
#include <deque>
#include <unordered_map>
#include <unordered_set>

template<class Container>
unsigned sizeofArray(const Container& arr)
{
	return (unsigned)(sizeof(typename Container::value_type) * arr.size());
}

template<class Container>
unsigned sizeofVector(const Container& arr)
{
	return (unsigned)(sizeof(typename Container::value_type) * arr.capacity());
}

template<class Container>
unsigned sizeofArray(const Container& arr, unsigned nSize)
{
	return arr.empty() ? 0u : (unsigned)(sizeof(typename Container::value_type) * nSize);
}

template<class Container>
unsigned capacityofArray(const Container& arr)
{
	return (unsigned)(arr.capacity() * sizeof(arr[0]));
}

template<class T>
unsigned countElements(const std::vector<T>& arrT, const T& x)
{
	unsigned nSum = 0;
	for (typename std::vector<T>::const_iterator iter = arrT.begin(); iter != arrT.end(); ++iter)
		if (x == *iter)
			++nSum;
	return nSum;
}

// [Timur]
/** Contain extensions for STL library.
 */
namespace stl
{
//! Implementation of std::make_unique - missing from C++/11
//! Remove when all compilers provide the default implementation.
template<class T, class... TArgs> inline std::unique_ptr<T> make_unique(TArgs&&... args)
{
	return (std::unique_ptr<T>(new T(std::forward<TArgs>(args)...)));
}

//! Compare member of class/struct.
//! e.g. Sort Vec3s by x component
//! std::sort(vec3s.begin(), vec3s.end(), stl::member_compare<Vec3, float, &Vec3::x>());
template<typename OWNER_TYPE, typename MEMBER_TYPE, MEMBER_TYPE OWNER_TYPE::* MEMBER_PTR, typename EQUALITY = std::less<MEMBER_TYPE>> struct member_compare
{
	inline bool operator()(const OWNER_TYPE& lhs, const OWNER_TYPE& rhs) const
	{
		return EQUALITY()(lhs.*MEMBER_PTR, rhs.*MEMBER_PTR);
	}
};

//! Compare member of class/struct against parameter.
//! e.g. Find Vec3 with x component less than 1.0
//! std::find_if(vec3s.begin(), vec3s.end(), stl::member_compare_param<Vec3, float, &Vec3::x>(1.0f));
template<typename OWNER_TYPE, typename MEMBER_TYPE, MEMBER_TYPE OWNER_TYPE::* MEMBER_PTR, typename EQUALITY = std::less<MEMBER_TYPE>> struct member_compare_param
{
	inline member_compare_param(const MEMBER_TYPE& _value)
		: value(_value)
	{
	}

	inline bool operator()(const OWNER_TYPE& rhs) const
	{
		return EQUALITY()(rhs.*MEMBER_PTR, value);
	}

	const MEMBER_TYPE& value;
};

//////////////////////////////////////////////////////////////////////////
//! Searches the given entry in the map by key, and if there is none, returns the default value
//////////////////////////////////////////////////////////////////////////
template<typename Map>
inline typename Map::mapped_type find_in_map(const Map& mapKeyToValue, const typename Map::key_type& key, typename Map::mapped_type valueDefault)
{
	typename Map::const_iterator it = mapKeyToValue.find(key);
	if (it == mapKeyToValue.end())
		return valueDefault;
	else
		return it->second;
}

//////////////////////////////////////////////////////////////////////////
//! Inserts and returns a reference to the given value in the map, or returns the current one if it's already there.
//////////////////////////////////////////////////////////////////////////
template<typename Map>
inline typename Map::mapped_type& map_insert_or_get(Map& mapKeyToValue, const typename Map::key_type& key, const typename Map::mapped_type& defValue = typename Map::mapped_type())
{
	std::pair<typename Map::iterator, bool> iresult = mapKeyToValue.insert(typename Map::value_type(key, defValue));
	return iresult.first->second;
}

//! Searches the given entry in the map by key, and if there is none, returns the default value.
//! The values are taken/returned in REFERENCEs rather than values.
template<typename Key, typename mapped_type, typename Traits, typename Allocator>
inline mapped_type& find_in_map_ref(std::map<Key, mapped_type, Traits, Allocator>& mapKeyToValue, const Key& key, mapped_type& valueDefault)
{
	typedef std::map<Key, mapped_type, Traits, Allocator> Map;
	typename Map::iterator it = mapKeyToValue.find(key);
	if (it == mapKeyToValue.end())
		return valueDefault;
	else
		return it->second;
}

template<typename Key, typename mapped_type, typename Traits, typename Allocator>
inline const mapped_type& find_in_map_ref(const std::map<Key, mapped_type, Traits, Allocator>& mapKeyToValue, const Key& key, const mapped_type& valueDefault)
{
	typedef std::map<Key, mapped_type, Traits, Allocator> Map;
	typename Map::const_iterator it = mapKeyToValue.find(key);
	if (it == mapKeyToValue.end())
		return valueDefault;
	else
		return it->second;
}

//////////////////////////////////////////////////////////////////////////
//! Fills vector with contents of map.
//////////////////////////////////////////////////////////////////////////
template<class Map, class Vector>
inline void map_to_vector(const Map& theMap, Vector& array)
{
	array.resize(0);
	array.reserve(theMap.size());
	for (typename Map::const_iterator it = theMap.begin(); it != theMap.end(); ++it)
	{
		array.push_back(it->second);
	}
}

//////////////////////////////////////////////////////////////////////////
//! Fills vector with contents of set.
//////////////////////////////////////////////////////////////////////////
template<class Set, class Vector>
inline void set_to_vector(const Set& theSet, Vector& array)
{
	array.resize(0);
	array.reserve(theSet.size());
	for (typename Set::const_iterator it = theSet.begin(); it != theSet.end(); ++it)
	{
		array.push_back(*it);
	}
}

//////////////////////////////////////////////////////////////////////////
template<typename Container, typename KeyFunc>
void sort(Container& cont, KeyFunc key)
{
	using Ref = typename Container::const_reference;
	std::sort(cont.begin(), cont.end(), [key](Ref a, Ref b)
	{
		return key(a) < key(b);
	});
}

//////////////////////////////////////////////////////////////////////////
template<typename C, typename V>
int find_index(const C& container, const V& value)
{
	auto it = std::find(container.begin(), container.end(), value);
	return it == container.end() ? -1 : int(it - container.begin());
}

template<typename C, typename P>
int find_index_if(const C& container, const P& pred)
{
	auto it = std::find_if(container.begin(), container.end(), pred);
	return it == container.end() ? -1 : int(it - container.begin());
}

template<typename C, typename V>
bool has_value(const C& container, const V& value)
{
	auto it = std::find(container.begin(), container.end(), value);
	return it != container.end();
}

template<typename C, typename P>
bool has_value_if(const C& container, const P& pred)
{
	auto it = std::find_if(container.begin(), container.end(), pred);
	return it != container.end();
}

template<typename C, typename P>
typename C::value_type find_value_if(const C& container, const P& pred)
{
	auto it = std::find_if(container.begin(), container.end(), pred);
	return it != container.end() ? *it : typename C::value_type();
}

//////////////////////////////////////////////////////////////////////////
//! Find and erase element from container.
//! \return true if item was find and erased, false if item not found.
template<class Container, class Value>
inline bool find_and_erase(Container& container, const Value& value)
{
	typename Container::iterator it = std::find(container.begin(), container.end(), value);
	if (it != container.end())
	{
		container.erase(it);
		return true;
	}
	return false;
}

template<typename K, typename P, typename A>
inline bool find_and_erase(std::set<K, P, A>& container, const K& value)
{
	return container.erase(value) > 0;
}

//! Find and erase element from container.
//! \return true if item was find and erased, false if item not found.
template<class CONTAINER, class PREDICATE> inline bool find_and_erase_if(CONTAINER& container, const PREDICATE& predicate)
{
	typename CONTAINER::iterator end = container.end(), i = std::find_if(container.begin(), end, predicate);

	if (i != end)
	{
		container.erase(i);

		return true;
	}

	return false;
}

//! Find and erase all elements matching value from container.
//! Assume that this will invalidate any exiting iterators.
//! Commonly used for removing NULL pointers from collections.
template<class Container>
inline void find_and_erase_all(Container& container, const typename Container::value_type& value)
{
	// Shuffles all elements == value to the end and returns the start of the removed elements.
	typename Container::iterator endIter(container.end());
	typename Container::iterator newEndIter(std::remove(container.begin(), endIter, value));

	// Delete the removed range at the back of the container (low-cost for vector).
	container.erase(newEndIter, endIter);
}

template<class Container, class Predicate> 
inline void find_and_erase_all_if(Container& container, const Predicate& predicate)
{
	// Shuffles all elements == predicate to the front and returns the start of the removed elements.
	typename Container::iterator endIter(container.end());
	typename Container::iterator newEndIter(std::remove_if(container.begin(), endIter, predicate));

	// Delete the removed range at the back of the container (low-cost for vector).
	container.erase(newEndIter, endIter);
}

//! Find and erase element from map.
//! \return true if item was find and erased, false if item not found.
template<class Container, class Key>
inline bool member_find_and_erase(Container& container, const Key& key)
{
	typename Container::iterator it = container.find(key);
	if (it != container.end())
	{
		container.erase(it);
		return true;
	}
	return false;
}

//! Push back to container unique element.
//! \return true if item added, false overwise.
template<class Container, class Value>
inline bool push_back_unique(Container& container, const Value& value)
{
	if (std::find(container.begin(), container.end(), value) == container.end())
	{
		container.push_back(value);
		return true;
	}
	return false;
}

//! Push back to container unique element.
//! \return true if item added, false overwise.
template<class CONTAINER, class PREDICATE, typename VALUE> inline bool push_back_unique_if(CONTAINER& container, const PREDICATE& predicate, const VALUE& value)
{
	typename CONTAINER::iterator end = container.end();

	if (std::find_if(container.begin(), end, predicate) == end)
	{
		container.push_back(value);

		return true;
	}
	else
	{
		return false;
	}
}

//////////////////////////////////////////////////////////////////////////
//! Push back to container contents of another container
template<class Container, class Iter>
inline void push_back_range(Container& container, Iter begin, Iter end)
{
	for (Iter it = begin; it != end; ++it)
	{
		container.push_back(*it);
	}
}

//////////////////////////////////////////////////////////////////////////
//! Push back to container contents of another container, if not already present
template<class Container, class Iter>
inline void push_back_range_unique(Container& container, Iter begin, Iter end)
{
	for (Iter it = begin; it != end; ++it)
	{
		push_back_unique(container, *it);
	}
}

//! Find element in container.
//! \return true if item found.
template<class Container, class Value>
inline bool find(Container& container, const Value& value)
{
	return std::find(container.begin(), container.end(), value) != container.end();
}

//! Find element in a sorted container using binary search with logarithmic efficiency.
template<class Iterator, class T>
inline Iterator binary_find(Iterator first, Iterator last, const T& value)
{
	Iterator it = std::lower_bound(first, last, value);
	return (it == last || value != *it) ? last : it;
}

//! Find element in a sorted container using binary search with logarithmic efficiency.
//! \return true if item was inserted.
template<class Container, class Value>
inline bool binary_insert_unique(Container& container, const Value& value)
{
	typename Container::iterator it = std::lower_bound(container.begin(), container.end(), value);
	if (it != container.end())
	{
		if (*it == value)
			return false;
		container.insert(it, value);
	}
	else
		container.insert(container.end(), value);
	return true;
}

//! Find element in a sorted container using binary search with logarithmic efficiency.
//! \return true if item was inserted.
template<class Container, class Value, typename Func>
inline bool binary_insert_unique(Container& container, const Value& value, Func order)
{
	typename Container::iterator it = std::lower_bound(container.begin(), container.end(), value, order);
	if (it != container.end())
	{
		if (*it == value)
			return false;
		container.insert(it, value);
	}
	else
		container.insert(container.end(), value);
	return true;
}
//! Find element in a sorted container using binary search with logarithmic efficiency and erase it if found.
//! \return true if item was erased.
template<class Container, class Value>
inline bool binary_erase(Container& container, const Value& value)
{
	typename Container::iterator it = std::lower_bound(container.begin(), container.end(), value);
	if (it != container.end() && *it == value)
	{
		container.erase(it);
		return true;
	}
	return false;
}

template<typename ItT, typename Func>
ItT remove_from_heap(ItT begin, ItT end, ItT at, Func order)
{
	using std::swap;

	--end;
	if (at == end)
		return at;

	size_t idx = std::distance(begin, at);
	swap(*end, *at);

	size_t length = std::distance(begin, end);
	size_t parent, child;

	if (idx > 0 && order(*(begin + idx / 2), *(begin + idx)))
	{
		do
		{
			parent = idx / 2;
			swap(*(begin + idx), *(begin + parent));
			idx = parent;

			if (idx == 0 || order(*(begin + idx), *(begin + idx / 2)))
				return end;
		}
		while (true);
	}
	else
	{
		do
		{
			child = idx * 2 + 1;
			if (child >= length)
				return end;

			ItT left = begin + child;
			ItT right = begin + child + 1;

			if (right < end && order(*left, *right))
				++child;

			if (order(*(begin + child), *(begin + idx)))
				return end;

			swap(*(begin + child), *(begin + idx));
			idx = child;
		}
		while (true);
	}

	return end;
}

struct container_object_deleter
{
	template<typename T>
	void operator()(const T* ptr) const
	{
		delete ptr;
	}
};

//! Convert arbitrary class to const char*.
template<class Type>
inline const char* constchar_cast(const Type& type)
{
	return type;
}

//! Specialization of string to const char cast.
template<>
inline const char* constchar_cast(const string& type)
{
	return type.c_str();
}

//! Case sensetive less key for any type convertable to const char*.
template<class Type>
struct less_strcmp
{
	bool operator()(const Type& left, const Type& right) const
	{
		return strcmp(constchar_cast(left), constchar_cast(right)) < 0;
	}
};

//! Case insensetive less key for any type convertable to const char*.
template<class Type>
struct less_stricmp
{
	bool operator()(const Type& left, const Type& right) const
	{
		return stricmp(constchar_cast(left), constchar_cast(right)) < 0;
	}
};

// Unordered_map usage:
// typedef std::unordered_map<string, int, stl::hash_stricmp<string>, stl::hash_stricmp<string> > StringToIntHash;

//! Simple uint32 hash that has the avalanche property (a change in one bit affects all others).
class hash_uint32
{
public:
	ILINE size_t operator()(uint32 a) const
	{
		a = (a + 0x7ed55d16) + (a << 12);
		a = (a ^ 0xc761c23c) ^ (a >> 19);
		a = (a + 0x165667b1) + (a << 5);
		a = (a + 0xd3a2646c) ^ (a << 9);
		a = (a + 0xfd7046c5) + (a << 3);
		a = (a ^ 0xb55a4f09) ^ (a >> 16);
		return a;
	}
};

//////////////////////////////////////////////////////////////////////////
//! Case sensitive string's "hash" and "equal_to" for unordered_map
template<class Key>
struct hash_strcmp
{
	size_t operator()(const Key& key) const
	{
		unsigned int h = 0;
		const char* s = constchar_cast(key);
		for (; * s; ++s) h = 5 * h + *(unsigned char*)s;
		return size_t(h);
	}

	bool operator()(const Key& key1, const Key& key2) const
	{
		return strcmp(constchar_cast(key1), constchar_cast(key2)) == 0;
	}
};

//////////////////////////////////////////////////////////////////////////
//! Case insensitive string's "hash" and "equal_to" for unordered_map
template<class Key>
struct hash_stricmp
{
	size_t operator()(const Key& key) const
	{
		unsigned int h = 0;
		const char* s = constchar_cast(key);
		for (; * s; ++s) h = 5 * h + tolower(*(unsigned char*)s);
		return size_t(h);
	}

	bool operator()(const Key& key1, const Key& key2) const
	{
		return stricmp(constchar_cast(key1), constchar_cast(key2)) == 0;
	}
};

//////////////////////////////////////////////////////////////////////////
template<class T>
class intrusive_linked_list_node
{
	typedef intrusive_linked_list_node<T> Self;
public:
	intrusive_linked_list_node()  { link_to_intrusive_list(this); }

	//! Not virtual by design.
	~intrusive_linked_list_node() { unlink_from_intrusive_list(this); }

	static T* get_intrusive_list_root()  { return static_cast<T*>(m_root_intrusive); };

	T*        get_next_intrusive() const { return static_cast<T*>(m_next_intrusive); }

	T*        get_prev_intrusive() const { return static_cast<T*>(m_prev_intrusive); }

private:
	static void link_to_intrusive_list(Self* pNode)
	{
		if (m_root_intrusive)
		{
			// Add to the beginning of the list.
			Self* head = m_root_intrusive;
			pNode->m_prev_intrusive = 0;
			pNode->m_next_intrusive = head;
			head->m_prev_intrusive = pNode;
			m_root_intrusive = pNode;
		}
		else
		{
			m_root_intrusive = pNode;
			pNode->m_prev_intrusive = 0;
			pNode->m_next_intrusive = 0;
		}
	}

	static void unlink_from_intrusive_list(Self* pNode)
	{
		if (pNode == m_root_intrusive)   // if head of list.
		{
			m_root_intrusive = pNode->m_next_intrusive;
			if (m_root_intrusive)
			{
				m_root_intrusive->m_prev_intrusive = 0;
			}
		}
		else
		{
			if (pNode->m_prev_intrusive)
			{
				pNode->m_prev_intrusive->m_next_intrusive = pNode->m_next_intrusive;
			}
			if (pNode->m_next_intrusive)
			{
				pNode->m_next_intrusive->m_prev_intrusive = pNode->m_prev_intrusive;
			}
		}
		pNode->m_next_intrusive = 0;
		pNode->m_prev_intrusive = 0;
	}

	static Self* m_root_intrusive;
	Self*        m_next_intrusive;
	Self*        m_prev_intrusive;
};

//will clear the container and force the capacity to 0
template<typename T, typename... Args>
inline void free_container(T& container, Args&&... args)
{
	container.~T();
	new(&container)T(std::forward<Args>(args) ...);
}

template<typename T, typename... Args>
inline void reconstruct(T& container, Args&&... args)
{
	free_container(container, std::forward<Args>(args) ...);
}

struct container_freer
{
	template<typename T> void operator()(T& container) const
	{
		stl::free_container(container);
	}
};

template<typename T>
struct scoped_set
{
	scoped_set(T& ref, T val)
		: m_ref(&ref)
		, m_oldVal(ref)
	{
		ref = val;
	}

	~scoped_set()
	{
		(*m_ref) = m_oldVal;
	}

private:
	scoped_set(const scoped_set<T>& other);
	scoped_set<T>& operator=(const scoped_set<T>& other);

private:
	T* m_ref;
	T  m_oldVal;
};

template<typename T, size_t Length, typename Func>
inline void for_each_array(T (&buffer)[Length], Func func)
{
	std::for_each(&buffer[0], &buffer[Length], func);
}

template<typename T>
inline void destruct(T* p)
{
	p->~T();
}

//! This class is used to work around inconsistencies and issues in STL allocator support.
//! In particular, VS2012 and VS2013 don't properly support minimal allocators, so we have to provide construct() function.
//! However, insufficient support for variadic templates makes it hard to provide C++11 compatible implementation.
//! Therefore, we supply this function once here, and re-use it in each allocator.
struct SAllocatorConstruct
{
#if !defined(_MSC_VER) || _MSC_VER >= 1900
	//! C++11 standard-compliant implementation.
	template<typename T, typename ... Args>
	static void construct(T* ptr, Args&& ... args)
	{
		::new(const_cast<void*>(static_cast<const void*>(ptr)))T(std::forward<Args>(args) ...);
	}
#else

	#pragma warning(push)
	#pragma warning(disable:4345)// behavior change: an object of POD type constructed with an initializer of the form () will be default-initialized

	//! Work-around implementation, 0 arguments.
	template<typename T>
	static void construct(T* ptr)
	{
		::new(static_cast<void*>(ptr))T();
	}

	#pragma warning(pop)

	//! Work-around implementation, 1 argument.
	template<typename T, typename Arg1>
	static void construct(T* ptr, Arg1&& arg1)
	{
		::new(static_cast<void*>(ptr))T(std::forward<Arg1>(arg1));
	}

	//! Work-around implementation, 2 arguments.
	template<typename T, typename Arg1, typename Arg2>
	static void construct(T* ptr, Arg1&& arg1, Arg2&& arg2)
	{
		::new(static_cast<void*>(ptr))T(std::forward<Arg1>(arg1), std::forward<Arg2>(arg2));
	}

	//! Work-around implementation, 3 arguments.
	template<typename T, typename Arg1, typename Arg2, typename Arg3>
	static void construct(T* ptr, Arg1&& arg1, Arg2&& arg2, Arg3&& arg3)
	{
		::new(static_cast<void*>(ptr))T(std::forward<Arg1>(arg1), std::forward<Arg2>(arg2), std::forward<Arg3>(arg3));
	}

	//! Note: add more implementations if required.
#endif
};

// Adapter for iterating over a container of non-null pointers
// Example:
//		stl::indirect_container<std::vector<T*>> cont;
//		cont.push_back(ptr); ...
//		for (auto& e : cont) e.process();

template<class C>
struct indirect_container : C
{
	struct const_iterator : C::const_iterator
	{
		typedef decltype (** C::const_iterator()) T;
		const_iterator(const typename C::const_iterator& it) : C::const_iterator(it) {}

		const T& operator*() const { return *C::const_iterator::operator*(); }
	};

	const_iterator begin() const { return C::begin(); }
	const_iterator end() const   { return C::end(); }

	struct iterator : C::iterator
	{
		typedef decltype (** C::iterator()) T;
		iterator(const typename C::iterator& it) : C::iterator(it) {}

		T& operator*() const { return *C::iterator::operator*(); }
	};

	iterator begin() { return C::begin(); }
	iterator end()   { return C::end(); }
};

//! Find the biggest of multiple values at compile time.
template<size_t...>
struct static_max;

template<size_t First>
struct static_max<First>
{
	static constexpr size_t value = First;
};

template<size_t First, size_t Second>
struct static_max<First, Second>
{
	static constexpr size_t value = First > Second ? First : Second;
};

template<size_t First, size_t Second, size_t... Others>
struct static_max<First, Second, Others...>
{
	static constexpr size_t value = First > Second ? static_max<First, Others...>::value : static_max<Second, Others...>::value;
};


template<unsigned int num>
struct static_log2
{
	enum
	{
		value = static_log2<(num >> 1)>::value + 1,
	};
};

template<>
struct static_log2<1>
{
	enum
	{
		value = 0,
	};
};

template<>
struct static_log2<0>
{
};


//! Provides a member typedef with suitable size and alignment to hold any of the specified types. The size is at least \p Len.
//! \note Can be replaced with std::aligned_union once GCC is upraded to 5.0 or later.
template<size_t Len, class... Types>
struct aligned_union
{
	static constexpr size_t alignment_value = static_max<alignof(Types)...>::value;

	struct type
	{
		alignas(alignment_value + 0) char c[static_max<Len, sizeof(Types)...>::value]; // The '+ 0' is required due to a bug in GCC 4.8
	};
};
}

#define DEFINE_INTRUSIVE_LINKED_LIST(Class) \
  template<> stl::intrusive_linked_list_node<Class>* stl::intrusive_linked_list_node<Class>::m_root_intrusive = 0;

//! Performs a less-than compare on a serial sequence space, such that earlier values compare less-than later values.
//! Unlike a normal integral value, this accounts for overflowing the limit of the underlying type.
//! For example, assuming a 2-bit unsigned underlying type (with possible values 0, 1, 2 and 3), the following will hold: 0 < 1 && 1 < 2 && 2 < 3 && 3 < 0.
//! Assuming two equal values V1 and V2, V2 can be incremented up to "(2 ^ (bits - 1) - 1)" times and V1 < V2 will continue to hold.
//! \see http://tools.ietf.org/html/rfc1982.
template<typename T>
struct SSerialCompare
{
	static_assert(std::is_integral<T>::value && std::is_unsigned<T>::value, "T must be an unsigned integral type");

	static const T limit = (T(1) << (sizeof(T) * 8 - 1));

	bool operator()(T lhs, T rhs)
	{
		return ((lhs < rhs) && (rhs - lhs < limit)) || ((lhs > rhs) && (lhs - rhs > limit));
	}
};


/**
 *! Remove once we move to C++17
 */
namespace stl {

	template <typename T> struct always_false : std::false_type {};


	// conjuction and negation helpers
	//! Remove once we move to C++17
	template <typename T>
	struct negation {
		static constexpr bool value = !T::value;
	};
	template <typename... Ts>
	struct conjuction : std::true_type {};
	template <typename T, typename... Ts>
	struct conjuction<T, Ts...> {
		static constexpr bool value = T::value && conjuction<Ts...>::value;
	};
	template <typename T>
	struct conjuction<T> {
		static constexpr bool value = T::value;
	};

}
