// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "FaceAnimation.h"

#include "FacialModel.h"
#include "LipSync.h"
#include "FacialInstance.h"
#include "FaceEffectorLibrary.h"
#include "FaceAnimSequence.h"
#include "FaceJoystick.h"
#include <CryCore/Containers/VectorMap.h>
#include <Cry3DEngine/I3DEngine.h>
#include <Cry3DEngine/CGF/CryHeaders.h>
#include "../CharacterInstance.h"

//////////////////////////////////////////////////////////////////////////
// CFacialAnimationContext
//////////////////////////////////////////////////////////////////////////
CFacialAnimationContext::CFacialAnimationContext(CFacialInstance* pFace)
	: m_nTotalAppliedEffectors(0)
	, m_pFace(pFace)
	, m_nLastChannelId(0)
	, m_bForceLastUpdate(false)
	, m_fBoneRotationSmoothingRemainingTime(0.0f)
{
	m_pFacialModel = pFace->GetModel();
}

//////////////////////////////////////////////////////////////////////////
uint32 CFacialAnimationContext::StartChannel(SFacialEffectorChannel& channel)
{
	channel.startFadeTime = m_time;
	channel.nChannelId = m_nLastChannelId++;
	channel.fCurrWeight = channel.fWeight;
	m_channels.push_back(channel);
	return channel.nChannelId;
}

//////////////////////////////////////////////////////////////////////////
uint32 CFacialAnimationContext::FadeInChannel(IFacialEffector* pEffector, float fWeight, float fFadeTime, float fLifeTime, int nRepeatCount)
{
	SFacialEffectorChannel ch;
	ch.status = SFacialEffectorChannel::STATUS_FADE_IN;
	ch.pEffector = (CFacialEffector*)pEffector;
	ch.fCurrWeight = 0;
	ch.fWeight = fWeight;
	ch.bIgnoreLifeTime = false;
	ch.fFadeTime = fFadeTime;
	ch.fLifeTime = fLifeTime;
	ch.nRepeatCount = nRepeatCount;
	return StartChannel(ch);
}

