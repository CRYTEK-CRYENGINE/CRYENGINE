// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AnimCameraNode.h"
#include "CompoundSplineTrack.h"
#include <CryMath/PNoise3.h>
#include "AnimSplineTrack.h"

#include <CrySystem/ISystem.h>
#include <Cry3DEngine/I3DEngine.h>
#include <CrySystem/IConsole.h>
#include <CryEntitySystem/IEntitySystem.h>
#include <CrySystem/ITimer.h>

CAnimCameraNode* CAnimCameraNode::m_pLastFrameActiveCameraNode;

#define s_nodeParamsInitialized s_nodeParamsInitializedCam
#define s_nodeParams            s_nodeParamsCam
#define AddSupportedParam       AddSupportedParamCam

namespace
{
bool s_nodeParamsInitialized = false;
std::vector<CAnimNode::SParamInfo> s_nodeParams;

void AddSupportedParam(const char* sName, int paramId, EAnimValue valueType)
{
	CAnimNode::SParamInfo param;
	param.name = sName;
	param.paramType = paramId;
	param.valueType = valueType;
	s_nodeParams.push_back(param);
}
};

CAnimCameraNode::CAnimCameraNode(const int id)
	: CAnimEntityNode(id)
	, m_fFOV(60.0f)
	, m_fDOF(ZERO)
	, m_fNearZ(DEFAULT_NEAR)
	, m_cv_r_PostProcessEffects(NULL)
	, m_bJustActivated(false)
	, m_cameraShakeSeedValue(0)
{
	SetSkipInterpolatedCameraNode(false);
	CAnimCameraNode::Initialize();
}

CAnimCameraNode::~CAnimCameraNode()
{
}

void CAnimCameraNode::Initialize()
{
	if (!s_nodeParamsInitialized)
	{
		s_nodeParamsInitialized = true;
		s_nodeParams.reserve(8);
		AddSupportedParam("Position", eAnimParamType_Position, eAnimValue_Vector);
		AddSupportedParam("Rotation", eAnimParamType_Rotation, eAnimValue_Quat);
		AddSupportedParam("Fov", eAnimParamType_FOV, eAnimValue_Float);
		AddSupportedParam("Event", eAnimParamType_Event, eAnimValue_Unknown);
		AddSupportedParam("Shake", eAnimParamType_ShakeMultiplier, eAnimValue_Vector4);
		AddSupportedParam("Depth of Field", eAnimParamType_DepthOfField, eAnimValue_Vector);
		AddSupportedParam("Near Z", eAnimParamType_NearZ, eAnimValue_Float);
		AddSupportedParam("Shutter Speed", eAnimParamType_ShutterSpeed, eAnimValue_Float);
	}
}

unsigned int CAnimCameraNode::GetParamCount() const
{
	return s_nodeParams.size();
}

CAnimParamType CAnimCameraNode::GetParamType(unsigned int nIndex) const
{
	if (nIndex < s_nodeParams.size())
	{
		return s_nodeParams[nIndex].paramType;
	}

	return eAnimParamType_Invalid;
}

bool CAnimCameraNode::GetParamInfoFromType(const CAnimParamType& paramId, SParamInfo& info) const
{
	for (unsigned int i = 0; i < s_nodeParams.size(); i++)
	{
		if (s_nodeParams[i].paramType == paramId)
		{
			info = s_nodeParams[i];
			return true;
		}
	}

	return false;
}

void CAnimCameraNode::CreateDefaultTracks()
{
	CreateTrack(eAnimParamType_Position);
	CreateTrack(eAnimParamType_Rotation);
	CreateTrack(eAnimParamType_FOV);
};

