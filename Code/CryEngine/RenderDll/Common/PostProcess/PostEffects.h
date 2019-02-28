// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   PostEffects.h : Post process effects

   Revision history:
* 18/06/2005: Re-organized (to minimize code dependencies/less annoying compiling times)
* Created by Tiago Sousa
   =============================================================================*/

#ifndef _POSTEFFECTS_H_
#define _POSTEFFECTS_H_

#include "PostProcessUtils.h"

struct SColorGradingMergeParams;
class CSoundEventsListener;

////////////////////////////////////////////////////////////////////////////////////////////////////
// Engine specific post-effects
////////////////////////////////////////////////////////////////////////////////////////////////////

struct SObjMotionBlurParams
{
	SObjMotionBlurParams() : pRenderObj(NULL), nFrameUpdateID(0)
	{}

	SObjMotionBlurParams(CRenderObject* pObj, const Matrix34A& objToWorld, uint32 frameUpdateID)
		: mObjToWorld(objToWorld), pRenderObj(pObj), nFrameUpdateID(frameUpdateID) {}

	Matrix34       mObjToWorld;
	CRenderObject* pRenderObj;
	uint32         nFrameUpdateID;
};

// Camera + Object motion blur handling
class CMotionBlur : public CPostEffect
{
public:
	CMotionBlur()
	{
		m_nRenderFlags = 0;
		m_nID = EPostEffectID::MotionBlur;

		// Register technique instance and it's parameters
		AddParamBool("MotionBlur_Active", m_pActive, 0);
		AddParamFloat("MotionBlur_Amount", m_pAmount, 0.0f);
		AddParamBool("MotionBlur_UseMask", m_pUseMask, 0);
		AddParamTex("MotionBlur_MaskTexName", m_pMaskTex, 0);
		AddParamFloat("MotionBlur_ExposureTime", m_pExposureTime, 0.004f);
		AddParamFloat("MotionBlur_VectorsScale", m_pVectorsScale, 1.5f);
		AddParamFloatNoTransition("MotionBlur_UserVectorsScale", m_pUserVectorsScale, 1.0f);
		AddParamFloat("MotionBlur_DrawNearZRangeOverride", m_pDrawNearZRangeOverride, 0.0f);
		AddParamFloatNoTransition("MotionBlur_ShutterSpeed", m_pShutterSpeed, -1.0f);

		AddParamFloatNoTransition("FilterRadialBlurring_Amount", m_pRadBlurAmount, 0.0f);
		AddParamFloatNoTransition("FilterRadialBlurring_ScreenPosX", m_pRadBlurScreenPosX, 0.5f);
		AddParamFloatNoTransition("FilterRadialBlurring_ScreenPosY", m_pRadBlurScreenPosY, 0.5f);
		AddParamFloatNoTransition("FilterRadialBlurring_Radius", m_pRadBlurRadius, 1.0f);
		AddParamTex("FilterRadialBlurring_MaskTexName", m_pRadialBlurMaskTex, 0);
		AddParamFloat("FilterRadialBlurring_MaskUVScaleX", m_pRadialBlurMaskUVScaleX, 1.0f);
		AddParamFloat("FilterRadialBlurring_MaskUVScaleY", m_pRadialBlurMaskUVScaleY, 1.0f);
		AddParamVec4("Global_DirectionalBlur_Vec", m_pDirectionalBlurVec, Vec4(0, 0, 0, 0)); // directional blur

		m_pMotionBlurDofTechName = "MotionBlurDof";
		m_pMotionBlurTechName = "MotionBlur";
		m_pDofTechName = "Dof";
		m_pDownscaleDofTechName = "DownscaleDof";
		m_pCompositeDofTechName = "CompositeDof";

		m_pRefDofMBNormalize = "RefDofMbNormalizePass";

		m_pPackVelocities = "PackVelocities";
		m_pDownscaleVelocity = "DownscaleVelocity";

		m_pViewProjPrevParam = "mViewProjPrev";

		m_pMotionBlurParamName = "vMotionBlurParams";
		m_pDofFocusParam0Name = "vDofParamsFocus0";
		m_pDofFocusParam1Name = "vDofParamsFocus1";
		m_pDirBlurParamName = "vDirectionalBlur";
		m_pRadBlurParamName = "vRadBlurParam";

		m_pBokehShape = NULL;
	}

	virtual ~CMotionBlur()
	{
		Release();
	}

	virtual int             Initialize();
	virtual int             CreateResources();
	virtual void            Release();

	virtual void            Render();
	void                    RenderObjectsVelocity();

	virtual void            Reset(bool bOnSpecChange = false);
	virtual bool            Preprocess(const SRenderViewInfo& viewInfo);
	virtual void            OnBeginFrame(const SRenderingPassInfo& passInfo);

	static void             SetupObject(CRenderObject* pObj, const SRenderingPassInfo& passInfo);
	static bool             GetPrevObjToWorldMat(CRenderObject* pObj, Matrix44A& res);
	static void             InsertNewElements();
	static void             FreeData();

	virtual const char*     GetName() const
	{
		return CRenderer::CV_r_UseMergedPosts ? "MotionBlur and Dof" : "MotionBlur";
	}

private:
	friend class CMotionBlurStage;

	CCryNameTSCRC m_pMotionBlurDofTechName;
	CCryNameTSCRC m_pRefDofMBNormalize;

	CCryNameTSCRC m_pMotionBlurTechName;
	CCryNameTSCRC m_pDofTechName;
	CCryNameTSCRC m_pDownscaleDofTechName;
	CCryNameTSCRC m_pCompositeDofTechName;

	CCryNameTSCRC m_pPackVelocities;
	CCryNameTSCRC m_pDownscaleVelocity;

	CCryNameR     m_pDofFocusParam0Name;
	CCryNameR     m_pDofFocusParam1Name;
	CCryNameR     m_pDirBlurParamName;
	CCryNameR     m_pRadBlurParamName;
	CCryNameR     m_pViewProjPrevParam;
	CCryNameR     m_pMotionBlurParamName;

	// bool, int, float, float, bool, texture, float, float, int, float, float
	CEffectParam* m_pAmount, * m_pUseMask, * m_pMaskTex, * m_pShutterSpeed;
	CEffectParam* m_pVectorsScale, * m_pUserVectorsScale, * m_pExposureTime;
	CEffectParam* m_pRadBlurAmount, * m_pRadBlurScreenPosX, * m_pRadBlurScreenPosY, * m_pRadBlurRadius;
	CEffectParam* m_pRadialBlurMaskTex, * m_pRadialBlurMaskUVScaleX, * m_pRadialBlurMaskUVScaleY;
	CEffectParam* m_pDrawNearZRangeOverride;
	CEffectParam* m_pDirectionalBlurVec;
	CTexture*     m_pBokehShape;

	typedef VectorMap<uintptr_t, SObjMotionBlurParams> OMBParamsMap;
	typedef OMBParamsMap::iterator                     OMBParamsMapItor;
	static OMBParamsMap m_pOMBData[3]; // triple buffering: t0: being written, t-1: current render frame, t-2: previous render frame
	static CThreadSafeRendererContainer<OMBParamsMap::value_type> m_FillData[RT_COMMAND_BUF_COUNT];
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

struct SDepthOfFieldParams
{
	SDepthOfFieldParams() : pMaskTex(0), fMaskBlendAmount(0.0f), bGameMode(false)
	{
	};

	Vec4      vFocus;
	Vec4      vMinZParams;
	bool      bGameMode;

