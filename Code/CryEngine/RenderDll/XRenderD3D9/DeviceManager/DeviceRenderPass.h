// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "DeviceResourceSet.h"

////////////////////////////////////////////////////////////////////////////
// Device Render Pass

class CDeviceRenderPassDesc : NoCopy
{
	friend class CDeviceObjectFactory;

public:
	enum { MaxRendertargetCount = 4 };
	enum { MaxOutputUAVCount = 3 };

	struct SHash { uint64 operator() (const CDeviceRenderPassDesc& desc)                                  const; };
	struct SEqual { bool   operator() (const CDeviceRenderPassDesc& lhs, const CDeviceRenderPassDesc& rhs) const; };

public:
	CDeviceRenderPassDesc();
	CDeviceRenderPassDesc(void* pInvalidateCallbackOwner, const SResourceBinding::InvalidateCallbackFunction& invalidateCallback);
	CDeviceRenderPassDesc(const CDeviceRenderPassDesc& other);
	CDeviceRenderPassDesc(const CDeviceRenderPassDesc& other, void* pInvalidateCallbackOwner, const SResourceBinding::InvalidateCallbackFunction& invalidateCallback);
	~CDeviceRenderPassDesc();

	bool HasChanged() const { return m_bResourcesInvalidated; }
	void AcceptAllChanges() { m_bResourcesInvalidated = false; }

	bool SetRenderTarget(uint32 slot, CTexture* pTexture, ResourceViewHandle hView = EDefaultResourceViews::RenderTarget);
	bool SetDepthTarget(CTexture* pTexture, ResourceViewHandle hView = EDefaultResourceViews::DepthStencil);
	bool SetOutputUAV(uint32 slot, CGpuBuffer* pBuffer);
	bool SetResources(const CDeviceRenderPassDesc& other);
	bool ClearResources() threadsafe;

	bool GetDeviceRendertargetViews(std::array<D3DSurface*, MaxRendertargetCount>& views, int& viewCount) const;
	bool GetDeviceDepthstencilView(D3DDepthSurface*& pView) const;

	const std::array<SResourceBinding, MaxRendertargetCount>& GetRenderTargets()           const { return m_renderTargets; }
	const            SResourceBinding&                        GetDepthTarget()             const { return m_depthTarget; }
	const std::array<SResourceBinding, MaxOutputUAVCount>&    GetOutputUAVs()              const { return m_outputUAVs; }

	static bool OnResourceInvalidated(void* pThis, SResourceBindPoint bindPoint, UResourceReference pResource, uint32 flags) threadsafe;

protected:
	bool UpdateResource(SResourceBindPoint bindPoint, SResourceBinding& dstResource, const SResourceBinding& srcResource);

	std::array<SResourceBinding, MaxRendertargetCount> m_renderTargets;
	std::array<SResourceBinding, MaxOutputUAVCount>    m_outputUAVs;
	SResourceBinding                                   m_depthTarget;

	SResourceBinding::InvalidateCallbackFunction       m_invalidateCallback;
	void*                                              m_invalidateCallbackOwner;

	std::atomic<bool>                                  m_bResourcesInvalidated;
};

class CDeviceRenderPass_Base : public NoCopy
{
	friend class CDeviceObjectFactory;

public:
	CDeviceRenderPass_Base();
	virtual ~CDeviceRenderPass_Base() {};

	bool         IsValid() const { return m_bValid; }
	void         Invalidate() { m_bValid = false; }
	uint64       GetHash() const { return m_nHash; }

	bool         Update(const CDeviceRenderPassDesc& passDesc);
	static bool  UpdateWithReevaluation(CDeviceRenderPassPtr& pRenderPass, CDeviceRenderPassDesc& passDesc);

private:
	virtual bool UpdateImpl(const CDeviceRenderPassDesc& passDesc) = 0;

protected:
	uint64                 m_nHash;
	uint32                 m_nUpdateCount;
	bool                   m_bValid;

#if !defined(RELEASE)
	std::array<DXGI_FORMAT, CDeviceRenderPassDesc::MaxRendertargetCount + 1>  m_targetFormats;
#endif
};

