// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLUnorderedAccessView.hpp
//  Version:     v1.00
//  Created:     18/06/2013 by Valerio Guagliumi.
//  Description: Declaration of the DXGL wrapper for ID3D11UnorderedAccessView
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CRYDXGLUNORDEREDACCESSVIEW__
#define __CRYDXGLUNORDEREDACCESSVIEW__

#include "CCryDXGLView.hpp"

namespace NCryOpenGL
{
struct SShaderView;
class CContext;
}

class CCryDXGLUnorderedAccessView : public CCryDXGLView
{
public:
	DXGL_IMPLEMENT_INTERFACE(CCryDXGLUnorderedAccessView, D3D11UnorderedAccessView)

	CCryDXGLUnorderedAccessView(CCryDXGLResource* pResource, const D3D11_UNORDERED_ACCESS_VIEW_DESC& kDesc, CCryDXGLDevice* pDevice);
	virtual ~CCryDXGLUnorderedAccessView();

	bool Initialize(NCryOpenGL::CContext* pContext);

#if OGL_SINGLE_CONTEXT
	NCryOpenGL::SShaderView* GetGLView(NCryOpenGL::CContext* pContext);
#else
	NCryOpenGL::SShaderView* GetGLView();
#endif

	// Implementation of ID3D11UnorderedAccessView
	void GetDesc(D3D11_UNORDERED_ACCESS_VIEW_DESC* pDesc);
protected:
	D3D11_UNORDERED_ACCESS_VIEW_DESC    m_kDesc;
	_smart_ptr<NCryOpenGL::SShaderView> m_spGLView;
};

#endif //__CRYDXGLUNORDEREDACCESSVIEW__
