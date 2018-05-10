// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "FacialInstance.h"

#include "FaceAnimation.h"
#include "FacialModel.h"
#include "FaceAnimSequence.h"
#include "LipSync.h"
#include "FaceEffectorLibrary.h"
#include "../PathExpansion.h"
#include <Cry3DEngine/I3DEngine.h>
#include <Cry3DEngine/CGF/CryHeaders.h>
#include <CryAudio/IAudioSystem.h>
#include "../CharacterManager.h"
#include "../CharacterInstance.h"

#define RANDOM_IDLES_EFFECTOR    "Random_Idles"
#define HEAD_PITCH_EFFECTOR      "Head_Pitch"
#define HEAD_YAW_EFFECTOR        "Head_Yaw"
#define HEAD_ROLL_EFFECTOR       "Head_Roll"

#define EYES_LEFT_RIGHT_EFFECTOR "Eyes_LeftRight"
#define EYES_UP_DOWN_EFFECTOR    "Eyes_UpDown"

#define BLINK_EFFECTOR           "Blink"
#define BLINK_PAUSE              (4.0f)
#define BLINK_DURATION           (0.1f)

CFacialInstance::CFacialInstance(CFacialModel* pDefaultSkeleton, CCharInstance* pChar)
{
	m_pDefaultSkeleton = pDefaultSkeleton;
	m_pFaceState = pDefaultSkeleton->CreateState();

	m_pMasterInstance = pChar;

	m_pAnimContext = new CFacialAnimationContext(this);

	//m_procFace.fCurTime = 0;

	m_pEyeMovement = new CEyeMovementFaceAnim(this);

	m_pMasterInstance->m_SkeletonAnim.m_facialDisplaceInfo.Initialize(m_pMasterInstance->m_pDefaultSkeleton->GetJointCount());

	// procedural eyes movements
	m_tProcFace.m_JitterEyesPositions[0].Set(-1, -1, -4);
	m_tProcFace.m_JitterEyesPositions[1].Set(1, 1, -2);
	m_tProcFace.m_JitterEyesPositions[2].Set(1, 1, -4);
	m_tProcFace.m_JitterEyesPositions[3].Set(-1, -1, -2);
	m_tProcFace.m_nCurrEyesJitter = 0;
	m_tProcFace.m_fEyesJitterScale = 0.008f;
	m_tProcFace.m_fLastJitterTime = 0;
	m_tProcFace.m_fJitterTimeChange = 2.0;
	m_tProcFace.m_pBlink = (CFacialEffector*)FindEffector("Blink");
	m_tProcFace.m_bEyesBlinking = false;
	m_tProcFace.m_fBlinkingTime = -1;
	m_tProcFace.m_bEnabled = true;

	m_currentLayer = -1;
	Reset();
}

//////////////////////////////////////////////////////////////////////////
IFacialModel* CFacialInstance::GetFacialModel()
{
	return m_pDefaultSkeleton;
}

//////////////////////////////////////////////////////////////////////////
IFaceState* CFacialInstance::GetFaceState()
{
	return m_pFaceState;
}

