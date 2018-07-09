// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
//	File:CrySizer.h
//  Declaration and definition of the CrySizer class, which is used to
//  calculate the memory usage by the subsystems and components, to help
//  the artists keep the memory budged low.
//
//	History:
//
//////////////////////////////////////////////////////////////////////

#ifndef _CRY_COMMON_CRY_SIZER_INTERFACE_HDR_
#define _CRY_COMMON_CRY_SIZER_INTERFACE_HDR_

//////////////////////////////////////////////////////////////////////////

// common containers for overloads
#include <CryCore/StlUtils.h>
#include <CryRenderer/Tarray.h>
#include <Cry3DEngine/CryPodArray.h>
#include <CryMath/Cry_Vector3.h>
#include <CryMath/Cry_Quat.h>
#include <CryMath/Cry_Color.h>

// forward declarations for overloads
struct AABB;
struct SVF_P3F;
struct SVF_P3F_C4B_T2F;
struct SVF_P3F_C4B_T2S;
struct SVF_P3S_C4B_T2S;
struct SPipTangents;

#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT
	#include <string.h> // workaround for Amd64 compiler
#endif

#include <CrySystem/IResourceCollector.h>       // <> required for Interfuscator. IResourceCollector

//! Flags applicable to the ICrySizer (retrieved via getFlags() method).
enum ICrySizerFlagsEnum
{
	//! If this flag is set, during getSize(), the subsystem must count all the objects it uses in the other subsystems also.
	CSF_RecurseSubsystems = 1 << 0,

	CSF_Reserved1         = 1 << 1,
	CSF_Reserved2         = 1 << 2
};

//! Helper functions to calculate size of the std containers.
namespace stl
{
struct MapLikeStruct
{
	bool  color;
	void* parent;
	void* left;
	void* right;
};
template<class Map>
inline size_t size_of_map(const Map& m)
{
	if (!m.empty())
	{
		return m.size() * sizeof(typename Map::value_type) + m.size() * sizeof(MapLikeStruct);
	}
	return 0;
}
template<class Map>
inline size_t size_of_set(const Map& m)
{
	if (!m.empty())
	{
		return m.size() * sizeof(typename Map::value_type) + m.size() * sizeof(MapLikeStruct);
	}
	return 0;
}
template<class List>
inline size_t size_of_list(const List& c)
{
	if (!c.empty())
	{
		return c.size() * sizeof(typename List::value_type) + c.size() * sizeof(void*) * 2; // sizeof stored type + 2 pointers prev,next
	}
	return 0;
}
template<class Deque>
inline size_t size_of_deque(const Deque& c)
{
	if (!c.empty())
	{
		return c.size() * sizeof(typename Deque::value_type);
	}
	return 0;
}
};

//! An instance of this class is passed down to each and every component in the system.
//! Every component it's passed to optionally pushes its name on top of the.
//!   component name stack (thus ensuring that all the components calculated down.
//!   the tree will be assigned the correct subsystem/component name).
//! Every component must Add its size with one of the Add* functions, and Add the.
//!   size of all its subcomponents recursively.
//! In order to push the component/system name on the name stack, the clients must.
//!   use the SIZER_COMPONENT_NAME macro or CrySizerComponentNameHelper class:.
//!
//! void X::getSize (ICrySizer* pSizer).
//! {.
//!   SIZER_COMPONENT_NAME(pSizer, X);.
//!   if (!pSizer->Add (this)).
//!     return;.
//!   pSizer->Add (m_arrMySimpleArray);.
//!   pSizer->Add (m_setMySimpleSet);.
//!   m_pSubobject->getSize (pSizer);.
//! }.
//!
//! The Add* functions return bool. If they return true, then the object has been added.
//! to the set for the first time, and you should go on recursively adding all its children.
//! If it returns false, then you can spare time and rather not go on into recursion;.
//! however it doesn't reflect on the results: an object that's added more than once is.
//! counted only once.
//! \note If you have an array (pointer), you should Add its size with addArray.
class ICrySizer
{
public:
	virtual ~ICrySizer(){}
	//! This class is used to push/pop the name to/from the stack automatically
	//! (to exclude stack overruns or underruns at runtime).
	friend class CrySizerComponentNameHelper;

	virtual void Release() = 0;

	//! \return Total calculated size.
	virtual size_t GetTotalSize() = 0;

	//! \return total objects added.
	virtual size_t GetObjectCount() = 0;

	//! Resets the counting.
	virtual void Reset() = 0;

	virtual void End() = 0;

