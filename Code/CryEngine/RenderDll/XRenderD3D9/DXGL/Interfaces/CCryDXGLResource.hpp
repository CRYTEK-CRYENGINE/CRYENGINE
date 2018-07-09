// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLResource.hpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Declaration of the DXGL wrapper for ID3D11Resource
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CRYDXGLRESOURCE__
#define __CRYDXGLRESOURCE__

#include "CCryDXGLDeviceChild.hpp"

namespace NCryOpenGL
{
class CDevice;
struct SResource;
struct SInitialDataCopy;
};

class CCryDXGLResource : public CCryDXGLDeviceChild
{
public:
	DXGL_IMPLEMENT_INTERFACE(CCryDXGLResource, D3D11Resource)

	virtual ~CCryDXGLResource();

#if OGL_SINGLE_CONTEXT
	virtual void Initialize() = 0;
#endif

	ILINE NCryOpenGL::SResource* GetGLResource()
	{
#if OGL_SINGLE_CONTEXT
		IF_UNLIKELY (!m_spGLResource)
			Initialize();
#endif
		return m_spGLResource;
	}

	// Implementation of ID3D11Resource
	void GetType(D3D11_RESOURCE_DIMENSION* pResourceDimension);
	void SetEvictionPriority(UINT EvictionPriority);
	UINT GetEvictionPriority(void);

#if !DXGL_FULL_EMULATION
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject)
	{
		if (SingleInterface<CCryDXGLResource>::Query(this, riid, ppvObject))
			return S_OK;
		return CCryDXGLDeviceChild::QueryInterface(riid, ppvObject);
	}
#endif //!DXGL_FULL_EMULATION
protected:
	CCryDXGLResource(D3D11_RESOURCE_DIMENSION eDimension, NCryOpenGL::SResource* pResource, CCryDXGLDevice* pDevice);
#if OGL_SINGLE_CONTEXT
	CCryDXGLResource(D3D11_RESOURCE_DIMENSION eDimension, NCryOpenGL::SInitialDataCopy* pInitialDataCopy, CCryDXGLDevice* pDevice);
#endif
protected:
	_smart_ptr<NCryOpenGL::SResource>        m_spGLResource;
#if OGL_SINGLE_CONTEXT
	_smart_ptr<NCryOpenGL::SInitialDataCopy> m_spInitialDataCopy;
#endif
	D3D11_RESOURCE_DIMENSION                 m_eDimension;
};

#endif //__CRYDXGLRESOURCE__
