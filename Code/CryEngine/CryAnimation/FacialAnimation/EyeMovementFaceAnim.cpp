// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "EyeMovementFaceAnim.h"

#include "FacialInstance.h"
#include "../CharacterManager.h"
#include "../CharacterInstance.h"
#include "FaceEffector.h"
#include "FaceEffectorLibrary.h"

#if defined(__GNUC__)
	#include <float.h> // FLT_MIN
#endif

const CFaceIdentifierHandle* CEyeMovementFaceAnim::RetrieveEffectorIdentifiers() const
{
	IFacialAnimation* pFaceAnim = g_pCharacterManager->GetIFacialAnimation();
	if (!pFaceAnim)
		return NULL;
	static const CFaceIdentifierHandle effectorIdentifiers[EYEMOVEMENT_EFFECTOR_AMOUNT] =
	{
		pFaceAnim->CreateIdentifierHandle("Eye_left_lower_middle"),  //2
		pFaceAnim->CreateIdentifierHandle("Eye_left_lower_right"),
		pFaceAnim->CreateIdentifierHandle("Eye_left_middle_right"),   //4
		pFaceAnim->CreateIdentifierHandle("Eye_left_upper_right"),
		pFaceAnim->CreateIdentifierHandle("Eye_left_upper_middle"),  //6
		pFaceAnim->CreateIdentifierHandle("Eye_left_upper_left"),
		pFaceAnim->CreateIdentifierHandle("Eye_left_middle_left"),  //0
		pFaceAnim->CreateIdentifierHandle("Eye_left_lower_left"),

		pFaceAnim->CreateIdentifierHandle("Eye_right_lower_middle"),  //2
		pFaceAnim->CreateIdentifierHandle("Eye_right_lower_right"),
		pFaceAnim->CreateIdentifierHandle("Eye_right_middle_right"),   //4
		pFaceAnim->CreateIdentifierHandle("Eye_right_upper_right"),
		pFaceAnim->CreateIdentifierHandle("Eye_right_upper_middle"),  //6
		pFaceAnim->CreateIdentifierHandle("Eye_right_upper_left"),
		pFaceAnim->CreateIdentifierHandle("Eye_right_middle_left"),  //0
		pFaceAnim->CreateIdentifierHandle("Eye_right_lower_left")
	};
	return effectorIdentifiers;
}

const char* CEyeMovementFaceAnim::szEyeBoneNames[] = {
	"eye_left_bone",
	"eye_right_bone"
};

uint32 CEyeMovementFaceAnim::ms_eyeBoneNamesCrc[] = { 0, 0 };

CEyeMovementFaceAnim::CEyeMovementFaceAnim(CFacialInstance* pInstance)
	: m_pInstance(pInstance),
	m_bInitialized(false)
{
	InitialiseChannels();
	if (ms_eyeBoneNamesCrc[0] == 0)
	{
		for (int i = 0; i < EyeCOUNT; ++i)
		{
			const char* const eyeName = szEyeBoneNames[i];
			ms_eyeBoneNamesCrc[i] = CCrc32::ComputeLowercase(eyeName);
		}
	}
}

void CEyeMovementFaceAnim::Update(float fDeltaTimeSec, const QuatTS& rAnimLocationNext, CCharInstance* pCharacter, CFacialEffectorsLibrary* pEffectorsLibrary, CFaceState* pFaceState)
{
	if (!m_bInitialized)
	{
		m_bInitialized = true;
		InitialiseBoneIDs(); // This needs to be done here because we need the parent to be attached, if this is an attachment. During
		                     // construction the sub-character is not yet attached.
	}

	QuatT additionalRotations[EyeCOUNT];
	CalculateEyeAdditionalRotation(pCharacter, pFaceState, pEffectorsLibrary, additionalRotations);

	for (EyeID eye = EyeID(0); eye < EyeCOUNT; eye = EyeID(eye + 1))
		UpdateEye(rAnimLocationNext, eye, additionalRotations[eye]);
}

void CEyeMovementFaceAnim::OnExpressionLibraryLoad()
{
	InitialiseChannels();
}

void CEyeMovementFaceAnim::InitialiseChannels()
{
	std::fill(m_channelEntries, m_channelEntries + EffectorCOUNT, CChannelEntry());
}

uint32 CEyeMovementFaceAnim::GetChannelForEffector(EffectorID effector)
{
	assert(effector >= 0 && effector < EffectorCOUNT);
	if (effector < 0 || effector >= EffectorCOUNT)
		return ~0;

	// Check whether the effector has already been loaded.
	if (!m_channelEntries[effector].loadingAttempted)
		CreateChannelForEffector(effector);

	return m_channelEntries[effector].id;
}