void CAnimCameraNode::Animate(SAnimContext& animContext)
{
	CAnimEntityNode::Animate(animContext);

	// If we are in editing mode we only apply post effects if the camera is active in the viewport
	const bool bApplyPostEffects = !gEnv->IsEditing() || animContext.m_activeCameraEntity == GetEntityId();

	//------------------------------------------------------------------------------
	///Depth of field track
	IEntity* pEntity = GetEntity();

	IAnimTrack* pDOFTrack = GetTrackForParameter(eAnimParamType_DepthOfField);

	Vec3 dof = Vec3(ZERO);
	if (pDOFTrack)
	{
		dof = stl::get<Vec3>(pDOFTrack->GetValue(animContext.time));
	}

	IAnimTrack* pFOVTrack = GetTrackForParameter(eAnimParamType_FOV);
	float fov = m_fFOV;

	bool bFOVTrackEnabled = false;
	if (pFOVTrack && !(pFOVTrack->GetFlags() & IAnimTrack::eAnimTrackFlags_Disabled))
	{
		bFOVTrackEnabled = true;
	}

	if (bFOVTrackEnabled && !IsSkipInterpolatedCameraNodeEnabled())
	{
		fov = stl::get<float>(pFOVTrack->GetValue(animContext.time));
	}

	IAnimTrack* pNearZTrack = GetTrackForParameter(eAnimParamType_NearZ);
	float nearZ = m_fNearZ;

	bool bNearZTrackEnabled = false;

	if (pNearZTrack && !(pNearZTrack->GetFlags() & IAnimTrack::eAnimTrackFlags_Disabled))
	{
		bNearZTrackEnabled = true;
	}

	if (bNearZTrackEnabled)
	{
		nearZ = stl::get<float>(pNearZTrack->GetValue(animContext.time));
	}

	// is this camera active ?
	if (bApplyPostEffects && (gEnv->pMovieSystem->GetCameraParams().cameraEntityId == GetEntityId()))
	{
		if (pEntity)
		{
			pEntity->SetFlags(pEntity->GetFlags() | ENTITY_FLAG_TRIGGER_AREAS);
		}

		SCameraParams CamParams = gEnv->pMovieSystem->GetCameraParams();

		if (bFOVTrackEnabled)
		{
			CamParams.fFOV = DEG2RAD(fov);
		}

		if (bNearZTrackEnabled)
		{
			CamParams.fNearZ = nearZ;
		}

		if ((bNearZTrackEnabled || bFOVTrackEnabled) && !IsSkipInterpolatedCameraNodeEnabled())
		{
			gEnv->pMovieSystem->SetCameraParams(CamParams);
		}

		bool bDOFTrackEnabled = false;
		if (pDOFTrack && !(pDOFTrack->GetFlags() & IAnimTrack::eAnimTrackFlags_Disabled))
		{
			bDOFTrackEnabled = true;
		}

		// If there is a "Depth of Field Track" with current camera
		if (bDOFTrackEnabled)
		{
			// Active Depth of field
			gEnv->p3DEngine->SetPostEffectParam("Dof_Active", true);
			// Set parameters
			gEnv->p3DEngine->SetPostEffectParam("Dof_FocusDistance", dof.x);
			gEnv->p3DEngine->SetPostEffectParam("Dof_FocusRange", dof.y);
			gEnv->p3DEngine->SetPostEffectParam("Dof_BlurAmount", dof.z);
		}
		else
		{
			// If a camera doesn't have a DoF track, it means it won't use the DoF processing.
			gEnv->p3DEngine->SetPostEffectParam("Dof_Active", false);
		}

		if (m_pLastFrameActiveCameraNode != this)
		{
			if (gEnv->pRenderer)
			{
				gEnv->pRenderer->EF_DisableTemporalEffects();
			}
			static_cast<CMovieSystem*>(gEnv->pMovieSystem)->OnCameraCut();
		}

		IAnimTrack* pShutterSpeedTrack = GetTrackForParameter(eAnimParamType_ShutterSpeed);
		if (pShutterSpeedTrack)
		{
			const float shutterSpeed = stl::get<float>(pShutterSpeedTrack->GetValue(animContext.time));
			gEnv->p3DEngine->SetPostEffectParam("MotionBlur_ShutterSpeed", shutterSpeed);
		}

		m_pLastFrameActiveCameraNode = this;
	}
	else
	{
		if (pEntity)
		{
			pEntity->ClearFlags(ENTITY_FLAG_TRIGGER_AREAS);
		}
	}

	bool bNodeAnimated = false;

	if (m_bJustActivated)
	{
		bNodeAnimated = true;
		m_bJustActivated = false;
	}

	if (fov != m_fFOV)
	{
		m_fFOV = fov;
		bNodeAnimated = true;
	}

	if (dof != m_fDOF)
	{
		m_fDOF = dof;
		bNodeAnimated = true;
	}

	if (nearZ != m_fNearZ)
	{
		m_fNearZ = nearZ;
		bNodeAnimated = true;
	}

	if (pEntity)
	{
		Quat rotation = pEntity->GetRotation();

		if (GetShakeRotation(animContext.time, rotation))
		{
			m_rotate = rotation;

			if (!IsSkipInterpolatedCameraNodeEnabled())
			{
				pEntity->SetRotation(rotation, ENTITY_XFORM_TRACKVIEW);
			}

			bNodeAnimated = true;
		}
	}

	if (bNodeAnimated && m_pOwner && !IsSkipInterpolatedCameraNodeEnabled())
	{
		m_bIgnoreSetParam = true;
		m_pOwner->OnNodeAnimated(this);
		m_bIgnoreSetParam = false;
	}
}

