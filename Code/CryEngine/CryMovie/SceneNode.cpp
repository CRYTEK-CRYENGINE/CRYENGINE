// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SceneNode.h"
#include "AnimSequence.h"
#include "AnimTrack.h"
#include "GotoTrack.h"
#include "Tracks.h"
#include <CrySystem/ISystem.h>
#include <CrySystem/ITimer.h>
#include "AnimCameraNode.h"
#include "Movie.h"

#include <CrySystem/IConsole.h>

#define s_nodeParamsInitialized s_nodeParamsInitializedScene
#define s_nodeParams            s_nodeParamsSene
#define AddSupportedParam       AddSupportedParamScene

float const kDefaultCameraFOV = 60.0f;

namespace
{
bool s_nodeParamsInitialized = false;
std::vector<CAnimNode::SParamInfo> s_nodeParams;

void AddSupportedParam(const char* sName, int paramId, EAnimValue valueType, int flags = 0)
{
	CAnimNode::SParamInfo param;
	param.name = sName;
	param.paramType = paramId;
	param.valueType = valueType;
	param.flags = (IAnimNode::ESupportedParamFlags)flags;
	s_nodeParams.push_back(param);
}
}

CAnimSceneNode::CAnimSceneNode(const int id) : CAnimNode(id)
{
	m_lastCameraKey = -1;
	m_lastEventKey = -1;
	m_lastConsoleKey = -1;
	m_lastSequenceKey = -1;
	m_nLastGotoKey = -1;
	m_lastCaptureKey = -1;
	m_bLastCapturingEnded = true;
	m_currentCameraEntityId = 0;
	m_cvar_t_FixedStep = NULL;
	m_pCamNodeOnHoldForInterp = 0;
	m_backedUpFovForInterp = kDefaultCameraFOV;
	m_pCurrentCameraTrack = 0;
	m_currentCameraTrackKeyNumber = 0;
	m_lastPrecachePoint = SAnimTime::Min();
	SetName("Scene");

	CAnimSceneNode::Initialize();

	SetFlags(GetFlags() | eAnimNodeFlags_CanChangeName);
}

CAnimSceneNode::~CAnimSceneNode()
{
}

void CAnimSceneNode::Initialize()
{
	if (!s_nodeParamsInitialized)
	{
		s_nodeParamsInitialized = true;
		s_nodeParams.reserve(9);
		AddSupportedParam("Camera", eAnimParamType_Camera, eAnimValue_Unknown);
		AddSupportedParam("Event", eAnimParamType_Event, eAnimValue_Unknown);
		AddSupportedParam("Sequence", eAnimParamType_Sequence, eAnimValue_Unknown);
		AddSupportedParam("Console", eAnimParamType_Console, eAnimValue_Unknown);
		AddSupportedParam("GoTo", eAnimParamType_Goto, eAnimValue_DiscreteFloat);
		AddSupportedParam("Capture", eAnimParamType_Capture, eAnimValue_Unknown);
		AddSupportedParam("Timewarp", eAnimParamType_TimeWarp, eAnimValue_Float);
		AddSupportedParam("FixedTimeStep", eAnimParamType_FixedTimeStep, eAnimValue_Float);
		AddSupportedParam("GameCameraInfluence", eAnimParamType_GameCameraInfluence, eAnimValue_Float);
	}
}

void CAnimSceneNode::CreateDefaultTracks()
{
	CreateTrack(eAnimParamType_Camera);
};

unsigned int CAnimSceneNode::GetParamCount() const
{
	return s_nodeParams.size();
}

CAnimParamType CAnimSceneNode::GetParamType(unsigned int nIndex) const
{
	if (nIndex < (int)s_nodeParams.size())
	{
		return s_nodeParams[nIndex].paramType;

	}

	return eAnimParamType_Invalid;
}

bool CAnimSceneNode::GetParamInfoFromType(const CAnimParamType& paramId, SParamInfo& info) const
{
	for (int i = 0; i < (int)s_nodeParams.size(); i++)
	{
		if (s_nodeParams[i].paramType == paramId)
		{
			info = s_nodeParams[i];
			return true;
		}
	}

	return false;
}

