// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "OpticsElement.h"
#include "AbstractMeshElement.h"
#include "MeshUtil.h"

class CTexture;
class IrisShafts : public COpticsElement, public AbstractMeshElement
{
	struct SShaderParams : COpticsElement::SShaderParamsBase
	{
		Vec4 meshCenterAndBrt;
	};

private:
	_smart_ptr<CTexture> m_pBaseTex;
	_smart_ptr<CTexture> m_pSpectrumTex;
	bool                 m_bUseSpectrumTex : 1;

	int                  m_nComplexity;
	int                  m_nSmoothLevel;
	int                  m_nColorComplexity;

	float                m_fPrevOcc;
	float                m_fPrimaryDir;
	float                m_fAngleRange;
	float                m_fConcentrationBoost;
	float                m_fBrightnessBoost;

	float                m_fSizeNoiseStrength;
	float                m_fThicknessNoiseStrength;
	float                m_fSpreadNoiseStrength;
	float                m_fSpacingNoiseStrength;

	float                m_fSpread;
	float                m_fThickness;
	int                  m_nNoiseSeed;

	int                  m_MaxNumberOfPolygon;
	CRenderPrimitive     m_primitive;

protected:
	void         GenMesh() override;

	float        ComputeSpreadParameters(const float thickness);
	int          ComputeDynamicSmoothLevel(int maxLevel, float spanAngle, float threshold);
	void         Invalidate() override
	{
		m_meshDirty = true;
	}

public:
	IrisShafts(const char* name);

#if defined(FLARES_SUPPORT_EDITING)
	void InitEditorParamGroups(DynArray<FuncVariableGroup>& groups);
#endif

	EFlareType GetType() override            { return eFT_IrisShafts; }
	bool       PreparePrimitives(const SPreparePrimitivesContext& context) override;
	void       Load(IXmlNode* pNode) override;

	bool       GetEnableSpectrumTex() const  { return m_bUseSpectrumTex; }
	void       SetEnableSpectrumTex(bool b)  { m_bUseSpectrumTex = b; }

	CTexture*  GetSpectrumTex() const        { return m_pSpectrumTex; }
	void       SetSpectrumTex(CTexture* tex) { m_pSpectrumTex = tex; }

	CTexture*  GetBaseTex() const            { return m_pBaseTex; }
	void       SetBaseTex(CTexture* tex)     { m_pBaseTex = tex; }

	int        GetNoiseSeed() const          { return m_nNoiseSeed; }
	void       SetNoiseSeed(int seed)
	{
		m_nNoiseSeed = seed;
		m_meshDirty = true;
	}

	int  GetComplexity() const { return m_nComplexity; }
	void SetComplexity(int n)
	{
		m_nComplexity = n;
		m_meshDirty = true;
	}

	int  GetColorComplexity() const { return m_nColorComplexity; }
	void SetColorComplexity(int n)
	{
		m_nColorComplexity = n;
		m_meshDirty = true;
	}

	int  GetSmoothLevel() const { return m_nSmoothLevel; }
	void SetSmoothLevel(int n)
	{
		m_nSmoothLevel = n;
		m_meshDirty = true;
	}

	float GetThickness() const { return m_fThickness; }
	void  SetThickness(float f)
	{
		m_fThickness = f;
		m_meshDirty = true;
	}

	float GetSpread() const { return m_fSpread; }
	void  SetSpread(float s)
	{
		m_fSpread = s;
		m_meshDirty = true;
	}

	float GetThicknessNoise() const { return m_fThicknessNoiseStrength; }
	void  SetThicknessNoise(float noise)
	{
		m_fThicknessNoiseStrength = noise;
		m_meshDirty = true;
	}

	float GetSpreadNoise() const { return m_fSpreadNoiseStrength; }
	void  SetSpreadNoise(float noise)
	{
		m_fThicknessNoiseStrength = noise;
		m_meshDirty = true;
	}

	float GetSizeNoise() const { return m_fSizeNoiseStrength; }
	void  SetSizeNoise(float noise)
	{
		m_fSizeNoiseStrength = noise;
		m_meshDirty = true;
	}

	float GetSpacingNoise() const { return m_fSpacingNoiseStrength; }
	void  SetSpacingNoise(float noise)
	{
		m_fSpacingNoiseStrength = noise;
		m_meshDirty = true;
	}

	void GetMemoryUsage(ICrySizer* pSizer) const override
	{
		pSizer->AddObject(this, sizeof(*this) + GetMeshDataSize());
	}
};
