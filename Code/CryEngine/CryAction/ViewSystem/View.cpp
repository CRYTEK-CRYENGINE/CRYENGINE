// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "View.h"
#include <CryMath/Cry_Camera.h>
#include <CrySystem/VR/IHMDDevice.h>
#include <CrySystem/VR/IHMDManager.h>
#include <ITimeDemoRecorder.h>
#include "GameObjects/GameObject.h"
#include "IGameSessionHandler.h"
#include "ViewSystem.h"
#include <DefaultComponents/Audio/ListenerComponent.h>

namespace Cry
{
static ICVar* pCamShakeMult = nullptr;
static ICVar* pHmdTrackingOrigin = nullptr;
}
//------------------------------------------------------------------------
CView::CView(ISystem* const pSystem)
	: m_pSystem(pSystem)
	, m_linkedTo(INVALID_ENTITYID)
	, m_linkedEntityCallback(nullptr)
	, m_frameAdditiveAngles(0.0f, 0.0f, 0.0f)
	, m_scale(1.0f)
	, m_zoomedScale(1.0f)
	, m_pAudioListenerComponent(nullptr)
	, m_pAudioListenerEntity(nullptr)
{
	if (!Cry::pCamShakeMult)
	{
		Cry::pCamShakeMult = gEnv->pConsole->GetCVar("c_shakeMult");
	}
	if (!Cry::pHmdTrackingOrigin)
	{
		Cry::pHmdTrackingOrigin = gEnv->pConsole->GetCVar("hmd_tracking_origin");
	}

	CreateAudioListener();
}

//------------------------------------------------------------------------
CView::~CView()
{
	if (m_pAudioListenerEntity != nullptr)
	{
		gEnv->pEntitySystem->RemoveEntityEventListener(m_pAudioListenerEntity->GetId(), ENTITY_EVENT_DONE, this);
		gEnv->pEntitySystem->RemoveEntity(m_pAudioListenerEntity->GetId(), true);
		m_pAudioListenerEntity = nullptr;
		m_pAudioListenerComponent = nullptr;
	}
}

//-----------------------------------------------------------------------
void CView::Release()
{
	delete this;
}