void CAnimSceneNode::Activate(bool bActivate)
{
	CAnimNode::Activate(bActivate);

	int trackCount = NumTracks();

	for (int paramIndex = 0; paramIndex < trackCount; paramIndex++)
	{
		CAnimParamType paramId = m_tracks[paramIndex]->GetParameterType();
		IAnimTrack* pTrack = m_tracks[paramIndex];

		if (paramId.GetType() != eAnimParamType_Sequence)
		{
			continue;
		}

		CSequenceTrack* pSequenceTrack = (CSequenceTrack*)pTrack;

		for (int currKey = 0; currKey < pSequenceTrack->GetNumKeys(); currKey++)
		{
			SSequenceKey key;

			pSequenceTrack->GetKey(currKey, &key);

			IAnimSequence* pSequence = gEnv->pMovieSystem->FindSequenceByGUID(key.m_sequenceGUID);

			if (pSequence)
			{
				if (bActivate)
				{
					pSequence->Activate();
				}
				else
				{
					pSequence->Deactivate();
				}
			}
		}

		if (m_cvar_t_FixedStep == NULL)
		{
			m_cvar_t_FixedStep = gEnv->pConsole->GetCVar("t_FixedStep");
		}
	}
}

void CAnimSceneNode::Animate(SAnimContext& animContext)
{
	if (animContext.bResetting)
	{
		return;
	}

	CCameraTrack* cameraTrack = nullptr;
	CEventTrack* pEventTrack = nullptr;
	CSequenceTrack* pSequenceTrack = nullptr;
	CConsoleTrack* pConsoleTrack = nullptr;
	CGotoTrack* pGotoTrack = nullptr;
	CCaptureTrack* pCaptureTrack = nullptr;

	int nCurrentSoundTrackIndex = 0;

	if (gEnv->IsEditor() && m_time > animContext.time)
	{
		m_lastPrecachePoint = SAnimTime::Min();
	}

	PrecacheDynamic(animContext.time);

	int trackCount = NumTracks();

	for (int paramIndex = 0; paramIndex < trackCount; paramIndex++)
	{
		CAnimParamType paramId = m_tracks[paramIndex]->GetParameterType();
		IAnimTrack* pTrack = m_tracks[paramIndex];

		if (pTrack->GetFlags() & IAnimTrack::eAnimTrackFlags_Disabled)
		{
			continue;
		}

		if (pTrack->IsMasked(animContext.trackMask))
		{
			continue;
		}

		switch (paramId.GetType())
		{
		case eAnimParamType_Camera:
			cameraTrack = (CCameraTrack*)pTrack;
			break;

		case eAnimParamType_Event:
			pEventTrack = (CEventTrack*)pTrack;
			break;

		case eAnimParamType_Sequence:
			pSequenceTrack = (CSequenceTrack*)pTrack;
			break;

		case eAnimParamType_Console:
			pConsoleTrack = (CConsoleTrack*)pTrack;
			break;

		case eAnimParamType_Capture:
			pCaptureTrack = (CCaptureTrack*)pTrack;
			break;

		case eAnimParamType_Goto:
			pGotoTrack = (CGotoTrack*)pTrack;
			break;

		case eAnimParamType_TimeWarp:
			{
				const TMovieSystemValue value = pTrack->GetValue(animContext.time);
				const float timeScale = std::max(stl::get<float>(value), 0.0f);

				float fixedTimeStep = 0;

				if (GetSequence()->GetFlags() & IAnimSequence::eSeqFlags_CanWarpInFixedTime)
				{
					fixedTimeStep = GetSequence()->GetFixedTimeStep();
				}

				if (fixedTimeStep == 0)
				{
					if (m_cvar_t_FixedStep && (m_cvar_t_FixedStep->GetFVal() != 0) && !(GetSequence()->GetFlags() & IAnimSequence::eSeqFlags_Capture))
					{
						m_cvar_t_FixedStep->Set(0.0f);
					}

					gEnv->pTimer->SetTimeScale(timeScale, ITimer::eTSC_Trackview);
				}
				else if (m_cvar_t_FixedStep)
				{
					m_cvar_t_FixedStep->Set(fixedTimeStep * timeScale);
				}
			}
			break;

		case eAnimParamType_FixedTimeStep:
			{
				const TMovieSystemValue value = pTrack->GetValue(animContext.time);
				const float timeStep = std::max(stl::get<float>(value), 0.0f);

				if (m_cvar_t_FixedStep)
				{
					m_cvar_t_FixedStep->Set(timeStep);
				}
			}
			break;
		}
	}

	// Check if a camera OVERRIDE is set.
	const char* overrideCamName = gEnv->pMovieSystem->GetOverrideCamName();
	IEntity* overrideCamEntity = NULL;
	EntityId overrideCamId = 0;

	if (overrideCamName != 0 && strlen(overrideCamName) > 0)
	{
		overrideCamEntity = gEnv->pEntitySystem->FindEntityByName(overrideCamName);

		if (overrideCamEntity)
		{
			overrideCamId = overrideCamEntity->GetId();
		}
	}

	if (overrideCamEntity)    // There is a valid overridden camera.
	{
		if (overrideCamId != gEnv->pMovieSystem->GetCameraParams().cameraEntityId)
		{
			SCameraKey key;
			cry_strcpy(key.m_selection, overrideCamName);
			ApplyCameraKey(key, animContext);
		}
	}
	else if (cameraTrack)     // No camera override. Just follow the standard procedure.
	{
		SCameraKey key;
		int cameraKey = cameraTrack->GetActiveKey(animContext.time, &key);
		if (cameraKey >= 0)
		{
			m_currentCameraTrackKeyNumber = cameraKey;
			m_pCurrentCameraTrack = cameraTrack;
			ApplyCameraKey(key, animContext);
			m_lastCameraKey = cameraKey;
		}
	}

	if (pEventTrack)
	{
		SEventKey key;
		int nEventKey = pEventTrack->GetActiveKey(animContext.time, &key);

		if (nEventKey != m_lastEventKey && nEventKey >= 0)
		{
			bool bNotTrigger = key.m_bNoTriggerInScrubbing && animContext.bSingleFrame && key.m_time != animContext.time;

			if (!bNotTrigger)
			{
				ApplyEventKey(key, animContext);
			}
		}

		m_lastEventKey = nEventKey;
	}

	if (pConsoleTrack)
	{
		SConsoleKey key;
		int nConsoleKey = pConsoleTrack->GetActiveKey(animContext.time, &key);

		if (nConsoleKey != m_lastConsoleKey && nConsoleKey >= 0)
		{
			if (!animContext.bSingleFrame || key.m_time == animContext.time) // If Single frame update key time must match current time.
			{
				ApplyConsoleKey(key, animContext);
			}
		}

		m_lastConsoleKey = nConsoleKey;
	}

	if (pSequenceTrack)
	{
		SSequenceKey key;
		int nSequenceKey = pSequenceTrack->GetActiveKey(animContext.time, &key);
		IAnimSequence* pSequence = gEnv->pMovieSystem->FindSequenceByGUID(key.m_sequenceGUID);

		if (!gEnv->IsEditing() && (nSequenceKey != m_lastSequenceKey || !gEnv->pMovieSystem->IsPlaying(pSequence)))
		{
			ApplySequenceKey(pSequenceTrack, m_lastSequenceKey, nSequenceKey, key, animContext);
		}

		m_lastSequenceKey = nSequenceKey;
	}

	if (pGotoTrack)
	{
		ApplyGotoKey(pGotoTrack, animContext);
	}

	if (pCaptureTrack && gEnv->pMovieSystem->IsInBatchRenderMode() == false)
	{
		SCaptureKey key;
		int nCaptureKey = pCaptureTrack->GetActiveKey(animContext.time, &key);
		bool justEnded = false;

		if (!m_bLastCapturingEnded && key.m_time + key.m_duration < animContext.time)
		{
			justEnded = true;
		}

		if (!animContext.bSingleFrame && !(gEnv->IsEditor() && gEnv->IsEditing()))
		{
			if (nCaptureKey != m_lastCaptureKey && nCaptureKey >= 0)
			{
				if (m_bLastCapturingEnded == false)
				{
					assert(0);
					gEnv->pMovieSystem->EndCapture();
					m_bLastCapturingEnded = true;
				}
				gEnv->pMovieSystem->StartCapture(GetSequence(), key);

				if (!key.m_bOnce)
				{
					m_bLastCapturingEnded = false;
				}

				m_lastCaptureKey = nCaptureKey;
			}
			else if (justEnded)
			{
				gEnv->pMovieSystem->EndCapture();
				m_bLastCapturingEnded = true;
			}
		}
	}

	m_time = animContext.time;

	if (m_pOwner)
	{
		m_pOwner->OnNodeAnimated(this);
	}
	else
	{
	}
}

