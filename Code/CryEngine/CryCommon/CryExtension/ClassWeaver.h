// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   ClassWeaver.h
//  Version:     v1.00
//  Created:     02/25/2009 by CarstenW
//  Description: Part of CryEngine's extension framework.
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#ifndef _CLASSWEAVER_H_
#define _CLASSWEAVER_H_

#pragma once

#include "TypeList.h"
#include "Conversion.h"
#include "RegFactoryNode.h"
#include <CryExtension/ICryUnknown.h>
#include <CryExtension/ICryFactory.h>

namespace CW
{

namespace Internal
{
template<class Dst> struct InterfaceCast;

template<class Dst>
struct InterfaceCast
{
	template<class T>
	static void* Op(T* p)
	{
		return (Dst*) p;
	}
};

template<>
struct InterfaceCast<ICryUnknown>
{
	template<class T>
	static void* Op(T* p)
	{
		return const_cast<ICryUnknown*>(static_cast<const ICryUnknown*>(static_cast<const void*>(p)));
	}
};
}

template<class TList> struct InterfaceCast;

template<>
struct InterfaceCast<TL::NullType>
{
	template<class T>
	static void* Op(T*, const CryInterfaceID&)
	{
		return 0;
	}
};

template<class Head, class Tail>
struct InterfaceCast<TL::Typelist<Head, Tail>>
{
	template<class T>
	static void* Op(T* p, const CryInterfaceID& iid)
	{
		if (cryiidof<Head>() == iid)
			return Internal::InterfaceCast<Head>::Op(p);
		return InterfaceCast<Tail>::Op(p, iid);
	}
};

template<class TList> struct FillIIDs;

template<>
struct FillIIDs<TL::NullType>
{
	static void Op(CryInterfaceID*)
	{
	}
};

template<class Head, class Tail>
struct FillIIDs<TL::Typelist<Head, Tail>>
{
	static void Op(CryInterfaceID* p)
	{
		*p++ = cryiidof<Head>();
		FillIIDs<Tail>::Op(p);
	}
};

namespace Internal
{
template<bool, typename S> struct PickList;

template<bool, typename S>
struct PickList
{
	typedef TL::BuildTypelist<>::Result Result;
};

template<typename S>
struct PickList<true, S>
{
	typedef typename S::FullCompositeList Result;
};
}

template<typename T>
struct ProbeFullCompositeList
{
private:
	typedef char y[1];
	typedef char n[2];

	template<typename S>
	static y& test(typename S::FullCompositeList*);

	template<typename>
	static n& test(...);

public:
	enum
	{
		listFound = sizeof(test<T>(0)) == sizeof(y)
	};

	typedef typename Internal::PickList<listFound, T>::Result ListType;
};

namespace Internal
{
template<class TList> struct CompositeQuery;

template<>
struct CompositeQuery<TL::NullType>
{
	template<typename T>
	static void* Op(const T&, const char*)
	{
		return 0;
	}
};

template<class Head, class Tail>
struct CompositeQuery<TL::Typelist<Head, Tail>>
{
	template<typename T>
	static void* Op(const T& ref, const char* name)
	{
		void* p = ref.Head::CompositeQueryImpl(name);
		return p ? p : CompositeQuery<Tail>::Op(ref, name);
	}
};
}

struct CompositeQuery
{
	template<typename T>
	static void* Op(const T& ref, const char* name)
	{
		return Internal::CompositeQuery<typename ProbeFullCompositeList<T>::ListType>::Op(ref, name);
	}
};

inline bool NameMatch(const char* name, const char* compositeName)
{
	if (!name || !compositeName)
		return false;
	size_t i = 0;
	for (; name[i] && name[i] == compositeName[i]; ++i)
	{
	}
	return name[i] == compositeName[i];
}

template<typename T>
void* CheckCompositeMatch(const char* name, const std::shared_ptr<T>& composite, const char* compositeName)
{
	typedef TC::SuperSubClass<ICryUnknown, T> Rel;
	static_assert(Rel::exists, "Unexpected enum value!");
	return NameMatch(name, compositeName) ? const_cast<void*>(static_cast<const void*>(&composite)) : 0;
}

}

