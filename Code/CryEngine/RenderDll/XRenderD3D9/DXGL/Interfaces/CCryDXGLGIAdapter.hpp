// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLGIAdapter.hpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Declaration of the DXGL wrapper for IDXGIAdapter
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CRYDXGLGIADAPTER__
#define __CRYDXGLGIADAPTER__

#include "CCryDXGLGIObject.hpp"

namespace NCryOpenGL
{
struct SAdapter;
}

class CCryDXGLGIFactory;
class CCryDXGLGIOutput;

class CCryDXGLGIAdapter : public CCryDXGLGIObject
{
public:
#if DXGL_FULL_EMULATION
	DXGL_IMPLEMENT_INTERFACE(CCryDXGLGIAdapter, DXGIAdapter)
#endif //DXGL_FULL_EMULATION
	DXGL_IMPLEMENT_INTERFACE(CCryDXGLGIAdapter, DXGIAdapter1)

	CCryDXGLGIAdapter(CCryDXGLGIFactory* pFactory, NCryOpenGL::SAdapter* pGLAdapter);
	virtual ~CCryDXGLGIAdapter();

	bool                  Initialize();
	D3D_FEATURE_LEVEL     GetSupportedFeatureLevel();
	NCryOpenGL::SAdapter* GetGLAdapter();

	// IDXGIObject overrides
	HRESULT GetParent(REFIID riid, void** ppParent);

	// IDXGIAdapter implementation
	HRESULT EnumOutputs(UINT Output, IDXGIOutput** ppOutput);
	HRESULT GetDesc(DXGI_ADAPTER_DESC* pDesc);
	HRESULT CheckInterfaceSupport(REFGUID InterfaceName, LARGE_INTEGER* pUMDVersion);

	// IDXGIAdapter1 implementation
	HRESULT GetDesc1(DXGI_ADAPTER_DESC1* pDesc);
protected:
	std::vector<_smart_ptr<CCryDXGLGIOutput>> m_kOutputs;
	_smart_ptr<CCryDXGLGIFactory>             m_spFactory;

	_smart_ptr<NCryOpenGL::SAdapter>          m_spGLAdapter;
	DXGI_ADAPTER_DESC                         m_kDesc;
	DXGI_ADAPTER_DESC1                        m_kDesc1;
	D3D_FEATURE_LEVEL                         m_eSupportedFeatureLevel;
};

#endif //__CRYDXGLGIADAPTER__
