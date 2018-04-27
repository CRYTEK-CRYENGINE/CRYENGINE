// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
// Precompiled header.
#pragma once

#include <CryCore/Platform/platform.h>


// std::unique_ptr and _smart_ptr conversions
#include <CryCore/smartptr.h>
#include <memory>

// Put these headers here, so we don't need to modify files from Cry3DEngine.
#include <CryMath/Cry_Geo.h>    // Used by MeshCompiler.cpp.
#include <CryMemory/CrySizer.h> // Used by ChunkFile.h.

#define TSmartPtr _smart_ptr


// Global lock
namespace Detail
{
extern CryCriticalSection g_lock;

inline bool AcquireGlobalLock()
{
	g_lock.Lock();
	return true;
}

inline bool TryAcquireGlobalLock()
{
	return g_lock.TryLock();
}

inline void ReleaseGlobalLock()
{
	g_lock.Unlock();
}
}

// Helper for accessing global state
class CScopedGlobalLock
{
public:
	CScopedGlobalLock()
		: m_bLocked(Detail::AcquireGlobalLock()) {}

	CScopedGlobalLock(const bool bTry)
		: m_bLocked(bTry && Detail::TryAcquireGlobalLock()) {}

	~CScopedGlobalLock()
	{
		if (m_bLocked)
		{
			Detail::ReleaseGlobalLock();
		}
	}

	CScopedGlobalLock(CScopedGlobalLock&& other)
		: m_bLocked(other.m_bLocked)
	{
		other.m_bLocked = false;
	}

	CScopedGlobalLock& operator=(CScopedGlobalLock&& other)
	{
		assert(&other != this);
		if (m_bLocked)
		{
			Detail::ReleaseGlobalLock();
		}
		m_bLocked = other.m_bLocked;
		other.m_bLocked = false;
		return *this;
	}

private:
	// no copy/assign
	CScopedGlobalLock(const CScopedGlobalLock&);
	CScopedGlobalLock& operator=(const CScopedGlobalLock&);

	// set if lock owned
	bool m_bLocked;
};

namespace Detail
{
struct DeleteUsingRelease
{
	template<typename T>
	void operator()(T* pObject) const
	{
		pObject->Release();
	}
};
}

// Helper to create std::unique_ptr that calls "Release" member instead of "delete".
// This way we can avoid _smart_ptr (which can be copied, but not moved) conflicts with std::unique_ptr (which can be moved, but not copied).
// By having a class/struct with only one of those allows us to avoid having to write copy or move constructors and/or assignment operators.
template<typename T>
inline std::unique_ptr<T, Detail::DeleteUsingRelease> MakeUniqueUsingRelease(T* pObject)
{
	return std::unique_ptr<T, Detail::DeleteUsingRelease>(pObject);
}

// Converts one "instance" of shared-ownership from _smart_ptr to an unique_ptr.
template<typename T>
inline std::unique_ptr<T, Detail::DeleteUsingRelease> MakeUniqueUsingRelease(_smart_ptr<T>&& pSmart)
{
	return MakeUniqueUsingRelease(ReleaseOwnership(pSmart));
}

// Converts an unique_ptr to a _smart_ptr (counting as one "instance" of shared-ownership)
template<typename T>
inline _smart_ptr<T> MakeSmartFromUnique(std::unique_ptr<T, Detail::DeleteUsingRelease>&& pUnique)
{
	// Note: There is no _smart_ptr API that allows us to construct without calling AddRef
	_smart_ptr<T> pSmart(pUnique.get()); // Calls AddRef()
	pUnique.reset();                     // Calls Release()
	return pSmart;
}


//struct Hash
//{
//	uint32 operator()(const string& str) const
//	{
//		return CryStringUtils::CalculateHashLowerCase(str.c_str());
//	}
//};
