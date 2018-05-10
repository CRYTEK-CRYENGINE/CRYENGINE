// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryString/CryString.h>
#include <CryMath/Cry_Vector2.h>
#include <CryMath/Cry_Vector3.h>
#include <CryMath/Cry_Matrix33.h>
#include <CryMath/Cry_Color.h>
#include <CryRenderer/IFlares.h>

#include "XRenderD3D9/GraphicsPipeline/Common/PrimitiveRenderPass.h"
#include "XRenderD3D9/GraphicsPipeline/StandardGraphicsPipeline.h"

class CD3D9Renderer;
class CTexture;
class RootOpticsElement;

namespace LensOpConst
{
static Vec4 _LO_DEF_VEC4(1.f, 1.f, 1.f, 0.f);
static Vec2 _LO_DEF_VEC2(0.f, 0.f);
static Vec2 _LO_DEF_VEC2_I(1.f, 1.f);
static Vec2 _LO_DEF_ANGLE(-90.f, 90.f);
static Matrix33 _LO_DEF_MX33(IDENTITY);
static ColorF _LO_DEF_CLR(1.f, 1.f, 1.f, 0.2f);
static ColorF _LO_DEF_CLR_BLK(0.f, 0.f, 0.f, 0.0f);
static float _LO_MIN(1e-6f);
}

typedef MFPVariable<IOpticsElementBase> OpticsMFPVariable;
typedef void (IOpticsElementBase::*     Optics_MFPtr)(void);

class COpticsElement : public IOpticsElementBase
{
public:
	struct SAuxParams
	{
		float perspectiveShortening;
		float linearDepth;
		float distance;
		float sensorVariationValue;
		float viewAngleFalloff;
		bool  attachToSun;
		bool  bMultiplyColor;
		bool  bForceRender;
		bool  bIgnoreOcclusionQueries;
	};

	struct SPreparePrimitivesContext
	{
		SPreparePrimitivesContext(CPrimitiveRenderPass& targetPass, std::vector<CPrimitiveRenderPass*>& prePasses)
			: pass(targetPass)
			, prePasses(prePasses)
			, viewInfoCount(0)
			, pViewInfo(nullptr)
			, lightWorldPos(ZERO)
		{
			ZeroStruct(auxParams);
			ZeroArray(lightScreenPos);
		}

		CPrimitiveRenderPass&                       pass;
		std::vector<CPrimitiveRenderPass*>&         prePasses;

		const SRenderViewInfo* pViewInfo;
		int                                         viewInfoCount;

		Vec3                                        lightWorldPos;
		Vec3                                        lightScreenPos[CCamera::eEye_eCount];
		SAuxParams                                  auxParams;
	};

	struct SShaderParamsBase
	{
		Vec4 outputDimAndSize;
		Vec4 occPatternInfo;
		Vec4 externTint;
		Matrix34 xform;
		Vec4 dynamics;
		Vec4 lightProjPos;
		Vec4 hdrParams;
	};

private:
	Vec2  xform_scale;
	Vec2  xform_translate;
	float xform_rotation;

protected:
	COpticsElement* m_pParent;
	Vec2            m_globalMovement;
	Matrix33        m_globalTransform;
	ColorF          m_globalColor;
	float           m_globalFlareBrightness;
	float           m_globalShaftBrightness;
	float           m_globalSize;
	float           m_globalPerspectiveFactor;
	float           m_globalDistanceFadingFactor;
	float           m_globalOrbitAngle;
	float           m_globalSensorSizeFactor;
	float           m_globalSensorBrightnessFactor;
	bool            m_globalAutoRotation       : 1;
	bool            m_globalCorrectAspectRatio : 1;
	bool            m_globalOcclusionBokeh     : 1;

	bool            m_bEnabled                 : 1;
	string          m_name;

protected:

