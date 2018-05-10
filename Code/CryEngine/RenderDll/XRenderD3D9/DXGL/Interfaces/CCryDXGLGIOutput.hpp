// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLGIOutput.hpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Declaration of the DXGL wrapper for IDXGIOutput
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CRYDXGLGIOUTPUT__
#define __CRYDXGLGIOUTPUT__

#include "CCryDXGLGIObject.hpp"

namespace NCryOpenGL
{
struct SOutput;
}

class CCryDXGLGIOutput : public CCryDXGLGIObject
{
public:
	DXGL_IMPLEMENT_INTERFACE(CCryDXGLGIOutput, DXGIOutput)

	CCryDXGLGIOutput(NCryOpenGL::SOutput* pGLOutput);
	virtual ~CCryDXGLGIOutput();

	bool                 Initialize();
	NCryOpenGL::SOutput* GetGLOutput();

	// IDXGIOutput implementation
	HRESULT GetDesc(DXGI_OUTPUT_DESC* pDesc);
	HRESULT GetDisplayModeList(DXGI_FORMAT EnumFormat, UINT Flags, UINT* pNumModes, DXGI_MODE_DESC* pDesc);
	HRESULT FindClosestMatchingMode(const DXGI_MODE_DESC* pModeToMatch, DXGI_MODE_DESC* pClosestMatch, IUnknown* pConcernedDevice);
	HRESULT WaitForVBlank(void);
	HRESULT TakeOwnership(IUnknown* pDevice, BOOL Exclusive);
	void    ReleaseOwnership(void);
	HRESULT GetGammaControlCapabilities(DXGI_GAMMA_CONTROL_CAPABILITIES* pGammaCaps);
	HRESULT SetGammaControl(const DXGI_GAMMA_CONTROL* pArray);
	HRESULT GetGammaControl(DXGI_GAMMA_CONTROL* pArray);
	HRESULT SetDisplaySurface(IDXGISurface* pScanoutSurface);
	HRESULT GetDisplaySurfaceData(IDXGISurface* pDestination);
	HRESULT GetFrameStatistics(DXGI_FRAME_STATISTICS* pStats);

protected:
	_smart_ptr<NCryOpenGL::SOutput> m_spGLOutput;
	std::vector<DXGI_MODE_DESC>     m_kDisplayModes;
	DXGI_OUTPUT_DESC                m_kDesc;
};

#endif //__CRYDXGLGIOUTPUT__
