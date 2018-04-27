// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "OpticsElement.h"
#include "AbstractMeshElement.h"

class CTexture;
class CameraOrbs : public COpticsElement, public AbstractMeshElement
{
	struct SShaderParams : SShaderParamsBase
	{
		Vec4 lightColorInfo;
		Vec4 lensDetailParams;
		Vec4 lensDetailParams2;
		Vec4 ambientDiffuseRGBK;
		Vec4 advShadingParams;
	};

private:
	_smart_ptr<CTexture>     m_pOrbTex;
	_smart_ptr<CTexture>     m_pLensTex;

	bool                     m_bUseLensTex        : 1;
	bool                     m_bOrbDetailShading  : 1;
	bool                     m_bLensDetailShading : 1;

	float                    m_fLensTexStrength;
	float                    m_fLensDetailShadingStrength;
	float                    m_fLensDetailBumpiness;

	bool                     m_bAdvancedShading : 1;
	ColorF                   m_cAmbientDiffuse;
	float                    m_fAbsorptance;
	float                    m_fTransparency;
	float                    m_fScatteringStrength;

	int                      m_nNumOrbs;
	float                    m_fIllumRadius;
	float                    m_fRotation;

	unsigned int             m_iNoiseSeed;
	float                    m_fSizeNoise;
	float                    m_fBrightnessNoise;
	float                    m_fRotNoise;
	float                    m_fClrNoise;

	std::vector<SpritePoint> m_OrbsList;
	CRenderPrimitive         m_GlowPrimitive;
	CRenderPrimitive         m_CameraLensPrimitive;
	float                    m_spriteAspectRatio;

	static const int         MAX_ORBS_NUMBER = 10000;

protected:

#if defined(FLARES_SUPPORT_EDITING)
	void InitEditorParamGroups(DynArray<FuncVariableGroup>& groups);
#endif
	void GenMesh()    override;
	void Invalidate() override  { m_meshDirty = true; }

public:
	CameraOrbs(const char* name, const int numOrbs = 100);

protected:
	void ApplyOrbFlags(uint64& rtFlags, bool detailShading) const;
	void ApplyAdvancedShadingFlag(uint64& rtFlags) const;
	void ApplyAdvancedShadingParams(SShaderParams& shaderParams, CRenderPrimitive& primitive, const ColorF& ambDiffuseRGBK, float absorptance, float transparency, float scattering) const;

public:

	void ScatterOrbs();

	int  GetNumOrbs() const { return m_OrbsList.size(); }
	void SetNumOrbs(int n)
	{
		n = clamp_tpl<int>(n, 0, MAX_ORBS_NUMBER);
		if (n != m_nNumOrbs)
		{
			m_nNumOrbs = n;
			if (m_nNumOrbs > 0)
				m_OrbsList.resize(m_nNumOrbs);
			m_meshDirty = true;
		}
	}

	EFlareType GetType() override                 { return eFT_CameraOrbs; }
	bool       PreparePrimitives(const SPreparePrimitivesContext& context) override;
	void       Load(IXmlNode* pNode) override;

	CTexture*  GetOrbTex();
	CTexture*  GetLensTex();
	void       SetOrbTex(CTexture* tex)           { m_pOrbTex = tex; }
	void       SetLensTex(CTexture* tex)          { m_pLensTex = tex; }
	void       SetUseLensTex(bool b)              { m_bUseLensTex = b; }
	bool       GetUseLensTex()                    { return m_bUseLensTex; }
	void       SetEnableOrbDetailShading(bool b)  { m_bOrbDetailShading = b; }
	bool       GetEnableOrbDetailShading()        { return m_bOrbDetailShading; }
	void       SetEnableLensDetailShading(bool b) { m_bLensDetailShading = b; }
	bool       GetEnableLensDetailShading()       { return m_bLensDetailShading; }

	void       SetSize(float s)
	{
		COpticsElement::SetSize(s);
		m_meshDirty = true;
	}

	float GetLensTexStrength() const                   { return m_fLensTexStrength; }
	void  SetLensTexStrength(float strength)           { m_fLensTexStrength = strength; }

	float GetLensDetailShadingStrength() const         { return m_fLensDetailShadingStrength; }
	void  SetLensDetailShadingStrength(float strength) { m_fLensDetailShadingStrength = strength; }

	float GetLensDetailBumpiness() const               { return m_fLensDetailBumpiness; }
	void  SetLensDetailBumpiness(float bumpiness)      { m_fLensDetailBumpiness = bumpiness; }

	float GetRotation()                                { return m_fRotation; }
	void  SetRotation(float rot)
	{
		m_fRotation = rot;
		m_meshDirty = true;
	}

	int  GetNoiseSeed() const { return m_iNoiseSeed; }
	void SetNoiseSeed(int s)
	{
		m_iNoiseSeed = s;
		m_meshDirty = true;
	}

	float GetSizeNoise() { return m_fSizeNoise; }
	void  SetSizeNoise(float s)
	{
		m_fSizeNoise = s;
		m_meshDirty = true;
	}

	float GetBrightnessNoise() { return m_fBrightnessNoise; }
	void  SetBrightnessNoise(float b)
	{
		m_fBrightnessNoise = b;
		m_meshDirty = true;
	}

	float GetRotationNoise() { return m_fRotNoise; }
	void  SetRotationNoise(float r)
	{
		m_fRotNoise = r;
		m_meshDirty = true;
	}

	float GetColorNoise() { return m_fClrNoise; }
	void  SetColorNoise(float clrNoise)
	{
		m_fClrNoise = clrNoise;
		m_meshDirty = true;
	}

	float GetIllumRange() { return m_fIllumRadius; }
	void  SetIllumRange(float range)
	{
		m_fIllumRadius = range;
	}

	bool   GetEnableAdvancedShading() const  { return m_bAdvancedShading; }
	void   SetEnableAdvancdShading(bool b)   { m_bAdvancedShading = b; }

	ColorF GetAmbientDiffuseRGBK() const     { return m_cAmbientDiffuse; }
	void   SetAmbientDiffuseRGBK(ColorF amb) { m_cAmbientDiffuse = amb; }

	float  GetAbsorptance() const            { return m_fAbsorptance; }
	void   SetAbsorptance(float a)           { m_fAbsorptance = a; }

	float  GetTransparency() const           { return m_fTransparency; }
	void   SetTransparency(float t)          { m_fTransparency = t; }

	float  GetScatteringStrength() const     { return m_fScatteringStrength; }
	void   SetScatteringStrength(float s)    { m_fScatteringStrength = s; }

	void   GetMemoryUsage(ICrySizer* pSizer) const override
	{
		pSizer->AddObject(this, sizeof(*this) + GetMeshDataSize());
	}
};