	Matrix33 m_mxTransform;
	float    m_fSize;
	float    m_fPerspectiveFactor;
	float    m_fDistanceFadingFactor;
	ColorF   m_Color;
	float    m_fBrightness;
	Vec2     m_vMovement;
	float    m_fOrbitAngle;
	float    m_fSensorSizeFactor;
	float    m_fSensorBrightnessFactor;
	Vec2     m_vDynamicsOffset;
	float    m_fDynamicsRange;
	float    m_fDynamicsFalloff;
	bool     m_bAutoRotation       : 1;
	bool     m_bCorrectAspectRatio : 1;
	bool     m_bOcclusionBokeh     : 1;
	bool     m_bDynamics           : 1;
	bool     m_bDynamicsInvert     : 1;

#if defined(FLARES_SUPPORT_EDITING)
	DynArray<FuncVariableGroup> paramGroups;
#endif
public:

	COpticsElement(const char* name, float size = 0.3f, const float brightness = 1.0f, const ColorF& color = LensOpConst::_LO_DEF_CLR);
	virtual ~COpticsElement() {}

	virtual void Load(IXmlNode* pNode);

	IOpticsElementBase* GetParent() const
	{
		return m_pParent;
	}

	RootOpticsElement* GetRoot();

	const char*        GetName() const { return m_name.c_str(); }
	void               SetName(const char* newName)
	{
		m_name = newName;
	}
	bool   IsEnabled() const                 { return m_bEnabled; }
	float  GetSize() const                   { return m_fSize; }
	float  GetPerspectiveFactor() const      { return m_fPerspectiveFactor; }
	float  GetDistanceFadingFactor() const   { return m_fDistanceFadingFactor; }
	float  GetBrightness() const             { return m_fBrightness; }
	ColorF GetColor() const                  { return m_Color; }
	Vec2   GetMovement() const               { return m_vMovement; }
	float  GetOrbitAngle() const             { return m_fOrbitAngle; }
	float  GetSensorSizeFactor() const       { return m_fSensorSizeFactor; }
	float  GetSensorBrightnessFactor() const { return m_fSensorBrightnessFactor; }
	bool   IsOccBokehEnabled() const         { return m_bOcclusionBokeh; }
	bool   HasAutoRotation() const           { return m_bAutoRotation; }
	bool   HasAspectRatioCorrection() const  { return m_bCorrectAspectRatio; }

	void   SetEnabled(bool b)                { m_bEnabled = b; }
	void   SetSize(float s)                  { m_fSize = s; }
	void   SetPerspectiveFactor(float p)     { m_fPerspectiveFactor = p; }
	void   SetDistanceFadingFactor(float p)  { m_fDistanceFadingFactor = p; }
	void   SetBrightness(float b)            { m_fBrightness = b; }
	void   SetColor(ColorF color)            { m_Color = color; Invalidate(); }
	void   SetMovement(Vec2 movement)
	{
		m_vMovement = movement;
		if (fabs(m_vMovement.x) < 0.0001f)
			m_vMovement.x = 0.001f;
		if (fabs(m_vMovement.y) < 0.0001f)
			m_vMovement.y = 0.001f;
	}
	void SetTransform(const Matrix33& xform)        { m_mxTransform = xform; }
	void SetOccBokehEnabled(bool b)                 { m_bOcclusionBokeh = b; }
	void SetOrbitAngle(float orbitAngle)            { m_fOrbitAngle = orbitAngle; }
	void SetSensorSizeFactor(float sizeFactor)      { m_fSensorSizeFactor = sizeFactor; }
	void SetSensorBrightnessFactor(float brtFactor) { m_fSensorBrightnessFactor = brtFactor; }
	void SetAutoRotation(bool b)                    { m_bAutoRotation = b; }
	void SetAspectRatioCorrection(bool b)           { m_bCorrectAspectRatio = b; }

	void SetParent(COpticsElement* pParent)
	{
		m_pParent = pParent;
	}