//------------------------------------------------------------------------
void CView::Update(float frameTime, bool isActive)
{
	//FIXME:some cameras may need to be updated always
	if (!isActive)
		return;

	CGameObject* pLinkedTo = GetLinkedGameObject();
	if (pLinkedTo && !pLinkedTo->CanUpdateView())
		pLinkedTo = nullptr;
	IEntity* pEntity = pLinkedTo ? nullptr : GetLinkedEntity();

	if (pLinkedTo || pEntity)
	{
		m_viewParams.SaveLast();

		const CCamera &sysCam = m_pSystem->GetViewCamera();

		//process screen shaking
		ProcessShaking(frameTime);

		//FIXME:to let the updateView implementation use the correct shakeVector
		m_viewParams.currentShakeShift = m_viewParams.rotation * m_viewParams.currentShakeShift;

		m_viewParams.frameTime = frameTime;
		//update view position/rotation
		if (pLinkedTo || m_linkedEntityCallback)
		{
			if (pLinkedTo)
				pLinkedTo->UpdateView(m_viewParams);
			else
				m_linkedEntityCallback->UpdateView(m_viewParams);

			if (!m_viewParams.position.IsValid())
			{
				m_viewParams.position = m_viewParams.GetPositionLast();
				CRY_ASSERT_MESSAGE(0, "Camera position is invalid, reverting to old position");
			}
			if (!m_viewParams.rotation.IsValid())
			{
				m_viewParams.rotation = m_viewParams.GetRotationLast();
				CRY_ASSERT_MESSAGE(0, "Camera rotation is invalid, reverting to old rotation");
			}
		}
		else
		{
			Matrix34 mat = pEntity->GetWorldTM();
			mat.OrthonormalizeFast();
			m_viewParams.position = mat.GetTranslation();
			m_viewParams.rotation = Quat(mat);
		}

		ApplyFrameAdditiveAngles(m_viewParams.rotation);

		if (pLinkedTo)
		{
			pLinkedTo->PostUpdateView(m_viewParams);
		}

		const float fNearZ = gEnv->pGameFramework->GetIViewSystem()->GetDefaultZNear();

		//see if the view have to use a custom near clipping plane
		const float nearPlane = (m_viewParams.nearplane >= 0.01f) ? (m_viewParams.nearplane) : fNearZ;
		const float farPlane = gEnv->p3DEngine->GetMaxViewDistance();
		float fov = (m_viewParams.fov < 0.001f) ? DEFAULT_FOV : m_viewParams.fov;

		// [VR] specific
		// Modify FOV based on the HDM device configuration
		IHmdManager* pHmdManager = gEnv->pSystem->GetHmdManager();
		IHmdDevice* pHmdDevice = nullptr;
		bool bHmdTrackingEnabled = false;
		if (pHmdManager)
		{
			pHmdDevice = pHmdManager->GetHmdDevice();
			if (pHmdDevice)
			{
				const HmdTrackingState& sensorState = pHmdDevice->GetLocalTrackingState();
				bHmdTrackingEnabled = sensorState.CheckStatusFlags(eHmdStatus_IsUsable) && static_cast<CViewSystem*>(gEnv->pGameFramework->GetIViewSystem())->ShouldApplyHmdOffset();

				if (pHmdManager->IsStereoSetupOk())
				{
					float arf_notUsed;
					pHmdDevice->GetCameraSetupInfo(fov, arf_notUsed);
				}
			}
		}

		m_camera.SetFrustum(sysCam.GetViewSurfaceX(), sysCam.GetViewSurfaceZ(), fov, nearPlane, farPlane, sysCam.GetPixelAspectRatio());

		//TODO: (14, 06, 2010, "the player view should always get updated, this due to the hud being visable, without shocking, in cutscenes - todo is to see if we can optimise this code");
		IActor* pActor = gEnv->pGameFramework->GetClientActor();
		if (pActor)
		{
			CGameObject* const linkToObj = static_cast<CGameObject*>(pActor->GetEntity()->GetProxy(ENTITY_PROXY_USER));

			if (linkToObj && linkToObj != pLinkedTo)
			{
				linkToObj->PostUpdateView(m_viewParams);
			}
		}

		//apply shake & set the view matrix
		m_viewParams.rotation *= m_viewParams.currentShakeQuat;
		m_viewParams.rotation.NormalizeSafe();
		m_viewParams.position += m_viewParams.currentShakeShift;

		// Camera space Rendering calculations on Entity
		if (pLinkedTo)
		{
			IEntity* pLinkedToEntity = pLinkedTo->GetEntity();
			if (pLinkedToEntity)
			{
				const int slotIndex = 0;
				uint32 entityFlags = pLinkedToEntity->GetSlotFlags(slotIndex);
				if (entityFlags & ENTITY_SLOT_RENDER_NEAREST)
				{
					// Get camera pos relative to entity
					const Vec3 cameraLocalPos = m_viewParams.position;

					// Set entity's camera space position
					const Vec3 cameraSpacePos(-cameraLocalPos * m_viewParams.rotation);
					pLinkedToEntity->SetSlotCameraSpacePos(slotIndex, cameraSpacePos);

					// Add world pos onto camera local pos
					m_viewParams.position = pLinkedToEntity->GetWorldPos() + cameraLocalPos;
				}
			}
		}

		// Blending between cameras needs to happen after Camera space rendering calculations have been applied
		// so that the m_viewParams.position is in World Space again
		m_viewParams.UpdateBlending(frameTime);

		// [VR] specific
		// Add HMD's pose tracking on top of current camera pose
		// Each game-title can decide whether to keep this functionality here or (most likely)
		// move it somewhere else.

		Quat q = m_viewParams.rotation;
		Vec3 pos = m_viewParams.position;
		Vec3 p = Vec3(ZERO);

		// Uses the recorded tracking if time demo is on playback
		// Otherwise uses real tracking from device
		ITimeDemoRecorder* pTimeDemoRecorder = gEnv->pGameFramework->GetITimeDemoRecorder();

		if (pTimeDemoRecorder && pTimeDemoRecorder->IsPlaying())
		{
			STimeDemoFrameRecord record;
			pTimeDemoRecorder->GetCurrentFrameRecord(record);

			p = q * record.hmdPositionOffset;
			q = q * record.hmdViewRotation;
		}
		else if (pHmdDevice && bHmdTrackingEnabled)
		{
			// For seated VR experiences, attach the camera to the actor entity.
			if (Cry::pHmdTrackingOrigin && Cry::pHmdTrackingOrigin->GetIVal() == (int)EHmdTrackingOrigin::Seated)
			{
				const IEntity* pEnt = GetLinkedEntity();
				if (const IActor* pActor = gEnv->pGameFramework->GetClientActor())
				{
					if (pEnt && pActor->GetEntity() == pEnt)
					{
						q = pEnt->GetWorldRotation();
						pos = pEnt->GetWorldPos();
					}
				}
			}
			pHmdDevice->EnableLateCameraInjectionForCurrentFrame(std::make_pair(q, pos));

			const HmdTrackingState& sensorState = pHmdDevice->GetLocalTrackingState();
			p = q * sensorState.pose.position;
			q = q * sensorState.pose.orientation;
		}

		Matrix34 viewMtx(q);
		viewMtx.SetTranslation(pos + p);
		m_camera.SetMatrix(viewMtx);
		UpdateAudioListener(viewMtx);
	}
	else
	{
		m_linkedTo = INVALID_ENTITYID;

		CCryAction* pCryAction = CCryAction::GetCryAction();
		if (!pCryAction->IsGameSessionMigrating())    // If we're host migrating, leave the camera where it was
		{
			// Check if we're leaving a game mid way through - if we are then stop the camera from being reset for a frame or 2 before the unload happens
			if (!pCryAction->GetIGameSessionHandler()->IsMidGameLeaving())
			{
				m_camera.SetPosition(Vec3(1, 1, 1));
			}
		}
	}
}

