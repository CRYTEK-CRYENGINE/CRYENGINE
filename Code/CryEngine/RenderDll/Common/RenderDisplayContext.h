// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "RendererResources.h"
#include "SwapChain.h"

class CRenderOutput;
class CTexture;
#if CRY_PLATFORM_DURANGO
class DXGIOutput;
#endif

//////////////////////////////////////////////////////////////////////////
//! Display Context represent a target rendering context
//////////////////////////////////////////////////////////////////////////
class CRenderDisplayContext
{
	friend class CD3D9Renderer;
	friend class CRenderOutput;
	friend class CRenderView;

public:
	using TexSmartPtr = _smart_ptr<CTexture>;
	using OutputSmartPtr = std::shared_ptr<CRenderOutput>;

protected:
	//! Debug name for this Render Output
	std::string              m_name;
	// Unique id to identify each context
	uint32                   m_uniqueId;

	// Denotes if context refers to main viewport
	bool                     m_bMainViewport = true;
	// Description of the DisplayContext
	IRenderer::SDisplayContextDescription m_desc;

	// Number of samples per output (real offscreen) pixel used in X/Y
	int                      m_nSSSamplesX = 1;
	int                      m_nSSSamplesY = 1;

	// Dimensions of viewport on output to render content into
	uint32_t                 m_DisplayWidth = 0;
	uint32_t                 m_DisplayHeight = 0;

	// Render output for this display context;
	OutputSmartPtr           m_pRenderOutput = nullptr;
	TexSmartPtr              m_pColorTarget = nullptr;
	TexSmartPtr              m_pDepthTarget = nullptr;
	float                    m_aspectRatio = 1.0f;

	SRenderViewport          m_viewport;

protected:
	CRenderDisplayContext(IRenderer::SDisplayContextDescription desc, std::string name, uint32 uniqueId) : m_name(name), m_uniqueId(uniqueId), m_desc(desc)
	{
		m_pRenderOutput = std::make_shared<CRenderOutput>(this);
	}

	CRenderDisplayContext(CRenderDisplayContext &&) = default;
	CRenderDisplayContext &operator=(CRenderDisplayContext &&) = default;

	virtual void         ReleaseResources();

	void                 SetDisplayResolutionAndRecreateTargets(uint32_t displayWidth, uint32_t displayHeight, const SRenderViewport& vp);

	virtual void         ChangeDisplayResolution(uint32_t displayWidth, uint32_t displayHeight, const SRenderViewport& vp);
	void                 ChangeDisplayResolution(uint32_t displayWidth, uint32_t displayHeight)
	{
		ChangeDisplayResolution(displayWidth, displayHeight, SRenderViewport(0, 0, displayWidth, displayHeight));
	}

public:
	virtual ~CRenderDisplayContext() noexcept {}

	virtual bool         IsSwapChainBacked() const { return false; }

	void                 BeginRendering();
	void                 EndRendering();

	virtual void         PrePresent() {}
	virtual void         PostPresent() {}
	virtual CTexture*    GetCurrentBackBuffer() const = 0;
	virtual CTexture*    GetPresentedBackBuffer() const = 0;

	std::string          GetName() const { return m_name; }
	uint32_t             GetID() const { return m_uniqueId; }

	OutputSmartPtr       GetRenderOutput() const { return m_pRenderOutput; };
	Vec2_tpl<uint32_t>   GetDisplayResolution() const { return Vec2_tpl<uint32_t>(m_DisplayWidth, m_DisplayHeight); }
	const SRenderViewport& GetViewport() const { return m_viewport; }
	float                GetAspectRatio() const { return m_aspectRatio; }

	bool                 IsMainViewport() const { return m_bMainViewport; }
	bool                 IsMainContext() const { return IsMainViewport(); }
	bool                 IsEditorDisplay() const;
	bool                 IsScalable() const { return IsMainViewport() && !IsEditorDisplay(); }
	bool                 IsDeferredShadeable() const { return IsMainViewport(); }
	bool                 IsSuperSamplingEnabled() const { return m_nSSSamplesX * m_nSSSamplesY > 1; }
	bool                 IsNativeScalingEnabled() const;
	bool                 IsHighDynamicRangeDisplay() const { return false; /* CRendererCVars::CV_r_HDRSwapChain */ }

	bool                 NeedsTempColor() const { return false; /* TODO: compare formats as well */ }
	bool                 NeedsDepthStencil() const { return (m_desc.renderFlags & (FRT_OVERLAY_DEPTH | FRT_OVERLAY_STENCIL)) != 0; }

	virtual ETEX_Format  GetColorFormat() const { return CRendererResources::GetDisplayFormat(); }
	ETEX_Format          GetDepthFormat() const { return CRendererResources::GetDepthFormat(); }

	// Get a temporary texture pointer (only valid for some short-lived scope
	CTexture* GetCurrentColorOutput() const
	{
		if (NeedsTempColor())
		{
			CRY_ASSERT(m_pColorTarget);
			return m_pColorTarget.get();
		}

		return GetCurrentBackBuffer();
	}

	CTexture* GetCurrentDepthOutput() const
	{
		CRY_ASSERT(m_pDepthTarget || !NeedsDepthStencil());
		return m_pDepthTarget.get();
	}

	// Get persistent texture pointers which are dirtied when changed
	virtual CTexture*    GetStorableColorOutput() = 0;
	inline  CTexture*    GetStorableDepthOutput() const { return GetCurrentDepthOutput(); }

private:
	void AllocateColorTarget();
	void AllocateDepthTarget();
};