bool CAnimCameraNode::GetShakeRotation(SAnimTime time, Quat& rotation)
{
	Vec4 shakeMult = Vec4(m_shakeParam[0].amplitudeMult, m_shakeParam[1].amplitudeMult,
	                      m_shakeParam[0].frequencyMult, m_shakeParam[1].frequencyMult);
	CCompoundSplineTrack* pShakeMultTrack = static_cast<CCompoundSplineTrack*>(GetTrackForParameter(eAnimParamType_ShakeMultiplier));
	CAnimSplineTrack* pFreqTrack[SHAKE_COUNT];
	memset(pFreqTrack, 0, sizeof(pFreqTrack));

	bool bShakeMultTrackEnabled = false;

	if (pShakeMultTrack && !(pShakeMultTrack->GetFlags() & IAnimTrack::eAnimTrackFlags_Disabled))
	{
		bShakeMultTrackEnabled = true;
	}

	if (bShakeMultTrackEnabled)
	{
		shakeMult = stl::get<Vec4>(pShakeMultTrack->GetValue(time));
		pFreqTrack[0] = static_cast<CAnimSplineTrack*>(pShakeMultTrack->GetSubTrack(2)); // Frequency A is in z component.
		pFreqTrack[1] = static_cast<CAnimSplineTrack*>(pShakeMultTrack->GetSubTrack(3)); // Frequency B is in w component.
	}

	if (bShakeMultTrackEnabled && shakeMult != Vec4(m_shakeParam[0].amplitudeMult, m_shakeParam[1].amplitudeMult,
	                                                m_shakeParam[0].frequencyMult, m_shakeParam[1].frequencyMult))
	{
		m_shakeParam[0].amplitudeMult = shakeMult.x;
		m_shakeParam[1].amplitudeMult = shakeMult.y;
		m_shakeParam[0].frequencyMult = shakeMult.z;
		m_shakeParam[1].frequencyMult = shakeMult.w;
	}

	bool shakeOn[SHAKE_COUNT];
	shakeOn[0] = m_shakeParam[0].amplitudeMult != 0;
	shakeOn[1] = m_shakeParam[1].amplitudeMult != 0;

	IEntity* pEntity = GetEntity();

	if (pEntity != NULL && bShakeMultTrackEnabled && (shakeOn[0] || shakeOn[1]))
	{
		if (GetTrackForParameter(eAnimParamType_Rotation))
		{
			IAnimTrack* pRotTrack = GetTrackForParameter(eAnimParamType_Rotation);
			rotation = stl::get<Quat>(pRotTrack->GetValue(time));
		}

		Ang3 angles = Ang3::GetAnglesXYZ(Matrix33(rotation)) * 180.0f / gf_PI;

		for (int i = 0; i < SHAKE_COUNT; ++i)
		{
			if (shakeOn[i])
			{
				angles = m_shakeParam[i].ApplyCameraShake(m_pNoiseGen, time, angles, pFreqTrack[i], GetEntityId(), i, m_cameraShakeSeedValue);
			}
		}

		rotation.SetRotationXYZ(angles * gf_PI / 180.0f);
		return true;
	}

	return false;
}