#define CRYINTERFACE_BEGIN() \
  private:                   \
    typedef TL::BuildTypelist < ICryUnknown

#define CRYINTERFACE_ADD(iname)                        , iname

#define CRYINTERFACE_END()                             > ::Result _UserDefinedPartialInterfaceList; \
  protected:                                                                                        \
    typedef TL::NoDuplicates<_UserDefinedPartialInterfaceList>::Result FullInterfaceList;

#define _CRY_TPL_APPEND0(base)                         TL::Append<base::FullInterfaceList, _UserDefinedPartialInterfaceList>::Result
#define _CRY_TPL_APPEND(base, intermediate)            TL::Append<base::FullInterfaceList, intermediate>::Result

#define CRYINTERFACE_ENDWITHBASE(base)                 > ::Result _UserDefinedPartialInterfaceList; \
  protected:                                                                                        \
    typedef TL::NoDuplicates<_CRY_TPL_APPEND0(base)>::Result FullInterfaceList;

#define CRYINTERFACE_ENDWITHBASE2(base0, base1)        > ::Result _UserDefinedPartialInterfaceList; \
  protected:                                                                                        \
    typedef TL::NoDuplicates<_CRY_TPL_APPEND(base0, _CRY_TPL_APPEND0(base1))>::Result FullInterfaceList;

#define CRYINTERFACE_ENDWITHBASE3(base0, base1, base2) > ::Result _UserDefinedPartialInterfaceList; \
  protected:                                                                                        \
    typedef TL::NoDuplicates<_CRY_TPL_APPEND(base0, _CRY_TPL_APPEND(base1, _CRY_TPL_APPEND0(base2)))>::Result FullInterfaceList;

#define CRYINTERFACE_SIMPLE(iname) \
  CRYINTERFACE_BEGIN()             \
  CRYINTERFACE_ADD(iname)          \
  CRYINTERFACE_END()

#define CRYCOMPOSITE_BEGIN()                         \
  private:                                           \
    void* CompositeQueryImpl(const char* name) const \
    {                                                \
      (void)(name);                                  \
      void* res = 0; (void)(res);                    \

#define CRYCOMPOSITE_ADD(member, membername)                                   \
  static_assert(CRY_ARRAY_COUNT(membername) > 1, "'membername' is too short"); \
  if ((res = CW::CheckCompositeMatch(name, member, membername)) != 0)          \
    return res;

#define _CRYCOMPOSITE_END(implclassname)                                  \
  return 0;                                                               \
  };                                                                      \
protected:                                                                \
  typedef TL::BuildTypelist<implclassname>::Result _PartialCompositeList; \
                                                                          \
  template<bool, typename S> friend struct CW::Internal::PickList;

#define CRYCOMPOSITE_END(implclassname) \
  _CRYCOMPOSITE_END(implclassname)      \
protected:                              \
  typedef _PartialCompositeList FullCompositeList;

#define _CRYCOMPOSITE_APPEND0(base)              TL::Append<_PartialCompositeList, CW::ProbeFullCompositeList<base>::ListType>::Result
#define _CRYCOMPOSITE_APPEND(base, intermediate) TL::Append<intermediate, CW::ProbeFullCompositeList<base>::ListType>::Result

#define CRYCOMPOSITE_ENDWITHBASE(implclassname, base) \
  _CRYCOMPOSITE_END(implclassname)                    \
protected:                                            \
  typedef _CRYCOMPOSITE_APPEND0 (base) FullCompositeList;

#define CRYCOMPOSITE_ENDWITHBASE2(implclassname, base0, base1) \
  _CRYCOMPOSITE_END(implclassname)                             \