	//! Adds an object identified by the unique pointer (it needs not be
	//! the actual object position in the memory, though it would be nice,
	//! but it must be unique throughout the system and unchanging for this object).
	//! \param nCount Only used for counting number of objects, it doesnt affect the size of the object.
	//! \return True if the object has actually been added (for the first time) and calculated.
	virtual bool AddObject(const void* pIdentifier, size_t nSizeBytes, int nCount = 1) = 0;

	template<typename Type>
	bool AddObjectSize(const Type* pObj)
	{
		return AddObject(pObj, sizeof *pObj);
	}

	//! temp dummy function while checking in the CrySizer changes, will be removed soon.
	template<typename Type>
	void AddObject(const Type& rObj)
	{
		rObj.GetMemoryUsage(this);
	}

	template<typename Type>
	void AddObject(Type* pObj)
	{
		if (pObj)
		{
			//forward to reference object to allow function overload
			this->AddObject(*pObj);
		}
	}

	// Overloads for smart_ptr and other common objects.
	template<typename T>
	void AddObject(const _smart_ptr<T>& rObj)      { this->AddObject(rObj.get()); }
	template<typename T>
	void AddObject(const std::shared_ptr<T>& rObj) { this->AddObject(rObj.get()); }
	template<typename T>
	void AddObject(const std::unique_ptr<T>& rObj) { this->AddObject(rObj.get()); }
	template<typename T, typename U>
	void AddObject(const std::pair<T, U>& rPair)
	{
		this->AddObject(rPair.first);
		this->AddObject(rPair.second);
	}
	void AddObject(const string& rString)              { this->AddObject(rString.c_str(), rString.capacity()); }
	void AddObject(const CryStringT<wchar_t>& rString) { this->AddObject(rString.c_str(), rString.capacity()); }
	void AddObject(const CryFixedStringT<32>&)         {}
	void AddObject(const wchar_t&)                     {}
	void AddObject(const char&)                        {}
	void AddObject(const unsigned char&)               {}
	void AddObject(const signed char&)                 {}
	void AddObject(const short&)                       {}
	void AddObject(const unsigned short&)              {}
	void AddObject(const int&)                         {}
	void AddObject(const unsigned int&)                {}
	void AddObject(const long&)                        {}
	void AddObject(const unsigned long&)               {}
	void AddObject(const float&)                       {}
	void AddObject(const bool&)                        {}
	void AddObject(const unsigned long long&)          {}
	void AddObject(const long long&)                   {}
	void AddObject(const double&)                      {}
	void AddObject(const Vec2&)                        {}
	void AddObject(const Vec3&)                        {}
	void AddObject(const Vec4&)                        {}
	void AddObject(const Ang3&)                        {}
	void AddObject(const Matrix34&)                    {}
	void AddObject(const Quat&)                        {}
	void AddObject(const QuatT&)                       {}
	void AddObject(const QuatTS&)                      {}
	void AddObject(const ColorF&)                      {}
	void AddObject(const AABB&)                        {}
	void AddObject(const SVF_P3F&)                     {}
	void AddObject(const SVF_P3F_C4B_T2F&)             {}
	void AddObject(const SVF_P3F_C4B_T2S&)             {}
	void AddObject(const SVF_P3S_C4B_T2S&)             {}
	void AddObject(const SPipTangents&)                {}
	void AddObject(void*)                              {}

	// Overloads for container, will automaticly traverse the content.
	template<typename T, typename Alloc>
	void AddObject(const std::list<T, Alloc>& rList)
	{
		// dummy struct to get correct element size
		struct Dummy { void* a; void* b; T t; };

		for (typename std::list<T, Alloc>::const_iterator it = rList.begin(); it != rList.end(); ++it)
		{
			if (this->AddObject(&(*it), sizeof(Dummy)))
			{
				this->AddObject(*it);
			}
		}

	}

	template<typename Key, typename Type, typename Hash, typename Pred, typename Alloc>
	void AddObject(const std::unordered_map<Key, Type, Hash, Pred, Alloc>& rMap)
	{
	}

	template<typename T, typename Alloc>
	void AddObject(const std::vector<T, Alloc>& rVector)
	{
		if (rVector.empty())
		{
			this->AddObject(&rVector, rVector.capacity() * sizeof(T));
			return;
		}

		if (!this->AddObject(&rVector[0], rVector.capacity() * sizeof(T)))
			return;

		for (typename std::vector<T, Alloc>::const_iterator it = rVector.begin(); it != rVector.end(); ++it)
		{
			this->AddObject(*it);
		}
	}

	template<typename T, int AlignSize>
	void AddObject(const stl::aligned_vector<T, AlignSize>& rVector)
	{
		// Not really correct as not taking alignment into the account
		if (rVector.empty())
		{
			this->AddObject(&rVector, rVector.capacity() * sizeof(T));
			return;
		}

		if (!this->AddObject(&rVector[0], rVector.capacity() * sizeof(T)))
			return;

		for (typename stl::aligned_vector<T, AlignSize>::const_iterator it = rVector.begin(); it != rVector.end(); ++it)
		{
			this->AddObject(*it);
		}
	}