//////////////////////////////////////////////////////////////////////////
void CAnimSceneNode::OnReset()
{
	// If camera from this sequence still active, remove it.
	// reset camera
	SCameraParams CamParams = gEnv->pMovieSystem->GetCameraParams();

	if (CamParams.cameraEntityId != 0 && CamParams.cameraEntityId == m_currentCameraEntityId)
	{
		CamParams.cameraEntityId = 0;
		CamParams.fFOV = 0;
		CamParams.fGameCameraInfluence = 0.0f;
		CamParams.justActivated = true;
		gEnv->pMovieSystem->SetCameraParams(CamParams);

		if (IEntity* pCameraEntity = gEnv->pEntitySystem->GetEntity(m_currentCameraEntityId))
		{
			pCameraEntity->ClearFlags(ENTITY_FLAG_TRIGGER_AREAS);
		}
	}

	if (m_lastSequenceKey >= 0)
	{
		{
			int trackCount = NumTracks();

			for (int paramIndex = 0; paramIndex < trackCount; paramIndex++)
			{
				CAnimParamType paramId = m_tracks[paramIndex]->GetParameterType();
				IAnimTrack* pTrack = m_tracks[paramIndex];

				if (paramId.GetType() != eAnimParamType_Sequence)
				{
					continue;
				}

				CSequenceTrack* pSequenceTrack = (CSequenceTrack*)pTrack;
				SSequenceKey prevKey;

				pSequenceTrack->GetKey(m_lastSequenceKey, &prevKey);
				IAnimSequence* pAnimSequence = gEnv->pMovieSystem->FindSequenceByGUID(prevKey.m_sequenceGUID);
				if (pAnimSequence)
				{
					gEnv->pMovieSystem->StopSequence(pAnimSequence);
				}
			}
		}
	}

	// If the last capturing hasn't finished properly, end it here.
	if (m_bLastCapturingEnded == false)
	{
		gEnv->pMovieSystem->EndCapture();
		m_bLastCapturingEnded = true;
	}

	m_lastEventKey = -1;
	m_lastConsoleKey = -1;
	m_lastSequenceKey = -1;
	m_nLastGotoKey = -1;
	m_lastCaptureKey = -1;
	m_bLastCapturingEnded = true;
	m_currentCameraEntityId = 0;

	if (GetTrackForParameter(eAnimParamType_TimeWarp))
	{
		gEnv->pTimer->SetTimeScale(1.0f, ITimer::eTSC_Trackview);

		if (m_cvar_t_FixedStep)
		{
			m_cvar_t_FixedStep->Set(0);
		}
	}

	if (GetTrackForParameter(eAnimParamType_FixedTimeStep) && m_cvar_t_FixedStep)
	{
		m_cvar_t_FixedStep->Set(0);
	}
}

