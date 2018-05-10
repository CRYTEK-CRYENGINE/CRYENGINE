// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLGIObject.hpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Declaration of the DXGL wrapper for IDXGIObject
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CRYDXGLGIOBJECT__
#define __CRYDXGLGIOBJECT__

#include "CCryDXGLBase.hpp"

class CCryDXGLGIObject : public CCryDXGLBase
{
public:
	DXGL_IMPLEMENT_INTERFACE(CCryDXGLGIObject, DXGIObject)

	CCryDXGLGIObject();
	virtual ~CCryDXGLGIObject();

	// IDXGIObject implementation
	HRESULT SetPrivateData(REFGUID Name, UINT DataSize, const void* pData);
	HRESULT SetPrivateDataInterface(REFGUID Name, const IUnknown* pUnknown);
	HRESULT GetPrivateData(REFGUID Name, UINT* pDataSize, void* pData);
	HRESULT GetParent(REFIID riid, void** ppParent);

#if !DXGL_FULL_EMULATION
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject)
	{
		if (SingleInterface<CCryDXGLGIObject>::Query(this, riid, ppvObject))
			return S_OK;
		return CCryDXGLBase::QueryInterface(riid, ppvObject);
	}
#endif //!DXGL_FULL_EMULATION

protected:
	CCryDXGLPrivateDataContainer m_kPrivateDataContainer;
};

#endif //__CRYDXGLGIOBJECT__
