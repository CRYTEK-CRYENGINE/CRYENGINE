// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLDeviceChild.hpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Declaration of the DXGL wrapper for ID3D11DeviceChild
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CRYDXGLDEVICECHILD__
#define __CRYDXGLDEVICECHILD__

#include "CCryDXGLBase.hpp"

class CCryDXGLDevice;

class CCryDXGLDeviceChild : public CCryDXGLBase
{
public:
	DXGL_IMPLEMENT_INTERFACE(CCryDXGLDeviceChild, D3D11DeviceChild)

	CCryDXGLDeviceChild(CCryDXGLDevice* pDevice = NULL);
	virtual ~CCryDXGLDeviceChild();

	void SetDevice(CCryDXGLDevice* pDevice);

	// ID3D11DeviceChild implementation
	void STDMETHODCALLTYPE    GetDevice(ID3D11Device** ppDevice);
	HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID guid, UINT* pDataSize, void* pData);
	HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID guid, UINT DataSize, const void* pData);
	HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID guid, const IUnknown* pData);

#if !DXGL_FULL_EMULATION
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject)
	{
		if (SingleInterface<CCryDXGLDeviceChild>::Query(this, riid, ppvObject))
			return S_OK;
		return CCryDXGLBase::QueryInterface(riid, ppvObject);
	}
#endif //!DXGL_FULL_EMULATION
protected:
	CCryDXGLDevice*              m_pDevice;
	CCryDXGLPrivateDataContainer m_kPrivateDataContainer;
};

#if !DXGL_FULL_EMULATION
struct ID3D11Counter : CCryDXGLDeviceChild {};
struct ID3D11ClassLinkage : CCryDXGLDeviceChild {};
#endif //!DXGL_FULL_EMULATION

#endif //__CRYDXGLDEVICECHILD__