protected:                                                     \
  typedef TL::NoDuplicates<_CRYCOMPOSITE_APPEND(base1, _CRYCOMPOSITE_APPEND0(base0))>::Result FullCompositeList;

#define CRYCOMPOSITE_ENDWITHBASE3(implclassname, base0, base1, base2) \
  _CRYCOMPOSITE_END(implclassname)                                    \
protected:                                                            \
  typedef TL::NoDuplicates<_CRYCOMPOSITE_APPEND(base2, _CRYCOMPOSITE_APPEND(base1, _CRYCOMPOSITE_APPEND0(base0)))>::Result FullCompositeList;

template<typename T>
class CFactory : public ICryFactory
{
public:
	virtual const char* GetName() const
	{
		return T::GetCName();
	}

	virtual const CryClassID& GetClassID() const
	{
		static constexpr CryClassID cid = T::GetCID();
		return cid;
	}

	virtual bool ClassSupports(const CryInterfaceID& iid) const
	{
		for (size_t i = 0; i < m_numIIDs; ++i)
		{
			if (iid == m_pIIDs[i])
				return true;
		}
		return false;
	}

	virtual void ClassSupports(const CryInterfaceID*& pIIDs, size_t& numIIDs) const
	{
		pIIDs = m_pIIDs;
		numIIDs = m_numIIDs;
	}

public:
	struct CustomDeleter
	{
		void operator()(T* p)
		{
			// Explicit call to the destructor
			p->~T();
			// Memory aligned free
			CryModuleMemalignFree(p);
		}
	};

	virtual ICryUnknownPtr CreateClassInstance() const
	{
		void* pAlignedMemory = CryModuleMemalign(sizeof(T), std::alignment_of<T>::value);
		
		return cryinterface_cast<ICryUnknown>(std::shared_ptr<T>(new(pAlignedMemory) T(), CustomDeleter()));
	}

	CFactory()
		: m_numIIDs(0)
		, m_pIIDs(0)
		, m_regFactory()
	{
		static CryInterfaceID supportedIIDs[TL::Length < typename T::FullInterfaceList > ::value];
		CW::FillIIDs<typename T::FullInterfaceList>::Op(supportedIIDs);
		m_pIIDs = &supportedIIDs[0];
		m_numIIDs = TL::Length<typename T::FullInterfaceList>::value;
		new(&m_regFactory)SRegFactoryNode(this);
	}

protected:
	CFactory(const CFactory&);
	CFactory& operator=(const CFactory&);

	size_t          m_numIIDs;
	CryInterfaceID* m_pIIDs;
	SRegFactoryNode m_regFactory;
};

template<typename T>
class CSingletonFactory : public CFactory<T>
{
public:
	CSingletonFactory()
		: CFactory<T>()
		, m_csCreateClassInstance()
	{
	}

	virtual ICryUnknownPtr CreateClassInstance() const
	{
		CryAutoLock<CryCriticalSection> lock(m_csCreateClassInstance);
		static ICryUnknownPtr p = CFactory<T>::CreateClassInstance();
		return p;
	}

	mutable CryCriticalSection m_csCreateClassInstance;
};

#define _CRYFACTORY_DECLARE(implclassname) \
  private:                                 \
    friend class CFactory<implclassname>;  \
    static CFactory<implclassname> s_factory;

#define _CRYFACTORY_DECLARE_SINGLETON(implclassname) \
  private:                                           \
    friend class CFactory<implclassname>;            \
    static CSingletonFactory<implclassname> s_factory;

#define _IMPLEMENT_ICRYUNKNOWN()                                           \
  public:                                                                  \
    virtual ICryFactory* GetFactory() const override                       \
    {                                                                      \
      return &s_factory;                                                   \
    }                                                                      \
                                                                           \
  protected:                                                               \
    virtual void* QueryInterface(const CryInterfaceID &iid) const override \
    {                                                                      \
      return CW::InterfaceCast<FullInterfaceList>::Op(this, iid);          \
    }                                                                      \
                                                                           \
    template<class TList> friend struct CW::Internal::CompositeQuery;      \
                                                                           \
    virtual void* QueryComposite(const char* name) const override          \
    {                                                                      \
      return CW::CompositeQuery::Op(*this, name);                          \
    }