//-----------------------------------------------------------------------
void CView::ApplyFrameAdditiveAngles(Quat& cameraOrientation)
{
	if ((m_frameAdditiveAngles.x != 0.f) || (m_frameAdditiveAngles.y != 0.f) || (m_frameAdditiveAngles.z != 0.f))
	{
		Ang3 cameraAngles(cameraOrientation);
		cameraAngles += m_frameAdditiveAngles;

		cameraOrientation.SetRotationXYZ(cameraAngles);

		m_frameAdditiveAngles.Set(0.0f, 0.0f, 0.0f);
	}
}

//------------------------------------------------------------------------
void CView::SetViewShake(Ang3 shakeAngle, Vec3 shakeShift, float duration, float frequency, float randomness, int shakeID, bool bFlipVec, bool bUpdateOnly, bool bGroundOnly)
{
	SShakeParams params;
	params.shakeAngle = shakeAngle;
	params.shakeShift = shakeShift;
	params.frequency = frequency;
	params.randomness = randomness;
	params.shakeID = shakeID;
	params.bFlipVec = bFlipVec;
	params.bUpdateOnly = bUpdateOnly;
	params.bGroundOnly = bGroundOnly;
	params.fadeInDuration = 0;          //
	params.fadeOutDuration = duration;  // originally it was faded out from start. that is why the values are set this way here, to preserve compatibility.
	params.sustainDuration = 0;         //

	SetViewShakeEx(params);
}

