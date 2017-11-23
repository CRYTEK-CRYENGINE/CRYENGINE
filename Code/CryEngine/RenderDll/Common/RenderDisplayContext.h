// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

class CRenderOutput;

//////////////////////////////////////////////////////////////////////////
//! Display Context represent a target rendering window with a swap chain.
//////////////////////////////////////////////////////////////////////////
class CRenderDisplayContext
{
public:
	typedef      _smart_ptr<CTexture     > TexSmartPtr;
	typedef std::shared_ptr<CRenderOutput> OutputSmartPtr;

	//! Debug name for this Display Context
	string                  m_name;
	// Unique id to identify each context
	uint32                  m_uniqueId = 0;
	// Handle and a pointer to WIN32 window
	CryDisplayContextHandle m_hWnd = 0;

	// Swap-chain and it's back-buffers
	DXGISwapChain*           m_pSwapChain = nullptr;
	std::vector<TexSmartPtr> m_backBuffersArray;
	CTexture*                m_pBackBufferPresented = nullptr;
	TexSmartPtr              m_pBackBufferProxy = nullptr;
	bool                     m_bSwapProxy = true;
	//	unsigned int m_nCurrentBackBufferIndex;

	//	std::vector<_smart_ptr<D3DSurface> > m_pBackBuffers;
	// Currently active back-buffer
	//	D3DSurface*  m_pBackBuffer;

	// Number of samples per output (real offscreen) pixel used in X/Y
	int m_nSSSamplesX = 1;
	int m_nSSSamplesY = 1;

	// Denotes if context refers to main viewport
	bool m_bMainViewport = true;

	// Denotes if input is HDR content
	bool m_bHDRRendering = false;

	// Description of the DisplayContext
	IRenderer::SDisplayContextDescription m_desc;

public:
	CRenderDisplayContext();
	~CRenderDisplayContext();

	bool                     IsValid() const { return m_pSwapChain != nullptr; }
	bool                     IsEditorDisplay() const { return (m_uniqueId != 0) || gRenDev->IsEditorMode(); }
	bool                     IsScalable() const { return m_bMainViewport && !IsEditorDisplay(); }
	bool                     IsHighDynamicRange() const { return m_bHDRRendering; /* SHDF_ALLOWHDR */ }
	bool                     IsDeferredShadeable() const { return m_bMainViewport; }
	bool                     IsSuperSamplingEnabled() const { return (m_nSSSamplesX * m_nSSSamplesY) > 1; }
	bool                     IsNativeScalingEnabled() const { return m_pRenderOutput ? GetDisplayResolution() != m_pRenderOutput->GetOutputResolution() : false; }

	bool                     NeedsDepthStencil() const { return (m_desc.renderFlags & (FRT_OVERLAY_DEPTH | FRT_OVERLAY_STENCIL)) != 0; }

	void                     SetHWND(CryDisplayContextHandle hWnd);
	CryDisplayContextHandle  GetHandle() const { return m_hWnd; }

	OutputSmartPtr           GetRenderOutput() const { return m_pRenderOutput; };
	void                     ToggleHDRRendering(bool bHDRRendering);

	CTexture*                GetCurrentBackBuffer();
	CTexture*                GetPresentedBackBuffer();
	CTexture*                GetCurrentColorOutput();
	CTexture*                GetCurrentDepthOutput();
	CTexture*                GetStorableColorOutput();
	CTexture*                GetStorableDepthOutput();

	void                     SetViewport(const SRenderViewport& vp);
	const SRenderViewport&   GetViewport() const { return m_viewport; }

	Vec2i                    GetDisplayResolution() const { return Vec2i(m_DisplayWidth, m_DisplayHeight); }

	void                     InitializeDisplayResolution(int displayWidth, int displayHeight);
	void                     ChangeDisplayResolution(int displayWidth, int displayHeight, bool bResizeTarget = false, bool bForce = false);

	void                     ShutDown();
	void                     AssignSwapChain(DXGISwapChain* pSwapChain);

	float                    GetAspectRatio() const { return m_aspectRatio; }
	const DXGI_SURFACE_DESC& GetSwapChainDesc() const { return m_swapChainDesc; }

	void                     PrePresent();
	void                     PostPresent();

	void                     SetLastCamera(CCamera::EEye eye, const CCamera& camera);
	const CCamera&           GetLastCamera(CCamera::EEye eye) const { return m_lastRenderCamera[eye]; }

private:
	// Dimensions of viewport on output to render content into
	int                      m_DisplayWidth = 0;
	int                      m_DisplayHeight = 0;

	// Render output for this display context;
	OutputSmartPtr           m_pRenderOutput = nullptr;
	TexSmartPtr              m_pColorTarget = nullptr;
	TexSmartPtr              m_pDepthTarget = nullptr;

	SRenderViewport          m_viewport;

	DXGI_SURFACE_DESC        m_swapChainDesc;      // Surface description of the BackBuffer
	float                    m_aspectRatio = 1.0f;

	//! Camera last used in main Rendering to this DisplayContext.
	CCamera                  m_lastRenderCamera[CCamera::eEye_eCount];

private:
	void ReleaseResources();
	void ReleaseBackBuffers();
	void ResizeSwapChain(bool bResizeTarget = false);

	void AllocateSwapChain();
	void AllocateBackBuffers();
	void AllocateColorTarget();
	void AllocateDepthTarget();

	bool ReadSwapChainSurfaceDesc();
};
typedef std::shared_ptr<CRenderDisplayContext> CRenderDisplayContextPtr;