	virtual void GetMemoryUsage(ICrySizer* pSizer) const;
	bool         IsVisible() const
	{
		return m_globalColor.a > LensOpConst::_LO_MIN && m_globalFlareBrightness > LensOpConst::_LO_MIN;
	}

	virtual void RenderPreview(const SLensFlareRenderParam* pParam, const Vec3& vPos) { assert(0); }
	virtual bool PreparePrimitives(const SPreparePrimitivesContext& context) { assert(0); return false; }

protected:
	void         updateXformMatrix();
	virtual void Invalidate() {}

	virtual void DeleteThis();

public:
	void SetScale(Vec2 scale)
	{
		xform_scale = scale;
		updateXformMatrix();
	}
	void SetRotation(float rot)
	{
		xform_rotation = rot;
		updateXformMatrix();
	}
	void SetTranslation(Vec2 xlation)
	{
		xform_translate = xlation;
		updateXformMatrix();
	}
	Vec2                        GetScale()                                            { return xform_scale; }
	Vec2                        GetTranslation()                                      { return xform_translate; }
	float                       GetRotation()                                         { return xform_rotation; }

	void                        SetDynamicsEnabled(bool enable)                       { m_bDynamics = enable; }
	void                        SetDynamicsOffset(Vec2 offset)                        { m_vDynamicsOffset = offset; }
	void                        SetDynamicsRange(float range)                         { m_fDynamicsRange = range; }
	void                        SetDynamicsInvert(bool invert)                        { m_bDynamicsInvert = invert; }
	void                        SetDynamicsFalloff(float falloff)                     { m_fDynamicsFalloff = falloff; }

	bool                        GetDynamicsEnabled() const                            { return m_bDynamics; }
	Vec2                        GetDynamicsOffset() const                             { return m_vDynamicsOffset; }
	float                       GetDynamicsRange() const                              { return m_fDynamicsRange; }
	bool                        GetDynamicsInvert() const                             { return m_bDynamicsInvert; }
	float                       GetDynamicsFalloff() const                            { return m_fDynamicsFalloff; }

	virtual void                AddElement(IOpticsElementBase* pElement)              {}
	virtual void                InsertElement(int nPos, IOpticsElementBase* pElement) {}
	virtual void                Remove(int i)                                         {}
	virtual void                RemoveAll()                                           {}
	virtual int                 GetElementCount() const                               { return 0; }
	virtual IOpticsElementBase* GetElementAt(int i) const
	{
#ifndef RELEASE
		iLog->Log("ERROR");
		__debugbreak();
#endif
		return 0;
	}

#if defined(FLARES_SUPPORT_EDITING)
	virtual DynArray<FuncVariableGroup> GetEditorParamGroups();
#endif

protected:

#if defined(FLARES_SUPPORT_EDITING)
	virtual void InitEditorParamGroups(DynArray<FuncVariableGroup>& groups);
#endif

public:

	virtual void validateGlobalVars(const SAuxParams& aux);

	float        computeMovementLocationX(const Vec3& vSrcProjPos)
	{
		return (vSrcProjPos.x - 0.5f) * m_globalMovement.x + 0.5f;
	}

	float computeMovementLocationY(const Vec3& vSrcProjPos)
	{
		return (vSrcProjPos.y - 0.5f) * m_globalMovement.y + 0.5f;
	}

	static const Vec3 computeOrbitPos(const Vec3& vSrcProjPos, float orbitAngle);

	void              ApplyOcclusionPattern(SShaderParamsBase& shaderParams, CRenderPrimitive& primitive);
	void              ApplyGeneralFlags(uint64& rtFlags);
	void              ApplyOcclusionBokehFlag(uint64& rtFlags);
	void              ApplySpectrumTexFlag(uint64& rtFlags, bool enabled);
	void              ApplyCommonParams(SShaderParamsBase& shaderParams, const SRenderViewport& viewport, const Vec3& lightProjPos, const Vec2& size);


	virtual EFlareType GetType()       { return eFT__Base__; }
	virtual bool       IsGroup() const { return false; }
};