//------------------------------------------------------------------------
void CView::SetViewShakeEx(const SShakeParams& params)
{
	float shakeMult = GetScale();
	if (shakeMult < 0.001f)
		return;

	int shakes(m_shakes.size());
	SShake* pSetShake(nullptr);

	for (int i = 0; i < shakes; ++i)
	{
		SShake* pShake = &m_shakes[i];
		if (pShake->ID == params.shakeID)
		{
			pSetShake = pShake;
			break;
		}
	}

	if (!pSetShake)
	{
		m_shakes.emplace_back(params.shakeID);
		pSetShake = &m_shakes.back();
	}

	if (pSetShake)
	{
		// this can be set dynamically
		pSetShake->frequency = max(0.00001f, params.frequency);

		// the following are set on a 'new' shake as well
		if (params.bUpdateOnly == false)
		{
			pSetShake->amount = params.shakeAngle * shakeMult;
			pSetShake->amountVector = params.shakeShift * shakeMult;
			pSetShake->randomness = params.randomness;
			pSetShake->doFlip = params.bFlipVec;
			pSetShake->groundOnly = params.bGroundOnly;
			pSetShake->isSmooth = params.isSmooth;
			pSetShake->permanent = params.bPermanent;
			pSetShake->fadeInDuration = params.fadeInDuration;
			pSetShake->sustainDuration = params.sustainDuration;
			pSetShake->fadeOutDuration = params.fadeOutDuration;
			pSetShake->timeDone = 0;
			pSetShake->updating = true;
			pSetShake->interrupted = false;
			pSetShake->goalShake = Quat(ZERO);
			pSetShake->goalShakeSpeed = Quat(ZERO);
			pSetShake->goalShakeVector = Vec3(ZERO);
			pSetShake->goalShakeVectorSpeed = Vec3(ZERO);
			pSetShake->nextShake = 0.0f;
		}
	}
}

//------------------------------------------------------------------------
void CView::SetScale(const float scale)
{
	CRY_ASSERT_MESSAGE(scale == 1.0f || m_scale == 1.0f, "Attempting to CView::SetScale but has already been set!");
	m_scale = scale;
}

void CView::SetZoomedScale(const float scale)
{
	CRY_ASSERT_MESSAGE(scale == 1.0f || m_zoomedScale == 1.0f, "Attempting to CView::SetZoomedScale but has already been set!");
	m_zoomedScale = scale;
}

//------------------------------------------------------------------------
const float CView::GetScale()
{
	float shakeMult(Cry::pCamShakeMult->GetFVal());
	return m_scale * shakeMult * m_zoomedScale;
}

//------------------------------------------------------------------------
void CView::ProcessShaking(float frameTime)
{
	m_viewParams.currentShakeQuat.SetIdentity();
	m_viewParams.currentShakeShift.zero();
	m_viewParams.shakingRatio = 0;
	m_viewParams.groundOnly = false;

	int shakes(m_shakes.size());
	for (int i = 0; i < shakes; ++i)
		ProcessShake(&m_shakes[i], frameTime);
}

//------------------------------------------------------------------------
void CView::ProcessShake(SShake* pShake, float frameTime)
{
	if (!pShake->updating)
		return;

	pShake->timeDone += frameTime;

	if (pShake->isSmooth)
	{
		ProcessShakeSmooth(pShake, frameTime);
	}
	else
	{
		ProcessShakeNormal(pShake, frameTime);
	}
}

//------------------------------------------------------------------------
void CView::ProcessShakeNormal(SShake* pShake, float frameTime)
{
	float endSustain = pShake->fadeInDuration + pShake->sustainDuration;
	float totalDuration = endSustain + pShake->fadeOutDuration;

	bool finalDamping = (!pShake->permanent && pShake->timeDone > totalDuration) || (pShake->interrupted && pShake->ratio < 0.05f);

	if (finalDamping)
		ProcessShakeNormal_FinalDamping(pShake, frameTime);
	else
	{
		ProcessShakeNormal_CalcRatio(pShake, frameTime, endSustain);
		ProcessShakeNormal_DoShaking(pShake, frameTime);

		//for the global shaking ratio keep the biggest
		if (pShake->groundOnly)
			m_viewParams.groundOnly = true;
		m_viewParams.shakingRatio = max(m_viewParams.shakingRatio, pShake->ratio);
		m_viewParams.currentShakeQuat *= pShake->shakeQuat;
		m_viewParams.currentShakeShift += pShake->shakeVector;
	}
}