	CTexture* pMaskTex;
	float     fMaskBlendAmount;
};

class CDepthOfField : public CPostEffect
{
public:
	CDepthOfField()
	{
		m_nRenderFlags = 0;
		m_nID = EPostEffectID::DepthOfField;

		// todo: add user values

		// Register technique and it's parameters
		AddParamBool("Dof_Active", m_pActive, 0);
		AddParamFloatNoTransition("Dof_FocusDistance", m_pFocusDistance, 3.5f);
		AddParamFloatNoTransition("Dof_FocusRange", m_pFocusRange, 0.0f);
		AddParamFloatNoTransition("Dof_FocusMin", m_pFocusMin, 0.4f);
		AddParamFloatNoTransition("Dof_FocusMax", m_pFocusMax, 10.0f);
		AddParamFloatNoTransition("Dof_FocusLimit", m_pFocusLimit, 100.0f);
		AddParamFloatNoTransition("Dof_MaxCoC", m_pMaxCoC, 12.0f);
		AddParamFloatNoTransition("Dof_CenterWeight", m_pCenterWeight, 1.0f);
		AddParamFloatNoTransition("Dof_BlurAmount", m_pBlurAmount, 1.0f);
		AddParamBool("Dof_UseMask", m_pUseMask, 0);
		AddParamTex("Dof_MaskTexName", m_pMaskTex, 0);
		AddParamBool("Dof_Debug", m_pDebug, 0);
		AddParamTex("Dof_BokehMaskName", m_pBokehMaskTex, 0);

		AddParamBool("Dof_User_Active", m_pUserActive, 0);
		AddParamFloatNoTransition("Dof_User_FocusDistance", m_pUserFocusDistance, 3.5f);
		AddParamFloatNoTransition("Dof_User_FocusRange", m_pUserFocusRange, 5.0f);
		AddParamFloatNoTransition("Dof_User_BlurAmount", m_pUserBlurAmount, 1.0f);

		AddParamFloatNoTransition("Dof_Tod_FocusRange", m_pTimeOfDayFocusRange, 1000.0f);
		AddParamFloatNoTransition("Dof_Tod_BlurAmount", m_pTimeOfDayBlurAmount, 0.0f);

		AddParamFloatNoTransition("Dof_FocusMinZ", m_pFocusMinZ, 0.0f);           // 0.4 is good default
		AddParamFloatNoTransition("Dof_FocusMinZScale", m_pFocusMinZScale, 0.0f); // 1.0 is good default

		AddParamFloatNoTransition("FilterMaskedBlurring_Amount", m_pMaskedBlurAmount, 0.0f);
		AddParamTex("FilterMaskedBlurring_MaskTexName", m_pMaskedBlurMaskTex, 0);

		m_pDofFocusParam0Name = "vDofParamsFocus0";
		m_pDofFocusParam1Name = "vDofParamsFocus1";
		m_pDofTaps = "g_Taps";

		m_fUserFocusRangeCurr = 0;
		m_fUserFocusDistanceCurr = 0;
		m_fUserBlurAmountCurr = 0;
		m_pNoise = 0;

		m_vBokehSamples.resize(8);
	}

	virtual int         CreateResources();
	virtual void        Release();
	virtual void        Render();

	SDepthOfFieldParams GetParams();

	virtual bool        Preprocess(const SRenderViewInfo& viewInfo);
	virtual void        Reset(bool bOnSpecChange = false);

	virtual const char* GetName() const
	{
		return "DepthOfField";
	}

private:

	CCryNameR m_pDofFocusParam0Name;
	CCryNameR m_pDofFocusParam1Name;
	CCryNameR m_pDofTaps;

	// bool, float, float, float, float
	CEffectParam* m_pUseMask, * m_pBokehMaskTex, * m_pFocusDistance, * m_pFocusRange, * m_pMaxCoC, * m_pCenterWeight, * m_pBlurAmount;
	// float, float, float, CTexture, bool
	CEffectParam* m_pFocusMin, * m_pFocusMax, * m_pFocusLimit, * m_pMaskTex, * m_pDebug;
	// bool, float, float, float, float
	CEffectParam* m_pUserActive, * m_pUserFocusDistance, * m_pUserFocusRange, * m_pUserBlurAmount;
	// float, float
	CEffectParam* m_pTimeOfDayFocusRange, * m_pTimeOfDayBlurAmount;
	// float, float
	CEffectParam* m_pFocusMinZ, * m_pFocusMinZScale;

	// float, CTexture
	CEffectParam*     m_pMaskedBlurAmount, * m_pMaskedBlurMaskTex;

	float             m_fUserFocusRangeCurr;
	float             m_fUserFocusDistanceCurr;
	float             m_fUserBlurAmountCurr;

	std::vector<Vec4> m_vBokehSamples;