//////////////////////////////////////////////////////////////////////////
void CFacialAnimationContext::StopChannel(uint32 nChannelId, float fFadeTime)
{
	uint32 nChannels = m_channels.size();
	for (uint32 i = 0; i < nChannels; ++i)
	{
		if (m_channels[i].nChannelId == nChannelId)
		{
			SFacialEffectorChannel& channel = m_channels[i];
			if ((fabs(channel.fCurrWeight) <= EFFECTOR_MIN_WEIGHT_EPSILON) || channel.fFadeTime == 0)
			{
				if (!channel.bNotRemovable)
				{
					// Immediately stop this channel.
					m_channels.erase(m_channels.begin() + i);
				}
				break;
			}
			else
			{
				channel.fFadeTime = fFadeTime;
				// Start fade out for this channel.
				channel.StartFadeOut(m_time);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialAnimationContext::RemoveChannel(uint32 nChannelId)
{
	uint32 nChannels = (uint32)m_channels.size();
	for (uint32 i = 0; i < nChannels; ++i)
	{
		if (m_channels[i].nChannelId == nChannelId)
		{
			m_bForceLastUpdate = true;
			m_channels.erase(m_channels.begin() + i);
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialAnimationContext::RemoveAllChannels()
{
	int nChannels = (int)m_channels.size();
	for (int i = nChannels - 1; i >= 0; i--)
	{
		m_bForceLastUpdate = true;
		if (!m_channels[i].bNotRemovable)
			m_channels.erase(m_channels.begin() + i);
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialAnimationContext::RemoveAllPreviewChannels()
{
	int nChannels = (int)m_channels.size();
	for (int i = nChannels - 1; i >= 0; i--)
	{
		m_bForceLastUpdate = true;
		if (!m_channels[i].bNotRemovable && m_channels[i].bPreview)
			m_channels.erase(m_channels.begin() + i);
	}
}

//////////////////////////////////////////////////////////////////////////
SFacialEffectorChannel* CFacialAnimationContext::GetChannel(uint32 nChannelId)
{
	uint32 nChannels = (uint32)m_channels.size();
	for (uint32 i = 0; i < nChannels; ++i)
	{
		if (m_channels[i].nChannelId == nChannelId)
			return &m_channels[i];
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CFacialAnimationContext::SetChannelWeight(uint32 nChannelId, float fWeight)
{
	SFacialEffectorChannel* ch = GetChannel(nChannelId);
	if (ch)
		ch->fWeight = fWeight;
}

//////////////////////////////////////////////////////////////////////////
bool CFacialAnimationContext::Update(CFaceState& faceState, const QuatTS& rAnimLocationNext)
{
	DEFINE_PROFILER_FUNCTION();

	m_time = gEnv->pTimer->GetFrameStartTime();

	int nChannels = m_channels.size();
	if (nChannels <= 0 && !m_bForceLastUpdate && m_sequences.empty())
		return false;

	m_bForceLastUpdate = false;

	ResetFaceState(faceState);

	m_nTotalAppliedEffectors = 0;

	//////////////////////////////////////////////////////////////////////////
	// Update playing sequences.
	if (!m_sequences.empty())
	{
		AnimatePlayingSequences(rAnimLocationNext);
	}
	//////////////////////////////////////////////////////////////////////////

	IF_UNLIKELY (Console::GetInst().ca_DebugFacial == 2)
		m_debugText = "";

	for (uint32 nCurChannel = 0; nCurChannel < m_channels.size(); )
	{
		SFacialEffectorChannel& channel = m_channels[nCurChannel];
		CryPrefetch(channel.pEffector);

		// the startFadeTime most not lie in the future for an active channel
		// this however CAN happen after a saveload - with horrible consequences
		// (exploding faces bug)
		IF_UNLIKELY (m_time < channel.startFadeTime)
			channel.startFadeTime = m_time;

		// Update channel weight.
		switch (channel.status)
		{
		case SFacialEffectorChannel::STATUS_ONE:
			channel.fCurrWeight = channel.fWeight;
			if (!channel.bIgnoreLifeTime && channel.fLifeTime != 0)
			{
				float fTime = (m_time - channel.startFadeTime).GetSeconds();
				if (fTime >= channel.fLifeTime)
				{
					channel.status = SFacialEffectorChannel::STATUS_FADE_OUT;
					channel.startFadeTime = m_time;
				}
			}
			break;
		case SFacialEffectorChannel::STATUS_FADE_IN:
			{
				float fFadingTime = (m_time - channel.startFadeTime).GetSeconds();

				IF_UNLIKELY (channel.fFadeTime == 0)
					channel.fFadeTime = 0.001f;

				float dt = fFadingTime / channel.fFadeTime;

				if (dt >= 1.0f)
				{
					dt = 1.0f;
					channel.status = SFacialEffectorChannel::STATUS_ONE;
					channel.startFadeTime = m_time;
					channel.fCurrWeight = channel.fWeight;
				}
				else
					channel.fCurrWeight = channel.fWeight * (dt * dt);
			}
			break;
		case SFacialEffectorChannel::STATUS_FADE_OUT:
			{
				float fFadingTime = (m_time - channel.startFadeTime).GetSeconds();
				IF_UNLIKELY (channel.fFadeTime == 0)
					channel.fFadeTime = 0.001f;
				float dt = fFadingTime / channel.fFadeTime;
				if (dt >= 1.0f)
				{
					// Remove this channel.
					channel.fCurrWeight = 0;
					if (channel.nRepeatCount == 0)
					{
						m_channels.erase(m_channels.begin() + nCurChannel);
						continue;
					}
					else
					{
						channel.nRepeatCount--;
						channel.startFadeTime = m_time;
						channel.status = SFacialEffectorChannel::STATUS_FADE_IN;
					}
				}
				else
					channel.fCurrWeight = channel.fWeight - channel.fWeight * (dt * dt);
			}
			break;
		}

		if (channel.pEffector)
		{
			ApplyEffector(faceState, channel.pEffector, channel.fCurrWeight, channel.fBalance, channel.bLipSync);
		}

		if (channel.bTemporary)
		{
			m_channels.erase(m_channels.begin() + nCurChannel);
			continue;
		}

		nCurChannel++;
	}

	if (m_nTotalAppliedEffectors > 0)
	{
		AverageFaceState(faceState);
		m_nTotalAppliedEffectors = 0;
	}

	IF_UNLIKELY (Console::GetInst().ca_DebugFacial == 2)
	{
		CCharInstance* pChar = m_pFace->GetMasterCharacter();
		if (pChar)
		{
			Vec3 pos = rAnimLocationNext.t;
			float color[4] = { 1, 1, 1, 1 };
			int16 id = pChar->GetIDefaultSkeleton().GetJointIDByName("Bip01 Head");
			if (id >= 0)
				pos += pChar->GetISkeletonPose()->GetAbsJointByID(id).t;
			IRenderAuxText::DrawLabelEx(pos, 1.1f, color, true, true, m_debugText.c_str());
		}
	}

	// Update the amount of time left for bone rotation smoothing.
	{
		const float deltaTime = gEnv->pTimer->GetFrameTime();
		const float oldBoneRotationSmoothingRemainingTime = m_fBoneRotationSmoothingRemainingTime;
		const float newBoneRotationSmoothingRemainingTime = m_fBoneRotationSmoothingRemainingTime - deltaTime;
		m_fBoneRotationSmoothingRemainingTime = max(0.f, newBoneRotationSmoothingRemainingTime);

		IF_UNLIKELY (Console::GetInst().ca_DebugFacial)
		{
			if (oldBoneRotationSmoothingRemainingTime)
			{
				IF_UNLIKELY (newBoneRotationSmoothingRemainingTime < 0.0f)
				{
					CryLogAlways("CFacialAnimationContext::Update(this=%p) disabling bone rotation smoothing.", this);
				}
			}
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CFacialAnimationContext::ResetFaceState(CFaceState& faceState)
{
	const int numWeights = faceState.GetNumWeights();

	faceState.Reset();

	m_effectorsPerIndex.clear();
	m_effectorsPerIndex.resize(numWeights, 0);

	m_lipsyncStrength.clear();
	m_lipsyncStrength.resize(numWeights, 1.f);
}

//////////////////////////////////////////////////////////////////////////
void CFacialAnimationContext::AverageFaceState(CFaceState& faceState)
{
	int nNumWeights = faceState.GetNumWeights();
	for (int i = 0; i < nNumWeights; i++)
	{
		//if (m_effectorsPerIndex[i] > 1)
		{
			// 16/02/2010: Disabled weight check for mocap-based facial animation. The C3D data might be >1.0f.
			/*if (faceState.m_weights[i] > 1.0f)
			   faceState.m_weights[i]  = 1.0f;*/

			//////////////////////////////////////////////////////////////////////////
#if USE_FACIAL_ANIMATION_EFFECTOR_BALANCE
			const float balanceOriginal = faceState.m_balance[i];
			const float absBalance = fabsf(balanceOriginal);
			const float balancePow = 1.0f / 3.0f;
			const float absBalanceScaled = powf(absBalance, balancePow);
			const float absBalanceScaled = absBalance;
			const float absBalanceScaledClamped = max(absBalanceScaled, 1.f);
			const float balance = (balanceOriginal < 0.f) ? -absBalanceScaledClamped : absBalanceScaledClamped;
			faceState.m_balance[i] = balance;
#endif
			//////////////////////////////////////////////////////////////////////////

			//faceState.SetWeight( i,faceState.GetWeight(i)/m_effectorsPerIndex[i] );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialAnimationContext::ApplyEffector(CFaceState& faceState, CFacialEffector* pEffector, float fWeight, float fBalance, bool bIsLipSync)
{
	int nStateIndex = pEffector->GetIndexInState();
	if (nStateIndex >= 0)
	{
		if (nStateIndex < faceState.GetNumWeights())
		{
			float fFinalWeight = fWeight;

			// This code allows us to modulate the effector strength on a per-morph basis based on whether the code is
			// being used for lipsync or not.
			fFinalWeight *= bIsLipSync ? 1.f : m_lipsyncStrength[nStateIndex];

			faceState.m_weights[nStateIndex] += fFinalWeight; //*fFinalWeight*fFinalWeight;
#if USE_FACIAL_ANIMATION_EFFECTOR_BALANCE
			faceState.m_balance[nStateIndex] += fBalance * fBalance * fBalance;
#endif
			//		faceState.m_weights[nStateIndex] += fFinalWeight;
			//		faceState.m_balance[nStateIndex] += fBalance;

			//faceState.SetWeight( nStateIndex,fWeight );
			m_effectorsPerIndex[nStateIndex]++;
			m_nTotalAppliedEffectors++;
		}
		return;
	}

	IF_UNLIKELY (Console::GetInst().ca_DebugFacial == 2)
	{
		string str;
		str.Format("%s:%.2f, ", pEffector->GetIdentifier().GetString(), fWeight);
		m_debugText += str;
	}

	// Apply all sub effectors.
	int nSubEff = pEffector->GetSubEffectorCount();
	for (int i = 0; i < nSubEff; i++)
	{
		CFacialEffCtrl* pCtrl = (CFacialEffCtrl*)pEffector->GetSubEffCtrl(i);
		if (pCtrl)
		{
			const float fEffectorWeight = pCtrl->Evaluate(fWeight);
			if (fabsf(fEffectorWeight) > EFFECTOR_MIN_WEIGHT_EPSILON)
			{
#if USE_FACIAL_ANIMATION_EFFECTOR_BALANCE
				// [MichaelS] If the controller has a balance, then use it. This is a little silly,
				// but is only used in the preview window of the editor, and is necessary for that
				// functionality.

				float fSubBalance = fBalance;
				IF_UNLIKELY (pCtrl->GetConstantBalance() != 0)
					fSubBalance = pCtrl->GetConstantBalance();
#else
				const float fSubBalance = 0.f;
#endif

				ApplyEffector(faceState, pCtrl->GetCEffector(), fEffectorWeight, fSubBalance, bIsLipSync);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialAnimationContext::UpdatePlayingSequences(const QuatTS& rAnimLocationNext)
{
	CTimeValue curTime = gEnv->pTimer->GetAsyncCurTime(); // We use real-time for sound sync.

	std::vector<CFacialAnimSequence*> tostop;

	int numSequences = (int)m_sequences.size();
	for (int i = 0; i < numSequences; i++)
	{
		SPlayingSequence& seq = m_sequences[i];

		IF_UNLIKELY (Console::GetInst().ca_DebugFacial == 1)
		{
			Vec3 pos = rAnimLocationNext.t;
			float color[4] = { 1, 1, 1, 1 };
			pos.z -= i * 0.1f;
			IRenderAuxText::DrawLabelExF(pos, 1.1f, color, true, true, "%s : (time=%.2f)", seq.pSequence->GetName(), seq.playTime);
		}

		if (seq.bPaused)
			continue;

		seq.playTime = (curTime - seq.startTime).GetSeconds();
		IF_UNLIKELY (seq.playTime >= seq.timeRange.end)
		{
			if (seq.bLooping)
			{
				float length = seq.timeRange.end - seq.timeRange.start;
				seq.playTime -= length;
				seq.startTime += length;
			}
			else
			{
				seq.playTime = seq.timeRange.end;
				tostop.push_back(seq.pSequence);
			}
		}
	}

	// Stop sequences that need to stop.
	{
		int toStopSize = (int)tostop.size();
		for (int i = 0; i < toStopSize; i++)
		{
			StopSequence(tostop[i]);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialAnimationContext::AnimatePlayingSequences(const QuatTS& rAnimLocationNext)
{
	int numSequences = (int)m_sequences.size();
	for (int i = 0; i < numSequences; i++)
	{
		SPlayingSequence& seq = m_sequences[i];

		seq.pSequence->Animate(rAnimLocationNext, seq.pSequenceInstance, seq.playTime);
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialAnimationContext::PlaySequence(CFacialAnimSequence* pSequence, bool bExclusive, bool bLooping)
{
	assert(pSequence);

	if (bExclusive)
	{
		StopAllSequences();
	}
	else
	{
		int numSequences = (int)m_sequences.size();
		for (int i = 0; i < numSequences; i++)
		{
			if (m_sequences[i].pSequence == pSequence)
			{
				// Already playing this sequence.
				return;
			}
		}
	}

	SPlayingSequence seq;
	seq.pSequence = pSequence;
	seq.pSequenceInstance = new CFacialAnimSequenceInstance;
	seq.bLooping = bLooping;
	seq.bPaused = false;
	seq.timeRange = pSequence->GetTimeRange();
	seq.startTime = gEnv->pTimer->GetAsyncCurTime(); // We use real-time for sound sync.
	seq.playTime = seq.timeRange.start;
	if (seq.playTime > seq.timeRange.end)
	{
		seq.playTime = seq.timeRange.end;
		seq.bLooping = false;
	}
	seq.fCurrWeight = 1;
	seq.fWeight = 1;
	seq.pSequenceInstance->BindChannels(this, pSequence);
	m_sequences.push_back(seq);

	IF_UNLIKELY (Console::GetInst().ca_DebugFacial)
	{
		CryLogAlways("Play Facial Sequence %s", pSequence->GetName());
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialAnimationContext::SeekSequence(IFacialAnimSequence* pSequence, float fTime)
{
	int numSequences = (int)m_sequences.size();
	for (int i = 0; i < numSequences; i++)
	{
		if (m_sequences[i].pSequence == pSequence)
		{
			m_sequences[i].startTime = gEnv->pTimer->GetAsyncCurTime() - fTime;
			m_sequences[i].playTime = fTime;
			return;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialAnimationContext::StopSequence(CFacialAnimSequence* pSequence)
{
	CRY_ASSERT(pSequence);
	int numSequences = (int)m_sequences.size();
	for (int i = 0; i < numSequences; i++)
	{
		if (m_sequences[i].pSequence == pSequence)
		{
			if (m_sequences[i].pSequenceInstance)
			{
				m_sequences[i].pSequenceInstance->UnbindChannels();
			}
			m_sequences.erase(m_sequences.begin() + i);
			break;
		}
	}

	if (Console::GetInst().ca_DebugFacial)
	{
		const char* szSequenceName = pSequence ? pSequence->GetName() : "NULL";
		CryLogAlways("Stop Facial Sequence %s", szSequenceName);
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialAnimationContext::PauseSequence(CFacialAnimSequence* pSequence, bool bPaused)
{
	int numSequences = (int)m_sequences.size();
	for (int i = 0; i < numSequences; i++)
	{
		if (m_sequences[i].pSequence == pSequence)
		{
			m_sequences[i].bPaused = bPaused;
			return;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CFacialAnimationContext::IsPlayingSequence(CFacialAnimSequence* pSequence)
{
	for (unsigned i = 0, end = m_sequences.size(); i < end; i++)
	{
		if (m_sequences[i].pSequence == pSequence)
		{
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CFacialAnimationContext::StopAllSequences()
{
	int numSequences = (int)m_sequences.size();
	for (int i = 0; i < numSequences; i++)
	{
		if (m_sequences[i].pSequenceInstance)
			m_sequences[i].pSequenceInstance->UnbindChannels();
	}
	m_sequences.clear();

	IF_UNLIKELY (Console::GetInst().ca_DebugFacial)
	{
		CryLogAlways("Stop All Facial Sequences");
	}
}

//////////////////////////////////////////////////////////////////////////
CFacialAnimation::CFacialAnimation()
{
	m_pPhonemeLibrary = new CPhonemesLibrary;
}

//////////////////////////////////////////////////////////////////////////
CFacialAnimation::~CFacialAnimation()
{
	delete m_pPhonemeLibrary;
}

//////////////////////////////////////////////////////////////////////////
IPhonemeLibrary* CFacialAnimation::GetPhonemeLibrary()
{
	return m_pPhonemeLibrary;
}

//////////////////////////////////////////////////////////////////////////
void CFacialAnimation::ClearAllCaches()
{
	m_loadedSequences.clear();
	m_effectorLibs.clear();
}

//////////////////////////////////////////////////////////////////////////
IFacialEffectorsLibrary* CFacialAnimation::CreateEffectorsLibrary()
{
	IFacialEffectorsLibrary* pLibrary = new CFacialEffectorsLibrary(this);
	return pLibrary;
}

//////////////////////////////////////////////////////////////////////////
void CFacialAnimation::ClearEffectorsLibraryFromCache(const char* filename)
{
	stack_string file(filename);
	PathUtil::ToUnixPath(file);

	int libIndex = -1;
	for (int i = 0, end = (int)m_effectorLibs.size(); i < end; i++)
	{
		if (stricmp(m_effectorLibs[i]->GetName(), file.c_str()) == 0)
			libIndex = i;
	}

	if (libIndex >= 0)
		m_effectorLibs.erase(m_effectorLibs.begin() + libIndex);
}

//////////////////////////////////////////////////////////////////////////
IFacialEffectorsLibrary* CFacialAnimation::LoadEffectorsLibrary(const char* filename)
{
	stack_string file(filename);
	PathUtil::ToUnixPath(file);

	for (int i = 0, end = (int)m_effectorLibs.size(); i < end; i++)
	{
		if (stricmp(m_effectorLibs[i]->GetName(), file) == 0)
			return m_effectorLibs[i];
	}

	XmlNodeRef root = g_pISystem->LoadXmlFromFile(file);
	if (!root)
		return 0;

	IFacialEffectorsLibrary* pLibrary = new CFacialEffectorsLibrary(this);
	pLibrary->Serialize(root, true);
	pLibrary->SetName(file);

	return pLibrary;
}

//////////////////////////////////////////////////////////////////////////
void CFacialAnimation::RegisterLib(CFacialEffectorsLibrary* pLib)
{
	stl::push_back_unique(m_effectorLibs, pLib);
}

//////////////////////////////////////////////////////////////////////////
void CFacialAnimation::UnregisterLib(CFacialEffectorsLibrary* pLib)
{
	stl::find_and_erase(m_effectorLibs, pLib);
}

//////////////////////////////////////////////////////////////////////////
IFacialAnimSequence* CFacialAnimation::CreateSequence()
{
	CFacialAnimSequence* pAnimSeq = new CFacialAnimSequence(this);
	pAnimSeq->SetInMemory(true);
	return pAnimSeq;
}

//////////////////////////////////////////////////////////////////////////
void CFacialAnimation::RenameAnimSequence(CFacialAnimSequence* pSeq, const char* sNewName, bool addToCache)
{
	if (!pSeq->m_name.empty())
		m_loadedSequences.erase(pSeq->m_name);
	pSeq->m_name = PathUtil::ToUnixPath(sNewName);
	if (addToCache)
		m_loadedSequences[pSeq->m_name] = pSeq;
}

//////////////////////////////////////////////////////////////////////////
void CFacialAnimation::OnSequenceStreamed(IFacialAnimSequence* pSequence, bool wasSuccess)
{
	if (wasSuccess)
	{
		pSequence->SetInMemory(true);
	}
	else
	{
		// if there are still other references to the sequence, they should be informed
		// that streaming has been completed through SetInMemory(true)
		// then the sequence is removed from the array, and an empty pointer is stored at its
		// place that is wont be loaded again.
		pSequence->SetInMemory(true);

		const char* sName = pSequence->GetName();
		SeqMap::iterator it = m_loadedSequences.find(CONST_TEMP_STRING(sName));

		if (it != m_loadedSequences.end())
			it->second = NULL;
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialAnimation::ClearSequenceFromCache(const char* filename)
{
	m_loadedSequences.erase(filename);
}

//////////////////////////////////////////////////////////////////////////
IFacialAnimSequence* CFacialAnimation::LoadSequence(const char* filename, bool bNoWarnings, bool addToCache)
{
	const string file = PathUtil::ReplaceExtension(PathUtil::ToUnixPath(filename), FACIAL_SEQUENCE_EXT);

	SeqMap::iterator it = m_loadedSequences.find(file);

	if (it != m_loadedSequences.end())
	{
		return it->second;
	}

	if (addToCache)
	{
		m_loadedSequences[file] = NULL;
	}

	XmlNodeRef root = g_pISystem->LoadXmlFromFile(file);

	if (!root)
	{
		if (!bNoWarnings)
		{
			gEnv->pLog->LogError(file, "Failed to load facial sequence");
		}
		return NULL;
	}

	IFacialAnimSequence* pSeq = new CFacialAnimSequence(this);
	if (pSeq)
	{
		RenameAnimSequence((CFacialAnimSequence*)pSeq, file, addToCache);
		unsigned flags = (gEnv->IsEditor() ? IFacialAnimSequence::SFLAG_ALL : IFacialAnimSequence::SFLAG_ALL & ~IFacialAnimSequence::SFLAG_CAMERA_PATH);
		pSeq->Serialize(root, true, IFacialAnimSequence::ESerializationFlags(flags));
		pSeq->SetInMemory(true);

		return pSeq;
	}
	else
	{
		gEnv->pLog->LogError(file, "Failed to allocate memory for sequence");
		return NULL;
	}
}

IFacialAnimSequence* CFacialAnimation::StartStreamingSequence(const char* filename)
{
	const string file = PathUtil::ReplaceExtension(PathUtil::ToUnixPath(filename), FACIAL_SEQUENCE_EXT);

	SeqMap::iterator it = m_loadedSequences.find(file);

	if (it != m_loadedSequences.end())
	{
		return it->second;
	}

	m_loadedSequences[file] = NULL;

	IFacialAnimSequence* pSeq = new CFacialAnimSequence(this);
	if (pSeq)
	{
		// start streaming
		pSeq->StartStreaming(file.c_str());

		RenameAnimSequence((CFacialAnimSequence*)pSeq, file, true);
		return pSeq;
	}
	else
	{
		return NULL;
	}
}

//////////////////////////////////////////////////////////////////////////
IFacialAnimSequence* CFacialAnimation::FindLoadedSequence(const char* filename) const
{
	const string file = PathUtil::ReplaceExtension(PathUtil::ToUnixPath(filename), FACIAL_SEQUENCE_EXT);

	SeqMap::const_iterator it = m_loadedSequences.find(file);

	if (it != m_loadedSequences.end())
	{
		return it->second;
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
IJoystickContext* CFacialAnimation::GetJoystickContext()
{
	return this;
}

//////////////////////////////////////////////////////////////////////////
IJoystick* CFacialAnimation::CreateJoystick(uint64 id)
{
	return new CFacialJoystick(id);
}

//////////////////////////////////////////////////////////////////////////
IJoystickSet* CFacialAnimation::CreateJoystickSet()
{
	return new CFacialJoystickSet();
}

//////////////////////////////////////////////////////////////////////////
IJoystickSet* CFacialAnimation::LoadJoystickSet(const char* filename, bool bNoWarnings)
{
	string file = PathUtil::ReplaceExtension(filename, FACIAL_JOYSTICK_EXT);

	XmlNodeRef root = g_pISystem->LoadXmlFromFile(file);
	if (!root)
	{
		if (!bNoWarnings)
			gEnv->pLog->LogError(file, "Failed to load facial joysticks %s", file.c_str());
		return 0;
	}

	CFacialJoystickSet* pJoystickSet = new CFacialJoystickSet();
	pJoystickSet->Serialize(root, true);

	return pJoystickSet;
}

//////////////////////////////////////////////////////////////////////////
IJoystickChannel* CFacialAnimation::CreateJoystickChannel(IFacialAnimChannel* pChannel)
{
	return new CFacialJoystickChannel(pChannel);
}

//////////////////////////////////////////////////////////////////////////
void CFacialAnimation::BindJoystickSetToSequence(IJoystickSet* pJoystickSet, IFacialAnimSequence* pSequence)
{
#ifdef FACE_STORE_ASSET_VALUES
	CFacialJoystickSet* pFaceJoystickSet = static_cast<CFacialJoystickSet*>(pJoystickSet);
	CFacialAnimSequence* pFacialAnimation = static_cast<CFacialAnimSequence*>(pSequence);
	if (pFaceJoystickSet && pFacialAnimation)
	{
		typedef VectorMap<string, IFacialAnimChannel*> ChannelMap;

		class JoystickSerializeContext : public IFacialJoystickSerializeContext
		{
		public:
			JoystickSerializeContext(ChannelMap& channelMap) : m_channelMap(channelMap) {}
			virtual IFacialAnimChannel* FindChannel(const char* szName)
			{
				ChannelMap::iterator itChannel = m_channelMap.find(szName);
				return (itChannel != m_channelMap.end() ? (*itChannel).second : 0);
			}

		private:
			ChannelMap& m_channelMap;
		};

		ChannelMap channelMap;
		{
			ChannelMap::container_type channelMapEntries;
			channelMapEntries.reserve(pSequence->GetChannelCount() * 2);
			for (int i = 0; i < pSequence->GetChannelCount(); ++i)
			{
				IFacialAnimChannel* pChannel = pSequence->GetChannel(i);
				string fullName;
				IFacialAnimChannel* pAncestor = pChannel;
				while (pAncestor)
				{
					const char* szChannelName = pAncestor ? pAncestor->GetName() : 0;
					szChannelName = szChannelName ? szChannelName : "";
					fullName = string(szChannelName) + "::" + fullName;
					pAncestor = pAncestor->GetParent();
				}
				channelMapEntries.push_back(std::make_pair(fullName, pChannel));

				const char* szChannelName = pChannel ? pChannel->GetName() : 0;
				szChannelName = szChannelName ? szChannelName : "";
				channelMapEntries.push_back(std::make_pair(szChannelName, pChannel));
			}

			channelMap.SwapElementsWithVector(channelMapEntries);
		}

		JoystickSerializeContext context(channelMap);

		pFaceJoystickSet->Bind(&context);
	}
#else
	CryFatalError("Not Supporeted on this platform!");
#endif
}

void CFacialAnimation::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(this, sizeof(*this));
	pSizer->AddObject(m_loadedSequences);
	pSizer->AddObject(m_effectorLibs);
	pSizer->AddObject(m_pPhonemeLibrary);
}

CFaceIdentifierHandle CFacialAnimation::CreateIdentifierHandle(const char* identifierString)
{
	return CFaceIdentifierHandle(
	  identifierString,
	  CCrc32::ComputeLowercase(identifierString)
	  );
}

const float SMOOTH_TIME_DURATION = 0.3f;
//////////////////////////////////////////////////////////////////////////
void CFacialAnimationContext::TemporarilyEnableAdditionalBoneRotationSmoothing()
{
	m_fBoneRotationSmoothingRemainingTime = SMOOTH_TIME_DURATION;
}

//////////////////////////////////////////////////////////////////////////
float CFacialAnimationContext::GetBoneRotationSmoothRatio()
{
	return m_fBoneRotationSmoothingRemainingTime / SMOOTH_TIME_DURATION;
}

void CFacialAnimationContext::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(this, sizeof(*this));
	pSizer->AddObject(m_time);
	pSizer->AddObject(m_channels);
	pSizer->AddObject(m_nTotalAppliedEffectors);
	pSizer->AddObject(m_effectorsPerIndex);
	pSizer->AddObject(m_lipsyncStrength);
	pSizer->AddObject(m_nLastChannelId);
	pSizer->AddObject(m_sequences);
	pSizer->AddObject(m_debugText);
	pSizer->AddObject(m_bForceLastUpdate);
	pSizer->AddObject(m_fBoneRotationSmoothingRemainingTime);
}