uint32 CEyeMovementFaceAnim::CreateChannelForEffector(EffectorID effector)
{
	assert(effector >= 0 && effector < EffectorCOUNT);
	if (effector < 0 || effector >= EffectorCOUNT)
		return ~0;

	// Look up the effector.
	CFaceIdentifierHandle ident = RetrieveEffectorIdentifiers()[effector];
	CFacialEffector* pEffector = static_cast<CFacialEffector*>(m_pInstance->FindEffector(ident));

	// Try to create a channel for the effector.
	uint32 id = ~0;
	if (pEffector)
	{
		SFacialEffectorChannel ch;
		ch.status = SFacialEffectorChannel::STATUS_ONE;
		ch.pEffector = (CFacialEffector*)pEffector;
		ch.fCurrWeight = 0;
		ch.fWeight = 0;
		ch.bIgnoreLifeTime = true;
		ch.fFadeTime = 0;
		ch.fLifeTime = 0;
		ch.nRepeatCount = 0;

		id = m_pInstance->GetAnimContext()->StartChannel(ch);
	}

	// Update the cache information.
	m_channelEntries[effector].id = id;
	m_channelEntries[effector].loadingAttempted = true;

	return id;
}

void CEyeMovementFaceAnim::UpdateEye(const QuatTS& rAnimLocationNext, EyeID eye, const QuatT& additionalRotation)
{
	if (m_eyeBoneIDs[eye] < 0)
		return;

	// Set the weights of all the channels to 0.
	for (DirectionID direction = static_cast<DirectionID>(0); direction < DirectionCOUNT; direction = static_cast<DirectionID>(direction + 1))
	{
		uint32 id = GetChannelForEffector(EffectorFromEyeAndDirection(eye, direction));
		if (id != ~0)
			m_pInstance->GetAnimContext()->SetChannelWeight(id, 0);
	}

	// Find the direction the eye is looking in.
	float angle = 0;
	float strength = 0;
	FindLookAngleAndStrength(eye, angle, strength, additionalRotation);

	// Find the two closest directions to the angle.
	float octantAngle = (angle + gf_PI) / gf_PI2;
	octantAngle = (octantAngle * DirectionCOUNT);
	assert(octantAngle >= 0);

	DirectionID directionInterpolateLow = DirectionID(int(floorf(octantAngle)));
	if (directionInterpolateLow >= DirectionCOUNT)
		directionInterpolateLow = DirectionID(directionInterpolateLow - DirectionCOUNT);

	DirectionID directionInterpolateHigh = DirectionID(int(ceilf(octantAngle + FLT_MIN)));
	if (directionInterpolateHigh >= DirectionCOUNT)
		directionInterpolateHigh = DirectionID(directionInterpolateHigh - DirectionCOUNT);

	float interpolationFraction = octantAngle - floorf(octantAngle);

	//float fC2[4] = {1,1,0,1};
	//g_pAuxGeom->Draw2dLabel( 1,g_YLine, 1.6f, fC2, false,"directionInterpolateLow: %d   directionInterpolateHigh: %d  interpolationFraction: %f",directionInterpolateLow,directionInterpolateHigh,interpolationFraction);
	//g_YLine+=16.0f;

	// Set the weight of the two channels.
	{
		uint32 id = GetChannelForEffector(EffectorFromEyeAndDirection(eye, directionInterpolateLow));
		if (id != ~0)
			m_pInstance->GetAnimContext()->SetChannelWeight(id, sinf((1.0f - interpolationFraction) * gf_PI * 0.5f) * strength);
	}
	{
		uint32 id = GetChannelForEffector(EffectorFromEyeAndDirection(eye, directionInterpolateHigh));
		if (id != ~0)
			m_pInstance->GetAnimContext()->SetChannelWeight(id, sinf(interpolationFraction * gf_PI * 0.5f) * strength);
	}

	if (Console::GetInst().ca_DebugFacialEyes)
	{
		stack_string text;
		text.Format("%.3f, %.3f, %.3f", octantAngle, strength, interpolationFraction * strength);
		DisplayDebugInfoForEye(rAnimLocationNext, eye, text);
	}
}

void CEyeMovementFaceAnim::InitialiseBoneIDs()
{
	CAttachmentManager* pMasterAttachmentManager = (CAttachmentManager*)m_pInstance->GetMasterCharacter()->GetIAttachmentManager();
	if (pMasterAttachmentManager)
	{
		CCharInstance* pMasterCharacterInstance = pMasterAttachmentManager->m_pSkelInstance;
		CDefaultSkeleton& rDefaultSkeleton = *pMasterAttachmentManager->m_pSkelInstance->m_pDefaultSkeleton;
		for (EyeID eye = EyeID(0); eye < EyeCOUNT; eye = EyeID(eye + 1))
			m_eyeBoneIDs[eye] = rDefaultSkeleton.GetJointIDByName(szEyeBoneNames[eye]);
	}

}