//////////////////////////////////////////////////////////////////////////
void CView::ProcessShakeSmooth(SShake* pShake, float frameTime)
{
	assert(pShake->timeDone >= 0);

	float endTimeFadeIn = pShake->fadeInDuration;
	float endTimeSustain = pShake->sustainDuration + endTimeFadeIn;
	float totalTime = endTimeSustain + pShake->fadeOutDuration;

	if (pShake->interrupted && endTimeFadeIn <= pShake->timeDone && pShake->timeDone < endTimeSustain)
	{
		pShake->timeDone = endTimeSustain;
	}

	float damping = 1.f;
	if (pShake->timeDone < endTimeFadeIn)
	{
		damping = pShake->timeDone / endTimeFadeIn;
	}
	else if (endTimeSustain < pShake->timeDone && pShake->timeDone < totalTime)
	{
		damping = (totalTime - pShake->timeDone) / (totalTime - endTimeSustain);
	}
	else if (totalTime <= pShake->timeDone)
	{
		pShake->shakeQuat.SetIdentity();
		pShake->shakeVector.zero();
		pShake->ratio = 0.0f;
		pShake->nextShake = 0.0f;
		pShake->flip = false;
		pShake->updating = false;
		return;
	}

	ProcessShakeSmooth_DoShaking(pShake, frameTime);

	if (pShake->groundOnly)
		m_viewParams.groundOnly = true;
	pShake->ratio = (3.f - 2.f * damping) * damping * damping; // smooth ration change
	m_viewParams.shakingRatio = max(m_viewParams.shakingRatio, pShake->ratio);
	m_viewParams.currentShakeQuat *= Quat::CreateSlerp(IDENTITY, pShake->shakeQuat, pShake->ratio);
	m_viewParams.currentShakeShift += Vec3::CreateLerp(ZERO, pShake->shakeVector, pShake->ratio);
}

//////////////////////////////////////////////////////////////////////////
void CView::GetRandomQuat(Quat& quat, SShake* pShake)
{
	quat.SetRotationXYZ(pShake->amount);
	float randomAmt(pShake->randomness);
	float len(fabs(pShake->amount.x) + fabs(pShake->amount.y) + fabs(pShake->amount.z));
	len /= 3.f;
	float r = len * randomAmt;
	quat *= Quat::CreateRotationXYZ(Ang3(cry_random(-r, r), cry_random(-r, r), cry_random(-r, r)));
}

//////////////////////////////////////////////////////////////////////////
void CView::GetRandomVector(Vec3& vec, SShake* pShake)
{
	vec = pShake->amountVector;
	float randomAmt(pShake->randomness);
	float len = fabs(pShake->amountVector.x) + fabs(pShake->amountVector.y) + fabs(pShake->amountVector.z);
	len /= 3.f;
	float r = len * randomAmt;
	vec += Vec3(cry_random(-r, r), cry_random(-r, r), cry_random(-r, r));
}

//////////////////////////////////////////////////////////////////////////
void CView::CubeInterpolateQuat(float t, SShake* pShake)
{
	Quat p0 = pShake->startShake;
	Quat p1 = pShake->goalShake;
	Quat v0 = pShake->startShakeSpeed * 0.5f;
	Quat v1 = pShake->goalShakeSpeed * 0.5f;

	pShake->shakeQuat = (((p0 * 2.f + p1 * -2.f + v0 + v1) * t
	                      + (p0 * -3.f + p1 * 3.f + v0 * -2.f - v1)) * t
	                     + (v0)) * t
	                    + p0;

	pShake->shakeQuat.Normalize();
}