	template<typename T, typename Alloc>
	void AddObject(const std::deque<T, Alloc>& rVector)
	{
		for (typename std::deque<T, Alloc>::const_iterator it = rVector.begin(); it != rVector.end(); ++it)
		{
			if (this->AddObject(&(*it), sizeof(T)))
			{
				this->AddObject(*it);
			}
		}
	}

	template<typename T, typename I, typename S>
	void AddObject(const DynArray<T, I, S>& rVector)
	{
		if (rVector.empty())
		{
			this->AddObject(rVector.begin(), rVector.get_alloc_size());
			return;
		}

		if (!this->AddObject(rVector.begin(), rVector.get_alloc_size())) return;

		for (typename DynArray<T, I, S>::const_iterator it = rVector.begin(); it != rVector.end(); ++it)
		{
			this->AddObject(*it);
		}
	}

	template<typename T>
	void AddObject(const TArray<T>& rVector)
	{
		if (!this->AddObject(rVector.begin(), rVector.capacity() * sizeof(T))) return;

		for (int i = 0, end = rVector.size(); i < end; ++i)
		{
			this->AddObject(rVector[i]);
		}
	}

	template<typename T>
	void AddObject(const PodArray<T>& rVector)
	{
		if (!this->AddObject(rVector.begin(), rVector.capacity() * sizeof(T))) return;

		for (typename PodArray<T>::const_iterator it = rVector.begin(); it != rVector.end(); ++it)
		{
			this->AddObject(*it);
		}
	}

	template<typename K, typename T, typename Comp, typename Alloc>
	void AddObject(const std::map<K, T, Comp, Alloc>& rVector)
	{
		// dummy struct to get correct element size
		struct Dummy { void* a; void* b; void* c; void* d; K k; T t; };

		for (typename std::map<K, T, Comp, Alloc>::const_iterator it = rVector.begin(); it != rVector.end(); ++it)
		{
			if (this->AddObject(&(*it), sizeof(Dummy)))
			{
				this->AddObject(it->first);
				this->AddObject(it->second);
			}
		}
	}

	template<typename T, typename Comp, typename Alloc>
	void AddObject(const std::set<T, Comp, Alloc>& rVector)
	{
		// dummy struct to get correct element size
		struct Dummy { void* a; void* b; void* c; void* d; T t; };

		for (typename std::set<T, Comp, Alloc>::const_iterator it = rVector.begin(); it != rVector.end(); ++it)
		{
			if (this->AddObject(&(*it), sizeof(Dummy)))
			{
				this->AddObject(*it);
			}
		}
	}

	template<typename TKey, typename TValue, typename TPredicate, typename TAlloc>
	void AddObject(const std::multimap<TKey, TValue, TPredicate, TAlloc>& rContainer)
	{
		AddContainer(rContainer);
	}
	////////////////////////////////////////////////////////////////////////////////////////
	template<typename T>
	bool Add(const T* pId, size_t num)
	{
		return AddObject(pId, num * sizeof(T));
	}

	template<class T>
	bool Add(const T& rObject)
	{
		return AddObject(&rObject, sizeof(T));
	}

	//! Used to collect the assets needed for streaming and to gather statistics always returns a valid reference.
	virtual IResourceCollector* GetResourceCollector() = 0;
	virtual void                SetResourceCollector(IResourceCollector* pColl) = 0;

	bool                        Add(const char* szText)
	{
		return AddObject(szText, strlen(szText) + 1);
	}

	template<class StringCls>
	bool AddString(const StringCls& strText)
	{
		if (!strText.empty())
		{
			return AddObject(strText.c_str(), strText.size());
		}
		else
			return false;
	}
#ifdef _XSTRING_
	template<class Elem, class Traits, class Allocator>
	bool Add(const std::basic_string<Elem, Traits, Allocator>& strText)
	{
		AddString(strText);
		return true;
	}
#endif

#ifndef NOT_USE_CRY_STRING
	bool Add(const string& strText)
	{
		AddString(strText);
		return true;
	}
#endif

	//! Template helper function to add generic stl container.
	template<typename Container>
	bool AddContainer(const Container& rContainer)
	{
		if (rContainer.capacity())
			return AddObject(&rContainer, rContainer.capacity() * sizeof(typename Container::value_type));
		return false;
	}
	template<typename Container>
	bool AddHashMap(const Container& rContainer)
	{
		if (!rContainer.empty())
			return AddObject(&(*rContainer.begin()), rContainer.size() * sizeof(typename Container::value_type));
		return false;
	}