void CAnimCameraNode::OnReset()
{
	CAnimEntityNode::OnReset();
	gEnv->p3DEngine->SetPostEffectParam("Dof_Active", 0);
	gEnv->p3DEngine->SetPostEffectParam("MotionBlur_ShutterSpeed", -1.0f);
}

void CAnimCameraNode::OnStop()
{
	m_pLastFrameActiveCameraNode = NULL;
}

void CAnimCameraNode::SetParameter(SAnimTime time, const EAnimParamType& paramType, const TMovieSystemValue& value)
{
	if (m_bIgnoreSetParam)
	{
		return;
	}

	switch (paramType)
	{
	case eAnimParamType_FOV:
		m_fFOV = stl::get<float>(value);
		break;
	case eAnimParamType_NearZ:
		m_fNearZ = stl::get<float>(value);
		break;
	case eAnimParamType_ShakeAmplitudeA:
		m_shakeParam[0].amplitude = stl::get<Vec3>(value);
		break;
	case eAnimParamType_ShakeAmplitudeB:
		m_shakeParam[1].amplitude = stl::get<Vec3>(value);
		break;
	case eAnimParamType_ShakeFrequencyA:
		m_shakeParam[0].frequency = stl::get<Vec3>(value);
		break;
	case eAnimParamType_ShakeFrequencyB:
		m_shakeParam[1].frequency = stl::get<Vec3>(value);
		break;
	case eAnimParamType_ShakeMultiplier:
		{
			const Vec4 values = stl::get<Vec4>(value);
			m_shakeParam[0].amplitudeMult = values.x;
			m_shakeParam[1].amplitudeMult = values.y;
			m_shakeParam[0].frequencyMult = values.z;
			m_shakeParam[1].frequencyMult = values.w;
		}
		break;
	case eAnimParamType_ShakeNoise:
		{
			const Vec4 values = stl::get<Vec4>(value);
			m_shakeParam[0].noiseAmpMult = values.x;
			m_shakeParam[1].noiseAmpMult = values.y;
			m_shakeParam[0].noiseFreqMult = values.z;
			m_shakeParam[1].noiseFreqMult = values.w;
		}
		break;
	case eAnimParamType_ShakeWorking:
		{
			const Vec4 values = stl::get<Vec4>(value);
			m_shakeParam[0].timeOffset = values.x;
			m_shakeParam[1].timeOffset = values.y;
		}
		break;
	}
}