//////////////////////////////////////////////////////////////////////////
void CView::CubeInterpolateVector(float t, SShake* pShake)
{
	Vec3 p0 = pShake->startShakeVector;
	Vec3 p1 = pShake->goalShakeVector;
	Vec3 v0 = pShake->startShakeVectorSpeed * 0.8f;
	Vec3 v1 = pShake->goalShakeVectorSpeed * 0.8f;

	pShake->shakeVector = (((p0 * 2.f + p1 * -2.f + v0 + v1) * t
	                        + (p0 * -3.f + p1 * 3.f + v0 * -2.f - v1)) * t
	                       + (v0)) * t
	                      + p0;

}

//////////////////////////////////////////////////////////////////////////
void CView::ProcessShakeSmooth_DoShaking(SShake* pShake, float frameTime)
{
	if (pShake->nextShake <= 0.0f)
	{
		pShake->nextShake = pShake->frequency;

		pShake->startShake = pShake->goalShake;
		pShake->startShakeSpeed = pShake->goalShakeSpeed;
		pShake->startShakeVector = pShake->goalShakeVector;
		pShake->startShakeVectorSpeed = pShake->goalShakeVectorSpeed;

		GetRandomQuat(pShake->goalShake, pShake);
		GetRandomQuat(pShake->goalShakeSpeed, pShake);
		GetRandomVector(pShake->goalShakeVector, pShake);
		GetRandomVector(pShake->goalShakeVectorSpeed, pShake);

		if (pShake->flip)
		{
			pShake->goalShake.Invert();
			pShake->goalShakeSpeed.Invert();
			pShake->goalShakeVector = -pShake->goalShakeVector;
			pShake->goalShakeVectorSpeed = -pShake->goalShakeVectorSpeed;
		}

		if (pShake->doFlip)
			pShake->flip = !pShake->flip;

	}

	pShake->nextShake -= frameTime;

	float t = (pShake->frequency - pShake->nextShake) / pShake->frequency;
	CubeInterpolateQuat(t, pShake);
	CubeInterpolateVector(t, pShake);
}

//////////////////////////////////////////////////////////////////////////
void CView::ProcessShakeNormal_FinalDamping(SShake* pShake, float frameTime)
{
	pShake->shakeQuat = Quat::CreateSlerp(pShake->shakeQuat, IDENTITY, frameTime * 5.0f);
	m_viewParams.currentShakeQuat *= pShake->shakeQuat;

	pShake->shakeVector = Vec3::CreateLerp(pShake->shakeVector, ZERO, frameTime * 5.0f);
	m_viewParams.currentShakeShift += pShake->shakeVector;

	float svlen2(pShake->shakeVector.len2());
	bool quatIsIdentity(Quat::IsEquivalent(IDENTITY, pShake->shakeQuat, 0.0001f));

	if (quatIsIdentity && svlen2 < 0.01f)
	{
		pShake->shakeQuat.SetIdentity();
		pShake->shakeVector.zero();

		pShake->ratio = 0.0f;
		pShake->nextShake = 0.0f;
		pShake->flip = false;

		pShake->updating = false;
	}
}

// "ratio" is the amplitude of the shaking
void CView::ProcessShakeNormal_CalcRatio(SShake* pShake, float frameTime, float endSustain)
{
	const float FADEOUT_TIME_WHEN_INTERRUPTED = 0.5f;

	if (pShake->interrupted)
		pShake->ratio = max(0.f, pShake->ratio - (frameTime / FADEOUT_TIME_WHEN_INTERRUPTED));    // fadeout after interrupted
	else if (pShake->timeDone >= endSustain && pShake->fadeOutDuration > 0)
	{
		float timeFading = pShake->timeDone - endSustain;
		pShake->ratio = clamp_tpl(1.f - timeFading / pShake->fadeOutDuration, 0.f, 1.f);   // fadeOut
	}
	else if (pShake->timeDone >= pShake->fadeInDuration)
	{
		pShake->ratio = 1.f; // sustain
	}
	else
	{
		pShake->ratio = min(1.f, pShake->timeDone / pShake->fadeInDuration);   // fadeIn
	}

	if (pShake->permanent && pShake->timeDone >= pShake->fadeInDuration && !pShake->interrupted)
		pShake->ratio = 1.f; // permanent standing
}

