// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CryTypeID.h"

struct ICryFactory;
struct ICryUnknown;

namespace InterfaceCastSemantics
{
#if !defined(SWIG)
template<class T>
struct has_cryiidof
{
	typedef char(&yes)[1];
	typedef char(&no)[2];

	template <typename C> static yes check(decltype(&C::IID));
	template <typename> static no check(...);

	static constexpr bool value = sizeof(check<T>(0)) == sizeof(yes);
};
#endif

template<class T>
const CryInterfaceID& cryiidof()
{
	return T::IID();
}

#if !defined(SWIG)
#define _BEFRIEND_CRYIIDOF() \
  template<class T> friend const CryInterfaceID &InterfaceCastSemantics::cryiidof(); \
  template<class T> friend struct InterfaceCastSemantics::has_cryiidof;
#else
#define _BEFRIEND_CRYIIDOF() \
  template<class T> friend const CryInterfaceID &InterfaceCastSemantics::cryiidof();
#endif

template<class Dst, class Src>
Dst* cryinterface_cast(Src* p)
{
	return static_cast<Dst*>(p ? p->QueryInterface(cryiidof<Dst>()) : nullptr);
}

template<class Dst, class Src>
Dst* cryinterface_cast(const Src* p)
{
	return static_cast<const Dst*>(p ? p->QueryInterface(cryiidof<Dst>()) : nullptr);
}

namespace Internal
{
template<class Dst, class Src>
struct cryinterface_cast_helper
{
	static std::shared_ptr<Dst> Op(const std::shared_ptr<Src>& p)
	{
		Dst* dp = cryinterface_cast<Dst>(p.get());
		return dp ? std::shared_ptr<Dst>(p, dp) : std::shared_ptr<Dst>();
	}
};

template<class Src>
struct cryinterface_cast_helper<ICryUnknown, Src>
{
	static std::shared_ptr<ICryUnknown> Op(const std::shared_ptr<Src>& p)
	{
		ICryUnknown* dp = cryinterface_cast<ICryUnknown>(p.get());
		return dp ? std::shared_ptr<ICryUnknown>(*((const std::shared_ptr<ICryUnknown>*)& p), dp) : std::shared_ptr<ICryUnknown>();
	}
};

template<class Src>
struct cryinterface_cast_helper<const ICryUnknown, Src>
{
	static std::shared_ptr<const ICryUnknown> Op(const std::shared_ptr<Src>& p)
	{
		const ICryUnknown* dp = cryinterface_cast<const ICryUnknown>(p.get());
		return dp ? std::shared_ptr<const ICryUnknown>(*((const std::shared_ptr<const ICryUnknown>*)& p), dp) : std::shared_ptr<const ICryUnknown>();
	}
};
}

template<class Dst, class Src>
std::shared_ptr<Dst> cryinterface_cast(const std::shared_ptr<Src>& p)
{
	return Internal::cryinterface_cast_helper<Dst, Src>::Op(p);
}

#define _BEFRIEND_CRYINTERFACE_CAST()                                                                \
  template<class Dst, class Src> friend Dst * InterfaceCastSemantics::cryinterface_cast(Src*);       \
  template<class Dst, class Src> friend Dst * InterfaceCastSemantics::cryinterface_cast(const Src*); \
  template<class Dst, class Src> friend std::shared_ptr<Dst> InterfaceCastSemantics::cryinterface_cast(const std::shared_ptr<Src> &);

}

using InterfaceCastSemantics::cryiidof;
using InterfaceCastSemantics::cryinterface_cast;

template<class S, class T>
bool CryIsSameClassInstance(S* p0, T* p1)
{
	return static_cast<const void*>(p0) == static_cast<const void*>(p1) || cryinterface_cast<const ICryUnknown>(p0) == cryinterface_cast<const ICryUnknown>(p1);
}

template<class S, class T>
bool CryIsSameClassInstance(const std::shared_ptr<S>& p0, T* p1)
{
	return CryIsSameClassInstance(p0.get(), p1);
}

