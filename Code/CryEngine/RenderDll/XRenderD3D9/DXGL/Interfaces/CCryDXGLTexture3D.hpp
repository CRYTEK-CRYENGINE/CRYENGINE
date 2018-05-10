// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLTexture3D.hpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Declaration of the DXGL wrapper for ID3D11Texture3D
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CRYDXGLTEXTURE3D__
#define __CRYDXGLTEXTURE3D__

#include "CCryDXGLTextureBase.hpp"

class CCryDXGLTexture3D : public CCryDXGLTextureBase
{
public:
	DXGL_IMPLEMENT_INTERFACE(CCryDXGLTexture3D, D3D11Texture3D)

	CCryDXGLTexture3D(const D3D11_TEXTURE3D_DESC& kDesc, NCryOpenGL::STexture* pGLTexture, CCryDXGLDevice* pDevice);
#if OGL_SINGLE_CONTEXT
	CCryDXGLTexture3D(const D3D11_TEXTURE3D_DESC& kDesc, NCryOpenGL::SInitialDataCopy* pInitialData, CCryDXGLDevice* pDevice);
#endif
	virtual ~CCryDXGLTexture3D();

#if OGL_SINGLE_CONTEXT
	virtual void Initialize();
#endif

	// Implementation of ID3D11Texture3D
	void GetDesc(D3D11_TEXTURE3D_DESC* pDesc);

#if !DXGL_FULL_EMULATION
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject)
	{
		if (SingleInterface<CCryDXGLTexture3D>::Query(this, riid, ppvObject))
			return S_OK;
		return CCryDXGLTextureBase::QueryInterface(riid, ppvObject);
	}
#endif //!DXGL_FULL_EMULATION
private:
	D3D11_TEXTURE3D_DESC m_kDesc;
};

#endif //__CRYDXGLTEXTURE3D__