//////////////////////////////////////////////////////////////////////////
void CView::ProcessShakeNormal_DoShaking(SShake* pShake, float frameTime)
{
	float t;
	if (pShake->nextShake <= 0.0f)
	{
		//angular
		pShake->goalShake.SetRotationXYZ(pShake->amount);
		if (pShake->flip)
			pShake->goalShake.Invert();

		//translational
		pShake->goalShakeVector = pShake->amountVector;
		if (pShake->flip)
			pShake->goalShakeVector = -pShake->goalShakeVector;

		if (pShake->doFlip)
			pShake->flip = !pShake->flip;

		//randomize it a little
		float randomAmt(pShake->randomness);
		float len(fabs(pShake->amount.x) + fabs(pShake->amount.y) + fabs(pShake->amount.z));
		len /= 3.0f;
		float r = len * randomAmt;
		pShake->goalShake *= Quat::CreateRotationXYZ(Ang3(cry_random(-r, r), cry_random(-r, r), cry_random(-r, r)));

		//translational randomization
		len = fabs(pShake->amountVector.x) + fabs(pShake->amountVector.y) + fabs(pShake->amountVector.z);
		len /= 3.0f;
		r = len * randomAmt;
		pShake->goalShakeVector += Vec3(cry_random(-r, r), cry_random(-r, r), cry_random(-r, r));

		//damp & bounce it in a non linear fashion
		t = 1.0f - (pShake->ratio * pShake->ratio);
		pShake->goalShake = Quat::CreateSlerp(pShake->goalShake, IDENTITY, t);
		pShake->goalShakeVector = Vec3::CreateLerp(pShake->goalShakeVector, ZERO, t);

		pShake->nextShake = pShake->frequency;
	}

	pShake->nextShake = max(0.0f, pShake->nextShake - frameTime);

	t = min(1.0f, frameTime * (1.0f / pShake->frequency));
	pShake->shakeQuat = Quat::CreateSlerp(pShake->shakeQuat, pShake->goalShake, t);
	pShake->shakeQuat.Normalize();
	pShake->shakeVector = Vec3::CreateLerp(pShake->shakeVector, pShake->goalShakeVector, t);
}

//------------------------------------------------------------------------
void CView::StopShake(int shakeID)
{
	uint32 num = m_shakes.size();
	for (uint32 i = 0; i < num; ++i)
	{
		if (m_shakes[i].ID == shakeID && m_shakes[i].updating)
		{
			m_shakes[i].interrupted = true;
		}
	}
}

//------------------------------------------------------------------------
void CView::ResetShaking()
{
	// disable shakes
	std::vector<SShake>::iterator iter = m_shakes.begin();
	std::vector<SShake>::iterator iterEnd = m_shakes.end();
	while (iter != iterEnd)
	{
		SShake& shake = *iter;
		shake.updating = false;
		shake.timeDone = 0;
		++iter;
	}
}

//------------------------------------------------------------------------
void CView::LinkTo(IGameObject* follow)
{
	CRY_ASSERT(follow);
	m_linkedTo = follow->GetEntityId();
	m_viewParams.targetPos = follow->GetEntity()->GetWorldPos();
}

//------------------------------------------------------------------------
void CView::LinkTo(IEntity* follow, IGameObjectView* callback)
{
	CRY_ASSERT(follow);
	m_linkedTo = follow->GetId();
	m_viewParams.targetPos = follow->GetWorldPos();
	m_linkedEntityCallback = callback;
}

//------------------------------------------------------------------------
void CView::SetFrameAdditiveCameraAngles(const Ang3& addFrameAngles)
{
	m_frameAdditiveAngles = addFrameAngles;
}

//------------------------------------------------------------------------
CGameObject* CView::GetLinkedGameObject()
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_linkedTo);

	if (!pEntity)
		return nullptr;

	return static_cast<CGameObject*>(pEntity->GetProxy(ENTITY_PROXY_USER));
}