template<class S, class T>
bool CryIsSameClassInstance(S* p0, const std::shared_ptr<T>& p1)
{
	return CryIsSameClassInstance(p0, p1.get());
}

template<class S, class T>
bool CryIsSameClassInstance(const std::shared_ptr<S>& p0, const std::shared_ptr<T>& p1)
{
	return CryIsSameClassInstance(p0.get(), p1.get());
}

namespace CompositeQuerySemantics
{

template<class Src>
std::shared_ptr<ICryUnknown> crycomposite_query(Src* p, const char* name, bool* pExposed = nullptr)
{
	void* pComposite = p ? p->QueryComposite(name) : nullptr;
	if(pExposed) *pExposed = pComposite != nullptr;
	return pComposite ? *static_cast<std::shared_ptr<ICryUnknown>*>(pComposite) : std::shared_ptr<ICryUnknown>();
}

template<class Src>
std::shared_ptr<const ICryUnknown> crycomposite_query(const Src* p, const char* name, bool* pExposed = nullptr)
{
	void* pComposite = p ? p->QueryComposite(name) : nullptr;
	if (pExposed) *pExposed = pComposite != nullptr;
	return pComposite ? *static_cast<std::shared_ptr<const ICryUnknown>*>(pComposite) : std::shared_ptr<const ICryUnknown>();
}

template<class Src>
std::shared_ptr<ICryUnknown> crycomposite_query(const std::shared_ptr<Src>& p, const char* name, bool* pExposed = nullptr)
{
	return crycomposite_query(p.get(), name, pExposed);
}

template<class Src>
std::shared_ptr<const ICryUnknown> crycomposite_query(const std::shared_ptr<const Src>& p, const char* name, bool* pExposed = nullptr)
{
	return crycomposite_query(p.get(), name, pExposed);
}
}

#define _BEFRIEND_CRYCOMPOSITE_QUERY()                                                                                                                   \
  template<class Src> friend std::shared_ptr<ICryUnknown> CompositeQuerySemantics::crycomposite_query(Src*, const char*, bool*);                         \
  template<class Src> friend std::shared_ptr<const ICryUnknown> CompositeQuerySemantics::crycomposite_query(const Src*, const char*, bool*);             \
  template<class Src> friend std::shared_ptr<ICryUnknown> CompositeQuerySemantics::crycomposite_query(const std::shared_ptr<Src> &, const char*, bool*); \
  template<class Src> friend std::shared_ptr<const ICryUnknown> CompositeQuerySemantics::crycomposite_query(const std::shared_ptr<const Src> &, const char*, bool*);

using CompositeQuerySemantics::crycomposite_query;

#define CRYINTERFACE_DECLARE(iname, iidHigh, iidLow) CRY_PP_ERROR("Deprecated macro: Use CRYINTERFACE_DECLARE_GUID instead. Please refer to the Migration Guide from CRYENGINE 5.3 to CRYENGINE 5.4 for more details.")

#define CRYINTERFACE_DECLARE_GUID(iname, guid)                                                \
  _BEFRIEND_CRYIIDOF()                                                                        \
  friend struct std::default_delete<iname>;                                                   \
private:                                                                                      \
  static const CryInterfaceID& IID() { static constexpr CryGUID sguid = guid; return sguid; } \
public:

struct ICryUnknown
{
	CRYINTERFACE_DECLARE_GUID(ICryUnknown, "10000000-1000-1000-1000-100000000000"_cry_guid);

	_BEFRIEND_CRYINTERFACE_CAST()
	_BEFRIEND_CRYCOMPOSITE_QUERY()

	virtual ICryFactory* GetFactory() const = 0;

protected:
	// Prevent explicit destruction from client side (exception is std::checked_delete which gets befriended).
	virtual ~ICryUnknown() = default;

	virtual void* QueryInterface(const CryInterfaceID& iid) const = 0;
	virtual void* QueryComposite(const char* name) const = 0;
};

typedef std::shared_ptr<ICryUnknown> ICryUnknownPtr;
typedef std::shared_ptr<const ICryUnknown> ICryUnknownConstPtr;
