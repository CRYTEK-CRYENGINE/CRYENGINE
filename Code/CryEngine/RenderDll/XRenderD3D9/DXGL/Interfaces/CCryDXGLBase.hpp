// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLBase.hpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Declaration of the reference counted base class for all
//               DXGL interface implementations
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CRYDXGLBASE__
#define __CRYDXGLBASE__

#include "../Definitions/CryDXGLGuid.hpp"
#include "../Definitions/ICryDXGLUnknown.hpp"

template<typename Interface>
struct SingleInterface
{
	template<typename Object>
	static bool Query(Object* pThis, REFIID riid, void** ppvObject)
	{
		if (riid == __uuidof(Interface))
		{
			*reinterpret_cast<Interface**>(ppvObject) = static_cast<Interface*>(pThis);
			static_cast<Interface*>(pThis)->AddRef();
			return true;
		}
		return false;
	}
};

#include "DXEmulation.hpp"

////////////////////////////////////////////////////////////////////////////
// Definition of basic types
////////////////////////////////////////////////////////////////////////////

class CCryDXGLBase
#if !DXGL_FULL_EMULATION
	: public IUnknown
#endif //!DXGL_FULL_EMULATION
{
public:
	DXGL_IMPLEMENT_INTERFACE(CCryDXGLBase, Unknown)

	CCryDXGLBase();
	virtual ~CCryDXGLBase();

#if DXGL_FULL_EMULATION
	SAggregateNode&           GetAggregateHead();
#else
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);
#endif //!DXGL_FULL_EMULATION
	ULONG STDMETHODCALLTYPE   AddRef();
	ULONG STDMETHODCALLTYPE   Release();
protected:
	uint32         m_uRefCount;
#if DXGL_FULL_EMULATION
	SAggregateNode m_kAggregateHead;
#endif //DXGL_FULL_EMULATION
};

class CCryDXGLPrivateDataContainer
{
public:
	CCryDXGLPrivateDataContainer();
	~CCryDXGLPrivateDataContainer();

	HRESULT GetPrivateData(REFGUID guid, UINT* pDataSize, void* pData);
	HRESULT SetPrivateData(REFGUID guid, UINT DataSize, const void* pData);
	HRESULT SetPrivateDataInterface(REFGUID guid, const IUnknown* pData);
protected:
	void    RemovePrivateData(REFGUID guid);
protected:
	struct SPrivateData;

	struct SGuidHashCompare
	{
		size_t operator()(const GUID& kGuid) const;
		bool   operator()(const GUID& kLeft, const GUID& kRight) const;
	};

	typedef std::unordered_map<GUID, SPrivateData*, SGuidHashCompare, SGuidHashCompare> TPrivateDataMap;
	TPrivateDataMap m_kPrivateDataMap;
};

#endif //__CRYDXGLBASE__