	CTexture*         m_pNoise;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CSunShafts : public CPostEffect
{
public:
	CSunShafts()
	{
		m_nID = EPostEffectID::SunShafts;
		m_pOcclQuery[0] = nullptr;
		m_pOcclQuery[1] = nullptr;

		AddParamBool("SunShafts_Active", m_pActive, 0);
		AddParamInt("SunShafts_Type", m_pShaftsType, 0);                                          // default shafts type - highest quality
		AddParamFloatNoTransition("SunShafts_Amount", m_pShaftsAmount, 0.25f);                    // shafts visibility
		AddParamFloatNoTransition("SunShafts_RaysAmount", m_pRaysAmount, 0.25f);                  // rays visibility
		AddParamFloatNoTransition("SunShafts_RaysAttenuation", m_pRaysAttenuation, 5.0f);         // rays attenuation
		AddParamFloatNoTransition("SunShafts_RaysSunColInfluence", m_pRaysSunColInfluence, 1.0f); // sun color influence
		AddParamVec4NoTransition("SunShafts_RaysCustomColor", m_pRaysCustomCol, Vec4(1.0f, 1.0f, 1.0f, 1.0f));
		AddParamFloat("Scratches_Strength", m_pScratchStrength, 0.0f);
		AddParamFloat("Scratches_Threshold", m_pScratchThreshold, 0.0f);
		AddParamFloat("Scratches_Intensity", m_pScratchIntensity, 0.7f);
		m_bShaftsEnabled = false;
		m_nVisSampleCount = 0;
	}

	virtual int  Initialize();
	virtual void Release();
	virtual void OnLostDevice();
	virtual bool Preprocess(const SRenderViewInfo& viewInfo);
	virtual void Render();
	virtual void Reset(bool bOnSpecChange = false);

	bool         IsVisible();
	bool         SunShaftsGen(CTexture* pSunShafts, CTexture* pPingPongRT = 0);
	void         GetSunShaftsParams(Vec4 pParams[2])
	{
		pParams[0] = m_pRaysCustomCol->GetParamVec4();
		pParams[1] = Vec4(0, 0, m_pRaysAmount->GetParam(), m_pRaysSunColInfluence->GetParam());
	}

	virtual const char* GetName() const
	{
		return "MergedSunShaftsEdgeAAColorCorrection";
	}

private:
	friend class CSunShaftsStage;

	bool   m_bShaftsEnabled;
	uint32 m_nVisSampleCount;

	// int, float, float, float, vec4
	CEffectParam*    m_pShaftsType, * m_pShaftsAmount, * m_pRaysAmount, * m_pRaysAttenuation, * m_pRaysSunColInfluence, * m_pRaysCustomCol;
	CEffectParam*    m_pScratchStrength, * m_pScratchThreshold, * m_pScratchIntensity;
	COcclusionQuery* m_pOcclQuery[2];
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CSharpening : public CPostEffect
{
public:
	CSharpening()
	{
		m_nID = EPostEffectID::Sharpening;

		AddParamInt("FilterSharpening_Type", m_pType, 0);
		AddParamFloat("FilterSharpening_Amount", m_pAmount, 1.0f);
	}

	virtual bool        Preprocess(const SRenderViewInfo& viewInfo);
	virtual void        Render();
	virtual void        Reset(bool bOnSpecChange = false);

	virtual const char* GetName() const
	{
		return "FilterSharpening";
	}

private:

	// float, int
	CEffectParam* m_pAmount, * m_pType;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CBlurring : public CPostEffect
{
public:
	CBlurring()
	{
		m_nID = EPostEffectID::Blurring;

		AddParamInt("FilterBlurring_Type", m_pType, 0);
		AddParamFloat("FilterBlurring_Amount", m_pAmount, 0.0f);
	}

	virtual bool        Preprocess(const SRenderViewInfo& viewInfo);
	virtual void        Render();
	virtual void        Reset(bool bOnSpecChange = false);

	virtual const char* GetName() const
	{
		return "FilterBlurring";
	}

private:

	// float, int
	CEffectParam* m_pAmount, * m_pType;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CUberGamePostProcess : public CPostEffect
{
	friend class CUberGamePostEffectPass;

public:

	// Bitmaks used to enable only certain effects or combinations of most expensive effects
	enum EPostsProcessMask
	{
		ePE_SyncArtifacts = (1 << 0),
		ePE_RadialBlur    = (1 << 1),
		ePE_ChromaShift   = (1 << 2),
	};

	CUberGamePostProcess()
	{
		m_nID = EPostEffectID::UberGamePostProcess;
		m_nCurrPostEffectsMask = 0;

		AddParamTex("tex_VisualArtifacts_Mask", m_pMask, 0);

		AddParamVec4("clr_VisualArtifacts_ColorTint", m_pColorTint, Vec4(1.0f, 1.0f, 1.0f, 1.0f));

		AddParamFloat("VisualArtifacts_Vsync", m_pVSyncAmount, 0.0f);
		AddParamFloat("VisualArtifacts_VsyncFreq", m_pVSyncFreq, 1.0f);

		AddParamFloat("VisualArtifacts_Interlacing", m_pInterlationAmount, 0.0f);
		AddParamFloat("VisualArtifacts_InterlacingTile", m_pInterlationTilling, 1.0f);
		AddParamFloat("VisualArtifacts_InterlacingRot", m_pInterlationRotation, 0.0f);

		AddParamFloat("VisualArtifacts_Pixelation", m_pPixelationScale, 0.0f);
		AddParamFloat("VisualArtifacts_Noise", m_pNoise, 0.0f);

		AddParamFloat("VisualArtifacts_SyncWaveFreq", m_pSyncWaveFreq, 0.0f);
		AddParamFloat("VisualArtifacts_SyncWavePhase", m_pSyncWavePhase, 0.0f);
		AddParamFloat("VisualArtifacts_SyncWaveAmplitude", m_pSyncWaveAmplitude, 0.0f);

		AddParamFloat("FilterChromaShift_User_Amount", m_pFilterChromaShiftAmount, 0.0f); // Kept for backward - compatibility
		AddParamFloat("FilterArtifacts_ChromaShift", m_pChromaShiftAmount, 0.0f);

		AddParamFloat("FilterGrain_Amount", m_pFilterGrainAmount, 0.0f);// Kept for backward - compatibility
		AddParamFloat("FilterArtifacts_Grain", m_pGrainAmount, 0.0f);
		AddParamFloatNoTransition("FilterArtifacts_GrainTile", m_pGrainTile, 1.0f);
	}

	virtual bool        Preprocess(const SRenderViewInfo& viewInfo);
	virtual void        Render();
	virtual void        Reset(bool bOnSpecChange = false);

	virtual const char* GetName() const
	{
		return "UberGamePostProcess";
	}

private:
	CEffectParam* m_pVSyncAmount;
	CEffectParam* m_pVSyncFreq;

	CEffectParam* m_pColorTint;

	CEffectParam* m_pInterlationAmount;
	CEffectParam* m_pInterlationTilling;
	CEffectParam* m_pInterlationRotation;

	CEffectParam* m_pPixelationScale;
	CEffectParam* m_pNoise;

	CEffectParam* m_pSyncWaveFreq;
	CEffectParam* m_pSyncWavePhase;
	CEffectParam* m_pSyncWaveAmplitude;

	CEffectParam* m_pFilterChromaShiftAmount;
	CEffectParam* m_pChromaShiftAmount;

	CEffectParam* m_pGrainAmount;
	CEffectParam* m_pFilterGrainAmount; // todo: add support
	CEffectParam* m_pGrainTile;

	CEffectParam* m_pMask;

	uint8         m_nCurrPostEffectsMask;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CColorGrading : public CPostEffect
{
public:
	CColorGrading()
	{
		m_nID = EPostEffectID::ColorGrading;

		// levels adjustment
		AddParamFloatNoTransition("ColorGrading_minInput", m_pMinInput, 0.0f);
		AddParamFloatNoTransition("ColorGrading_gammaInput", m_pGammaInput, 1.0f);
		AddParamFloatNoTransition("ColorGrading_maxInput", m_pMaxInput, 255.0f);
		AddParamFloatNoTransition("ColorGrading_minOutput", m_pMinOutput, 0.0f);
		AddParamFloatNoTransition("ColorGrading_maxOutput", m_pMaxOutput, 255.0f);

		// generic color adjustment
		AddParamFloatNoTransition("ColorGrading_Brightness", m_pBrightness, 1.0f);
		AddParamFloatNoTransition("ColorGrading_Contrast", m_pContrast, 1.0f);
		AddParamFloatNoTransition("ColorGrading_Saturation", m_pSaturation, 1.0f);

		// filter color
		m_pDefaultPhotoFilterColor = Vec4(0.952f, 0.517f, 0.09f, 1.0f);
		AddParamVec4NoTransition("clr_ColorGrading_PhotoFilterColor", m_pPhotoFilterColor, m_pDefaultPhotoFilterColor);   // use photoshop default orange
		AddParamFloatNoTransition("ColorGrading_PhotoFilterColorDensity", m_pPhotoFilterColorDensity, 0.0f);

		// selective color
		AddParamVec4NoTransition("clr_ColorGrading_SelectiveColor", m_pSelectiveColor, Vec4(0.0f, 0.0f, 0.0f, 0.0f));
		AddParamFloatNoTransition("ColorGrading_SelectiveColorCyans", m_pSelectiveColorCyans, 0.0f);
		AddParamFloatNoTransition("ColorGrading_SelectiveColorMagentas", m_pSelectiveColorMagentas, 0.0f);
		AddParamFloatNoTransition("ColorGrading_SelectiveColorYellows", m_pSelectiveColorYellows, 0.0f);
		AddParamFloatNoTransition("ColorGrading_SelectiveColorBlacks", m_pSelectiveColorBlacks, 0.0f);

		// mist adjustment
		AddParamFloatNoTransition("ColorGrading_GrainAmount", m_pGrainAmount, 0.0f);
		AddParamFloatNoTransition("ColorGrading_SharpenAmount", m_pSharpenAmount, 1.0f);

		// user params
		AddParamFloatNoTransition("ColorGrading_Saturation_Offset", m_pSaturationOffset, 0.0f);
		AddParamVec4NoTransition("ColorGrading_PhotoFilterColor_Offset", m_pPhotoFilterColorOffset, Vec4(0.0f, 0.0f, 0.0f, 0.0f));
		AddParamFloatNoTransition("ColorGrading_PhotoFilterColorDensity_Offset", m_pPhotoFilterColorDensityOffset, 0.0f);
		AddParamFloatNoTransition("ColorGrading_GrainAmount_Offset", m_pGrainAmountOffset, 0.0f);
	}

	virtual bool        Preprocess(const SRenderViewInfo& viewInfo);
	virtual void        Render();
	virtual void        Reset(bool bOnSpecChange = false);
	bool                UpdateParams(SColorGradingMergeParams& pMergeParams, bool bUpdateChart = true);

	virtual const char* GetName() const
	{
		return "ColorGrading";
	}

private:

	// levels adjustment
	CEffectParam* m_pMinInput;
	CEffectParam* m_pGammaInput;
	CEffectParam* m_pMaxInput;
	CEffectParam* m_pMinOutput;
	CEffectParam* m_pMaxOutput;

	// generic color adjustment
	CEffectParam* m_pBrightness;
	CEffectParam* m_pContrast;
	CEffectParam* m_pSaturation;
	CEffectParam* m_pSaturationOffset;

	// filter color
	CEffectParam* m_pPhotoFilterColor;
	CEffectParam* m_pPhotoFilterColorDensity;
	CEffectParam* m_pPhotoFilterColorOffset;
	CEffectParam* m_pPhotoFilterColorDensityOffset;
	Vec4          m_pDefaultPhotoFilterColor;

	// selective color
	CEffectParam* m_pSelectiveColor;
	CEffectParam* m_pSelectiveColorCyans;
	CEffectParam* m_pSelectiveColorMagentas;
	CEffectParam* m_pSelectiveColorYellows;
	CEffectParam* m_pSelectiveColorBlacks;

	// misc adjustments
	CEffectParam* m_pGrainAmount;
	CEffectParam* m_pGrainAmountOffset;

	CEffectParam* m_pSharpenAmount;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CUnderwaterGodRays : public CPostEffect
{
public:
	CUnderwaterGodRays()
	{
		m_nID = EPostEffectID::UnderwaterGodRays;

		AddParamFloat("UnderwaterGodRays_Amount", m_pAmount, 1.0f);
		AddParamInt("UnderwaterGodRays_Quality", m_pQuality, 1); // 0 = low, 1 = med, 2= high, 3= ultra-high, 4= crazy high, and so on

	}

	virtual bool        Preprocess(const SRenderViewInfo& viewInfo);
	virtual void        Render();
	virtual void        Reset(bool bOnSpecChange = false);

	virtual const char* GetName() const
	{
		return "UnderwaterGodRays";
	}

private:

	// float, int
	CEffectParam* m_pAmount, * m_pQuality;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CVolumetricScattering : public CPostEffect
{
public:
	CVolumetricScattering()
	{
		m_nID = EPostEffectID::VolumetricScattering;

		AddParamFloat("VolumetricScattering_Amount", m_pAmount, 0.0f);
		AddParamFloat("VolumetricScattering_Tilling", m_pTilling, 1.0f);
		AddParamFloat("VolumetricScattering_Speed", m_pSpeed, 1.0f);
		AddParamVec4("clr_VolumetricScattering_Color", m_pColor, Vec4(0.5f, 0.75f, 1.0f, 1.0f));

		AddParamInt("VolumetricScattering_Type", m_pType, 0);       // 0 = alien environment, 1 = ?, 2 = ?? ???
		AddParamInt("VolumetricScattering_Quality", m_pQuality, 1); // 0 = low, 1 = med, 2= high, 3= ultra-high, 4= crazy high, and so on
	}

	virtual bool        Preprocess(const SRenderViewInfo& viewInfo);
	virtual void        Render();
	virtual void        Reset(bool bOnSpecChange = false);

	virtual const char* GetName() const
	{
		return "VolumetricScattering";
	}

private:

	// float, int, int
	CEffectParam* m_pAmount, * m_pTilling, * m_pSpeed, * m_pColor, * m_pType, * m_pQuality;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Game/Hud specific post-effects
////////////////////////////////////////////////////////////////////////////////////////////////////

class CAlienInterference : public CPostEffect
{
public:
	CAlienInterference()
	{
		m_nID = EPostEffectID::AlienInterference;

		AddParamFloat("AlienInterference_Amount", m_pAmount, 0);
		AddParamVec4NoTransition("clr_AlienInterference_Color", m_pTintColor, Vec4(Vec3(0.85f, 0.95f, 1.25f) * 0.5f, 1.0f));
	}

	virtual bool        Preprocess(const SRenderViewInfo& viewInfo);
	virtual void        Render();
	virtual void        Reset(bool bOnSpecChange = false);

	virtual const char* GetName() const
	{
		return "AlienInterference";
	}

private:

	// float
	CEffectParam* m_pAmount;
	// vec4
	CEffectParam* m_pTintColor;

};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
class CWaterDroplets : public CPostEffect
{
public:
	CWaterDroplets()
	{
		m_nID = EPostEffectID::WaterDroplets;

		AddParamFloat("WaterDroplets_Amount", m_pAmount, 0.0f);
	}

	virtual bool        Preprocess(const SRenderViewInfo& viewInfo);
	virtual void        Render();
	virtual void        Reset(bool bOnSpecChange = false);

	virtual const char* GetName() const
	{
		return "WaterDroplets";
	}

private:

	// float
	CEffectParam* m_pAmount;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CWaterFlow : public CPostEffect
{
public:
	CWaterFlow()
	{
		m_nID = EPostEffectID::WaterFlow;

		AddParamFloat("WaterFlow_Amount", m_pAmount, 0.0f);
	}

	virtual bool        Preprocess(const SRenderViewInfo& viewInfo);
	virtual void        Render();
	virtual void        Reset(bool bOnSpecChange = false);

	virtual const char* GetName() const
	{
		return "WaterFlow";
	}

private:

	// float
	CEffectParam* m_pAmount;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CScreenFrost : public CPostEffect
{
public:
	CScreenFrost()
	{
		m_nID = EPostEffectID::ScreenFrost;

		AddParamFloat("ScreenFrost_Amount", m_pAmount, 0.0f);             // amount of visible frost
		AddParamFloat("ScreenFrost_CenterAmount", m_pCenterAmount, 1.0f); // amount of visible frost in center

		m_fRandOffset = 0;
	}

	virtual bool        Preprocess(const SRenderViewInfo& viewInfo);
	virtual void        Render();
	virtual void        Reset(bool bOnSpecChange = false);

	virtual const char* GetName() const
	{
		return "ScreenFrost";
	}

private:

	// float, float
	CEffectParam* m_pAmount, * m_pCenterAmount;
	float         m_fRandOffset;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CRainDrops : public CPostEffect
{
public:
	CRainDrops()
	{
		m_nID = EPostEffectID::RainDrops;

		AddParamFloat("RainDrops_Amount", m_pAmount, 0.0f);                        // amount of visible droplets
		AddParamFloat("RainDrops_SpawnTimeDistance", m_pSpawnTimeDistance, 0.35f); // amount of visible droplets
		AddParamFloat("RainDrops_Size", m_pSize, 5.0f);                            // drop size
		AddParamFloat("RainDrops_SizeVariation", m_pSizeVar, 2.5f);                // drop size variation

		m_uCurrentDytex = 0;
		m_pVelocityProj = Vec3(0, 0, 0);

		m_nAliveDrops = 0;

		m_pPrevView.SetIdentity();
		m_pViewProjPrev.SetIdentity();

		m_bFirstFrame = true;
	}

	virtual ~CRainDrops()
	{
		Release();
	}

	virtual int         CreateResources();
	virtual bool        Preprocess(const SRenderViewInfo& viewInfo);
	virtual void        Render();
	virtual void        Reset(bool bOnSpecChange = false);
	virtual void        Release();

	virtual const char* GetName() const;

private:

	// Rain particle properties
	struct SRainDrop
	{
		// set default data
		SRainDrop() : m_pPos(0, 0, 0), m_fSize(5.0f), m_fSizeVar(2.5f), m_fSpawnTime(0.0f), m_fLifeTime(2.0f), m_fLifeTimeVar(1.0f),
			m_fWeight(1.0f), m_fWeightVar(0.25f)
		{

		}

		// Screen position
		Vec3  m_pPos;
		// Size and variation (bigger also means more weight)
		float m_fSize, m_fSizeVar;
		// Spawn time
		float m_fSpawnTime;
		// Life time and variation
		float m_fLifeTime, m_fLifeTimeVar;
		// Weight and variation
		float m_fWeight, m_fWeightVar;
	};

	//in Preprocess(const SRenderViewInfo& viewInfo), check if effect is active
	bool IsActiveRain();

	// Compute current interpolated view matrix
	Matrix44 ComputeCurrentView(int iViewportWidth, int iViewportHeight);

	// Spawn a particle
	void SpawnParticle(SRainDrop*& pParticle, int iRTWidth, int iRTHeight);
	// Update all particles
	void UpdateParticles(int iRTWidth, int iRTHeight);
	// Generate rain drops map
	void RainDropsMapGen();
	// Draw the raindrops
	void DrawRaindrops(int iViewportWidth, int iViewportHeight, int iRTWidth, int iRTHeight);
	// Apply the blur and movement to it
	void ApplyExtinction(CTexture*& rptexPrevRT, int iViewportWidth, int iViewportHeight, int iRTWidth, int iRTHeight);

	// Final draw pass, merge with backbuffer
	void DrawFinal(CTexture*& rptexCurrRT);

	// float
	CEffectParam* m_pAmount;
	CEffectParam* m_pSpawnTimeDistance;
	CEffectParam* m_pSize;
	CEffectParam* m_pSizeVar;

	uint16        m_uCurrentDytex;
	bool          m_bFirstFrame;

	//todo: use generic screen particles
	typedef std::vector<SRainDrop*> SRainDropsVec;
	typedef SRainDropsVec::iterator SRainDropsItor;
	SRainDropsVec    m_pDropsLst;

	Vec3             m_pVelocityProj;
	Matrix44         m_pPrevView;
	Matrix44         m_pViewProjPrev;

	int              m_nAliveDrops;
	static const int m_nMaxDropsCount = 100;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CNightVision : public CPostEffect
{
public:
	CNightVision()
	{
		m_nID = EPostEffectID::NightVision;

		AddParamBool("NightVision_Active", m_pActive, 0);
		AddParamFloat("NightVision_BlindAmount", m_pAmount, 0.0f);

		m_bWasActive = false;
		m_fRandOffset = 0;
		m_fPrevEyeAdaptionBase = 0.25f;
		m_fPrevEyeAdaptionSpeed = 100.0f;

		m_pGradient = 0;
		m_pNoise = 0;
		m_fActiveTime = 0.0f;
	}

	virtual int         CreateResources();
	virtual void        Release();
	virtual bool        Preprocess(const SRenderViewInfo& viewInfo);
	virtual void        Render();
	virtual void        Reset(bool bOnSpecChange = false);

	virtual const char* GetName() const
	{
		return "NightVision";
	}

private:
	// float
	CEffectParam* m_pAmount;

	bool          m_bWasActive;
	float         m_fActiveTime;

	float         m_fRandOffset;
	float         m_fPrevEyeAdaptionSpeed;
	float         m_fPrevEyeAdaptionBase;

	CTexture*     m_pGradient;
	CTexture*     m_pNoise;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CSonarVision : public CPostEffect
{
public:
	CSonarVision()
	{
		m_nID = EPostEffectID::SonarVision;

		AddParamBool("SonarVision_Active", m_pActive, 0);
		AddParamFloat("SonarVision_Amount", m_pAmount, 0.0f);

		m_pGradient = 0;
		m_pNoise = 0;

		m_pszAmbientTechName = "SonarVisionAmbient";
		m_pszSoundHintsTechName = "SonarVisionSoundHint";
		m_pszTechFinalComposition = "SonarVisionFinalComposition";
		m_pszTechNameGhosting = "SonarVisionGhosting";
		m_pszParamNameVS = "SonarVisionParamsVS";
		m_pszParamNamePS = "SonarVisionParamsPS";

		m_pSoundEventsListener = NULL;
	}

	virtual int         CreateResources();
	virtual void        Release();
	virtual bool        Preprocess(const SRenderViewInfo& viewInfo);
	virtual void        Render();
	virtual void        Reset(bool bOnSpecChange = false);

	void                UpdateSoundEvents();

	void                AmbientPass();
	void                SoundHintsPass();
	void                GhostingPass();
	void                FinalComposePass();

	virtual const char* GetName() const
	{
		return "SonarVision";
	}

private:

	CSoundEventsListener* m_pSoundEventsListener;

	CEffectParam*         m_pAmount;
	CTexture*             m_pGradient;
	CTexture*             m_pNoise;

	CCryNameTSCRC         m_pszAmbientTechName;
	CCryNameTSCRC         m_pszSoundHintsTechName;
	CCryNameTSCRC         m_pszTechFinalComposition;
	CCryNameTSCRC         m_pszTechNameGhosting;
	CCryNameR             m_pszParamNameVS;
	CCryNameR             m_pszParamNamePS;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CThermalVision : public CPostEffect
{
public:
	CThermalVision()
	{
		m_nRenderFlags = 0; // thermal vision replaces "rendering" , no need to update
		m_nID = EPostEffectID::ThermalVision;

		AddParamBool("ThermalVision_Active", m_pActive, 0);
		AddParamBool("ThermalVision_RenderOffscreen", m_pRenderOffscreen, 0);
		AddParamFloat("ThermalVision_Amount", m_pAmount, 0.0f);
		AddParamFloat("ThermalVision_CamMovNoiseAmount", m_pCamMovNoiseAmount, 0.0f);

		m_pGradient = 0;
		m_pNoise = 0;

		m_pszAmbientTechName = "ThermalVisionAmbient";
		m_pszTechFinalComposition = "ThermalVisionComposition";
		m_pszTechNameGhosting = "ThermalVisionGhosting";
		m_pszParamNameVS = "ThermalVisionParamsVS";
		m_pszParamNameVS2 = "ThermalVisionParamsVS2";
		m_pszParamNamePS = "ThermalVisionParamsPS";
		m_fBlending = m_fCameraMovementAmount = 0.0f;

		m_bRenderOffscreen = false;
		m_bInit = true;

		::memset(m_pSonarParams, 0, sizeof(m_pSonarParams));
	}

	virtual int  CreateResources();
	virtual void Release();
	virtual bool Preprocess(const SRenderViewInfo& viewInfo);
	virtual void Render();
	virtual void Reset(bool bOnSpecChange = false);

	void         AmbientPass();
	void         HeatSourcesPasses();
	void         GhostingPass();
	void         FinalComposePass();

	bool         GetTransitionEffectState()
	{
		if (m_fBlending < 0.01f)
			return true;

		return false;
	}

	// Get sonar params: position/radius/lifetime/color multiplier
	void GetSonarParams(Vec4 pParams[3])
	{
		pParams[0] = Vec4(m_pSonarParams[0].vPosition, m_pSonarParams[0].fRadius);
		pParams[1] = Vec4(m_pSonarParams[1].vPosition, m_pSonarParams[1].fRadius);
		pParams[2] = Vec4(m_pSonarParams[0].fCurrLifetime / (m_pSonarParams[0].fLifetime + 1e-6f),
		                  m_pSonarParams[0].fColorMul,
		                  m_pSonarParams[1].fCurrLifetime / (m_pSonarParams[1].fLifetime + 1e-6f),
		                  m_pSonarParams[1].fColorMul);
	}

	virtual const char* GetName() const
	{
		return "ThermalVision";
	}

	void UpdateRenderingInfo()
	{
		m_bRenderOffscreen = (m_pRenderOffscreen->GetParam() > 0.5f);
	}

private:
	struct SSonarParams
	{
		Vec3  vPosition;
		float fRadius;
		float fLifetime;
		float fCurrLifetime;
		float fColorMul;
	};

	SSonarParams  m_pSonarParams[2];

	CEffectParam* m_pAmount, * m_pRenderOffscreen, * m_pCamMovNoiseAmount;
	CTexture*     m_pGradient;
	CTexture*     m_pNoise;

	float         m_fBlending;
	float         m_fCameraMovementAmount;

	bool          m_bRenderOffscreen;
	bool          m_bInit;

	CCryNameTSCRC m_pszAmbientTechName;
	CCryNameTSCRC m_pszTechFinalComposition;
	CCryNameTSCRC m_pszTechNameGhosting;
	CCryNameR     m_pszParamNameVS;
	CCryNameR     m_pszParamNameVS2;
	CCryNameR     m_pszParamNamePS;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CHudSilhouettes : public CPostEffect
{
public:
	CHudSilhouettes()
	{
		m_nRenderFlags = 0;
		m_nID = EPostEffectID::HUDSilhouettes;

		m_deferredSilhouettesOptimisedTech = "DeferredSilhouettesOptimised";
		m_psParamName = "psParams";
		m_vsParamName = "vsParams";

		m_bSilhouettesOptimisedTechAvailable = false;
		m_pSilhouetesRT = NULL;

		AddParamBool("HudSilhouettes_Active", m_pActive, 0);
		AddParamFloatNoTransition("HudSilhouettes_Amount", m_pAmount, 1.0f); //0.0f gives funky blending result ? investigate
		AddParamFloatNoTransition("HudSilhouettes_FillStr", m_pFillStr, 0.15f);
		AddParamInt("HudSilhouettes_Type", m_pType, 1);

		FindIfSilhouettesOptimisedTechAvailable();
	}

	virtual bool        Preprocess(const SRenderViewInfo& viewInfo);
	virtual void        Render();
	virtual void        Reset(bool bOnSpecChange = false);

	virtual const char* GetName() const
	{
		return "HUDSilhouettes";
	}

private:

	void RenderDeferredSilhouettes(float fBlendParam, float fType);
	void RenderDeferredSilhouettesOptimised(float fBlendParam, float fType);

	void FindIfSilhouettesOptimisedTechAvailable()
	{

		if (CShaderMan::s_shPostEffectsGame)
		{
			m_bSilhouettesOptimisedTechAvailable = (CShaderMan::s_shPostEffectsGame->mfFindTechnique(m_deferredSilhouettesOptimisedTech)) ? true : false;
		}

	}

	CCryNameTSCRC m_deferredSilhouettesOptimisedTech;
	CCryNameR     m_vsParamName;
	CCryNameR     m_psParamName;

	// float
	CEffectParam* m_pAmount, * m_pFillStr, * m_pType;
	CTexture*     m_pSilhouetesRT;

	bool          m_bSilhouettesOptimisedTechAvailable;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CFlashBang : public CPostEffect
{
	friend class CFlashBangPass;

public:
	CFlashBang()
	{
		m_nID = EPostEffectID::FlashBang;

		AddParamBool("FlashBang_Active", m_pActive, 0);
		AddParamFloat("FlashBang_DifractionAmount", m_pDifractionAmount, 1.0f);
		AddParamFloat("FlashBang_Time", m_pTime, 2.0f);               // flashbang time duration in seconds
		AddParamFloat("FlashBang_BlindAmount", m_pBlindAmount, 0.5f); // flashbang blind time (fraction of frashbang time)

		m_pGhostImage = nullptr;
		m_fSpawnTime = 0.0f;
	}

	virtual ~CFlashBang()
	{
		Release();
	}

	virtual bool        Preprocess(const SRenderViewInfo& viewInfo);
	virtual void        Release();
	virtual void        Render();
	virtual void        Reset(bool bOnSpecChange = false);

	virtual const char* GetName() const
	{
		return "FlashBang";
	}

private:

	SDynTexture* m_pGhostImage;

	float        m_fSpawnTime;

	// float, float
	CEffectParam* m_pTime, * m_pDifractionAmount, * m_pBlindAmount;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CPostAA : public CPostEffect
{
public:
	CPostAA()
	{
		m_nRenderFlags = 0;
		m_nID = EPostEffectID::PostAA;
	}

	virtual int         CreateResources() { return 1; }
	virtual void        Release() {}
	virtual bool        Preprocess(const SRenderViewInfo& viewInfo) { return true; }
	virtual void        Render();
	virtual void        Reset(bool bOnSpecChange = false) {}

	virtual const char* GetName() const
	{
		return "PostAA";
	}

private:
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CPostStereo : public CPostEffect
{
public:
	CPostStereo()
	{
		//		m_nRenderFlags = 0;
		m_nID = EPostEffectID::PostStereo;
	}

	virtual bool        Preprocess(const SRenderViewInfo& viewInfo);
	virtual void        Render();
	virtual void        Reset(bool bOnSpecChange = false);
	virtual const char* GetName() const
	{
		return "PostStereo";
	}
private:

};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CImageGhosting : public CPostEffect
{
public:
	CImageGhosting()
	{
		m_nRenderFlags = 0;
		m_nID = EPostEffectID::ImageGhosting;
		AddParamFloat("ImageGhosting_Amount", m_pAmount, 0);
		m_bInit = true;
	}

	virtual bool        Preprocess(const SRenderViewInfo& viewInfo);
	virtual void        Render();
	virtual void        Reset(bool bOnSpecChange = false);
	virtual const char* GetName() const
	{
		return "ImageGhosting";
	}

private:

	CEffectParam* m_pAmount;
	bool          m_bInit;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
struct SHudData
{
public:
	SHudData() : pRE(0), pShaderItem(0), pShaderResources(0), pRO(0), pDiffuse(0), pFlashPlayer(0), nSortVal(0), nFlashWidth(0), nFlashHeight(0)
	{
	}

	SHudData(const CRenderElement* pInRE, const SShaderItem* pInShaderItem, const CShaderResources* pInShaderResources, CRenderObject* pInRO) :
		pRE(pInRE),
		pShaderItem(pInShaderItem),
		pShaderResources(pInShaderResources),
		pRO(pInRO),
		pDiffuse(0),
		pFlashPlayer(0),
		nSortVal(0),
		nFlashWidth(0),
		nFlashHeight(0)
	{
	}

public:
	const CRenderElement* pRE;
	CRenderObject*          pRO;
	const SShaderItem*      pShaderItem; // to be removed after Alpha MS
	const CShaderResources* pShaderResources;

	SEfResTexture*          pDiffuse;
	IFlashPlayer*           pFlashPlayer;

	uint32                  nSortVal;

	int16                   nFlashWidth;
	int16                   nFlashHeight;

	static int16            s_nFlashWidthMax;
	static int16            s_nFlashHeightMax;

private:
	friend class CHud3D;
	void Init();
};

////////////////////////////////////////////////////////////////////////////////////////////////////
struct HudDataSortCmp
{
	bool operator()(const SHudData& h0, const SHudData& h1) const
	{
		return h0.nSortVal > h1.nSortVal;
	}
};

////////////////////////////////////////////////////////////////////////////////////////////////////
class CHud3D : public CPostEffect
{
	friend class CHud3DPass;

public:

	typedef CThreadSafeRendererContainer<SHudData> SHudDataVec;

public:

	CHud3D()
	{
		m_nRenderFlags = PSP_REQUIRES_UPDATE;
		m_nID = EPostEffectID::HUD3D;

		m_pHUD_RT = 0;
		m_pHUDScaled_RT = 0;
		m_pNoise = 0;

		m_mProj.SetIdentity();

		AddParamFloatNoTransition("HUD3D_OpacityMultiplier", m_pOpacityMul, 1.0f);
		AddParamFloatNoTransition("HUD3D_GlowMultiplier", m_pGlowMul, 1.0f);
		AddParamVec4NoTransition("HUD3D_GlowColorMultiplier", m_pGlowColorMul, Vec4(1.0f, 1.0f, 1.0f, 1.0f));
		AddParamFloatNoTransition("HUD3D_ChromaShift", m_pChromaShift, 0.0f);
		AddParamFloatNoTransition("HUD3D_DofMultiplier", m_pDofMultiplier, 1.0f);
		AddParamVec4NoTransition("clr_HUD3D_Color", m_pHudColor, Vec4(1.0f, 1.0f, 1.0f, 1.0f));

		AddParamFloat("HUD3D_FOV", m_pFOV, 55.0f);

		AddParamFloat("HUD3D_Interference", m_pInterference, 0.0f);

		AddParamFloat("HUD3D_InterferenceMinValue", m_pInterferenceMinValue, 0.0f);

		// x= hudItemOverrideStrength, y= interferenceRandFrequency, z= dofInterferenceStrength, w = free
		AddParamVec4("HUD3D_InterferenceParams0", m_pInterferenceParams[0], Vec4(1.0f, 0.055f, 0.8f, 0.0f));

		// x = randomGrainStrengthScale, y= randomFadeStrengthScale, z= chromaShiftStrength, w= chromaShiftDist
		AddParamVec4("HUD3D_InterferenceParams1", m_pInterferenceParams[1], Vec4(0.3f, 0.4f, 0.25f, 1.0f));

		// x= disruptScale, y= disruptMovementScale, z= noiseStrength, w = barScale
		AddParamVec4("HUD3D_InterferenceParams2", m_pInterferenceParams[2], Vec4(0.3f, 1.0f, 0.2f, 0.8f));

		// xyz= barColor, w= barColorMultiplier
		AddParamVec4("HUD3D_InterferenceParams3", m_pInterferenceParams[3], Vec4(0.39f, 0.6f, 1.0f, 5.0f));

		AddParamBool("HUD3D_OverrideCacheDelay", m_pOverideCacheDelay, 0.0f);

		m_pGeneralTechName = "General";
		m_pDownsampleTechName = "Downsample4x4";
		m_pUpdateBloomTechName = "UpdateBloomRT";
		m_pHudParamName = "HudParams";
		m_pHudEffectsParamName = "HudEffectParams";
		m_pMatViewProjParamName = "mViewProj";
		m_pHudTexCoordParamName = "HudTexCoordParams";
		m_pHudOverrideColorMultParamName = "HudOverrideColorMult";

		m_nFlashUpdateFrameID = -1;
		m_accumulatedFrameTime = 0.0f;

		m_maxParallax = 0.0f;
		m_interferenceRandTimer = 0.0f;

		m_interferenceRandNums = Vec4(0.0f, 0.0f, 0.0f, 0.0f);

#ifndef _RELEASE
		m_pTexToTexTechName = "TextureToTexture";
		m_bDebugView = false;
		m_bWireframeEnabled = false;
#endif
	}

	virtual int         CreateResources();
	virtual void        Release();
	virtual bool        Preprocess(const SRenderViewInfo& viewInfo);

	virtual void        Update();
	virtual void        OnBeginFrame(const SRenderingPassInfo& passInfo);

	virtual void        Reset(bool bOnSpecChange = false);
	virtual void        AddRE(const CRenderElement* re, const SShaderItem* pShaderItem, CRenderObject* pObj, const SRenderingPassInfo& passInfo);
	virtual void        Render();

	void                FlashUpdateRT();
	void                UpdateBloomRT(CTexture* pDstRT, CTexture* pBlurDst);

	virtual const char* GetName() const
	{
		return "3DHud";
	}

	CEffectParam* GetFov() const
	{
		return m_pFOV;
	}

	void SetMaxParallax(float maxParallax)
	{
		m_maxParallax = maxParallax;
	}

private:
	// Shared shader params/textures setup
	void CalculateProjMatrix();
	void SetShaderParams(SHudData& pData);
	void SetTextures(SHudData& pData);
	void RenderMesh(const CRenderElement* pRE, SShaderPass* pPass);

	void DownsampleHud4x4(CTexture* pDstRT);
	void FinalPass();
	void ReleaseFlashPlayerRef(const uint32 nThreadID);
	void RenderFinalPass();

private:

	Vec4  m_interferenceRandNums;
	float m_interferenceRandTimer;

	// Vec4, float
	CEffectParam* m_pOpacityMul;
	CEffectParam* m_pGlowMul;
	CEffectParam* m_pGlowColorMul;
	CEffectParam* m_pChromaShift;
	CEffectParam* m_pHudColor;
	CEffectParam* m_pDofMultiplier;
	CEffectParam* m_pFOV;
	CEffectParam* m_pInterference;
	CEffectParam* m_pInterferenceMinValue;
	CEffectParam* m_pInterferenceParams[4];
	CEffectParam* m_pOverideCacheDelay;

	CTexture*     m_pHUD_RT;
	CTexture*     m_pHUDScaled_RT;
	CTexture*     m_pNoise;

	SHudDataVec   m_pRenderData[RT_COMMAND_BUF_COUNT];

	Matrix44A     m_mProj;

	CCryNameTSCRC m_pGeneralTechName;
	CCryNameTSCRC m_pDownsampleTechName;
	CCryNameTSCRC m_pUpdateBloomTechName;
	CCryNameR     m_pHudParamName;
	CCryNameR     m_pHudEffectsParamName;
	CCryNameR     m_pMatViewProjParamName;
	CCryNameR     m_pHudTexCoordParamName;
	CCryNameR     m_pHudOverrideColorMultParamName;

	float         m_maxParallax;
	int32         m_nFlashUpdateFrameID;
	float         m_accumulatedFrameTime;

#ifndef _RELEASE
	CCryNameTSCRC m_pTexToTexTechName;
	bool          m_bDebugView;
	bool          m_bWireframeEnabled;
#endif
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CFilterKillCamera : public CPostEffect
{
	friend class CKillCameraPass;

public:

	CFilterKillCamera()
	{
		m_nID = EPostEffectID::KillCamera;

		AddParamBool("FilterKillCamera_Active", m_pActive, 0);
		AddParamInt("FilterKillCamera_Mode", m_pMode, 0);
		AddParamFloat("FilterKillCamera_GrainStrength", m_pGrainStrength, 0.0f);
		AddParamVec4("FilterKillCamera_ChromaShift", m_pChromaShift, Vec4(1.0f, 0.5f, 0.1f, 1.0f)); // xyz = offset, w = strength
		AddParamVec4("FilterKillCamera_Vignette", m_pVignette, Vec4(1.0f, 1.0f, 0.5f, 1.4f));       // xy = screen scale, z = radius, w = blind noise vignette scale
		AddParamVec4("FilterKillCamera_ColorScale", m_pColorScale, Vec4(1.0f, 1.0f, 1.0f, 1.0f));
		AddParamVec4("FilterKillCamera_Blindness", m_pBlindness, Vec4(0.5f, 0.5f, 1.0f, 0.7f)); // x = blind duration, y = blind fade out duration, z = blindness grey scale, w = blind noise min scale

		m_blindTimer = 0.0f;
		m_lastMode = 0;
	}

	virtual int         Initialize();
	virtual bool        Preprocess(const SRenderViewInfo& viewInfo);
	virtual void        Render();
	virtual void        Reset(bool bOnSpecChange = false);

	virtual const char* GetName() const
	{
		return "FilterKillCamera";
	}

private:

	CCryNameTSCRC m_techName;
	CCryNameR     m_paramName;

	CEffectParam* m_pGrainStrength;
	CEffectParam* m_pChromaShift;
	CEffectParam* m_pVignette;
	CEffectParam* m_pColorScale;
	CEffectParam* m_pBlindness;
	CEffectParam* m_pMode;
	float         m_blindTimer;
	int           m_lastMode;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CNanoGlass : public CPostEffect
{
public:

	struct SRenderData
	{
		SRenderData()
		{
			pRenderElement = NULL;
			pRenderObject = NULL;
		}

		const CRenderElement* pRenderElement;
		const CRenderObject*    pRenderObject;
	};

	CNanoGlass()
	{
		m_nID = EPostEffectID::NanoGlass;
		m_pHexOutline = NULL;
		m_pHexRand = NULL;
		m_pHexGrad = NULL;
		m_pNoise = NULL;
		m_pHudMask = NULL;
		m_pHudMaskTemp = NULL;
		m_noiseTime = 0.0f;
		m_time = 0.0f;
		m_cornerGlowCountdownTimer = 0.0f;
		m_cornerGlowStrength = 0.0f;

		AddParamBool("NanoGlass_Active", m_pActive, 0);
		AddParamFloat("NanoGlass_Alpha", m_pEffectAlpha, 0.0f);
		AddParamFloat("NanoGlass_StartAnimPos", m_pStartAnimPos, 0.0f);
		AddParamFloat("NanoGlass_EndAnimPos", m_pEndAnimPos, 0.0f);
		AddParamFloat("NanoGlass_Brightness", m_pBrightness, 1.0f);

		AddParamVec4("NanoGlass_Movement0", m_pMovement[0], Vec4(0.0f, 0.0f, 0.0f, 0.0f));  // x = timeSpeed, y = waveStrength, z = movementStretchScale, w = movementWaveFrequency
		AddParamVec4("NanoGlass_Movement1", m_pMovement[1], Vec4(0.0f, 0.0f, 0.16f, 0.0f)); // x = movementStrengthX, y = movementStrengthY, z = noiseSpeed, w = vignetteStrength
		AddParamVec4("NanoGlass_Filter", m_pFilter, Vec4(0.0f, 0.0f, 0.0f, 0.0f));          // x = backBufferBrightessScalar, y = mistAlpha, z = noiseStrength, w = overChargeStrength
		AddParamVec4("NanoGlass_HexColor", m_pHexColor, Vec4(0.0f, 0.0f, 0.0f, 1.0f));      // xyz = Hex color, w = Hex color strength
		AddParamVec4("NanoGlass_Hex", m_pHexParams, Vec4(26.25f, 0.3f, 0.1f, 10.0f));       // x = hexTexScale, y = hexOutlineMin, z = vignetteTexOffset, w = vignetteSaturation
		AddParamVec4("NanoGlass_Vignette", m_pVignette, Vec4(1.55f, 1.2f, 0.0f, 0.0f));     // xy = vignetteScreenScale , z = mistSinStrength, w = free
		AddParamVec4("NanoGlass_CornerGlow", m_pCornerGlow, Vec4(3.0f, 7.0f, 0.3f, 0.0f));  // x = minCornerGlow, y = maxCornerGlow , z = maxCornerGlowRandTime, w = free

		m_nRenderFlags = 0;

		m_paramName = "psParams";
		m_shaderTechName = "NanoGlass";
		m_viewProjMatrixParamName = "mViewProj";
	}

	virtual int         CreateResources();
	virtual void        Release();
	virtual bool        Preprocess(const SRenderViewInfo& viewInfo);
	virtual void        Render();
	virtual void        Reset(bool bOnSpecChange = false);

	virtual const char* GetName() const
	{
		return "NanoGlass";
	}

	virtual void OnBeginFrame(const SRenderingPassInfo& passInfo)
	{
		const threadID nThreadID = gRenDev->GetMainThreadID();
		if (!passInfo.IsRecursivePass())
			m_pRenderData[nThreadID].pRenderElement = NULL;
	}

	virtual void AddRE(const CRenderElement* pRE, const SShaderItem* pShaderItem, CRenderObject* pObj, const SRenderingPassInfo& passInfo);

private:

	void RenderPass(bool bDebugPass, bool bIsHudRendering);
	void CreateHudMask(CHud3D* pHud3D);
	void DownSampleBackBuffer();

	SRenderData   m_pRenderData[RT_COMMAND_BUF_COUNT];

	CCryNameTSCRC m_shaderTechName;
	CCryNameR     m_paramName;
	CCryNameR     m_viewProjMatrixParamName;

	float         m_time;
	float         m_noiseTime;
	float         m_cornerGlowCountdownTimer;
	float         m_cornerGlowStrength;

	CTexture*     m_pHexRand;
	CTexture*     m_pHexGrad;
	CTexture*     m_pHexOutline;
	CTexture*     m_pNoise;
	CTexture*     m_pHudMask;
	CTexture*     m_pHudMaskTemp;

	CEffectParam* m_pEffectAlpha;
	CEffectParam* m_pStartAnimPos;
	CEffectParam* m_pEndAnimPos;
	CEffectParam* m_pBrightness;

	CEffectParam* m_pVignette;
	CEffectParam* m_pMovement[2];
	CEffectParam* m_pFilter;
	CEffectParam* m_pHexColor;
	CEffectParam* m_pHexParams;
	CEffectParam* m_pCornerGlow;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CScreenBlood : public CPostEffect
{
public:
	CScreenBlood()
	{
		m_nRenderFlags = 0;
		m_nID = EPostEffectID::ScreenBlood;
		AddParamFloat("ScreenBlood_Amount", m_pAmount, 0.0f);                        // damage amount
		AddParamVec4("ScreenBlood_Border", m_pBorder, Vec4(0.0f, 0.0f, 2.0f, 1.0f)); // Border: x=xOffset y=yOffset z=range w=alpha
	}

	virtual bool        Preprocess(const SRenderViewInfo& viewInfo);
	virtual void        Render();
	virtual void        Reset(bool bOnSpecChange = false);
	virtual const char* GetName() const
	{
		return "ScreenBlood";
	}

private:

	CEffectParam * m_pAmount;
	CEffectParam* m_pBorder;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CScreenFader : public CPostEffect
{
public:
	CScreenFader()
	{
		m_nRenderFlags = 0;
		m_nID = EPostEffectID::ScreenFader;
		AddParamVec4("ScreenFader_Color", m_pColor, Vec4(0.0f, 0.0f, 0.0f, 0.0f));  // Fader color
	}

	virtual bool        Preprocess(const SRenderViewInfo& viewInfo);
	virtual void        Render();
	virtual void        Reset(bool bOnSpecChange = false);
	virtual const char* GetName() const
	{
		return "ScreenFader";
	}

private:

	CEffectParam * m_pColor;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class CPost3DRenderer : public CPostEffect
{
public:

	enum ERenderMeshMode
	{
		eRMM_Default = 0,
		eRMM_Custom,
		eRMM_DepthOnly
	};

	enum EPost3DRendererFlags
	{
		eP3DR_HasSilhouettes                     = (1 << 0),
		eP3DR_DirtyFlashRT                       = (1 << 1),
		eP3DR_ClearOnResolveTempRT               = (1 << 2),
		eP3DR_ClearOnResolveFlashRT              = (1 << 3),
		eP3DR_ClearOnResolvePrevBackBufferCopyRT = (1 << 4)
	};

	CPost3DRenderer()
	{
		m_nID = EPostEffectID::Post3DRenderer;

		AddParamBool("Post3DRenderer_Active", m_pActive, 0);
		AddParamFloat("Post3DRenderer_FOVScale", m_pFOVScale, 0.5f);
		AddParamFloat("Post3DRenderer_SilhouetteStrength", m_pSilhouetteStrength, 0.3f);
		AddParamFloat("Post3DRenderer_EdgeFadeScale", m_pEdgeFadeScale, 0.2f); // Between 0 and 1.0
		AddParamFloat("Post3DRenderer_PixelAspectRatio", m_pPixelAspectRatio, 1.0f);
		AddParamVec4("Post3DRenderer_Ambient", m_pAmbient, Vec4(0.0f, 0.0f, 0.0f, 0.2f));

		m_gammaCorrectionTechName = "Post3DRendererGammaCorrection";
		m_alphaCorrectionTechName = "Post3DRendererAlphaCorrection";
		m_texToTexTechName = "TextureToTexture";
		m_customRenderTechName = "CustomRenderPass";
		m_combineSilhouettesTechName = "Post3DRendererSilhouttes";
		m_silhouetteTechName = "BinocularView";

		m_psParamName = "psParams";
		m_vsParamName = "vsParams";

		m_nRenderFlags = 0;

		m_pFlashRT = NULL;
		m_pTempRT = NULL;

		m_edgeFadeScale = 0.0f;
		m_alpha = 1.0f;
		m_groupCount = 0;
		m_post3DRendererflags = 0;
		m_deferDisableFrameCountDown = 0;
	}

	virtual bool        Preprocess(const SRenderViewInfo& viewInfo);
	virtual void        Render();
	virtual void        Reset(bool bOnSpecChange = false);

	virtual const char* GetName() const
	{
		return "Post3DRenderer";
	}

private:
	bool HasModelsToRender() const;

	void ClearFlashRT();

	void RenderGroup(uint8 groupId);
	void RenderMeshes(uint8 groupId, float screenRect[4], ERenderMeshMode renderMeshMode = eRMM_Default);
	void RenderDepth(uint8 groupId, float screenRect[4], bool bCustomRender = false);
	void AlphaCorrection();
	void GammaCorrection(float screenRect[4]);
	void RenderSilhouettes(uint8 groupId, float screenRect[4]);
	void SilhouetteOutlines(CTexture* pOutlineTex, CTexture* pGlowTex);
	void SilhouetteGlow(CTexture* pOutlineTex, CTexture* pGlowTex);
	void SilhouetteCombineBlurAndOutline(CTexture* pOutlineTex, CTexture* pGlowTex);
	uint64 ApplyShaderQuality(EShaderType shaderType = eST_General);

	CCryNameTSCRC m_gammaCorrectionTechName;
	CCryNameTSCRC m_alphaCorrectionTechName;
	CCryNameTSCRC m_texToTexTechName;
	CCryNameTSCRC m_customRenderTechName;
	CCryNameTSCRC m_combineSilhouettesTechName;
	CCryNameTSCRC m_silhouetteTechName;

	CCryNameR     m_psParamName;
	CCryNameR     m_vsParamName;

	CEffectParam* m_pFOVScale;
	CEffectParam* m_pAmbient;
	CEffectParam* m_pSilhouetteStrength;
	CEffectParam* m_pEdgeFadeScale;
	CEffectParam* m_pPixelAspectRatio;

	CTexture*     m_pFlashRT;
	CTexture*     m_pTempRT;

	float         m_alpha;
	float         m_edgeFadeScale;

	uint8         m_groupCount;
	uint8         m_post3DRendererflags;
	uint8         m_deferDisableFrameCountDown;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

#endif