TMovieSystemValue CAnimCameraNode::GetParameter(SAnimTime time, const EAnimParamType& paramType) const
{
	switch (paramType)
	{
	case eAnimParamType_FOV:
		return TMovieSystemValue(m_fFOV);
	case eAnimParamType_NearZ:
		return TMovieSystemValue(m_fNearZ);
	case eAnimParamType_ShakeAmplitudeA:
		return TMovieSystemValue(m_shakeParam[0].amplitude);
	case eAnimParamType_ShakeAmplitudeB:
		return TMovieSystemValue(m_shakeParam[1].amplitude);
	case eAnimParamType_ShakeFrequencyA:
		return TMovieSystemValue(m_shakeParam[0].frequency);
	case eAnimParamType_ShakeFrequencyB:
		return TMovieSystemValue(m_shakeParam[1].frequency);
	case eAnimParamType_ShakeMultiplier:
		return TMovieSystemValue(Vec4(m_shakeParam[0].amplitudeMult, m_shakeParam[1].amplitudeMult, m_shakeParam[0].frequencyMult, m_shakeParam[1].frequencyMult));
	case eAnimParamType_ShakeNoise:
		return TMovieSystemValue(Vec4(m_shakeParam[0].noiseAmpMult, m_shakeParam[1].noiseAmpMult, m_shakeParam[0].noiseFreqMult, m_shakeParam[1].noiseFreqMult));
	case eAnimParamType_ShakeWorking:
		return TMovieSystemValue(Vec4(m_shakeParam[0].timeOffset, m_shakeParam[1].timeOffset, 0.0f, 0.0f));
	}

	return TMovieSystemValue(SMovieSystemVoid());
}

void CAnimCameraNode::InitializeTrackDefaultValue(IAnimTrack* pTrack, const CAnimParamType& paramType)
{
	if (paramType.GetType() == eAnimParamType_FOV)
	{
		if (pTrack)
		{
			pTrack->SetDefaultValue(TMovieSystemValue(m_fFOV));
		}
	}
	else if (paramType.GetType() == eAnimParamType_NearZ)
	{
		if (pTrack)
		{
			pTrack->SetDefaultValue(TMovieSystemValue(m_fNearZ));
		}
	}
	else if (paramType.GetType() == eAnimParamType_ShakeMultiplier)
	{
		if (pTrack)
		{
			pTrack->SetDefaultValue(TMovieSystemValue(
			                          Vec4(m_shakeParam[0].amplitudeMult, m_shakeParam[1].amplitudeMult,
			                               m_shakeParam[0].frequencyMult, m_shakeParam[1].frequencyMult)));
		}
	}
}

void CAnimCameraNode::Serialize(XmlNodeRef& xmlNode, bool bLoading, bool bLoadEmptyTracks)
{
	CAnimEntityNode::Serialize(xmlNode, bLoading, bLoadEmptyTracks);

	if (bLoading)
	{
		xmlNode->getAttr("FOV", m_fFOV);
		xmlNode->getAttr("NearZ", m_fNearZ);
		xmlNode->getAttr("AmplitudeA", m_shakeParam[0].amplitude);
		xmlNode->getAttr("AmplitudeAMult", m_shakeParam[0].amplitudeMult);
		xmlNode->getAttr("FrequencyA", m_shakeParam[0].frequency);
		xmlNode->getAttr("FrequencyAMult", m_shakeParam[0].frequencyMult);
		xmlNode->getAttr("NoiseAAmpMult", m_shakeParam[0].noiseAmpMult);
		xmlNode->getAttr("NoiseAFreqMult", m_shakeParam[0].noiseFreqMult);
		xmlNode->getAttr("TimeOffsetA", m_shakeParam[0].timeOffset);
		xmlNode->getAttr("AmplitudeB", m_shakeParam[1].amplitude);
		xmlNode->getAttr("AmplitudeBMult", m_shakeParam[1].amplitudeMult);
		xmlNode->getAttr("FrequencyB", m_shakeParam[1].frequency);
		xmlNode->getAttr("FrequencyBMult", m_shakeParam[1].frequencyMult);
		xmlNode->getAttr("NoiseBAmpMult", m_shakeParam[1].noiseAmpMult);
		xmlNode->getAttr("NoiseBFreqMult", m_shakeParam[1].noiseFreqMult);
		xmlNode->getAttr("TimeOffsetB", m_shakeParam[1].timeOffset);
		xmlNode->getAttr("ShakeSeed", m_cameraShakeSeedValue);
	}
	else
	{
		xmlNode->setAttr("FOV", m_fFOV);
		xmlNode->setAttr("NearZ", m_fNearZ);
		xmlNode->setAttr("AmplitudeA", m_shakeParam[0].amplitude);
		xmlNode->setAttr("AmplitudeAMult", m_shakeParam[0].amplitudeMult);
		xmlNode->setAttr("FrequencyA", m_shakeParam[0].frequency);
		xmlNode->setAttr("FrequencyAMult", m_shakeParam[0].frequencyMult);
		xmlNode->setAttr("NoiseAAmpMult", m_shakeParam[0].noiseAmpMult);
		xmlNode->setAttr("NoiseAFreqMult", m_shakeParam[0].noiseFreqMult);
		xmlNode->setAttr("TimeOffsetA", m_shakeParam[0].timeOffset);
		xmlNode->setAttr("AmplitudeB", m_shakeParam[1].amplitude);
		xmlNode->setAttr("AmplitudeBMult", m_shakeParam[1].amplitudeMult);
		xmlNode->setAttr("FrequencyB", m_shakeParam[1].frequency);
		xmlNode->setAttr("FrequencyBMult", m_shakeParam[1].frequencyMult);
		xmlNode->setAttr("NoiseBAmpMult", m_shakeParam[1].noiseAmpMult);
		xmlNode->setAttr("NoiseBFreqMult", m_shakeParam[1].noiseFreqMult);
		xmlNode->setAttr("TimeOffsetB", m_shakeParam[1].timeOffset);
		xmlNode->setAttr("ShakeSeed", m_cameraShakeSeedValue);
	}
}

