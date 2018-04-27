// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "OpticsElement.h"

class CTexture;
class CD3D9Renderer;

class ImageSpaceShafts : public COpticsElement
{
	struct SShaderParams : COpticsElement::SShaderParamsBase
	{
		Vec4 screenSize;
		Vec4 meshCenterAndBrt;
		Vec4 color;
		Vec4 sceneDepth;
	};

protected:
	static CTexture* m_pOccBuffer;
	static CTexture* m_pDraftBuffer;

protected:
	bool                 m_bHighQualityMode;
	bool                 m_bTexDirty;
	_smart_ptr<CTexture> m_pGoboTex;

	CRenderPrimitive     m_occlusionPrimitive;
	CRenderPrimitive     m_shaftGenPrimitive;
	CRenderPrimitive     m_blendPrimitive;

	CPrimitiveRenderPass m_occlusionPass;
	CPrimitiveRenderPass m_shaftGenPass;

	virtual void InitTextures();

public:

	ImageSpaceShafts(const char* name);

	EFlareType GetType() { return eFT_ImageSpaceShafts; }

	void       InitEditorParamGroups(DynArray<FuncVariableGroup>& groups);
	bool       PrepareOcclusion(CTexture* pDestRT, CTexture* pGoboTex, SamplerStateHandle samplerState);
	bool       PrepareShaftGen(CTexture* pDestRT, CTexture* pOcclTex, SamplerStateHandle samplerState);
	bool       PreparePrimitives(const SPreparePrimitivesContext& context);
	void       Load(IXmlNode* pNode);

	bool       IsHighQualityMode() const  { return m_bHighQualityMode; }
	void       SetHighQualityMode(bool b) { m_bHighQualityMode = b; m_bTexDirty = true; }

	CTexture*  GetGoboTex() const         { return m_pGoboTex; }
	void       SetGoboTex(CTexture* tex)  { m_pGoboTex = tex; }
};
