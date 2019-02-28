// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLQuery.hpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Declaration of the DXGL wrapper for ID3D11Query
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CRYDXGLQUERY__
#define __CRYDXGLQUERY__

#include "CCryDXGLDeviceChild.hpp"

namespace NCryOpenGL
{
struct SQuery;
class CContext;
}

class CCryDXGLQuery : public CCryDXGLDeviceChild
{
public:
	DXGL_IMPLEMENT_INTERFACE(CCryDXGLQuery, D3D11Query)
#if DXGL_FULL_EMULATION
	DXGL_IMPLEMENT_INTERFACE(CCryDXGLQuery, D3D11Asynchronous)
#endif //DXGL_FULL_EMULATION

	CCryDXGLQuery(const D3D11_QUERY_DESC& kDesc, NCryOpenGL::SQuery* pGLQuery, CCryDXGLDevice* pDevice);
	virtual ~CCryDXGLQuery();

#if OGL_SINGLE_CONTEXT
	NCryOpenGL::SQuery* GetGLQuery(NCryOpenGL::CContext* pContext);
#else
	NCryOpenGL::SQuery* GetGLQuery();
#endif

	// ID3D11Asynchronous implementation
	UINT GetDataSize(void);

	// ID3D11Query implementation
	void GetDesc(D3D11_QUERY_DESC* pDesc);

#if !DXGL_FULL_EMULATION
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject)
	{
		if (SingleInterface<CCryDXGLQuery>::Query(this, riid, ppvObject))
			return S_OK;
		return CCryDXGLDeviceChild::QueryInterface(riid, ppvObject);
	}
#endif //!DXGL_FULL_EMULATION
private:
	D3D11_QUERY_DESC               m_kDesc;
	_smart_ptr<NCryOpenGL::SQuery> m_spGLQuery;
};

#endif //__CRYDXGLQUERY__