#define _ENFORCE_CRYFACTORY_USAGE(implclassname, cname, cidHigh, cidLow) CRY_PP_ERROR("Deprecated macro: Use _ENFORCE_CRYFACTORY_USAGE_GUID instead. Please refer to the Migrating Guide from CRYENGINE 5.3 to CRYENGINE 5.4 for more details.")

#define _ENFORCE_CRYFACTORY_USAGE_GUID(implclassname, cname, guid)                                                  \
  public:                                                                                                           \
    static const char* GetCName() { return cname; }                                                                 \
    static constexpr CryClassID GetCID() { return guid; }                                                           \
    static std::shared_ptr<implclassname> CreateClassInstance()                                                     \
    {                                                                                                               \
      ICryUnknownPtr p = s_factory.CreateClassInstance();                                                           \
      return std::shared_ptr<implclassname>(*static_cast<std::shared_ptr<implclassname>*>(static_cast<void*>(&p))); \
    }

#define _BEFRIEND_OPS()           \
  _BEFRIEND_CRYINTERFACE_CAST()   \
  _BEFRIEND_CRYCOMPOSITE_QUERY()

#define CRYGENERATE_CLASS(implclassname, cname, cidHigh, cidLow) CRY_PP_ERROR("Deprecated macro: Use CRYGENERATE_CLASS_GUID instead. Please refer to the Migrating Guide from CRYENGINE 5.3 to CRYENGINE 5.4 for more details.")

#define CRYGENERATE_CLASS_GUID(implclassname, cname, classGuid)  \
  friend struct CFactory<implclassname>::CustomDeleter;          \
  _CRYFACTORY_DECLARE(implclassname)                             \
  _BEFRIEND_OPS()                                                \
  _IMPLEMENT_ICRYUNKNOWN()                                       \
  _ENFORCE_CRYFACTORY_USAGE_GUID(implclassname, cname, classGuid)


#define CRYGENERATE_CLASS_FROM_INTERFACE(implclassname, interfaceName, cname, cidHigh, cidLow) CRY_PP_ERROR("Deprecated macro: Use CRYGENERATE_CLASS_FROM_INTERFACE_GUID instead. Please refer to the Migrating Guide from CRYENGINE 5.3 to CRYENGINE 5.4 for more details.")

#define CRYGENERATE_CLASS_FROM_INTERFACE_GUID(implclassname, interfaceName, cname, classGuid) \
  CRYINTERFACE_SIMPLE(interfaceName)                                                           \
  CRYGENERATE_CLASS_GUID(implclassname, cname, classGuid)

#define CRYGENERATE_SINGLETONCLASS(implclassname, cname, cidHigh, cidLow) CRY_PP_ERROR("Deprecated macro: Use CRYGENERATE_SINGLETONCLASS_GUID instead. Please refer to the Migrating Guide from CRYENGINE 5.3 to CRYENGINE 5.4 for more details.")

#define CRYGENERATE_SINGLETONCLASS_GUID(implclassname, cname, classGuid) \
  friend struct CFactory<implclassname>::CustomDeleter;                  \
  _CRYFACTORY_DECLARE_SINGLETON(implclassname)                           \
  _BEFRIEND_OPS()                                                        \
  _IMPLEMENT_ICRYUNKNOWN()                                               \
  _ENFORCE_CRYFACTORY_USAGE_GUID(implclassname, cname, classGuid)

#define CRYREGISTER_CLASS(implclassname) \
  CFactory<implclassname> implclassname::s_factory;

#define CRYREGISTER_SINGLETON_CLASS(implclassname) \
  CSingletonFactory<implclassname> implclassname::s_factory;

#endif // #ifndef _CLASSWEAVER_H_