void CAnimSceneNode::OnPause()
{
}

void CAnimSceneNode::ApplyCameraKey(SCameraKey& key, SAnimContext& animContext)
{
	SCameraKey nextKey;
	int nextCameraKeyNumber = m_currentCameraTrackKeyNumber + 1;
	bool bInterpolateCamera = false;

	if (nextCameraKeyNumber < m_pCurrentCameraTrack->GetNumKeys())
	{
		m_pCurrentCameraTrack->GetKey(nextCameraKeyNumber, &nextKey);

		SAnimTime interTime = nextKey.m_time - animContext.time;

		if (interTime >= SAnimTime(0) && interTime <= SAnimTime(key.m_blendTime))
		{
			bInterpolateCamera = true;
		}
	}

	if (!bInterpolateCamera && m_pCamNodeOnHoldForInterp)
	{
		m_pCamNodeOnHoldForInterp->SetSkipInterpolatedCameraNode(false);
		m_pCamNodeOnHoldForInterp = 0;
	}

	// First, check the child nodes of this director, then global nodes.
	IAnimNode* pFirstCameraNode = m_pSequence->FindNodeByName(key.m_selection, this);
	IAnimEntityNode* pFirstCameraNodeEntity = pFirstCameraNode ? pFirstCameraNode->QueryEntityNodeInterface() : NULL;

	if (pFirstCameraNode == NULL)
	{
		pFirstCameraNode = m_pSequence->FindNodeByName(key.m_selection, NULL);
	}

	SCameraParams cameraParams;
	cameraParams.cameraEntityId = 0;
	cameraParams.fFOV = 0;
	cameraParams.justActivated = true;

	IAnimTrack* pCameraInfluenceTrack = GetTrackForParameter(eAnimParamType_GameCameraInfluence);
	if (pCameraInfluenceTrack)
	{
		const TMovieSystemValue value = pCameraInfluenceTrack->GetValue(animContext.time);
		const float fGameCameraInfluence = stl::get<float>(value);
		cameraParams.fGameCameraInfluence = clamp_tpl(fGameCameraInfluence, 0.0f, 1.0f);
	}

	IEntity* pFirstCameraEntity = gEnv->pEntitySystem->FindEntityByName(key.m_selection);

	if (pFirstCameraEntity)
	{
		cameraParams.cameraEntityId = pFirstCameraEntity->GetId();
	}

	float fFirstCameraFOV = kDefaultCameraFOV;

	if (pFirstCameraNode && pFirstCameraNode->GetType() == eAnimNodeType_Camera)
	{
		cameraParams.fNearZ = DEFAULT_NEAR;

		IAnimTrack* pNearZTrack = pFirstCameraNode->GetTrackForParameter(eAnimParamType_NearZ);
		if (pNearZTrack)
		{
			const TMovieSystemValue value = pNearZTrack->GetValue(animContext.time);
			cameraParams.fNearZ = stl::get<float>(value);
		}

		IAnimTrack* pFOVTrack = pFirstCameraNode->GetTrackForParameter(eAnimParamType_FOV);
		if (pFOVTrack)
		{
			const TMovieSystemValue value = pFOVTrack->GetValue(animContext.time);
			fFirstCameraFOV = stl::get<float>(value);
		}

		cameraParams.fFOV = DEG2RAD(fFirstCameraFOV);
	}

	IEntity* pSecondCameraEntity = gEnv->pEntitySystem->FindEntityByName(nextKey.m_selection);

	if (bInterpolateCamera && pFirstCameraEntity && pSecondCameraEntity)
	{
		float t = 1 - ((nextKey.m_time - animContext.time).ToFloat() / key.m_blendTime);
		t = min(t, 1.0f);
		t = pow(t, 3) * (t * (t * 6 - 15) + 10);

		// FOV
		float fSecondCameraFOV = kDefaultCameraFOV;

		if (!m_pCamNodeOnHoldForInterp && pFirstCameraNode)
		{
			IAnimTrack* pFOVTrack = pFirstCameraNode->GetTrackForParameter(eAnimParamType_FOV);
			if (pFOVTrack)
			{
				const TMovieSystemValue value = pFOVTrack->GetValue(animContext.time);
				m_backedUpFovForInterp = stl::get<float>(value);
			}
		}

		if (pFirstCameraNode && pFirstCameraNode->GetType() == eAnimNodeType_Camera)
		{
			m_pCamNodeOnHoldForInterp = (CAnimEntityNode*)pFirstCameraNode;
			m_pCamNodeOnHoldForInterp->SetSkipInterpolatedCameraNode(true);
		}

		if (pSecondCameraEntity)
		{
			IEntityCameraComponent* pSecondCameraProxy = (IEntityCameraComponent*)pSecondCameraEntity->GetProxy(ENTITY_PROXY_CAMERA);

			if (pSecondCameraProxy)
			{
				CCamera& camera = pSecondCameraProxy->GetCamera();
				fSecondCameraFOV = RAD2DEG(camera.GetFov());
			}
		}

		IAnimNode* pSecondCameraNode = m_pSequence->FindNodeByName(nextKey.m_selection, this);

		if (pSecondCameraNode == NULL)
		{
			pSecondCameraNode = m_pSequence->FindNodeByName(nextKey.m_selection, NULL);
		}

		if (pSecondCameraNode && pSecondCameraNode->GetType() == eAnimNodeType_Camera)
		{
			IAnimTrack* pFOVTrack = pSecondCameraNode->GetTrackForParameter(eAnimParamType_FOV);
			if (pFOVTrack)
			{
				const TMovieSystemValue value = pFOVTrack->GetValue(animContext.time);
				fSecondCameraFOV = stl::get<float>(value);
			}
		}

		fFirstCameraFOV = m_backedUpFovForInterp + (fSecondCameraFOV - m_backedUpFovForInterp) * t;

		if (pFirstCameraNode && pFirstCameraNode->GetType() == eAnimNodeType_Camera)
		{
			IAnimTrack* pFOVTrack = pFirstCameraNode->GetTrackForParameter(eAnimParamType_FOV);
			if (pFOVTrack)
			{
				pFOVTrack->SetDefaultValue(TMovieSystemValue(fFirstCameraFOV));
			}
		}

		cameraParams.fFOV = DEG2RAD(fFirstCameraFOV);

		// Position
		Vec3 vNextKeyPos;
		vNextKeyPos = pSecondCameraEntity->GetPos();

		if (pSecondCameraNode && pSecondCameraNode->GetType() == eAnimNodeType_Camera)
		{
			IAnimTrack* pPosTrack = pSecondCameraNode->GetTrackForParameter(eAnimParamType_Position);
			if (pPosTrack)
			{
				const TMovieSystemValue value = pPosTrack->GetValue(animContext.time);
				vNextKeyPos = stl::get<Vec3>(value);
				;
			}
		}

		if (pSecondCameraEntity->GetParent())
		{
			IEntity* pParent = pSecondCameraEntity->GetParent();

			if (pParent)
			{
				vNextKeyPos = pParent->GetWorldTM() * vNextKeyPos;
			}
		}

		QuatT xformSecondCamera(IDENTITY);

		Vec3 vFirstCamPos;
		vFirstCamPos = pFirstCameraEntity->GetPos();

		if (pFirstCameraNode && pFirstCameraNode->GetType() == eAnimNodeType_Camera)
		{
			IAnimTrack* pPosTrack = pFirstCameraNode->GetTrackForParameter(eAnimParamType_Position);
			if (pPosTrack)
			{
				const TMovieSystemValue value = pPosTrack->GetValue(animContext.time);
				vFirstCamPos = stl::get<Vec3>(value);
			}
		}

		if (pFirstCameraEntity->GetParent())
		{
			IEntity* pParent = pFirstCameraEntity->GetParent();

			if (pParent)
			{
				vFirstCamPos = pParent->GetWorldTM() * vFirstCamPos;
			}
		}

		Vec3 vNewPos = vFirstCamPos + (vNextKeyPos - vFirstCamPos) * t;

		QuatT xformFirstCamera(IDENTITY);
		QuatT xformFirstCameraTmp(IDENTITY);

		if (!pFirstCameraEntity->GetParent())
		{
			if (pFirstCameraNode && pFirstCameraNode->GetType() == eAnimNodeType_Camera)
			{
				((CAnimEntityNode*)pFirstCameraNode)->SetCameraInterpolationPosition(vNewPos);
			}

			pFirstCameraEntity->SetPos(vNewPos);
		}
		else if (pFirstCameraEntity->GetParent())
		{
			Matrix34 m = pFirstCameraEntity->GetParent()->GetWorldTM();
			m = m.GetInverted();
			Vec3 vMovePos(m * vNewPos);

			if (pFirstCameraNode && pFirstCameraNode->GetType() == eAnimNodeType_Camera)
			{
				((CAnimEntityNode*)pFirstCameraNode)->SetCameraInterpolationPosition(vMovePos);
			}

			pFirstCameraEntity->SetPos(vMovePos);
		}

		// Rotation
		Quat firstCameraRotation;
		Quat secondCameraRotation;

		firstCameraRotation = pFirstCameraEntity->GetRotation();

		IAnimTrack* pOrgRotationTrack = (pFirstCameraNode && pFirstCameraNode->GetType() == eAnimNodeType_Camera ? pFirstCameraNode->GetTrackForParameter(eAnimParamType_Rotation) : 0);

		if (pOrgRotationTrack)
		{
			const TMovieSystemValue value = pOrgRotationTrack->GetValue(animContext.time);
			firstCameraRotation = stl::get<Quat>(value);
		}

		if (pFirstCameraEntity->GetParent())
		{
			IEntity* pParent = pFirstCameraEntity->GetParent();

			if (pParent)
			{
				firstCameraRotation = pParent->GetWorldRotation() * firstCameraRotation;
			}
		}

		secondCameraRotation = pSecondCameraEntity->GetRotation();

		if (pSecondCameraNode && pSecondCameraNode->GetType() == eAnimNodeType_Camera)
		{
			IAnimTrack* pRotationTrack = (pSecondCameraNode ? pSecondCameraNode->GetTrackForParameter(eAnimParamType_Rotation) : 0);

			if (pRotationTrack)
			{
				const TMovieSystemValue value = pRotationTrack->GetValue(animContext.time);
				secondCameraRotation = stl::get<Quat>(value);
			}
		}

		if (pSecondCameraEntity->GetParent())
		{
			IEntity* pParent = pSecondCameraEntity->GetParent();

			if (pParent)
			{
				secondCameraRotation = pParent->GetWorldRotation() * secondCameraRotation;
			}
		}

		Quat rotNew;
		rotNew.SetSlerp(firstCameraRotation, secondCameraRotation, t);

		if (!pFirstCameraEntity->GetParent())
		{
			pFirstCameraEntity->SetRotation(rotNew);

			if (pFirstCameraNode && pFirstCameraNode->GetType() == eAnimNodeType_Camera)
			{
				((CAnimEntityNode*)pFirstCameraNode)->SetCameraInterpolationRotation(rotNew);
			}

		}
		else if (pFirstCameraEntity->GetParent())
		{
			Matrix34 m = pFirstCameraEntity->GetWorldTM();
			m.SetRotationXYZ(Ang3(rotNew));
			m.SetTranslation(pFirstCameraEntity->GetWorldTM().GetTranslation());
			pFirstCameraEntity->SetWorldTM(m);

			if (pFirstCameraNode && pFirstCameraNode->GetType() == eAnimNodeType_Camera)
			{
				((CAnimEntityNode*)pFirstCameraNode)->SetCameraInterpolationRotation(pFirstCameraEntity->GetRotation());
			}
		}
	}

	m_currentCameraEntityId = cameraParams.cameraEntityId;
	gEnv->pMovieSystem->SetCameraParams(cameraParams);
}

