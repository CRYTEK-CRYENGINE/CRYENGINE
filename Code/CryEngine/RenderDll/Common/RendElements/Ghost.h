// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "OpticsElement.h"
#include "OpticsGroup.h"
#include "AbstractMeshElement.h"

class CLensGhost : public COpticsElement
{
	struct SShaderParams : COpticsElement::SShaderParamsBase
	{
		Vec4 meshCenterAndBrt;
		Vec4 baseTexSize;
		Vec4 ghostTileInfo;
		Vec4 color;
	};

	_smart_ptr<CTexture> m_pTex;
	Vec4                 m_vTileDefinition;
	CRenderPrimitive     m_primitive;

protected:
#if defined(FLARES_SUPPORT_EDITING)
	void InitEditorParamGroups(DynArray<FuncVariableGroup>& groups);
#endif

public:

	CLensGhost(const char* name, CTexture* externalTex = NULL);

	Matrix34 mx33to34(Matrix33& mx)
	{
		return Matrix34(
		  mx.m00, mx.m01, mx.m02, 0,
		  mx.m10, mx.m11, mx.m12, 0,
		  mx.m20, mx.m21, mx.m22, 0);
	}

	EFlareType GetType() override { return eFT_Ghost; }
	bool       PreparePrimitives(const SPreparePrimitivesContext& context) override;
	void       Load(IXmlNode* pNode) override;

	CTexture*  GetTexture();
	void       SetTexture(CTexture* tex) { m_pTex = tex; }

	int        GetTileIndex()            { return (int)m_vTileDefinition.w; }
	int        GetTotalTileCount()       { return (int)m_vTileDefinition.z; }
	int        GetTileCountX()           { return (int)m_vTileDefinition.x; }
	int        GetTileCountY()           { return (int)m_vTileDefinition.y; }

	void       SetTileIndex(int idx)
	{
		if (idx >= 0 && idx <= GetTotalTileCount())
			m_vTileDefinition.w = (float)idx;
	}

	void SetTileCountX(int n)
	{
		int totalCount = GetTotalTileCount();
		if (totalCount > n * GetTileCountY())
			totalCount = n * GetTileCountY();

		SetTileDefinition(n, GetTileCountY(), totalCount, GetTileIndex());
	}

	void SetTileCountY(int n)
	{
		int totalCount = GetTotalTileCount();
		if (totalCount > GetTileCountX() * n)
			totalCount = GetTileCountX() * n;

		SetTileDefinition(GetTileCountX(), n, totalCount, GetTileIndex());
	}

	void SetTotalTileCount(int t)
	{
		SetTileDefinition(GetTileCountX(), GetTileCountY(), t, GetTileIndex());
	}

	void SetTileDefinition(int countX, int countY, int totalCount, int index)
	{
		if (totalCount > countX * countY)
			return;

		m_vTileDefinition.x = (float)countX;
		m_vTileDefinition.y = (float)countY;
		m_vTileDefinition.z = (float)totalCount;
		m_vTileDefinition.w = (float)index;
	}

	void SetTileDefinition(Vec4 df)
	{
		SetTileDefinition((int)df.x, (int)df.y, (int)df.z, (int)df.w);
	}
};

class CMultipleGhost : public COpticsGroup
{
protected:
	_smart_ptr<CTexture> m_pTex;

	Vec2                 m_vRange;
	float                m_fXOffsetNoise;
	float                m_fYOffsetNoise;
	Vec2                 m_vPositionFactor;
	Vec2                 m_vPositionOffset;
	int                  m_nCount;
	int                  m_nRandSeed;

	float                m_fSizeNoise;
	float                m_fBrightnessNoise;
	float                m_fColorNoise;

	bool                 m_bContentDirty : 1;

protected:
	void InitEditorParamGroups(DynArray<FuncVariableGroup>& groups);

public:
	CMultipleGhost(const char* name) :
		COpticsGroup(name),
		m_nCount(0),
		m_nRandSeed(0),
		m_bContentDirty(true),
		m_fXOffsetNoise(0),
		m_fYOffsetNoise(0),
		m_fSizeNoise(0.4f),
		m_fColorNoise(0.3f),
		m_fBrightnessNoise(0.3f)
	{
		m_vRange.set(0.1f, 0.7f);
		m_vPositionFactor.set(1, 1);
		m_vPositionOffset.set(0, 0);
		SetSize(0.04f);
		SetBrightness(0.3f);
		SetCount(20);
	}

public:
	void       GenGhosts(const SAuxParams& aux);

	EFlareType GetType() override               { return eFT_MultiGhosts; }
	bool       IsGroup() const override         { return false; }
	int        GetElementCount() const override { return 0; }
	bool       PreparePrimitives(const SPreparePrimitivesContext& context) override;
	void       Load(IXmlNode* pNode) override;

	CTexture*  GetTexture() const { return m_pTex; }
	void       SetTexture(CTexture* tex)
	{
		m_pTex = tex;
		m_bContentDirty = true;
	}

	float GetXOffsetNoise() const { return m_fXOffsetNoise; }
	void  SetXOffsetNoise(float x)
	{
		m_fXOffsetNoise = x;
		m_bContentDirty = true;
	}

	float GetYOffsetNoise() const { return m_fYOffsetNoise; }
	void  SetYOffsetNoise(float y)
	{
		m_fYOffsetNoise = y;
		m_bContentDirty = true;
	}

	Vec2 GetPositionFactor() const { return m_vPositionFactor; }
	void SetPositionFactor(Vec2 positionFactor)
	{
		m_vPositionFactor = positionFactor;
		m_bContentDirty = true;
	}

	Vec2 GetPositionOffset() const { return m_vPositionOffset; }
	void SetPositionOffset(Vec2 offsets)
	{
		m_vPositionOffset = offsets;
		m_bContentDirty = true;
	}

	Vec2 GetRange() const { return m_vRange; }
	void SetRange(Vec2 range)
	{
		m_vRange = range;
		m_bContentDirty = true;
	}

	int  GetCount() const { return m_nCount; }
	void SetCount(int count)
	{
		if (count < 0)
			return;
		m_nCount = count;
		RemoveAll();
		for (int i = 0; i < count; i++)
		{
			CLensGhost* ghost = new CLensGhost("SubGhost");
			ghost->SetAutoRotation(true);
			ghost->SetAspectRatioCorrection(true);
			ghost->SetOccBokehEnabled(true);
			ghost->SetSensorSizeFactor(1);
			ghost->SetSensorBrightnessFactor(1);
			Add(ghost);
		}
		m_bContentDirty = true;
	}

	int  GetRandSeed() const { return m_nRandSeed; }
	void SetRandSeed(int n)
	{
		m_nRandSeed = n;
		m_bContentDirty = true;
	}

	float GetSizeNoise() const { return m_fSizeNoise; }
	void  SetSizeNoise(float n)
	{
		m_fSizeNoise = n;
		m_bContentDirty = true;
	}

	float GetBrightnessNoise() const { return m_fBrightnessNoise; }
	void  SetBrightnessNoise(float n)
	{
		m_fBrightnessNoise = n;
		m_bContentDirty = true;
	}

	float GetColorNoise() const { return m_fColorNoise; }
	void  SetColorNoise(float n)
	{
		m_fColorNoise = n;
		m_bContentDirty = true;
	}
};
