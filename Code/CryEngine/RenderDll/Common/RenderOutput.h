// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <Common/Textures/TempDepthTexture.h>

class CRenderDisplayContext;

//////////////////////////////////////////////////////////////////////////
//! Render Output is a target for a Render View rendering.
class CRenderOutput
{
public:
	typedef _smart_ptr<CTexture> TexSmartPtr;

public:
	CRenderOutput() = delete;
	~CRenderOutput();

	CRenderOutput(CRenderDisplayContext* pDisplayContext);
	CRenderOutput(CTexture* pTexture, uint32 clearFlags, ColorF clearColor, float clearDepth);
	CRenderOutput(SDynTexture* pDynTexture, int32 outputWidth, int32 outputHeight, bool useTemporaryDepthBuffer, uint32 clearFlags, ColorF clearColor, float clearDepth);

	void                   BeginRendering(CRenderView* pRenderView);
	void                   EndRendering(CRenderView* pRenderView);

	//! HDR and Z Depth render target
	CTexture*              GetColorTarget() const;
	CTexture*              GetDepthTarget() const;

	//! Retrieve display context associated with this RenderOutput.
	CRenderDisplayContext* GetDisplayContext() const { return m_outputType == EOutputType::DisplayContext ? m_pDisplayContext : nullptr; };

	void                   SetViewport(const SRenderViewport& viewport);
	const SRenderViewport& GetViewport() const { return m_viewport; }

	//! Get resolution of the output target surface
	//! Note that Viewport can be smaller then this.
	Vec2i                  GetOutputResolution() const { return Vec2i(m_OutputWidth, m_OutputHeight); }
	Vec2i                  GetDisplayResolution() const; /* TODO: inline */

	void                   InitializeOutputResolution(int outputWidth, int outputHeight);
	void                   ChangeOutputResolution(int outputWidth, int outputHeight);

	void                   ReleaseResources();

	void                   InitializeDisplayContext();
	void                   ReinspectDisplayContext();

	uint32                 m_hasBeenCleared = 0;

private:
	enum class EOutputType
	{
		DisplayContext,
		Texture,
		DynTexture
	};

	EOutputType            m_outputType = EOutputType::DisplayContext;
	int32                  m_OutputWidth  = -1;
	int32                  m_OutputHeight = -1;

	union 
	{
		CRenderDisplayContext* m_pDisplayContext;
		CTexture*              m_pTexture;
		SDynTexture*           m_pDynTexture;
	};

	TexSmartPtr            m_pColorTarget = nullptr;
	TexSmartPtr            m_pDepthTarget = nullptr;

	uint32                 m_clearTargetFlag = 0;
	ColorF                 m_clearColor = {};
	float                  m_clearDepth = 0.0f;

	bool                                         m_bUseEyeColorBuffer = false;
	bool                                         m_bUseTempDepthBuffer = false;
	CResourcePool<STempDepthTexture>::value_type m_pTempDepthTexture = nullptr;

	SRenderViewport        m_viewport;

private:
	void AllocateColorTarget();
	void AllocateDepthTarget();
};
typedef std::shared_ptr<CRenderOutput> CRenderOutputPtr;