void CAnimSceneNode::ApplyEventKey(SEventKey& key, SAnimContext& animContext)
{
	gEnv->pMovieSystem->SendGlobalEvent("Event_" + key.m_event);
}

void CAnimSceneNode::ApplySequenceKey(IAnimTrack* pTrack, int nPrevKey, int nCurrKey, SSequenceKey& key, SAnimContext& animContext)
{
	if (nCurrKey >= 0 && (key.m_sequenceGUID != CryGUID::Null()))
	{
		IAnimSequence* pSequence = gEnv->pMovieSystem->FindSequenceByGUID(key.m_sequenceGUID);

		if (pSequence)
		{
			SAnimTime startTime = SAnimTime::Min();
			SAnimTime endTime = SAnimTime::Max();
			SAnimTime duration;

			if (key.m_boverrideTimes)
			{
				duration = max((key.m_endTime - key.m_startTime), SAnimTime(0));
				startTime = key.m_startTime;
				endTime = key.m_endTime;
			}
			else
			{
				duration = pSequence->GetTimeRange().Length();
			}

			SAnimContext newAnimContext = animContext;
			newAnimContext.time = std::min(animContext.time - key.m_time + key.m_startTime, duration + key.m_startTime);

			if (static_cast<CAnimSequence*>(pSequence)->GetTime() != newAnimContext.time)
			{
				pSequence->Animate(newAnimContext);
			}
		}
	}
}

