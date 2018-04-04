// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#pragma once

#include "ITexture.h"

enum EStereoDevice
{
	STEREO_DEVICE_NONE      = 0,
	STEREO_DEVICE_FRAMECOMP = 1,
	STEREO_DEVICE_DRIVER    = 2, //!< Nvidia and AMD drivers.

	STEREO_DEVICE_DEFAULT   = 100 //!< Auto-detect device.
};

enum EStereoSubmission
{
	STEREO_SUBMISSION_SEQUENTIAL        = 0, // first left eye submission, then right eye submission [1x GPU]
	STEREO_SUBMISSION_PARALLEL          = 1, // left and right eye are submitted using the same submissions [2x GPU]
	STEREO_SUBMISSION_PARALLEL_MULTIRES = 2  // left/right and wide/near are submitted using the same submission [4x GPU]
};

enum EStereoMode
{
	STEREO_MODE_NO_STEREO      = 0, //!< Stereo disabled.
	STEREO_MODE_DUAL_RENDERING = 1,
	STEREO_MODE_POST_STEREO    = 2, //!< Extract from depth.
	STEREO_MODE_MONO_RENDERING = 3,
	STEREO_MODE_COUNT,
};

enum EStereoOutput
{
	STEREO_OUTPUT_STANDARD              = 0,
	STEREO_OUTPUT_SIDE_BY_SIDE_SQUEEZED = 1,
	STEREO_OUTPUT_CHECKERBOARD          = 2,
	STEREO_OUTPUT_ABOVE_AND_BELOW       = 3,
	STEREO_OUTPUT_SIDE_BY_SIDE          = 4,
	STEREO_OUTPUT_LINE_BY_LINE          = 5,
	STEREO_OUTPUT_ANAGLYPH              = 6,
	STEREO_OUTPUT_HMD                   = 7,
	STEREO_OUTPUT_COUNT,
};

enum EStereoDeviceState
{
	STEREO_DEVSTATE_OK = 0,
	STEREO_DEVSTATE_UNSUPPORTED_DEVICE,
	STEREO_DEVSTATE_REQ_1080P,
	STEREO_DEVSTATE_REQ_FRAMEPACKED,
	STEREO_DEVSTATE_BAD_DRIVER,
	STEREO_DEVSTATE_REQ_FULLSCREEN
};

// ------------------------------------------------------------------------------------------------------------------
// RenderLayer namespace gathers layer definitions and properties common to all VR platforms

namespace RenderLayer
{
typedef uint32 TLayerId;

enum ELayerType
{
	eLayer_Scene3D = 0,        // regular 3D scene (requires two eyes)
	eLayer_Quad,               // quad in 3D space (monoscopic)
	eLayer_Quad_HeadLoked,     // quad locked to the head position (monoscopic)

	eLayer_NumLayerTypes
};

// supported 3D layers
enum ESceneLayers
{
	eSceneLayers_0 = 0,

	eSceneLayers_Total,
};

// supported quad layers
enum EQuadLayers
{
	eQuadLayers_0 = 0,   // Default layer used to render FLASH 2D and hmd_projection != 0
	eQuadLayers_1,

	eQuadLayers_Total
};

// TODO: evaluate this possibility
enum ELayerList
{
	eLayerList_SceneLayer_0 = 0,
	eLayerList_QuadLayer_0,
	eLayerList_QuadLayer_1,