using CRenderDisplayContextPtr = std::shared_ptr<CRenderDisplayContext>;

//////////////////////////////////////////////////////////////////////////
//! Display Context implemented by a swap-chain
//////////////////////////////////////////////////////////////////////////
class CSwapChainBackedRenderDisplayContext : public CRenderDisplayContext
{
	friend class CD3D9Renderer;

private:
	// Handle and a pointer to WIN32 window
	HWND                     m_hWnd = 0;

	CSwapChain               m_swapChain;
	DXGIOutput*              m_pOutput = nullptr;

	std::vector<TexSmartPtr> m_backBuffersArray;
	CTexture*                m_pBackBufferPresented = nullptr;
	TexSmartPtr              m_pBackBufferProxy = nullptr;
	bool                     m_bSwapProxy = true;
	bool                     m_fullscreen = false;
	bool                     m_bVSync = false;

private:
	void                 CreateSwapChain(HWND hWnd, bool vsync);
	void                 CreateOutput();
	void                 ShutDown();
	void                 ReleaseResources() override;
#if CRY_PLATFORM_WINDOWS
	void                 EnforceFullscreenPreemption();
#endif
	void                 ChangeOutputIfNecessary(bool isFullscreen, bool vsync = true);
	void                 ChangeDisplayResolution(uint32_t displayWidth, uint32_t displayHeight, const SRenderViewport& vp) override;
	void                 ChangeDisplayResolution(uint32_t displayWidth, uint32_t displayHeight)
	{
		ChangeDisplayResolution(displayWidth, displayHeight, SRenderViewport(0, 0, displayWidth, displayHeight));
	}

	CSwapChain&          GetSwapChain() { return m_swapChain; }
	const CSwapChain&    GetSwapChain() const { return m_swapChain; }

	template <typename... Args>
	void                 CreateSwapChain(Args&&... args) {
		m_swapChain = {};
		m_swapChain = CSwapChain::CreateSwapChain(std::forward<Args>(args)...);
	}

public:
	CSwapChainBackedRenderDisplayContext(IRenderer::SDisplayContextDescription desc, std::string name, uint32 uniqueId) : CRenderDisplayContext(desc, name, uniqueId) {}
	~CSwapChainBackedRenderDisplayContext() { ShutDown(); }

	CSwapChainBackedRenderDisplayContext(CSwapChainBackedRenderDisplayContext &&) = default;
	CSwapChainBackedRenderDisplayContext &operator=(CSwapChainBackedRenderDisplayContext &&) = default;

	bool                 IsSwapChainBacked() const override final { return true; }

	void                 PrePresent() override;
	void                 PostPresent() override;

	ETEX_Format          GetColorFormat() const override { return GetBackBufferFormat(); };
	ETEX_Format          GetBackBufferFormat() const;

	CTexture*            GetCurrentBackBuffer() const override;
	CTexture*            GetPresentedBackBuffer() const override { return m_pBackBufferPresented; }
	CTexture*            GetStorableColorOutput() override;

	HWND                 GetWindowHandle() const { return m_hWnd; }
	//! Gets the resolution of the monitor that this context's window is currently present on
	RectI                GetCurrentMonitorBounds() const;

	void                 SetFullscreenState(bool isFullscreen);
	bool                 IsFullscreen() const { return m_fullscreen; }
	bool                 GetVSyncState() const { return m_bVSync; }

	Vec2_tpl<uint32_t>   FindClosestMatchingScreenResolution(const Vec2_tpl<uint32_t> &resolution) const;

#if defined(SUPPORT_DEVICE_INFO)
	uint32               GetRefreshRateNumerator() const { return m_swapChain.GetRefreshRateNumerator(); }
	uint32               GetRefreshRateDemoninator() const { return m_swapChain.GetRefreshRateDemoninator(); }
#endif

private:
	void ReleaseBackBuffers();
	void AllocateBackBuffers();
};

//////////////////////////////////////////////////////////////////////////
//! Display Context implemented by custom buffers
//////////////////////////////////////////////////////////////////////////
class CCustomRenderDisplayContext : public CRenderDisplayContext
{
private:
	uint32_t                 m_swapChainIndex = 0;
	std::vector<TexSmartPtr> m_backBuffersArray;
	CTexture*                m_pBackBufferPresented = nullptr;
	TexSmartPtr              m_pBackBufferProxy = nullptr;
	bool                     m_bSwapProxy = true;

	void                 ShutDown();
	void                 ReleaseResources() override;

public:
	// Creates a display context with manual control of the swapchain
	CCustomRenderDisplayContext(IRenderer::SDisplayContextDescription desc, std::string name, uint32 uniqueId, std::vector<TexSmartPtr> &&backBuffersArray, uint32_t initialSwapChainIndex = 0);
	~CCustomRenderDisplayContext() { ShutDown(); }

	CCustomRenderDisplayContext(CCustomRenderDisplayContext &&) = default;
	CCustomRenderDisplayContext &operator=(CCustomRenderDisplayContext &&) = default;

	void             SetSwapChainIndex(uint32_t index);

	void             PrePresent() override;
	void             PostPresent() override;

	CTexture*        GetCurrentBackBuffer() const override { return this->m_backBuffersArray[m_swapChainIndex]; }
	CTexture*        GetPresentedBackBuffer() const override { return m_pBackBufferPresented; }
	CTexture*        GetStorableColorOutput() override;
};