void CEyeMovementFaceAnim::FindLookAngleAndStrength(EyeID eye, float& angle, float& strength, const QuatT& additionalRotation)
{
	CAttachmentManager* pMasterAttachmentManager = (CAttachmentManager*)m_pInstance->GetMasterCharacter()->GetIAttachmentManager();
	if (pMasterAttachmentManager == 0)
		return;

	CCharInstance* pMasterCharacterInstance = pMasterAttachmentManager->m_pSkelInstance;
	CSkeletonPose* pSkeletonPose = &pMasterCharacterInstance->m_SkeletonPose;
	if (pSkeletonPose == 0)
		return;

	// Find the transform of the bone and the default transform.
	QuatT eyeBoneTransform = pSkeletonPose->GetRelJointByID(m_eyeBoneIDs[eye]);

	CAttachmentBONE* pAttachment = 0;
	int32 aidx = -1;
	if (eye == 0)
		aidx = pMasterAttachmentManager->GetIndexByName("eye_left");
	if (eye == 1)
		aidx = pMasterAttachmentManager->GetIndexByName("eye_right");
	if (aidx >= 0)
	{
		pAttachment = (CAttachmentBONE*)pMasterAttachmentManager->GetInterfaceByIndex(aidx);
		eyeBoneTransform.q = pAttachment->m_addTransformation.q;
	}

	eyeBoneTransform = eyeBoneTransform * additionalRotation;
	Vec3 forward = eyeBoneTransform.GetColumn1();

	angle = 0.0f;
	strength = 0.0f;
	if (fabsf(forward.x) > 0.001f || fabsf(forward.z) > 0.001f)
	{
		angle = atan2f(forward.z, forward.x);
		strength = (acos_tpl(forward.y) / 15.0f) * 180.0f / 3.14159f;
		if (strength > 2.0f)
			strength = 2.0f;
	}

	//	float fC1[4] = {1,0,0,1};
	//	g_pAuxGeom->Draw2dLabel( 1,g_YLine, 1.6f, fC1, false,"x: %f  y: %f  z: %f    angle:%f strength:%f",forward.x,forward.y,forward.z,angle,strength);
	//	g_YLine+=16.0f;
}

void CEyeMovementFaceAnim::DisplayDebugInfoForEye(const QuatTS& rAnimLocationNext, EyeID eye, const string& text)
{
	CAttachmentManager* pMasterAttachmentManager = (CAttachmentManager*)m_pInstance->GetMasterCharacter()->GetIAttachmentManager();
	if (pMasterAttachmentManager == 0)
		return;
	CCharInstance* pMasterCharacterInstance = pMasterAttachmentManager->m_pSkelInstance;
	CSkeletonPose* pSkeletonPose = &pMasterCharacterInstance->m_SkeletonPose;

	QuatT eyeBoneTransform = (pSkeletonPose ? pSkeletonPose->GetAbsJointByID(m_eyeBoneIDs[eye]) : QuatT(IDENTITY));
	Vec3 pos = rAnimLocationNext.t;
	float color[4] = { 1, 1, 1, 1 };
	pos += (pSkeletonPose ? pSkeletonPose->GetAbsJointByID(m_eyeBoneIDs[eye]).t : Vec3(ZERO));
	IRenderAuxText::DrawLabelEx(pos, 2.0f, color, true, true, text.c_str());
}

void CEyeMovementFaceAnim::CalculateEyeAdditionalRotation(CCharInstance* pCharacter, CFaceState* pFaceState, CFacialEffectorsLibrary* pEffectorsLibrary, QuatT additionalRotation[EyeCOUNT])
{
	for (EyeID eye = EyeID(0); eye < EyeCOUNT; eye = EyeID(eye + 1))
		additionalRotation[eye] = IDENTITY;

	if (pFaceState != NULL && pEffectorsLibrary != NULL)
	{
		int nNumIndices = pFaceState->GetNumWeights();
		for (int i = 0; i < nNumIndices; i++)
		{
			const float fWeight = pFaceState->GetWeight(i);
			if (fabsf(fWeight) < EFFECTOR_MIN_WEIGHT_EPSILON)
				continue;

			CFacialEffector* pEffector = pEffectorsLibrary->GetEffectorFromIndex(i);
			if (pEffector)
			{
				switch (pEffector->GetType())
				{
				case EFE_TYPE_BONE:
					{
						CFacialEffectorBone* pBoneEffector = static_cast<CFacialEffectorBone*>(pEffector);
						for (EyeID eye = EyeID(0); eye < EyeCOUNT; eye = EyeID(eye + 1))
						{
							const uint32 bonEffectorBoneCrc = pBoneEffector->GetAttachmentCRC();
							const uint32 eyeCrc = ms_eyeBoneNamesCrc[eye];
							if (bonEffectorBoneCrc == eyeCrc)
							{
								QuatT rotation(IDENTITY);
								rotation.q.SetNlerp(pBoneEffector->GetQuatT().q, IDENTITY, fWeight);
								additionalRotation[eye] = rotation * additionalRotation[eye];
							}
						}
					}
					break;
				}
			}
		}
	}
}

void CEyeMovementFaceAnim::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(this, sizeof(*this));
	pSizer->AddObject(m_channelEntries, sizeof(CChannelEntry));
	pSizer->AddObject(m_eyeBoneIDs);
	pSizer->AddObject(m_bInitialized);
	pSizer->AddObject(RetrieveEffectorIdentifiers(), EYEMOVEMENT_EFFECTOR_AMOUNT * sizeof(CFaceIdentifierHandle));
}