//------------------------------------------------------------------------
IEntity* CView::GetLinkedEntity()
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_linkedTo);
	return pEntity;
}

void CView::GetMemoryUsage(ICrySizer* s) const
{
	s->AddObject(this, sizeof(*this));
	s->AddObject(m_shakes);
}

void CView::Serialize(TSerialize ser)
{
	if (ser.IsReading())
	{
		ResetShaking();
	}
}

//////////////////////////////////////////////////////////////////////////
void CView::PostSerialize()
{
	CreateAudioListener();
}

//////////////////////////////////////////////////////////////////////////
void CView::UpdateAudioListener(Matrix34 const& worldTM)
{
	if (m_pAudioListenerEntity != nullptr)
	{
		m_pAudioListenerEntity->SetWorldTM(worldTM);
	}
}

//////////////////////////////////////////////////////////////////////////
void CView::OnEntityEvent(IEntity* pEntity, const SEntityEvent& event)
{
	switch (event.event)
	{
	case ENTITY_EVENT_DONE:
		// In case something destroys our listener entity before we had the chance to remove it.
		if ((m_pAudioListenerEntity != nullptr) && (pEntity->GetId() == m_pAudioListenerEntity->GetId()))
		{
			gEnv->pEntitySystem->RemoveEntityEventListener(m_pAudioListenerEntity->GetId(), ENTITY_EVENT_DONE, this);
			m_pAudioListenerEntity = nullptr;
			m_pAudioListenerComponent = nullptr;
		}
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CView::CreateAudioListener()
{
	IEntity* const pIEntity = GetLinkedEntity();

	if (m_pAudioListenerEntity == nullptr && pIEntity != nullptr)
	{
		SEntitySpawnParams spawnParams;
		spawnParams.pClass = gEnv->pEntitySystem->GetClassRegistry()->GetDefaultClass();
		spawnParams.vPosition = pIEntity->GetWorldPos();
		spawnParams.qRotation = pIEntity->GetWorldRotation();

		// We don't want the audio listener to serialize as the entity gets completely removed and recreated during save/load!
		// NOTE: If we set ENTITY_FLAG_NO_SAVE *after* we spawn the entity, it will make it to m_dynamicEntities in GameSerialize.cpp
		// (via CGameSerialize::OnSpawn) and GameSerialize will attempt to serialize it despite the flag with current (5.2.2) implementation
		spawnParams.nFlags = ENTITY_FLAG_NO_SAVE;
		m_pAudioListenerEntity = gEnv->pEntitySystem->SpawnEntity(spawnParams, true);

		if (m_pAudioListenerEntity != nullptr)
		{
			gEnv->pEntitySystem->AddEntityEventListener(m_pAudioListenerEntity->GetId(), ENTITY_EVENT_DONE, this);
			m_pAudioListenerEntity->SetName(pIEntity->GetName());

			m_pAudioListenerComponent = m_pAudioListenerEntity->GetOrCreateComponent<Cry::Audio::DefaultComponents::CListenerComponent>();
			CRY_ASSERT(m_pAudioListenerComponent != nullptr);
			m_pAudioListenerComponent->SetComponentFlags(m_pAudioListenerComponent->GetComponentFlags() | IEntityComponent::EFlags::UserAdded);
		}
		else
		{
			CryFatalError("<Audio>: AudioListenerEntity creation failed in CView::CreateAudioListener!");
		}
	}

	if (m_pAudioListenerEntity != nullptr)
	{
		m_pAudioListenerEntity->InvalidateTM(ENTITY_XFORM_POS);
	}
}

//////////////////////////////////////////////////////////////////////////
void CView::SetActive(bool const bActive)
{
	if (bActive)
	{
		// Make sure we have a valid audio listener entity on an active view!
		CreateAudioListener();
	}

	if (m_pAudioListenerComponent != nullptr)
	{
		m_pAudioListenerComponent->SetActive(bActive);
	}
}