void CAnimSceneNode::ApplyConsoleKey(SConsoleKey& key, SAnimContext& animContext)
{
	if (key.m_command[0])
	{
		gEnv->pConsole->ExecuteString(key.m_command);
	}
}

void CAnimSceneNode::ApplyGotoKey(CGotoTrack* poGotoTrack, SAnimContext& animContext)
{
	SDiscreteFloatKey discreteFloadKey;

	int currentActiveKeyIndex = poGotoTrack->GetActiveKey(animContext.time, &discreteFloadKey);
	if (currentActiveKeyIndex != m_nLastGotoKey && currentActiveKeyIndex >= 0)
	{
		if (!animContext.bSingleFrame)
		{
			if (discreteFloadKey.m_value >= 0)
			{
				string fullname = m_pSequence->GetName();
				gEnv->pMovieSystem->GoToFrame(fullname.c_str(), discreteFloadKey.m_value);
			}
		}
	}

	m_nLastGotoKey = currentActiveKeyIndex;
}

bool CAnimSceneNode::GetEntityTransform(IAnimSequence* pSequence, IEntity* pEntity, SAnimTime time, Vec3& vCamPos, Quat& qCamRot)
{
	const uint iNodeCount = pSequence->GetNodeCount();

	for (uint i = 0; i < iNodeCount; ++i)
	{
		IAnimNode* pNode = pSequence->GetNode(i);
		IAnimEntityNode* pEntityNode = pNode ? pNode->QueryEntityNodeInterface() : NULL;

		if (pNode && pNode->GetType() == eAnimNodeType_Camera && pEntityNode && pEntityNode->GetEntity() == pEntity)
		{
			IAnimTrack* pPosTrack = pNode->GetTrackForParameter(eAnimParamType_Position);
			if (pPosTrack)
			{
				const TMovieSystemValue value = pPosTrack->GetValue(time);
				vCamPos = stl::get<Vec3>(value);
			}

			IAnimTrack* pOrgRotationTrack = pNode->GetTrackForParameter(eAnimParamType_Rotation);
			if (pOrgRotationTrack != NULL)
			{
				const TMovieSystemValue value = pOrgRotationTrack->GetValue(time);
				qCamRot = stl::get<Quat>(value);
			}

			return true;
		}
	}

	return false;
}

