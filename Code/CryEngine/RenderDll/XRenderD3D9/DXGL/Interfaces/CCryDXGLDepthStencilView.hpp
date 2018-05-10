// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLDepthStencilView.hpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Declaration of the DXGL wrapper for ID3D11DepthStencilView
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CRYDXGLDEPTHSTENCILVIEW__
#define __CRYDXGLDEPTHSTENCILVIEW__

#include "CCryDXGLView.hpp"

namespace NCryOpenGL
{
struct SOutputMergerView;
class CContext;
}

class CCryDXGLDepthStencilView : public CCryDXGLView
{
public:
	DXGL_IMPLEMENT_INTERFACE(CCryDXGLDepthStencilView, D3D11DepthStencilView)

	CCryDXGLDepthStencilView(CCryDXGLResource* pResource, const D3D11_DEPTH_STENCIL_VIEW_DESC& kDesc, CCryDXGLDevice* pDevice);
	virtual ~CCryDXGLDepthStencilView();

	bool Initialize(NCryOpenGL::CContext* pContext);

#if OGL_SINGLE_CONTEXT
	NCryOpenGL::SOutputMergerView* GetGLView(NCryOpenGL::CContext* pContext);
#else
	NCryOpenGL::SOutputMergerView* GetGLView();
#endif

	// Implementation of ID3D11DepthStencilView
	void GetDesc(D3D11_DEPTH_STENCIL_VIEW_DESC* pDesc);
protected:
	D3D11_DEPTH_STENCIL_VIEW_DESC             m_kDesc;
	_smart_ptr<NCryOpenGL::SOutputMergerView> m_spGLView;
};

#endif //__CRYDXGLDEPTHSTENCILVIEW__