//////////////////////////////////////////////////////////////////////////
void CFacialInstance::Update(CFacialDisplaceInfo& info, float fDeltaTimeSec, const QuatTS& rAnimLocationNext)
{
	DEFINE_PROFILER_FUNCTION();

	if (Console::GetInst().ca_UseFacialAnimation == 0)
		return;

#if USE_FACIAL_ANIMATION_FRAMERATE_LIMITING
	if (m_frameSkipper.UpdateDue())
#endif
	{
		Vec3 camPosIgnored(0, 0, 0);
		ApplyProceduralFaceBehaviour(camPosIgnored); // TODO: Fix eye procedural movement. Just getting blinking back for the moment.
		UpdatePlayingSequences(fDeltaTimeSec, rAnimLocationNext);
		UpdateProceduralFaceBehaviour();
		CFacialEffectorsLibrary* pLibrary = (m_pDefaultSkeleton ? static_cast<CFacialEffectorsLibrary*>(m_pDefaultSkeleton->GetLibrary()) : 0);
		m_pEyeMovement->Update(fDeltaTimeSec, rAnimLocationNext, m_pMasterInstance, pLibrary, m_pFaceState);
		if (m_pAnimContext->Update(*m_pFaceState, rAnimLocationNext))
		{
			m_pDefaultSkeleton->FillInfoMapFromFaceState(info, m_pFaceState, m_pMasterInstance, m_forcedRotations.size(), m_forcedRotations.begin(), m_pAnimContext->GetBoneRotationSmoothRatio());
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialInstance::UpdatePlayingSequences(float fDeltaTimeSec, const QuatTS& rAnimLocationNext)
{
	m_pAnimContext->UpdatePlayingSequences(rAnimLocationNext);

	// Check whether the sequence has finished.
	IFacialAnimSequence* pSequence = (m_currentLayer >= 0 ? m_layers[m_currentLayer].sequence : _smart_ptr<IFacialAnimSequence>(0));
	if (pSequence && !m_pAnimContext->IsPlayingSequence((CFacialAnimSequence*)pSequence))
	{
		if (Console::GetInst().ca_DebugFacial)
			CryLogAlways("CFacialInstance::Update - (this=%p) sequence \"%s\" finished", this, (pSequence ? pSequence->GetName() : "NULL"));
		IFacialAnimSequence* pOldSequence = (m_currentLayer >= 0 ? m_layers[m_currentLayer].sequence : _smart_ptr<IFacialAnimSequence>(0));
		if (m_currentLayer >= 0)
			m_layers[m_currentLayer] = LayerInfo();
		UpdateCurrentSequence(pOldSequence);
	}

	ProcessWaitingLipSync();
}

//////////////////////////////////////////////////////////////////////////
uint32 CFacialInstance::StartEffectorChannel(IFacialEffector* pEffector, float fWeight, float fFadeTime, float fLifeTime, int nRepeatCount)
{
	uint32 uChannelID = ~0;
	if (m_pAnimContext)
		uChannelID = m_pAnimContext->FadeInChannel(pEffector, fWeight, fFadeTime, fLifeTime, nRepeatCount);
	return uChannelID;
}

//////////////////////////////////////////////////////////////////////////
void CFacialInstance::StopEffectorChannel(uint32 nChannelID, float fFadeOutTime)
{
	if (m_pAnimContext)
		m_pAnimContext->StopChannel(nChannelID, fFadeOutTime);
}

//////////////////////////////////////////////////////////////////////////
void CFacialInstance::PreviewEffector(IFacialEffector* pEffector, float fWeight, float fBalance)
{
	m_pAnimContext->RemoveAllPreviewChannels();

	if (pEffector)
	{
		SFacialEffectorChannel channel;
		channel.bPreview = true;
		channel.status = SFacialEffectorChannel::STATUS_ONE;
		channel.pEffector = (CFacialEffector*)pEffector;
		channel.fWeight = fWeight;
		channel.fBalance = fBalance;

		m_pAnimContext->StartChannel(channel);
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialInstance::PreviewEffectors(IFacialEffector** pEffectors, float* fWeights, float* fBalances, int nEffectorsCount)
{
	m_pAnimContext->RemoveAllPreviewChannels();

	for (int i = 0; i < nEffectorsCount; i++)
	{
		if (fWeights[i] == 0)
			continue;

		SFacialEffectorChannel channel;
		channel.bPreview = true;
		channel.status = SFacialEffectorChannel::STATUS_ONE;
		channel.pEffector = (CFacialEffector*)pEffectors[i];
		channel.fWeight = fWeights[i];
		channel.fBalance = fBalances[i];

		m_pAnimContext->StartChannel(channel);

	}
}

//////////////////////////////////////////////////////////////////////////
IFacialEffector* CFacialInstance::FindEffector(CFaceIdentifierHandle ident)
{
	IFacialEffectorsLibrary* pLib = m_pDefaultSkeleton->GetLibrary();
	if (pLib)
	{
		return pLib->Find(ident);
	}
	return NULL;
}

IFacialEffector* CFacialInstance::FindEffector(const char* identStr)
{
	IFacialEffectorsLibrary* pLib = m_pDefaultSkeleton->GetLibrary();
	if (pLib)
	{
		return pLib->Find(identStr);
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
void CFacialInstance::PlaySequence(IFacialAnimSequence* pSequence, EFacialSequenceLayer layer, bool bExclusive, bool bLooping)
{
	if (Console::GetInst().ca_DebugFacial)
	{
		CryLogAlways("CFacialInstance::PlaySequence(this=%p, sequence=\"%s\", layer=%d)",
		             this, (pSequence ? pSequence->GetName() : "NULL"), layer);
	}

	IFacialAnimSequence* pOldSequence = (m_currentLayer >= 0 ? m_layers[m_currentLayer].sequence : _smart_ptr<IFacialAnimSequence>(0));

	m_layers[layer] = LayerInfo(pSequence, bExclusive, bLooping);

	// TODO: Better way to handle this? (left over when moving from CAnimatedCharacter to here).
	m_layers[layer].bLooping = m_layers[layer].bLooping ||
	                           (layer == eFacialSequenceLayer_AIExpression) || (layer == eFacialSequenceLayer_AGStateAndAIAlertness);

	UpdateCurrentSequence(pOldSequence);
}

//////////////////////////////////////////////////////////////////////////
void CFacialInstance::SeekSequence(EFacialSequenceLayer layer, float fTime)
{
	CFacialAnimSequence* pSequence = (CFacialAnimSequence*)m_layers[layer].sequence.get();
	if (pSequence)
		m_pAnimContext->SeekSequence(pSequence, fTime);
}

//////////////////////////////////////////////////////////////////////////
void CFacialInstance::StopSequence(EFacialSequenceLayer layer)
{
	if (Console::GetInst().ca_DebugFacial)
	{
		CryLogAlways("CFacialInstance::StopSequence(this=%p, sequence=\"%s\", layer=%d)",
		             this, (m_layers[layer].sequence ? m_layers[layer].sequence->GetName() : "NULL"), layer);
	}

	IFacialAnimSequence* pOldSequence = (m_currentLayer >= 0 ? m_layers[m_currentLayer].sequence : _smart_ptr<IFacialAnimSequence>(0));

	CFacialAnimSequence* pSequence = (CFacialAnimSequence*)m_layers[layer].sequence.get();
	if (pSequence)
		m_pAnimContext->StopSequence(pSequence);
	m_layers[layer] = LayerInfo();
	UpdateCurrentSequence(pOldSequence);
}

//////////////////////////////////////////////////////////////////////////
void CFacialInstance::StopAllSequencesAndChannels()
{
	m_pAnimContext->StopAllSequences();
	m_pAnimContext->RemoveAllChannels();
	Reset();
}

void CFacialInstance::SetUseFrameRateLimiting(bool useFrameRateLimiting)
{
#if USE_FACIAL_ANIMATION_FRAMERATE_LIMITING
	m_frameSkipper.SetEnabled(useFrameRateLimiting);
#endif
}

//////////////////////////////////////////////////////////////////////////
bool CFacialInstance::IsPlaySequence(IFacialAnimSequence* pSequence, EFacialSequenceLayer layer)
{
	if (layer < 0 || layer >= eFacialSequenceLayer_COUNT)
	{
		CryWarning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, "Invalid layer passed - %d", layer);
		return false;
	}

	return m_layers[layer].sequence == pSequence;
}

//////////////////////////////////////////////////////////////////////////
void CFacialInstance::PauseSequence(EFacialSequenceLayer layer, bool bPaused)
{
	CFacialAnimSequence* pSequence = (CFacialAnimSequence*)m_layers[layer].sequence.get();
	if (pSequence)
		m_pAnimContext->PauseSequence(pSequence, bPaused);
}
//////////////////////////////////////////////////////////////////////////
namespace
{
bool GetFacialSequenceFilenameForSound(const uint32 nSoundId, IFacialEffectorsLibrary* pEffectorsLib, CCharInstance* pMasterInstance, stack_string& filenameOut)
{
	REINST("getting sound file name for lipsync");
	return true;
	//_smart_ptr<ISound> pSound = gEnv->pAudioSystem->GetSound(nSoundId);
	//if (!pSound)
	//	return false;

	//stack_string sndName = pSound->GetName();
	//sndName.replace( ':','/' );

	//// TODO: we need a proper way of getting the actual sound path from soundsystem. What we get right now, includes a non existent language folder that was added for awareness purposes on debug info
	//// also that imaginary folder is not always in the same position. That can happens when the first slash is an inverted slash.
	//static ICVar* const pCVar = gEnv->pConsole->GetCVar("g_languageAudio");
	//if (pCVar)
	//{
	//	stack_string sLanguage("/");
	//	sLanguage += pCVar->GetString();
	//	sndName.replace( sLanguage, "" );
	//}

	//if (sndName.find('/') != stack_string::npos || sndName.find('\\') != stack_string::npos)
	//{
	//	const stack_string sequenceFileName = sndName;
	//	filenameOut = PathUtil::ReplaceExtension(PathUtil::ToUnixPath(sequenceFileName),FACIAL_SEQUENCE_EXT);
	//	const bool exists = gEnv->pCryPak->IsFileExist( filenameOut.c_str() );
	//	if (exists)
	//		return true;
	//}

	//sndName = stack_string(PathUtil::GetFileName( sndName ));

	//if (pEffectorsLib != NULL)
	//{
	//	// Try to find sequence in the same folder as facial expression.
	//	const stack_string sequenceFilename = PathUtil::Make( PathUtil::GetPath(stack_string(pEffectorsLib->GetName())),sndName );
	//	filenameOut = PathUtil::ReplaceExtension(PathUtil::ToUnixPath(sequenceFilename),FACIAL_SEQUENCE_EXT);
	//	const bool exists = gEnv->pCryPak->IsFileExist( filenameOut.c_str() );
	//	if (exists)
	//		return true;
	//}

	//// Try to find sequence in the same folder as character.
	//const stack_string sequenceFilename = PathUtil::Make( PathUtil::GetPath(stack_string(pMasterInstance->GetFilePath())),sndName );
	//filenameOut = PathUtil::ReplaceExtension(PathUtil::ToUnixPath(sequenceFilename),FACIAL_SEQUENCE_EXT);
	//const bool exists = gEnv->pCryPak->IsFileExist( filenameOut.c_str() );
	//return exists;
}
}

//////////////////////////////////////////////////////////////////////////
void CFacialInstance::LipSyncWithSound(uint32 nSoundId, bool bStop)
{
	if (bStop)
	{
		const int32 waitingSoundId = m_waitingLipSync.GetSoundID();
		if (waitingSoundId == nSoundId)
		{
			m_waitingLipSync.Done();
		}

		stack_string fsqName = "";
		IFacialEffectorsLibrary* pEffectorsLib = m_pDefaultSkeleton->GetLibrary();
		const bool fsqExists = GetFacialSequenceFilenameForSound(nSoundId, pEffectorsLib, m_pMasterInstance, fsqName);
		if (fsqExists)
		{
			IFacialAnimSequence* pSequence = g_pCharacterManager->GetIFacialAnimation()->FindLoadedSequence(fsqName);
			if (pSequence)
			{
				m_pAnimContext->StopSequence((CFacialAnimSequence*)(pSequence));
			}
		}
	}
	else
	{
		stack_string fsqName = "";
		IFacialEffectorsLibrary* pEffectorsLib = m_pDefaultSkeleton->GetLibrary();
		const bool fsqExists = GetFacialSequenceFilenameForSound(nSoundId, pEffectorsLib, m_pMasterInstance, fsqName);
		if (fsqExists)
		{
			IFacialAnimSequence* pSequence = g_pCharacterManager->GetIFacialAnimation()->StartStreamingSequence(fsqName.c_str());
			if (pSequence)
			{
				m_waitingLipSync.Done();
				m_waitingLipSync.StartWaiting(pSequence, nSoundId);
			}
		}
	}
}

void CFacialInstance::ProcessWaitingLipSync()
{
	if (m_waitingLipSync.IsWaiting())
	{
		IFacialAnimSequence* pSequence = m_waitingLipSync.GetAnimSequence();
		assert(pSequence);
		if (pSequence->IsInMemory())
		{
			REINST(lipsync to voice line)
			// We found a facial sequence corresponding to this file.
			/*const int32 soundId = m_waitingLipSync.GetSoundID();
			   _smart_ptr<ISound> pSound = gEnv->pAudioSystem->GetSound(soundId);
			   if (pSound)
			   {
			   PlaySequence(pSequence, eFacialSequenceLayer_Dialogue, false);
			   const int nSoundMillis = pSound->GetInterfaceExtended()->GetCurrentSamplePos(true);
			   const float soundSeconds = static_cast<float>(nSoundMillis)/1000.0f;
			   SeekSequence(eFacialSequenceLayer_Dialogue, soundSeconds);
			   }*/

			m_waitingLipSync.Done();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialInstance::OnExpressionLibraryLoad()
{
	m_pEyeMovement->OnExpressionLibraryLoad();
}

//////////////////////////////////////////////////////////////////////////
IFacialAnimSequence* CFacialInstance::LoadSequence(const char* sSequenceName, bool addToCache)
{
	if (!sSequenceName)
		return 0;

	if (!m_pMasterInstance)
	{
		gEnv->pLog->LogError("Loading facial animation sequence failed - instance has no master character instance (Code error).");
		return 0;
	}

	CAnimationSet* pAnimationSet = m_pMasterInstance->m_pDefaultSkeleton->m_pAnimationSet;
	if (!pAnimationSet)
	{
		gEnv->pLog->LogError("Loading facial animation sequence failed - facial animation instance has no animation set (Code error).");
		return 0;
	}

	const char* szSequencePattern = (pAnimationSet && sSequenceName ? pAnimationSet->GetFacialAnimationPathByName(sSequenceName) : 0);
	if (!szSequencePattern)
	{
		//GDC09HACK
		//	AnimWarning("Facial sequence \"%s\" not listed in chrparams file for character \"%s\".", sSequenceName, m_pMasterInstance->m_pDefaultSkeleton->GetModelFilePath());
		return 0;
	}

	static const int MAX_PATH_LENGTH = 1024;
	char pathBuffer[MAX_PATH_LENGTH] = "";
	int patternLength = strlen(szSequencePattern);
	if (patternLength + 1 > MAX_PATH_LENGTH)
	{
		//GDC09HACK
		//	AnimWarning("Facial sequence chrparams file pattern for \"%s\" exceeds maximum length (%d)", sSequenceName, MAX_PATH_LENGTH);
		return 0;
	}

	PathExpansion::SelectRandomPathExpansion(szSequencePattern, pathBuffer);

	IFacialAnimSequence* pSequence = (szSequencePattern ? g_pCharacterManager->GetIFacialAnimation()->StartStreamingSequence(pathBuffer) : 0);
	return pSequence;
}

//////////////////////////////////////////////////////////////////////////
void PrecacheFacialExpressionCallback(void* userData, const char* expression)
{
	if (expression)
		g_pCharacterManager->GetIFacialAnimation()->StartStreamingSequence(expression);
}
void CFacialInstance::PrecacheFacialExpression(const char* sSequenceName)
{
	if (!sSequenceName)
		return;

	if (!m_pMasterInstance)
	{
		gEnv->pLog->LogError("Loading facial animation sequence failed - instance has no master character instance (Code error).");
		return;
	}

	CAnimationSet* pAnimationSet = m_pMasterInstance->m_pDefaultSkeleton->m_pAnimationSet;
	if (!pAnimationSet)
	{
		gEnv->pLog->LogError("Loading facial animation sequence failed - facial animation instance has no animation set (Code error).");
		return;
	}

	const char* szSequencePattern = (pAnimationSet && sSequenceName ? pAnimationSet->GetFacialAnimationPathByName(sSequenceName) : 0);
	if (!szSequencePattern)
	{
		//GDC09HACK
		//	AnimWarning("Facial sequence \"%s\" not listed in chrparams file for character \"%s\".", sSequenceName, m_pMasterInstance->m_pDefaultSkeleton->GetModelFilePath());
		return;
	}

	const int MAX_PATH_LENGTH = 1024;
	int patternLength = strlen(szSequencePattern);
	if (patternLength + 1 > MAX_PATH_LENGTH)
	{
		//GDC09HACK
		//	AnimWarning("Facial sequence chrparams file pattern for \"%s\" exceeds maximum length (%d)", sSequenceName, MAX_PATH_LENGTH);
		return;
	}

	CRY_ASSERT(szSequencePattern);
	PathExpansion::EnumeratePathExpansions(szSequencePattern, PrecacheFacialExpressionCallback, 0);
}

//////////////////////////////////////////////////////////////////////////
void CFacialInstance::UpdateProceduralFaceBehaviour()
{

	if (m_tProcFace.m_bEyesBlinking)
	{
		if (!m_tProcFace.m_pBlink)
			m_tProcFace.m_pBlink = (CFacialEffector*)FindEffector("Blink");

		float fCurrTime = gEnv->pTimer->GetCurrTime();

		if (m_tProcFace.m_fBlinkingTime < 0)
			m_tProcFace.m_fBlinkingTime = fCurrTime;

		if (m_tProcFace.m_pBlink)
		{
			SFacialEffectorChannel effch;
			effch.status = SFacialEffectorChannel::STATUS_ONE;
			effch.bTemporary = true;
			effch.pEffector = m_tProcFace.m_pBlink;
			effch.fWeight = 1.0f; //cry_random(0.0f, 1.0f);
			m_pAnimContext->StartChannel(effch);
		}

		if (fCurrTime - m_tProcFace.m_fBlinkingTime > 0.1f)
		{
			m_tProcFace.m_bEyesBlinking = false;
			m_tProcFace.m_fBlinkingTime = -1;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialInstance::ApplyProceduralFaceBehaviour(Vec3& vCamPos)
{

	if (m_tProcFace.m_bEnabled == false || Console::GetInst().ca_eyes_procedural == 0)
	{
		m_tProcFace.m_bEyesBlinking = false;
		return;
	}

	float fCurrtime = gEnv->pTimer->GetCurrTime();

	// simple rapid eyes movements, staring at eyes and mouth when talking
	if (fCurrtime - m_tProcFace.m_fLastJitterTime > m_tProcFace.m_fJitterTimeChange)
	{
		m_tProcFace.m_fLastJitterTime = fCurrtime;

		// golden proportion 3/5
		m_tProcFace.m_nCurrEyesJitter = cry_random(0, 12) & 3;
		m_tProcFace.m_fJitterTimeChange = cry_random(0.1f, 1.5f);
		m_tProcFace.m_fEyesJitterScale = cry_random(0.002f, 0.02f);

		// eyes movements are in sync with eyes blinking.
		if ((cry_random(0, 4) & 3) == 1)
			m_tProcFace.m_bEyesBlinking = true;
	}

	vCamPos += (m_tProcFace.m_JitterEyesPositions[m_tProcFace.m_nCurrEyesJitter] * m_tProcFace.m_fEyesJitterScale);

}

//////////////////////////////////////////////////////////////////////////
void CFacialInstance::EnableProceduralFacialAnimation(bool bEnable)
{
	m_tProcFace.m_bEnabled = bEnable;
}

//////////////////////////////////////////////////////////////////////////
bool CFacialInstance::IsProceduralFacialAnimationEnabled() const
{
	return m_tProcFace.m_bEnabled;
}

//////////////////////////////////////////////////////////////////////////
void CFacialInstance::SetForcedRotations(int numForcedRotations, CFacialAnimForcedRotationEntry* forcedRotations)
{
	m_forcedRotations.resize(numForcedRotations);
	if (forcedRotations && numForcedRotations)
		memcpy(&m_forcedRotations[0], forcedRotations, numForcedRotations * sizeof(forcedRotations[0]));
}

//////////////////////////////////////////////////////////////////////////
void CFacialInstance::SetMasterCharacter(ICharacterInstance* pMasterInstance)
{
	m_pMasterInstance = (CCharInstance*)pMasterInstance;
}

//////////////////////////////////////////////////////////////////////////
void CFacialInstance::TemporarilyEnableBoneRotationSmoothing()
{
	if (Console::GetInst().ca_DebugFacial)
		CryLogAlways("CFacialInstance::TemporarilyEnableBoneRotationSmoothing - (this=%p) enabling bone rotation smoothing", this);
	m_pAnimContext->TemporarilyEnableAdditionalBoneRotationSmoothing();
}

//////////////////////////////////////////////////////////////////////////
void CFacialInstance::Reset()
{
	if (Console::GetInst().ca_DebugFacial)
		CryLogAlways("CFacialInstance::Reset(this=%p)", this);

	IFacialAnimSequence* pOldSequence = (m_currentLayer >= 0 ? m_layers[m_currentLayer].sequence : _smart_ptr<IFacialAnimSequence>(0));
	for (int layer = 0; layer < eFacialSequenceLayer_COUNT; ++layer)
		m_layers[layer] = LayerInfo();
	m_currentLayer = -1;
	UpdateCurrentSequence(pOldSequence);
}

//////////////////////////////////////////////////////////////////////////
void CFacialInstance::UpdateCurrentSequence(IFacialAnimSequence* pPreviousSequence)
{
	m_currentLayer = -1;
	for (int i = 0; i < eFacialSequenceLayer_COUNT; ++i)
	{
		if (m_layers[i].sequence)
		{
			m_currentLayer = i;
			LayerInfo& layer = m_layers[m_currentLayer];

			if (Console::GetInst().ca_DebugFacial)
				CryLogAlways("CFacialInstance::UpdateCurrentSequence (this=%p) - activating layer %d (sequence = \"%s\")", this, i, (layer.sequence ? layer.sequence->GetName() : "NULL"));

			break;
		}
	}

	// check if it changed
	LayerInfo newSequence;
	if (m_currentLayer >= 0)
		newSequence = m_layers[m_currentLayer];
	if (pPreviousSequence != newSequence.sequence)
	{
		// DEBUGGING
		//CryLogAlways("this = %p, prev = %p, new = %p", this, pPreviousSequence, newSequence.sequence);

		if (Console::GetInst().ca_DebugFacial)
			CryLogAlways("CFacialInstance::UpdateCurrentSequence - (this=%p) change detected", this);

		// stop old sequence
		if (pPreviousSequence)
		{
			if (Console::GetInst().ca_DebugFacial)
				CryLogAlways("CFacialInstance::UpdateCurrentSequence - (this=%p) stopping sequence \"%s\"", this, (pPreviousSequence ? pPreviousSequence->GetName() : "NULL"));
			m_pAnimContext->StopSequence((CFacialAnimSequence*)pPreviousSequence);
		}

		if (newSequence.sequence)
		{
			if (Console::GetInst().ca_DebugFacial)
				CryLogAlways("CFacialInstance::UpdateCurrentSequence - (this=%p) starting sequence \"%s\"", this, (newSequence.sequence ? newSequence.sequence->GetName() : "NULL"));
			m_pAnimContext->PlaySequence((CFacialAnimSequence*)newSequence.sequence.get(), newSequence.bExclusive, newSequence.bLooping);
		}

		if (Console::GetInst().ca_DebugFacial)
			CryLogAlways("CFacialInstance::UpdateCurrentSequence - (this=%p) enabling bone rotation smoothing", this);
		m_pAnimContext->TemporarilyEnableAdditionalBoneRotationSmoothing();
	}
}

void CFacialInstance::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(this, sizeof(*this));
	pSizer->AddObject(m_layers);
	pSizer->AddObject(m_currentLayer);
	pSizer->AddObject(m_pAnimContext);
	pSizer->AddObject(m_pEyeMovement);
	pSizer->AddObject(&m_forcedRotations, sizeof(m_forcedRotations) + sizeof(CFacialAnimForcedRotationEntry) * m_forcedRotations.capacity());
	pSizer->AddObject(m_waitingLipSync);
	pSizer->AddObject(m_tProcFace);
}