bool CAnimSceneNode::GetEntityTransform(IEntity* pEntity, SAnimTime time, Vec3& vCamPos, Quat& qCamRot)
{
	CRY_ASSERT(pEntity != NULL);

	vCamPos = pEntity->GetPos();
	qCamRot = pEntity->GetRotation();

	bool bFound = GetEntityTransform(m_pSequence, pEntity, time, vCamPos, qCamRot);

	uint numTracks = GetTrackCount();

	for (uint trackIndex = 0; trackIndex < numTracks; ++trackIndex)
	{
		IAnimTrack* pAnimTrack = GetTrackByIndex(trackIndex);

		if (pAnimTrack->GetParameterType() == eAnimParamType_Sequence)
		{
			CSequenceTrack* pSequenceTrack = static_cast<CSequenceTrack*>(pAnimTrack);

			const uint numKeys = pSequenceTrack->GetNumKeys();

			for (uint keyIndex = 0; keyIndex < numKeys; ++keyIndex)
			{
				SSequenceKey key;
				pSequenceTrack->GetKey(keyIndex, &key);

				CAnimSequence* pSubSequence = static_cast<CAnimSequence*>(gEnv->pMovieSystem->FindSequenceByGUID(key.m_sequenceGUID));
				if (pSubSequence)
				{
					bool bSubSequence = GetEntityTransform(pSubSequence, pEntity, time, vCamPos, qCamRot);
					bFound = bFound || bSubSequence;
				}
			}
		}
	}

	if (pEntity->GetParent())
	{
		IEntity* pParent = pEntity->GetParent();

		if (pParent)
		{
			vCamPos = pParent->GetWorldTM() * vCamPos;
			qCamRot = pParent->GetWorldRotation() * qCamRot;
		}
	}

	return bFound;
}