void CAnimCameraNode::Activate(bool bActivate)
{
	CAnimEntityNode::Activate(bActivate);

	if (bActivate)
	{
		m_bJustActivated = true;
	}
};

Ang3 CAnimCameraNode::ShakeParam::ApplyCameraShake(CPNoise3& noiseGen, SAnimTime time, Ang3 angles, CAnimSplineTrack* pFreqTrack, EntityId camEntityId, int shakeIndex, const uint shakeSeed)
{
	Ang3 rotation;
	Ang3 rotationNoise;

	float noiseAmpMult = this->amplitudeMult * this->noiseAmpMult;

	float t = this->timeOffset;

	this->phase = Vec3((t + 15.0f) * this->frequency.x,
	                   (t + 55.1f) * this->frequency.y,
	                   (t + 101.2f) * this->frequency.z);
	this->phaseNoise = Vec3((t + 70.0f) * this->frequency.x * this->noiseFreqMult,
	                        (t + 10.0f) * this->frequency.y * this->noiseFreqMult,
	                        (t + 30.5f) * this->frequency.z * this->noiseFreqMult);

	const float phaseDelta = stl::get<float>(pFreqTrack->GetValue(time));

	this->phase += this->frequency * phaseDelta;
	this->phaseNoise += this->frequency * this->noiseFreqMult * phaseDelta;

	rotation.x = noiseGen.Noise1D(this->phase.x) * this->amplitude.x * this->amplitudeMult;
	rotationNoise.x = noiseGen.Noise1D(this->phaseNoise.x) * this->amplitude.x * noiseAmpMult;

	rotation.y = noiseGen.Noise1D(this->phase.y) * this->amplitude.y * this->amplitudeMult;
	rotationNoise.y = noiseGen.Noise1D(this->phaseNoise.y) * this->amplitude.y * noiseAmpMult;

	rotation.z = noiseGen.Noise1D(this->phase.z) * this->amplitude.z * this->amplitudeMult;
	rotationNoise.z = noiseGen.Noise1D(this->phaseNoise.z) * this->amplitude.z * noiseAmpMult;

	angles += rotation + rotationNoise;

	return angles;
}

#undef s_nodeParamsInitialized
#undef s_nodeParams
#undef AddSupportedParam