	eLayerList_Total
};

// helper functions
//inline const char * GetLayerList_UICONFIG() { COMPILE_TIME_ASSERT(eLayerList_SceneLayer_0 == 0 && eLayerList_Total == eLayerList_QuadLayer_1 + 1); return "enum_int:SCENE3D_0,QUAD_0, QUAD_1"; }
inline const char*  GetLayerType_UICONFIG()             { static_assert(eLayer_Scene3D == 0 && eLayer_NumLayerTypes == 3, "Unexpected enum value!"); return "enum_int:SCENE3D=0,QUAD=1,QUAD_LOCKED=2"; }
inline const char*  GetQuadLayer_UICONFIG()             { static_assert(eQuadLayers_0 == 0 && eQuadLayers_Total == eQuadLayers_1 + 1, "Unexpected enum value!"); return "enum_int:QUAD_0=0, QUAD_1=1"; }
inline const char*  GetQuadLayerType_UICONFIG()         { static_assert(eLayer_Quad == 1 && eLayer_Quad_HeadLoked == 2, "Unexpected enum value!"); return "enum_int:QuadFree=1, QuadLocked=2"; }
inline bool         IsQuadLayer(TLayerId id)            { return id >= eQuadLayers_0 && id < eQuadLayers_Total; }
inline bool         IsScene3DLayer(TLayerId id)         { return id >= eSceneLayers_0 && id < eSceneLayers_Total; }
inline EQuadLayers  SafeCastToQuadLayer(TLayerId id)    { return IsQuadLayer(id) ? static_cast<EQuadLayers>(id) : eQuadLayers_0; }
inline ESceneLayers SafeCastToScene3DLayer(TLayerId id) { return IsScene3DLayer(id) ? static_cast<ESceneLayers>(id) : eSceneLayers_0; }

// layer properties
class CProperties
{
public:
	CProperties() : m_type(eLayer_Scene3D), m_id(eSceneLayers_0), m_bActive(false), m_pTexture(nullptr) {}
	~CProperties() { SAFE_RELEASE(m_pTexture); }

	const QuatTS& GetPose()  const               { return m_pose; }
	ELayerType    GetType()  const               { return m_type; }
	TLayerId      GetId()    const               { return m_id;   }
	bool          IsActive() const               { return m_bActive; }
	ITexture*     GetTexture() const             { return m_pTexture; }

	void          SetPose(const QuatTS& pose_)   { m_pose = pose_; }
	void          SetType(ELayerType type_)      { m_type = type_; }
	void          SetId(TLayerId id_)            { m_id = id_; }
	void          SetActive(bool bActive_)       { m_bActive = bActive_;  }
	void          SetTexture(ITexture* pTexture) { SAFE_RELEASE(m_pTexture); m_pTexture = pTexture; }
private:
	QuatTS     m_pose;
	ELayerType m_type;
	TLayerId   m_id;
	bool       m_bActive;
	ITexture*  m_pTexture;
};
};

// ------------------------------------------------------------------------------------------------------------------
struct IHmdRenderer;
struct IStereoRenderer
{
	enum EHmdRender
	{
		eHR_Eyes = 0,
		eHR_Latency
	};

	// <interfuscator:shuffle>
	virtual ~IStereoRenderer(){}

	virtual EStereoDevice      GetDevice() = 0;
	virtual EStereoDeviceState GetDeviceState() = 0;
	virtual void               GetInfo(EStereoDevice* device, EStereoMode* mode, EStereoOutput* output, EStereoDeviceState* state) = 0;

	virtual bool               GetStereoEnabled() = 0;
	virtual float              GetStereoStrength() = 0;
	virtual float              GetMaxSeparationScene(bool half = true) = 0;
	virtual float              GetZeroParallaxPlaneDist() = 0;
	virtual void               GetNVControlValues(bool& stereoEnabled, float& stereoStrength) = 0;

	virtual void               OnHmdDeviceChanged(IHmdDevice* hmdDevice) = 0;
	virtual IHmdRenderer*      GetIHmdRenderer() { return nullptr; }

	// </interfuscator:shuffle>
};

// ------------------------------------------------------------------------------------------------------------------
struct IHmdRenderer
{
	// <interfuscator:shuffle>
	virtual ~IHmdRenderer() {};

	virtual bool Initialize() = 0;
	virtual void Shutdown() = 0;

	virtual void ReleaseBuffers() = 0;
	virtual void OnResolutionChanged() = 0;

	// Called at the beginning of the render frame
	virtual void PrepareFrame() = 0;

	// Submits the frame to the Hmd device
	virtual void                      SubmitFrame() = 0;

	virtual void                      RenderSocialScreen() = 0;

	virtual RenderLayer::CProperties* GetQuadLayerProperties(RenderLayer::EQuadLayers id) = 0;
	virtual RenderLayer::CProperties* GetSceneLayerProperties(RenderLayer::ESceneLayers id) = 0;

	// </interfuscator:shuffle>
};