void CAnimSceneNode::Serialize(XmlNodeRef& xmlNode, bool bLoading, bool bLoadEmptyTracks)
{
	CAnimNode::Serialize(xmlNode, bLoading, bLoadEmptyTracks);

	// To enable renaming even for previously saved director nodes
	SetFlags(GetFlags() | eAnimNodeFlags_CanChangeName);
}

void CAnimSceneNode::PrecacheStatic(SAnimTime startTime)
{
	m_lastPrecachePoint = SAnimTime::Min();

	const uint numTracks = GetTrackCount();

	for (uint trackIndex = 0; trackIndex < numTracks; ++trackIndex)
	{
		IAnimTrack* pAnimTrack = GetTrackByIndex(trackIndex);

		if (pAnimTrack->GetParameterType() == eAnimParamType_Sequence)
		{
			CSequenceTrack* pSequenceTrack = static_cast<CSequenceTrack*>(pAnimTrack);

			const uint numKeys = pSequenceTrack->GetNumKeys();

			for (uint keyIndex = 0; keyIndex < numKeys; ++keyIndex)
			{
				SSequenceKey key;
				pSequenceTrack->GetKey(keyIndex, &key);

				CAnimSequence* pSubSequence = static_cast<CAnimSequence*>(gEnv->pMovieSystem->FindSequenceByGUID(key.m_sequenceGUID));
				if (pSubSequence)
				{
					pSubSequence->PrecacheStatic(startTime - (key.m_startTime + key.m_time));
				}
			}
		}
	}
}

void CAnimSceneNode::PrecacheDynamic(SAnimTime time)
{
	const uint numTracks = GetTrackCount();
	SAnimTime lastPrecachePoint = m_lastPrecachePoint;

	for (uint trackIndex = 0; trackIndex < numTracks; ++trackIndex)
	{
		IAnimTrack* pAnimTrack = GetTrackByIndex(trackIndex);

		if (pAnimTrack->GetParameterType() == eAnimParamType_Sequence)
		{
			CSequenceTrack* pSequenceTrack = static_cast<CSequenceTrack*>(pAnimTrack);

			const uint numKeys = pSequenceTrack->GetNumKeys();

			for (uint keyIndex = 0; keyIndex < numKeys; ++keyIndex)
			{
				SSequenceKey key;
				pSequenceTrack->GetKey(keyIndex, &key);

				CAnimSequence* pSubSequence = static_cast<CAnimSequence*>(gEnv->pMovieSystem->FindSequenceByGUID(key.m_sequenceGUID));
				if (pSubSequence)
				{
					pSubSequence->PrecacheDynamic(time - (key.m_startTime + key.m_time));
				}
			}
		}
		else if (pAnimTrack->GetParameterType() == eAnimParamType_Camera)
		{
			const float fPrecacheCameraTime = CMovieSystem::m_mov_cameraPrecacheTime;

			if (fPrecacheCameraTime > 0.f)
			{
				CCameraTrack* pCameraTrack = static_cast<CCameraTrack*>(pAnimTrack);

				SCameraKey key;
				int keyID = pCameraTrack->GetActiveKey(time + SAnimTime(fPrecacheCameraTime), &key);

				if (time < key.m_time && (time + SAnimTime(fPrecacheCameraTime)) > key.m_time && key.m_time > m_lastPrecachePoint)
				{
					lastPrecachePoint = max(key.m_time, lastPrecachePoint);
					IEntity* pCameraEntity = gEnv->pEntitySystem->FindEntityByName(key.m_selection);

					if (pCameraEntity != NULL)
					{
						Vec3 vCamPos(ZERO);
						Quat qCamRot(IDENTITY);

						if (GetEntityTransform(pCameraEntity, key.m_time, vCamPos, qCamRot))
						{
							gEnv->p3DEngine->AddPrecachePoint(vCamPos, qCamRot.GetColumn1(), fPrecacheCameraTime);
						}
						else
						{
							CryWarning(VALIDATOR_MODULE_MOVIE, VALIDATOR_WARNING, "Could not find animation node for camera %s in sequence %s", key.m_selection, m_pSequence->GetName());
						}
					}
				}
			}
		}
	}

	m_lastPrecachePoint = lastPrecachePoint;
}

void CAnimSceneNode::InitializeTrackDefaultValue(IAnimTrack* pTrack, const CAnimParamType& paramType)
{
	if (paramType.GetType() == eAnimParamType_TimeWarp)
	{
		pTrack->SetDefaultValue(TMovieSystemValue(1.0f));
	}
}

#undef s_nodeParamsInitialized
#undef s_nodeParams
#undef AddSupportedParam