	//! Specialization of the AddContainer for the std::list.
	template<typename Type, typename TAlloc>
	bool AddContainer(const std::list<Type, TAlloc>& rContainer)
	{
		if (!rContainer.empty())
			return AddObject(&(*rContainer.begin()), stl::size_of_list(rContainer));
		return false;
	}
	//! Specialization of the AddContainer for the std::deque.
	template<typename Type, typename TAlloc>
	bool AddContainer(const std::deque<Type, TAlloc>& rContainer)
	{
		if (!rContainer.empty())
			return AddObject(&(*rContainer.begin()), stl::size_of_deque(rContainer));
		return false;
	}
	//! Specialization of the AddContainer for the std::map.
	template<typename TKey, typename TValue, typename TPredicate, typename TAlloc>
	bool AddContainer(const std::map<TKey, TValue, TPredicate, TAlloc>& rContainer)
	{
		if (!rContainer.empty())
			return AddObject(&(*rContainer.begin()), stl::size_of_map(rContainer));
		return false;
	}
	//! Specialization of the AddContainer for the std::multimap.
	template<typename TKey, typename TValue, typename TPredicate, typename TAlloc>
	bool AddContainer(const std::multimap<TKey, TValue, TPredicate, TAlloc>& rContainer)
	{
		if (!rContainer.empty())
			return AddObject(&(*rContainer.begin()), stl::size_of_map(rContainer));
		return false;
	}
	//! Specialization of the AddContainer for the std::set.
	template<typename TKey, typename TPredicate, typename TAlloc>
	bool AddContainer(const std::set<TKey, TPredicate, TAlloc>& rContainer)
	{
		if (!rContainer.empty())
			return AddObject(&(*rContainer.begin()), stl::size_of_set(rContainer));
		return false;
	}

	//! Specialization of the AddContainer for the std::unordered_map.
	template<typename KEY, typename TYPE, class HASH, class EQUAL, class ALLOCATOR>
	bool AddContainer(const std::unordered_map<KEY, TYPE, HASH, EQUAL, ALLOCATOR>& rContainer)
	{
		if (!rContainer.empty())
		{
			return AddObject(&(*rContainer.begin()), rContainer.size() * sizeof(typename std::unordered_map<KEY, TYPE, HASH, EQUAL, ALLOCATOR>::value_type));
		}
		else
		{
			return false;
		}
	}

	void Test()
	{
		std::map<int, float> mymap;
		AddContainer(mymap);
	}

	//! \return The flags.
	unsigned GetFlags() const { return m_nFlags; }

protected:
	//! These functions must operate on the component name stack.
	//! They are to be only accessible from within class CrySizerComponentNameHelper,
	//! which should be used through macro SIZER_COMPONENT_NAME.
	virtual void Push(const char* szComponentName) = 0;

	//! Pushes the name that is the name of the previous component . (dot) this name.
	virtual void PushSubcomponent(const char* szSubcomponentName) = 0;
	virtual void Pop() = 0;

	unsigned m_nFlags;
};

//! This is on-stack class that is only used to push/pop component names.
//! to/from the sizer name stack.
//! Usage:
//! Create an instance of this class at the start of a function, before.
//! calling Add* methods of the sizer interface. Everything added in the.
//! function and below will be considered this component, unless.
//! explicitly set otherwise.
class CrySizerComponentNameHelper
{
public:
	//! Pushes the component name on top of the name stack of the given sizer.
	CrySizerComponentNameHelper(ICrySizer* pSizer, const char* szComponentName, bool bSubcomponent) :
		m_pSizer(pSizer)
	{
		if (bSubcomponent)
			pSizer->PushSubcomponent(szComponentName);
		else
			pSizer->Push(szComponentName);
	}

	//! Pops the component name off top of the name stack of the sizer.
	~CrySizerComponentNameHelper()
	{
		m_pSizer->Pop();
	}

protected:
	ICrySizer* m_pSizer;
};

//! Use this to push (and automatically pop) the sizer component name at the beginning of the getSize() function.
#define SIZER_COMPONENT_NAME(pSizerPointer, szComponentName)    PREFAST_SUPPRESS_WARNING(6246) CrySizerComponentNameHelper __sizerHelper(pSizerPointer, szComponentName, false)
#define SIZER_SUBCOMPONENT_NAME(pSizerPointer, szComponentName) PREFAST_SUPPRESS_WARNING(6246) CrySizerComponentNameHelper __sizerHelper(pSizerPointer, szComponentName, true)

#endif //_CRY_COMMON_CRY_SIZER_INTERFACE_HDR_
