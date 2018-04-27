// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "FaceAnimSequence.h"

#include "FaceAnimation.h"
#include "FacialInstance.h"
#include "../CharacterInstance.h"
#include <CryCore/Containers/VectorMap.h>
#include "FaceJoystick.h"
#include "FaceChannelKeyCleanup.h"
#include "FaceChannelSmoothing.h"
#include "CharacterManager.h"

#define INVALID_CHANNEL_ID (~0)
#define MIN_CHANNEL_WEIGHT (0.01f)
#define PHONEME_FADE_TIME  (0.1f)

//////////////////////////////////////////////////////////////////////////
void CFacialAnimChannelInterpolator::CleanupKeys(float errorMax)
{
	FaceChannel::CleanupKeys(this, errorMax);
}

//////////////////////////////////////////////////////////////////////////
void CFacialAnimChannelInterpolator::SmoothKeys(float sigma)
{
	FaceChannel::GaussianBlurKeys(this, sigma);
}

//////////////////////////////////////////////////////////////////////////
void CFacialAnimChannelInterpolator::RemoveNoise(float sigma, float threshold)
{
	FaceChannel::RemoveNoise(this, sigma, threshold);
}

//////////////////////////////////////////////////////////////////////////
CFacialAnimChannel::CFacialAnimChannel(int index)
{
	m_nFlags = 0;
	m_nInstanceChannelId = index;
}

//////////////////////////////////////////////////////////////////////////
CFacialAnimChannel::~CFacialAnimChannel()
{
	for (size_t i = 0, count = m_splines.size(); i < count; ++i)
		delete m_splines[i];
}

//////////////////////////////////////////////////////////////////////////
void CFacialAnimChannel::SetIdentifier(CFaceIdentifierHandle ident)
{
	m_name = ident;
	if (!m_effectorName.GetCRC32())
		m_effectorName = ident;
}

//////////////////////////////////////////////////////////////////////////
const CFaceIdentifierHandle CFacialAnimChannel::GetIdentifier()
{
	if (!m_name.GetCRC32())
	{
		if (m_pEffector)
			m_name = m_pEffector->GetIdentifier();
	}

	return m_name.GetHandle();
}

//////////////////////////////////////////////////////////////////////////
void CFacialAnimChannel::SetEffector(IFacialEffector* pEffector)
{
	m_pEffector = (CFacialEffector*)pEffector;
	if (m_pEffector)
	{
		m_name = m_pEffector->GetIdentifier();
		m_effectorName = m_name.GetHandle();
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialAnimChannel::CreateInterpolator()
{
	m_splines.clear();
	m_splines.push_back(new CFacialAnimChannelInterpolator);
	m_splines.back()->InsertKeyFloat(FacialEditorSnapTimeToFrame(0), 0);
	m_splines.back()->InsertKeyFloat(FacialEditorSnapTimeToFrame(0.5f), 0);
	m_splines.back()->InsertKeyFloat(FacialEditorSnapTimeToFrame(1), 0);
}

//////////////////////////////////////////////////////////////////////////
void CFacialAnimChannel::AddInterpolator()
{
	m_splines.push_back(new CFacialAnimChannelInterpolator);
}

//////////////////////////////////////////////////////////////////////////
void CFacialAnimChannel::DeleteInterpolator(int i)
{
	if (i >= 0 && i < int(m_splines.size()))
		m_splines.erase(m_splines.begin() + i);
}

//////////////////////////////////////////////////////////////////////////
void CFacialAnimChannel::CleanupKeys(float fErrorMax)
{
	if (!m_splines.empty())
		m_splines.back()->CleanupKeys(fErrorMax);
}

//////////////////////////////////////////////////////////////////////////
void CFacialAnimChannel::SmoothKeys(float sigma)
{
	if (!m_splines.empty())
		m_splines.back()->SmoothKeys(sigma);
}

//////////////////////////////////////////////////////////////////////////
void CFacialAnimChannel::RemoveNoise(float sigma, float threshold)
{
	if (!m_splines.empty())
		m_splines.back()->RemoveNoise(sigma, threshold);
}

//////////////////////////////////////////////////////////////////////////
float CFacialAnimChannel::Evaluate(float t)
{
	float total = 0;
	for (size_t i = 0, count = m_splines.size(); i < count; ++i)
	{
		float v = 0;
		m_splines[i]->interpolate(t, v);
		total += v;
	}
	return total;
}

#ifdef FACE_STORE_ASSET_VALUES
void CFacialAnimChannel::SetName(const char* name)
{
	m_name = g_pCharacterManager->GetFacialAnimation()->CreateIdentifierHandle(name);
}
#endif

//////////////////////////////////////////////////////////////////////////
// CFacialAnimSequence
//////////////////////////////////////////////////////////////////////////
CFacialAnimSequence::CFacialAnimSequence(CFacialAnimation* pFaceAnim)
{
	m_nRefCount = 0;
	m_name = "Default Sequence";
	m_pFaceAnim = pFaceAnim;
	m_bInMemory = false;
	m_pStreamingData = NULL;
}

//////////////////////////////////////////////////////////////////////////
CFacialAnimSequence::~CFacialAnimSequence()
{
	if (m_pStream)
		m_pStream->Abort();

	assert(m_pStreamingData == NULL);
}

//////////////////////////////////////////////////////////////////////////
void CFacialAnimSequence::Release()
{
	if (--m_nRefCount <= 0)
		delete this;
}

//////////////////////////////////////////////////////////////////////////
bool CFacialAnimSequence::StartStreaming(const char* sFilename)
{
	if (m_pStream)
	{
		m_pStream->Abort();
		m_pStream = NULL;
	}

	assert(m_pStreamingData == NULL);

	m_pStreamingData = new Data;
	m_pStreamingData->m_nValidateID = m_data.m_nValidateID;
	m_pStreamingData->m_nProceduralChannelsValidateID = m_data.m_nProceduralChannelsValidateID;
	m_pStreamingData->m_nSoundEntriesValidateID = m_data.m_nSoundEntriesValidateID;

	StreamReadParams params;
	params.dwUserData = 0;
	params.nSize = 0;
	params.pBuffer = NULL;
	params.nLoadTime = 10000;
	params.nMaxLoadTime = 1000;
	m_pStream = g_pISystem->GetStreamEngine()->StartRead(eStreamTaskTypeAnimation, sFilename, this, &params);

	return m_pStream != NULL;
}

//////////////////////////////////////////////////////////////////////////
void CFacialAnimSequence::SetName(const char* sNewName)
{
	m_pFaceAnim->RenameAnimSequence(this, sNewName);
};

//////////////////////////////////////////////////////////////////////////
IFacialAnimChannel* CFacialAnimSequence::GetChannel(int nIndex)
{
	assert(nIndex >= 0 && nIndex < (int)m_data.m_channels.size());
	return m_data.m_channels[nIndex];
}

//////////////////////////////////////////////////////////////////////////
IFacialAnimChannel* CFacialAnimSequence::CreateChannel()
{
	return m_data.CreateChannel();
}

//////////////////////////////////////////////////////////////////////////
IFacialAnimChannel* CFacialAnimSequence::CreateChannelGroup()
{
	return m_data.CreateChannelGroup();
}

//////////////////////////////////////////////////////////////////////////
void CFacialAnimSequence::RemoveChannel(IFacialAnimChannel* pChannelToRemove)
{
	if (pChannelToRemove)
	{
		m_data.m_nValidateID++;

		std::vector<bool> indicesToDelete;
		indicesToDelete.resize(m_data.m_channels.size());
		std::fill(indicesToDelete.begin(), indicesToDelete.end(), false);
		for (int i = 0, end = (int)m_data.m_channels.size(); i < end; ++i)
		{
			for (IFacialAnimChannel* pChannel = m_data.m_channels[i]; pChannel; pChannel = pChannel->GetParent())
			{
				if (pChannel == pChannelToRemove)
					indicesToDelete[i] = true;
			}
		}

		std::vector<_smart_ptr<CFacialAnimChannel>>::iterator it = m_data.m_channels.begin();
		for (std::vector<bool>::iterator shouldDel = indicesToDelete.begin(), end = indicesToDelete.end(); shouldDel != end; ++shouldDel)
		{
			if (*shouldDel)
				it = m_data.m_channels.erase(it);
			else
				++it;
		}

		for (int i = 0, end = (int)m_data.m_channels.size(); i < end; i++)
			m_data.m_channels[i]->SetInstanceChannelId(i);
	}
}

//////////////////////////////////////////////////////////////////////////
int CFacialAnimSequence::GetSoundEntryCount()
{
	return m_data.m_soundEntries.size();
}

//////////////////////////////////////////////////////////////////////////
void CFacialAnimSequence::InsertSoundEntry(int index)
{
	if (index < 0 || index > int(m_data.m_soundEntries.size()))
		g_pILog->LogError("CFacialAnimSequence: Inserting sound entry at invalid location (Code bug).");
	else
		m_data.m_soundEntries.insert(m_data.m_soundEntries.begin() + index, CFacialAnimSoundEntry());
}

//////////////////////////////////////////////////////////////////////////
void CFacialAnimSequence::DeleteSoundEntry(int index)
{
	if (index < 0 || index > int(m_data.m_soundEntries.size()))
		g_pILog->LogError("CFacialAnimSequence: Deleting non-existent sound entry (Code bug).");
	else
		m_data.m_soundEntries.erase(m_data.m_soundEntries.begin() + index);
}

//////////////////////////////////////////////////////////////////////////
IFacialAnimSoundEntry* CFacialAnimSequence::GetSoundEntry(int index)
{
	return ((index >= 0 && index < int(m_data.m_soundEntries.size())) ? &m_data.m_soundEntries[index] : 0);
}

//////////////////////////////////////////////////////////////////////////
void CFacialAnimSequence::Animate(const QuatTS& rAnimLocationNext, CFacialAnimSequenceInstance* pInstance, float fTime)
{
	if (IsInMemory())
	{
		assert(pInstance);

		UpdateProceduralChannels();

		if (pInstance->m_channels.size() != m_data.m_channels.size() || pInstance->m_nValidateID != m_data.m_nValidateID)
		{
			// Sequence was changed, must rebind all channels.
			pInstance->BindChannels(pInstance->m_pAnimContext, this);
		}

		// MichaelS - Loop through balance channels and evaluate them first.
		{

			float* balances = &pInstance->m_pAnimContext->GetInstance()->GetState()->m_balance[0];

			int* stateIndices = 0;
			uint32 num = pInstance->m_balanceChannelStateIndices.size();
			if (num)
				stateIndices = &pInstance->m_balanceChannelStateIndices[0];

			uint32 end = pInstance->m_balanceChannelEntries.size();
			for (uint32 i = 0; i < end; ++i)
			{
				CFacialAnimSequenceInstance::BalanceChannelEntry& entry = pInstance->m_balanceChannelEntries[i];
				assert(entry.nChannelIndex >= 0);
				CFacialAnimChannel* pChannel = m_data.m_channels[entry.nChannelIndex];
				entry.fEvaluatedBalance = pChannel->Evaluate(fTime);

				// Loop through all the morphs in the state array that refer to this balance.
				for (int stateIndexIndex = 0; stateIndexIndex < entry.nMorphIndexCount; ++stateIndexIndex)
				{
					assert(stateIndices != 0 && !"Zero Pointer Crash");
					int stateIndex = stateIndices[entry.nMorphIndexStartIndex + stateIndexIndex];
					balances[stateIndex] = entry.fEvaluatedBalance * entry.fEvaluatedBalance * entry.fEvaluatedBalance;
				}
			}
		}

		float fPhonemeStrength = 1.0f;
		float fMorphTargetVertexDrag = 1.0f;
		float fProceduralStrength = 0.0f;
		bool bHasBakedLipsynch = false;

		CCharInstance* pCharacter = pInstance->m_pAnimContext->GetInstance()->GetMasterCharacter();
		//pCharacter->m_Morphing.m_fMorphVertexDrag = 1.0f;

		int numChannels = (int)m_data.m_channels.size();
		for (int i = 0; i < numChannels; i++)
		{
			CFacialAnimSequenceInstance::ChannelInfo& chinfo = pInstance->m_channels[i];
			CFacialAnimChannel* pChannel = m_data.m_channels[i];

			int flags = pChannel->GetFlags();

			if (flags & IFacialAnimChannel::FLAG_BAKED_LIPSYNC_GROUP)
			{
				bHasBakedLipsynch = true;
				continue;
			}
			if (flags & IFacialAnimChannel::FLAG_PHONEME_STRENGTH)
			{
				fPhonemeStrength = pChannel->Evaluate(fTime);
				continue;
			}
			if (flags & IFacialAnimChannel::FLAG_PROCEDURAL_STRENGTH)
			{
				fProceduralStrength = pChannel->Evaluate(fTime);
				continue;
			}
			if (flags & IFacialAnimChannel::FLAG_VERTEX_DRAG)
			{
				fMorphTargetVertexDrag = 0.1f + pChannel->Evaluate(fTime) * 2.0f;
				//pCharacter->m_Morphing.m_fMorphVertexDrag = fMorphTargetVertexDrag;
				continue;
			}
			if (flags & IFacialAnimChannel::FLAG_LIPSYNC_CATEGORY_STRENGTH)
			{
				IFacialEffector* pEffector = pChannel->GetEffector();
				float fStrength = pChannel->Evaluate(fTime);
				if (pEffector)
				{
					for (size_t subEffectorIndex = 0, subEffectorCount = pEffector->GetSubEffectorCount(); subEffectorIndex < subEffectorCount; ++subEffectorIndex)
					{
						IFacialEffector* pSubEffector = pEffector->GetSubEffector(subEffectorIndex);
						int index = pSubEffector->GetIndexInState();
						if (index >= 0)
							pInstance->m_pAnimContext->SetLipsyncStrength(index, fStrength);
					}
				}
				continue;
			}

			if (!chinfo.bUse || chinfo.pEffector == NULL)
				continue;

			float w = pChannel->Evaluate(fTime);
			//if (fabs(w) > MIN_CHANNEL_WEIGHT)
			{
				SFacialEffectorChannel* pEffectorAnimChannel = pInstance->m_pAnimContext->GetChannel(chinfo.nChannelId);
				assert(pEffectorAnimChannel);   // Channel must exist.
				if (pEffectorAnimChannel)
				{
					pEffectorAnimChannel->fCurrWeight = w;
					pEffectorAnimChannel->fWeight = w;
					if (chinfo.nBalanceChannelListIndex >= 0)
						pEffectorAnimChannel->fBalance = pInstance->m_balanceChannelEntries[chinfo.nBalanceChannelListIndex].fEvaluatedBalance;
				}
			}
		}

		for (int i = 0; i < CProceduralChannelSet::ChannelType_count; i++)
		{
			CFacialAnimSequenceInstance::ChannelInfo& chinfo = pInstance->m_proceduralChannels[i];
			CProceduralChannel* pChannel = m_data.m_proceduralChannels.GetChannel(static_cast<CProceduralChannelSet::ChannelType>(i));

			if (!chinfo.bUse || chinfo.pEffector == NULL)
				continue;

			float fWeight;
			pChannel->GetInterpolator()->interpolate(fTime, fWeight);
			fWeight *= fProceduralStrength;
			if (fabs(fWeight) > MIN_CHANNEL_WEIGHT)
			{
				SFacialEffectorChannel* pEffectorAnimChannel = pInstance->m_pAnimContext->GetChannel(chinfo.nChannelId);
				assert(pEffectorAnimChannel);   // Channel must exist.
				if (pEffectorAnimChannel)
				{
					pEffectorAnimChannel->fCurrWeight = fWeight;
					pEffectorAnimChannel->fWeight = fWeight;
					if (chinfo.nBalanceChannelListIndex >= 0)
						pEffectorAnimChannel->fBalance = pInstance->m_balanceChannelEntries[chinfo.nBalanceChannelListIndex].fEvaluatedBalance;
				}
			}
		}

		// It is possible for the animator to override the lipsync with a manual channel. These channels are stored in the
		// 'BakedLipSync' folder. We need to find all such channels and pass it to the lip synching code.
		CFacialSentence::OverridesMap overriddenPhonemes;
		if (bHasBakedLipsynch)
		{
			std::vector<CFaceIdentifierHandle> foundChannels;
			foundChannels.reserve(200);

			CFaceIdentifierHandle bakedLipSyncHandle = m_pFaceAnim->CreateIdentifierHandle("BakedLipSync");

			for (int channelIndex = 0, channelCount = (int)m_data.m_channels.size(); channelIndex < channelCount; ++channelIndex)
			{
				CFacialAnimChannel* pChannel = m_data.m_channels[channelIndex];
				IFacialAnimChannel* pChannelParent = (pChannel ? pChannel->GetParent() : 0);
				CFaceIdentifierHandle parentName = (pChannelParent ? pChannelParent->GetIdentifier() : CFaceIdentifierHandle());
				if (m_pFaceAnim)
				{
					bool inFolder = bakedLipSyncHandle.GetCRC32() == parentName.GetCRC32();
					if (inFolder)
					{
						CFaceIdentifierHandle name = (pChannel ? pChannel->GetIdentifier() : CFaceIdentifierHandle());
						foundChannels.push_back(name);
					}
				}
			}

			overriddenPhonemes.SwapElementsWithVector(foundChannels);
		}

		for (int i = 0, count = m_data.m_soundEntries.size(); i < count; ++i)
		{
			if (m_data.m_soundEntries[i].m_pSentence)
				m_data.m_soundEntries[i].m_pSentence->Animate(rAnimLocationNext, pInstance->m_pAnimContext, fTime - m_data.m_soundEntries[i].m_startTime, fPhonemeStrength * Console::GetInst().ca_lipsync_phoneme_strength, overriddenPhonemes);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialAnimSequence::SerializeLoad(Data& data, XmlNodeRef& xmlNode, ESerializationFlags flags)
{
	++data.m_nValidateID;
	if (flags & SFLAG_ANIMATION)
		data.m_channels.clear();
	if (flags & SFLAG_SOUND_ENTRIES)
		data.m_soundEntries.clear();

	if (flags & SFLAG_CAMERA_PATH)
	{
		data.m_cameraPathPosition.clear();
		data.m_cameraPathOrientation.clear();
		data.m_cameraPathFOV.clear();
	}

	if (flags & SFLAG_ANIMATION)
	{
		xmlNode->getAttr("Flags", data.m_nFlags);
		xmlNode->getAttr("StartTime", data.m_timeRange.start);
		xmlNode->getAttr("EndTime", data.m_timeRange.end);

		data.m_joystickFile = xmlNode->getAttr("Joysticks");
	}

	// Load all sub channels.
	XmlNodeRef rootSentenceNode = 0;
	for (int i = 0; i < xmlNode->getChildCount(); i++)
	{
		XmlNodeRef childNode = xmlNode->getChild(i);
		if (childNode->isTag("Channel"))
		{
			if (flags & SFLAG_ANIMATION)
				SerializeChannelLoad(data, NULL, childNode);
		}
		else if (childNode->isTag("Sentence"))
		{
			rootSentenceNode = childNode;
		}
		else if (childNode->isTag("SoundEntry"))
		{
			if (flags & SFLAG_SOUND_ENTRIES)
			{
				int soundEntry = int(data.m_soundEntries.size());
				data.m_soundEntries.resize(data.m_soundEntries.size() + 1);
				data.m_soundEntries[soundEntry].m_sound = childNode->getAttr("Sound");
				childNode->getAttr("StartTime", data.m_soundEntries[soundEntry].m_startTime);
				XmlNodeRef nodeSentence = childNode->findChild("Sentence");
				if (nodeSentence != 0 && data.m_soundEntries[soundEntry].m_pSentence != NULL)
					data.m_soundEntries[soundEntry].m_pSentence->Serialize(nodeSentence, true);
			}
		}
		else if (childNode->isTag("SkeletonAnimationEntry"))
		{
			if (flags & SFLAG_ANIMATION)
			{
				int skeletonAnimationEntry = int(data.m_skeletonAnimationEntries.size());
				data.m_skeletonAnimationEntries.resize(data.m_skeletonAnimationEntries.size() + 1);
				const char* szAnimationName = childNode->getAttr("AnimationName");
				data.m_skeletonAnimationEntries[skeletonAnimationEntry].m_animationName = (szAnimationName ? szAnimationName : "");
				childNode->getAttr("StartTime", data.m_skeletonAnimationEntries[skeletonAnimationEntry].m_startTime);
			}
		}
		else if (childNode->isTag("CameraPathPositions"))
		{
			if (flags & SFLAG_CAMERA_PATH)
				data.m_cameraPathPosition.SerializeSpline(childNode, true);
		}
		else if (childNode->isTag("CameraPathOrientations"))
		{
			if (flags & SFLAG_CAMERA_PATH)
				data.m_cameraPathOrientation.SerializeSpline(childNode, true);
		}
		else if (childNode->isTag("CameraPathFOV"))
		{
			if (flags & SFLAG_CAMERA_PATH)
				data.m_cameraPathFOV.SerializeSpline(childNode, true);
		}
	}

	// Old sequences have a single sound entry that is serialized in the root node.
	if (flags & SFLAG_SOUND_ENTRIES)
	{
		if (xmlNode->haveAttr("Sound") || rootSentenceNode)
		{
			int soundEntry = int(data.m_soundEntries.size());
			data.m_soundEntries.resize(data.m_soundEntries.size() + 1);
			data.m_soundEntries[soundEntry].m_sound = xmlNode->getAttr("Sound");
			data.m_soundEntries[soundEntry].m_startTime = 0.0f;
			if (rootSentenceNode != 0 && data.m_soundEntries[soundEntry].m_pSentence != NULL)
				data.m_soundEntries[soundEntry].m_pSentence->Serialize(rootSentenceNode, true);
		}
	}

	// Old sequences have a single skeleton animation entry that is serialized in the root node.
	if (flags & SFLAG_ANIMATION)
	{
		if (xmlNode->haveAttr("SkeletonAnimation"))
		{
			int skeletonAnimationEntry = int(data.m_skeletonAnimationEntries.size());
			data.m_skeletonAnimationEntries.resize(data.m_skeletonAnimationEntries.size() + 1);
			data.m_skeletonAnimationEntries[skeletonAnimationEntry].m_animationName = xmlNode->getAttr("SkeletonAnimation");
			xmlNode->getAttr("SkeletonAnimationStart", data.m_skeletonAnimationEntries[skeletonAnimationEntry].m_startTime);
		}
	}

	GenerateProceduralChannels(data);
}

void CFacialAnimSequence::SerializeChannelLoad(Data& data, IFacialAnimChannel* pChannel, XmlNodeRef& node)
{
	data.m_nValidateID++;
	IFacialAnimChannel* pParentChannel = pChannel;

	CFacialAnimation* pFaceAnim = g_pCharacterManager->GetFacialAnimation();
	if (!pFaceAnim)
		return;

	int flags = 0;
	node->getAttr("Flags", flags);
	const char* sName = node->getAttr("Name");
	if (flags & IFacialAnimChannel::FLAG_GROUP)
	{
		// This is group.
		pChannel = data.CreateChannelGroup();
		pChannel->SetFlags(flags);
		pChannel->SetIdentifier(pFaceAnim->CreateIdentifierHandle(sName));
		pChannel->SetParent(pParentChannel);

		// Load all sub channels.
		for (int i = 0; i < node->getChildCount(); i++)
		{
			XmlNodeRef childNode = node->getChild(i);
			if (childNode->isTag("Channel"))
			{
				SerializeChannelLoad(data, pChannel, childNode);
			}
		}
	}
	else
	{
		// This is a normal effector.
		pChannel = data.CreateChannel();
		pChannel->SetFlags(flags);
		pChannel->SetIdentifier(pFaceAnim->CreateIdentifierHandle(sName));
		pChannel->SetParent(pParentChannel);
		// This is an effector.
		int splineCount = 0;
		for (int childIndex = 0, childCount = node->getChildCount(); childIndex < childCount; ++childIndex)
		{
			XmlNodeRef splineNode = node->getChild(childIndex);
			if (splineNode && stricmp("Spline", splineNode->getTag()) == 0)
			{
				if (splineCount >= pChannel->GetInterpolatorCount())
					pChannel->AddInterpolator();
				ISplineInterpolator* pInterpolator = pChannel->GetInterpolator(splineCount);
				++splineCount;
				pInterpolator->SerializeSpline(splineNode, true);
			}
		}
	}
}

void CFacialAnimSequence::SerializeChannelSave(IFacialAnimChannel* pChannel, XmlNodeRef& node)
{
#ifdef FACE_STORE_ASSET_VALUES
	node->setAttr("Flags", pChannel->GetFlags());
	node->setAttr("Name", pChannel->GetName());
	if (pChannel->GetFlags() & IFacialAnimChannel::FLAG_GROUP)
	{
		for (int j = 0; j < (int)m_data.m_channels.size(); j++)
		{
			IFacialAnimChannel* pSubChannel = m_data.m_channels[j];
			if (pSubChannel->GetParent() == pChannel)
			{
				XmlNodeRef subChannelNode = node->newChild("Channel");
				SerializeChannelSave(pSubChannel, subChannelNode);
			}
		}
	}
	else
	{
		for (int i = 0, count = pChannel->GetInterpolatorCount(); i < count; ++i)
		{
			XmlNodeRef splineNode = node->newChild("Spline");
			pChannel->GetInterpolator(i)->SerializeSpline(splineNode, false);
		}
	}
#endif
}

//////////////////////////////////////////////////////////////////////////
int CFacialAnimSequence::GetSkeletonAnimationEntryCount()
{
	return int(m_data.m_skeletonAnimationEntries.size());
}

//////////////////////////////////////////////////////////////////////////
void CFacialAnimSequence::InsertSkeletonAnimationEntry(int index)
{
	if (index < 0 || index > int(m_data.m_skeletonAnimationEntries.size()))
		g_pILog->LogError("CFacialAnimSequence: Inserting skeleton animation entry at invalid location (Code bug).");
	else
		m_data.m_skeletonAnimationEntries.insert(m_data.m_skeletonAnimationEntries.begin() + index, CFacialAnimSkeletalAnimationEntry());
}

//////////////////////////////////////////////////////////////////////////
void CFacialAnimSequence::DeleteSkeletonAnimationEntry(int index)
{
	if (index < 0 || index >= int(m_data.m_skeletonAnimationEntries.size()))
		g_pILog->LogError("CFacialAnimSequence: Deleting skeleton animation entry at invalid location (Code bug).");
	else
		m_data.m_skeletonAnimationEntries.erase(m_data.m_skeletonAnimationEntries.begin() + index);
}

//////////////////////////////////////////////////////////////////////////
IFacialAnimSkeletonAnimationEntry* CFacialAnimSequence::GetSkeletonAnimationEntry(int index)
{
	if (index < 0 || index >= int(m_data.m_skeletonAnimationEntries.size()))
	{
		g_pILog->LogError("CFacialAnimSequence: Getting skeleton animation entry at invalid location (Code bug).");
		return 0;
	}
	return &m_data.m_skeletonAnimationEntries[index];
}

//////////////////////////////////////////////////////////////////////////
void CFacialAnimSequence::SetJoystickFile(const char* joystickFile)
{
	m_data.m_joystickFile = joystickFile;
}

//////////////////////////////////////////////////////////////////////////
const char* CFacialAnimSequence::GetJoystickFile() const
{
	return m_data.m_joystickFile.c_str();
}

//////////////////////////////////////////////////////////////////////////
void CFacialAnimSequence::Serialize(XmlNodeRef& xmlNode, bool bLoading, ESerializationFlags flags)
{
	if (bLoading)
	{
		SerializeLoad(m_data, xmlNode, flags);
	}
	else
	{
		//////////////////////////////////////////////////////////////////////////
		xmlNode->removeAllChilds();
		if (flags & SFLAG_ANIMATION)
		{
			for (int i = 0, end = (int)m_data.m_channels.size(); i < end; i++)
			{
				IFacialAnimChannel* pChannel = m_data.m_channels[i];
				if (pChannel->GetParent()) // Only save top level channels.
					continue;

				XmlNodeRef channelNode = xmlNode->newChild("Channel");

				SerializeChannelSave(pChannel, channelNode);
			}

			xmlNode->setAttr("Flags", m_data.m_nFlags);
			xmlNode->setAttr("StartTime", m_data.m_timeRange.start);
			xmlNode->setAttr("EndTime", m_data.m_timeRange.end);
			xmlNode->setAttr("Joysticks", m_data.m_joystickFile);
		}

		if (flags & SFLAG_SOUND_ENTRIES)
		{
			for (int soundEntry = 0, soundEntryCount = m_data.m_soundEntries.size(); soundEntry < soundEntryCount; ++soundEntry)
			{
				XmlNodeRef soundEntryNode(xmlNode->newChild("SoundEntry"));
				soundEntryNode->setAttr("Sound", m_data.m_soundEntries[soundEntry].m_sound);
				soundEntryNode->setAttr("StartTime", m_data.m_soundEntries[soundEntry].m_startTime);
				if (m_data.m_soundEntries[soundEntry].m_pSentence)
				{
					XmlNodeRef nodeRef(soundEntryNode->newChild("Sentence"));
					m_data.m_soundEntries[soundEntry].m_pSentence->Serialize(nodeRef, false);
				}
			}
		}

		if (flags & SFLAG_ANIMATION)
		{
			for (int skeletonAnimationEntry = 0, skeletonAnimationCount = m_data.m_skeletonAnimationEntries.size(); skeletonAnimationEntry < skeletonAnimationCount; ++skeletonAnimationEntry)
			{
				XmlNodeRef skeletonAnimationEntryNode(xmlNode->newChild("SkeletonAnimationEntry"));
				skeletonAnimationEntryNode->setAttr("AnimationName", m_data.m_skeletonAnimationEntries[skeletonAnimationEntry].m_animationName);
				skeletonAnimationEntryNode->setAttr("StartTime", m_data.m_skeletonAnimationEntries[skeletonAnimationEntry].m_startTime);
			}
		}

		if (flags & SFLAG_CAMERA_PATH)
		{
			XmlNodeRef cameraPathPositionsNode(xmlNode->newChild("CameraPathPositions"));
			m_data.m_cameraPathPosition.SerializeSpline(cameraPathPositionsNode, false);
			XmlNodeRef cameraPathOrientationsNode(xmlNode->newChild("CameraPathOrientations"));
			m_data.m_cameraPathOrientation.SerializeSpline(cameraPathOrientationsNode, false);
			XmlNodeRef cameraPathFOVNode(xmlNode->newChild("CameraPathFOV"));
			m_data.m_cameraPathFOV.SerializeSpline(cameraPathFOVNode, false);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialAnimSequence::MergeSequence(IFacialAnimSequence* pMergeSequence, const Functor1wRet<const char*, MergeCollisionAction>& collisionStrategy)
{
#ifdef FACE_STORE_ASSET_VALUES
	if (this == pMergeSequence)
	{
		CryWarning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, "Attempting to merge sequence with itself - aborting.");
		return;
	}

	if (!pMergeSequence)
	{
		CryWarning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, "No merge sequence supplied (NULL) - aborting.");
		return;
	}

	PREFAST_ASSUME(pMergeSequence); // wouldve returned if was NULL
	std::vector<string> originalFullPaths, mergeFullPaths;
	std::vector<string>* fullPathsList[2] = { &originalFullPaths, &mergeFullPaths };
	Channels* channelsList[2] = { &m_data.m_channels, &((CFacialAnimSequence*)pMergeSequence)->m_data.m_channels };

	for (int instance = 0; instance < 2; ++instance)
	{
		std::vector<string>& fullPaths = *fullPathsList[instance];
		Channels& channels = *channelsList[instance];

		fullPaths.resize(channels.size());
		for (size_t i = 0, end = fullPaths.size(); i < end; ++i)
		{
			fullPaths[i] = channels[i]->GetName();
			for (IFacialAnimChannel* pChannel = channels[i]->GetParent(); pChannel; pChannel = pChannel->GetParent())
				fullPaths[i] = string().Format("%s@:@%s", pChannel->GetName(), fullPaths[i].c_str());
		}
	}

	typedef VectorMap<string, IFacialAnimChannel*> NameChannelMap;

	// Merge in the channels.
	{
		NameChannelMap nameChannelMap;
		{
			NameChannelMap::container_type nameChannelMapEntries;
			nameChannelMapEntries.reserve(originalFullPaths.size());
			for (size_t i = 0, end = originalFullPaths.size(); i < end; ++i)
				nameChannelMapEntries.push_back(std::make_pair(originalFullPaths[i], m_data.m_channels[i]));
			nameChannelMap.SwapElementsWithVector(nameChannelMapEntries);
		}

		for (int channelIndex = 0; channelIndex < pMergeSequence->GetChannelCount(); ++channelIndex)
		{
			IFacialAnimChannel* pMergeChannel = pMergeSequence->GetChannel(channelIndex);
			NameChannelMap::iterator itNameEntry = nameChannelMap.find(mergeFullPaths[channelIndex]);
			IFacialAnimChannel* pTargetChannel = 0;
			bool copyChannel = true;
			if (itNameEntry == nameChannelMap.end())
			{
				pTargetChannel = CreateChannel();
				originalFullPaths.push_back(mergeFullPaths[channelIndex]);
			}
			else
			{
				pTargetChannel = (*itNameEntry).second;
				if (!pTargetChannel->IsGroup())
					copyChannel = collisionStrategy(pMergeChannel->GetName()) == MergeCollisionActionOverwrite;
			}

			if (pTargetChannel && copyChannel)
			{
				pTargetChannel->SetName(pMergeChannel->GetName());
				pTargetChannel->SetFlags(pMergeChannel->GetFlags());
				pTargetChannel->SetEffector(pMergeChannel->GetEffector());
				ISplineInterpolator* pMergeSpline = pMergeChannel->GetLastInterpolator();
				ISplineInterpolator* pTargetSpline = pTargetChannel->GetLastInterpolator();
				if (pMergeSpline && pTargetSpline)
				{
					while (pTargetSpline->GetKeyCount())
						pTargetSpline->RemoveKey(0);
					for (int key = 0; key < pMergeSpline->GetKeyCount(); ++key)
					{
						ISplineInterpolator::ValueType value;
						pMergeSpline->GetKeyValue(key, value);
						int originalKeyIndex = pTargetSpline->InsertKey(FacialEditorSnapTimeToFrame(pMergeSpline->GetKeyTime(key)), value);
						pTargetSpline->SetKeyFlags(originalKeyIndex, pMergeSpline->GetKeyFlags(key));
						ISplineInterpolator::ValueType tin, tout;
						pMergeSpline->GetKeyTangents(key, tin, tout);
						pTargetSpline->SetKeyTangents(originalKeyIndex, tin, tout);
					}
				}
			}
		}
	}

	// Update the parents of the channels.
	{
		NameChannelMap nameChannelMap;
		{
			NameChannelMap::container_type nameChannelMapEntries;
			nameChannelMapEntries.reserve(originalFullPaths.size());
			for (size_t i = 0; i < originalFullPaths.size(); ++i)
				nameChannelMapEntries.push_back(std::make_pair(originalFullPaths[i], m_data.m_channels[i]));
			nameChannelMap.SwapElementsWithVector(nameChannelMapEntries);
		}

		typedef VectorMap<IFacialAnimChannel*, int> ChannelIndexMap;
		ChannelIndexMap channelIndexMap;
		{
			ChannelIndexMap::container_type channelIndexMapEntries;
			channelIndexMapEntries.reserve(pMergeSequence->GetChannelCount());
			for (int i = 0; i < pMergeSequence->GetChannelCount(); ++i)
				channelIndexMapEntries.push_back(std::make_pair(pMergeSequence->GetChannel(i), i));
			channelIndexMap.SwapElementsWithVector(channelIndexMapEntries);
		}

		for (int channelIndex = 0; channelIndex < pMergeSequence->GetChannelCount(); ++channelIndex)
		{
			IFacialAnimChannel* pMergeChannel = pMergeSequence->GetChannel(channelIndex);
			NameChannelMap::iterator itNameEntry = nameChannelMap.find(mergeFullPaths[channelIndex]);

			IFacialAnimChannel* pTargetChannel = 0;
			if (itNameEntry != nameChannelMap.end())
				pTargetChannel = (*itNameEntry).second;

			ChannelIndexMap::iterator itParentIndexEntry = channelIndexMap.end();
			if (pMergeChannel->GetParent())
				itParentIndexEntry = channelIndexMap.find(pMergeChannel->GetParent());

			NameChannelMap::iterator itParentEntry = nameChannelMap.end();
			if (itParentIndexEntry != channelIndexMap.end())
				itParentEntry = nameChannelMap.find(mergeFullPaths[(*itParentIndexEntry).second]);

			IFacialAnimChannel* pTargetParent = 0;
			if (itParentEntry != nameChannelMap.end())
				pTargetParent = (*itParentEntry).second;

			if (pTargetChannel)
				pTargetChannel->SetParent(pTargetParent);
		}
	}

	GenerateProceduralChannels(m_data);
#else
	CryFatalError("Not available on this platform");
#endif
}

void CFacialAnimSequence::StreamAsyncOnComplete(IReadStream* pStream, unsigned nError)
{
	//	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_FSQ, 0, GetName());
	//	CRY_PROFILE_FUNCTION(PROFILE_ANIMATION);

	if (pStream->IsError())
	{
		return;
	}

	const char* buf = (const char*) pStream->GetBuffer();
	uint32 lengthBuf = pStream->GetBytesRead();
	XmlNodeRef root = g_pISystem->LoadXmlFromBuffer(buf, lengthBuf, false);

	if (!root)
	{
		// Used as a signal to the MT callback that an error occured
		SAFE_DELETE(m_pStreamingData);
		return;
	}

	unsigned flags = (gEnv->IsEditor() ? IFacialAnimSequence::SFLAG_ALL : IFacialAnimSequence::SFLAG_ALL & ~IFacialAnimSequence::SFLAG_CAMERA_PATH);
	SerializeLoad(*m_pStreamingData, root, (ESerializationFlags)flags);

	pStream->FreeTemporaryMemory();
}

void CFacialAnimSequence::StreamOnComplete(IReadStream* pStream, unsigned nError)
{
	using std::swap;

	CRY_PROFILE_FUNCTION(PROFILE_ANIMATION);

	const char* file = pStream->GetName();

	if (!m_pStreamingData)
	{
		gEnv->pLog->LogError(file, "Failed to load facial sequence");
		m_pFaceAnim->OnSequenceStreamed(this, false);
		return;
	}

	if (pStream->IsError())
	{
		gEnv->pLog->LogError(file, "Failed to stream facial sequence (%s)", file);
		m_pFaceAnim->OnSequenceStreamed(this, false);
		SAFE_DELETE(m_pStreamingData);
		return;
	}

	swap(*m_pStreamingData, m_data);

	m_pFaceAnim->OnSequenceStreamed(this, true);

	m_pStream = NULL;
	SAFE_DELETE(m_pStreamingData);
}

//////////////////////////////////////////////////////////////////////////
void CFacialAnimSequence::UpdateProceduralChannels()
{
	UpdateSoundEntriesValidateID();
	if (m_data.m_nProceduralChannelsValidateID != m_data.m_nSoundEntriesValidateID)
		GenerateProceduralChannels(m_data);
}

//////////////////////////////////////////////////////////////////////////
void CFacialAnimSequence::GenerateProceduralChannels(Data& data)
{
	CProceduralChannel* pUpDownChannel = data.m_proceduralChannels.GetChannel(CProceduralChannelSet::ChannelTypeHeadUpDown);
	while (pUpDownChannel->GetInterpolator()->GetKeyCount())
		pUpDownChannel->GetInterpolator()->RemoveKey(0);

	CProceduralChannel* pLeftRightChannel = data.m_proceduralChannels.GetChannel(CProceduralChannelSet::ChannelTypeHeadRightLeft);
	while (pLeftRightChannel->GetInterpolator()->GetKeyCount())
		pLeftRightChannel->GetInterpolator()->RemoveKey(0);

	pLeftRightChannel->GetInterpolator()->InsertKeyFloat(0, 0);
	pUpDownChannel->GetInterpolator()->InsertKeyFloat(0, 0);

	for (int soundEntry = 0, soundEntryCount = data.m_soundEntries.size(); soundEntry < soundEntryCount; ++soundEntry)
	{
		float fLeftRight = 0.0f;
		float fUpDown = 0.0f;
		float fLeftRightMax = 0.10f;
		float fUpDownMax = 0.12f;
		float fLeftRightDecayRate = 0.7f;
		float fUpDownDecayRate = 0.8f;
		float fLeftRightJumpMax = 0.10f;
		float fUpDownJumpMax = 0.15f;
		float fOldTime = data.m_soundEntries[soundEntry].m_startTime;
		float fTimeDeltaThreshold = 0.4f;
		for (int wordIndex = 0; wordIndex < data.m_soundEntries[soundEntry].m_pSentence->GetWordCount(); ++wordIndex)
		{
			IFacialSentence::Word lastWord;
			if (wordIndex > 0)
				data.m_soundEntries[soundEntry].m_pSentence->GetWord(wordIndex - 1, lastWord);

			IFacialSentence::Word word;
			data.m_soundEntries[soundEntry].m_pSentence->GetWord(wordIndex, word);

			float fTime = float(word.startTime) / 1000 + data.m_soundEntries[soundEntry].m_startTime;
			float fTimeDelta = fTime - fOldTime;

			float fRandLeftRight = cry_random(-1.0f, 1.0f);
			fRandLeftRight = sinf(fRandLeftRight * 3.14159f);
			float fJumpLeftRight = fRandLeftRight * fLeftRightJumpMax * fTimeDelta;
			fLeftRight += fJumpLeftRight;
			if (fLeftRight < -fLeftRightMax)
				fLeftRight = -fLeftRightMax;
			if (fLeftRight > fLeftRightMax)
				fLeftRight = fLeftRightMax;
			fLeftRight *= fLeftRightDecayRate;
			pLeftRightChannel->GetInterpolator()->InsertKeyFloat(float(word.startTime) / 1000, fLeftRight);

			if (fTimeDelta > fTimeDeltaThreshold && wordIndex > 0)
				pLeftRightChannel->GetInterpolator()->InsertKeyFloat(float(lastWord.endTime) / 1000 + data.m_soundEntries[soundEntry].m_startTime, fLeftRight);

			float fRandUpDown = cry_random(-1.0f, 1.0f);
			fRandUpDown = sinf(fRandUpDown * 3.14159f);
			float fJumpUpDown = fRandUpDown * fUpDownJumpMax * fTimeDelta;
			fUpDown += fJumpUpDown;
			if (fUpDown < -fUpDownMax)
				fUpDown = -fUpDownMax;
			if (fUpDown > fUpDownMax)
				fUpDown = fUpDownMax;
			fUpDown *= fUpDownDecayRate;
			pUpDownChannel->GetInterpolator()->InsertKeyFloat(float(word.startTime) / 1000 + data.m_soundEntries[soundEntry].m_startTime, fUpDown);

			if (fTimeDelta > fTimeDeltaThreshold && wordIndex > 0)
				pUpDownChannel->GetInterpolator()->InsertKeyFloat(float(lastWord.endTime) / 1000 + data.m_soundEntries[soundEntry].m_startTime, fUpDown);

			fOldTime = fTime;
		}
	}

	data.m_nProceduralChannelsValidateID = data.m_nSoundEntriesValidateID;
}

//////////////////////////////////////////////////////////////////////////
void CFacialAnimSequence::UpdateSoundEntriesValidateID()
{
	for (int i = 0, count = m_data.m_soundEntries.size(); i < count; ++i)
	{
		if (m_data.m_soundEntries[i].IsSentenceInvalid())
		{
			m_data.m_soundEntries[i].ValidateSentence();
			++m_data.m_nSoundEntriesValidateID;
		}
	}
}

void CFacialAnimSequence::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(this, sizeof(*this));
	pSizer->AddObject(m_name);
	pSizer->AddObject(m_data.m_joystickFile);
	pSizer->AddObject(m_data.m_skeletonAnimationEntries);
	pSizer->AddObject(m_data.m_channels);
	pSizer->AddObject(m_data.m_soundEntries);
	pSizer->AddObject(m_data.m_proceduralChannels);
	pSizer->AddObject(m_data.m_cameraPathPosition);
	pSizer->AddObject(m_data.m_cameraPathFOV);
}
//////////////////////////////////////////////////////////////////////////
// CFacialAnimSoundEntry
//////////////////////////////////////////////////////////////////////////

CFacialAnimSoundEntry::CFacialAnimSoundEntry()
	: m_pSentence(new CFacialSentence)
{
	m_startTime = 0.0f;
	m_nSentenceValidateID = m_pSentence->GetValidateID();
}

void CFacialAnimSoundEntry::SetSoundFile(const char* sSoundFile)
{
	m_sound = sSoundFile;
}

const char* CFacialAnimSoundEntry::GetSoundFile()
{
	return m_sound.c_str();
}

float CFacialAnimSoundEntry::GetStartTime()
{
	return m_startTime;
}

void CFacialAnimSoundEntry::SetStartTime(float time)
{
	m_startTime = time;
}

void CFacialAnimSoundEntry::ValidateSentence()
{
	m_nSentenceValidateID = m_pSentence->GetValidateID();
}

bool CFacialAnimSoundEntry::IsSentenceInvalid()
{
	return m_nSentenceValidateID != m_pSentence->GetValidateID();
}

//////////////////////////////////////////////////////////////////////////
// CFacialAnimSkeletalAnimationEntry
//////////////////////////////////////////////////////////////////////////

CFacialAnimSkeletalAnimationEntry::CFacialAnimSkeletalAnimationEntry()
	: m_startTime(0.0f)
	, m_endTime(1.0f)
{
}

void CFacialAnimSkeletalAnimationEntry::SetName(const char* skeletonAnimationFile)
{
	m_animationName = skeletonAnimationFile;
}

const char* CFacialAnimSkeletalAnimationEntry::GetName() const
{
	return m_animationName.c_str();
}

void CFacialAnimSkeletalAnimationEntry::SetStartTime(float time)
{
	m_startTime = time;
}
float CFacialAnimSkeletalAnimationEntry::GetStartTime() const
{
	return m_startTime;
}

void CFacialAnimSkeletalAnimationEntry::SetEndTime(float time)
{
	m_endTime = time;
}
float CFacialAnimSkeletalAnimationEntry::GetEndTime() const
{
	return m_endTime;
}

//////////////////////////////////////////////////////////////////////////
// CFacialAnimSequenceInstance
//////////////////////////////////////////////////////////////////////////

class EffectorChannelPair
{
public:
	EffectorChannelPair(IFacialEffector* pEffector, int channelIndex) : pEffector(pEffector), channelIndex(channelIndex) {}
	bool operator<(const EffectorChannelPair& other) const  { return pEffector < other.pEffector; }
	bool operator>(const EffectorChannelPair& other) const  { return pEffector > other.pEffector; }
	bool operator!=(const EffectorChannelPair& other) const { return pEffector != other.pEffector; }
	bool operator==(const EffectorChannelPair& other) const { return pEffector == other.pEffector; }
	IFacialEffector* pEffector;
	int              channelIndex;
};

void CFacialAnimSequenceInstance::BindChannels(CFacialAnimationContext* pContext, CFacialAnimSequence* pSequence)
{
	bool bFoundCategoryBalance = false;

	UnbindChannels();

	m_balanceChannelStateIndices.clear();
	m_balanceChannelStateIndices.reserve(pContext->GetInstance()->GetState()->GetNumWeights());

	uint32 numChannels = pSequence->GetChannelCount();

	m_pAnimContext = pContext;
	CFacialInstance* pInstance = pContext->GetInstance();

	m_channels.resize(numChannels);
	for (uint32 i = 0; i < numChannels; i++)
		m_channels[i].nBalanceChannelListIndex = -1;
	int rootBalanceChannelEntryIndex = -1;
	for (uint32 i = 0; i < numChannels; i++)
	{
		CFacialAnimChannel* pSeqChannel = (CFacialAnimChannel*)pSequence->GetChannel(i);
		ChannelInfo& chinfo = m_channels[i];
		chinfo.nChannelId = INVALID_CHANNEL_ID;

		// MichaelS - If this channel is a balance controller, add it to the list of balance controllers.
		if (pSeqChannel->GetFlags() & IFacialAnimChannel::FLAG_BALANCE)
		{
			size_t balanceChannelEntryIndex = m_balanceChannelEntries.size();
			m_balanceChannelEntries.resize(balanceChannelEntryIndex + 1);
			BalanceChannelEntry& entry = m_balanceChannelEntries[balanceChannelEntryIndex];
			entry.nChannelIndex = i;
			entry.nMorphIndexStartIndex = 0;
			entry.nMorphIndexCount = 0;

			// The controller could either be one that applies to a certain category (ie folder) of expressions,
			// or it could be one that applies to a set of expressions in the sequence. If it is the latter,
			// then we need to associate the channel with its parent, so that it can be propagated to other channels
			// in the sequence.
			if ((pSeqChannel->GetFlags() & IFacialAnimChannel::FLAG_CATEGORY_BALANCE) == 0)
			{
				// Associate the channel with the parent.
				CFacialAnimChannel* pParentChannel = (CFacialAnimChannel*)pSeqChannel->GetParent();
				if (pParentChannel)
				{
					ChannelInfo& parentInfo = m_channels[pParentChannel->GetInstanceChannelId()];
					parentInfo.nBalanceChannelListIndex = balanceChannelEntryIndex;
				}
				else
				{
					rootBalanceChannelEntryIndex = balanceChannelEntryIndex;
				}
			}
			else
			{
				bFoundCategoryBalance = true;
			}
		}

		chinfo.bUse = false;
		if (!pSeqChannel->IsGroup() && (pSeqChannel->GetFlags() & (IFacialAnimChannel::FLAG_PHONEME_STRENGTH | IFacialAnimChannel::FLAG_PROCEDURAL_STRENGTH)) == 0)
		{
			//chinfo.pEffector = pSeqChannel->GetEffectorPtr();
			chinfo.pEffector = (CFacialEffector*)pInstance->FindEffector(pSeqChannel->GetEffectorIdentifier());

			if (chinfo.pEffector && !(pSeqChannel->GetFlags() & IFacialAnimChannel::FLAG_BALANCE))
			{
				chinfo.bUse = true;

				// Make a channel in animation context.
				SFacialEffectorChannel effch;
				effch.bNotRemovable = true; // This channel cannot be removed by anim context.
				effch.status = SFacialEffectorChannel::STATUS_ONE;
				effch.pEffector = chinfo.pEffector;
				chinfo.nChannelId = m_pAnimContext->StartChannel(effch);
				chinfo.bUse = true;
			}
		}
	}

	// MichaelS - Propagate balance channels from parents to children.
	for (uint32 i = 0; i < numChannels; ++i)
	{
		CFacialAnimChannel* pSeqChannel = (CFacialAnimChannel*)pSequence->GetChannel(i);
		ChannelInfo& chinfo = m_channels[i];

		if (chinfo.nBalanceChannelListIndex == -1)
		{
			CFacialAnimChannel* pParentChannel = (CFacialAnimChannel*)pSeqChannel->GetParent();
			int nBalanceChannelListIndex = rootBalanceChannelEntryIndex;
			while (pParentChannel)
			{
				assert(pParentChannel->GetInstanceChannelId() >= 0 && pParentChannel->GetInstanceChannelId() < int(m_channels.size()));
				ChannelInfo& parentInfo = m_channels[pParentChannel->GetInstanceChannelId()];
				if (parentInfo.nBalanceChannelListIndex != -1)
				{
					nBalanceChannelListIndex = parentInfo.nBalanceChannelListIndex;
					break;
				}

				pParentChannel = (CFacialAnimChannel*)pParentChannel->GetParent();
			}

			chinfo.nBalanceChannelListIndex = nBalanceChannelListIndex;
		}
	}

	// MichaelS - Apply category-based balance controls.
	if (bFoundCategoryBalance)
	{
		typedef std::vector<EffectorChannelPair> EffectorMultiMap;
		EffectorMultiMap effectorChannelMultiMap;
		effectorChannelMultiMap.reserve(m_channels.size());
		for (int channelIndex = 0, end = int(m_channels.size()); channelIndex < end; ++channelIndex)
			effectorChannelMultiMap.push_back(EffectorChannelPair(m_channels[channelIndex].pEffector, channelIndex));
		std::sort(effectorChannelMultiMap.begin(), effectorChannelMultiMap.end());

		for (int balanceChannelIndex = 0; balanceChannelIndex < int(m_balanceChannelEntries.size()); ++balanceChannelIndex)
		{
			// Currently only category balance controls have an effector.
			if (m_channels[m_balanceChannelEntries[balanceChannelIndex].nChannelIndex].pEffector)
			{
				BalanceChannelEntry& entry = m_balanceChannelEntries[balanceChannelIndex];
				entry.nMorphIndexStartIndex = int(m_balanceChannelStateIndices.size());
				entry.nMorphIndexCount = 0;

				class Recurser
				{
				public:
					Recurser(EffectorMultiMap& effectorChannelMultiMap, Channels& channels, int balanceChannelIndex, std::vector<int>& balanceChannelStateIndices)
						: effectorChannelMultiMap(effectorChannelMultiMap),
						channels(channels),
						balanceChannelIndex(balanceChannelIndex),
						balanceChannelStateIndices(balanceChannelStateIndices)
					{
					}

					void operator()(IFacialEffector* pEffector)
					{
						std::pair<EffectorMultiMap::iterator, EffectorMultiMap::iterator> range = std::equal_range(
						  effectorChannelMultiMap.begin(), effectorChannelMultiMap.end(), EffectorChannelPair(pEffector, 0));
						for (EffectorMultiMap::iterator itPair = range.first; itPair != range.second; ++itPair)
						{
							if (channels[(*itPair).channelIndex].nBalanceChannelListIndex == -1)
								channels[(*itPair).channelIndex].nBalanceChannelListIndex = balanceChannelIndex;
						}

						if (pEffector->GetIndexInState() >= 0)
							balanceChannelStateIndices.push_back(pEffector->GetIndexInState());

						for (int subEffectorIndex = 0; subEffectorIndex < pEffector->GetSubEffectorCount(); ++subEffectorIndex)
							(*this)(pEffector->GetSubEffector(subEffectorIndex));
					}

				private:
					EffectorMultiMap& effectorChannelMultiMap;
					Channels&         channels;
					int               balanceChannelIndex;
					std::vector<int>& balanceChannelStateIndices;
				};

				Recurser(effectorChannelMultiMap, m_channels, balanceChannelIndex, m_balanceChannelStateIndices)(
				  m_channels[m_balanceChannelEntries[balanceChannelIndex].nChannelIndex].pEffector);

				entry.nMorphIndexCount = int(m_balanceChannelStateIndices.size()) - entry.nMorphIndexStartIndex;
			}
		}
	}

	BindProceduralChannels(pContext, pSequence);

	m_nValidateID = pSequence->GetValidateId();
}

//////////////////////////////////////////////////////////////////////////
void CFacialAnimSequenceInstance::BindProceduralChannels(CFacialAnimationContext* pContext, CFacialAnimSequence* pSequence)
{
	UnbindProceduralChannels();

	CFacialInstance* pInstance = pContext->GetInstance();

	m_proceduralChannels.resize(CProceduralChannelSet::ChannelType_count);
	for (int i = 0; i < CProceduralChannelSet::ChannelType_count; i++)
	{
		CProceduralChannel* pSeqChannel = pSequence->GetProceduralChannelSet().GetChannel(static_cast<CProceduralChannelSet::ChannelType>(i));
		ChannelInfo& chinfo = m_proceduralChannels[i];
		chinfo.bUse = false;
		chinfo.nChannelId = INVALID_CHANNEL_ID;
		chinfo.pEffector = (CFacialEffector*)pInstance->FindEffector(pSeqChannel->GetEffectorIdentifier());
		chinfo.nBalanceChannelListIndex = -1;
		if (chinfo.pEffector && pSeqChannel->GetInterpolator()->GetKeyCount())
		{
			chinfo.bUse = true;

			// Make a channel in animation context.
			SFacialEffectorChannel effch;
			effch.bNotRemovable = true; // This channel cannot be removed by anim context.
			effch.status = SFacialEffectorChannel::STATUS_ONE;
			effch.pEffector = chinfo.pEffector;
			chinfo.nChannelId = m_pAnimContext->StartChannel(effch);
		}
	}

	m_nValidateID = pSequence->GetValidateId();
}

//////////////////////////////////////////////////////////////////////////
void CFacialAnimSequenceInstance::UnbindChannels()
{
	if (!m_pAnimContext)
		return;
	int numChannels = (int)m_channels.size();
	for (int i = 0; i < numChannels; i++)
	{
		ChannelInfo& chinfo = m_channels[i];
		if (chinfo.nChannelId != INVALID_CHANNEL_ID)
		{
			m_pAnimContext->RemoveChannel(chinfo.nChannelId);
		}
	}
	if (m_nCurrentPhonemeChannelId != -1)
		m_pAnimContext->RemoveChannel(m_nCurrentPhonemeChannelId);
	m_channels.clear();
	m_balanceChannelEntries.clear();

	CCharInstance* pMasterCharacter = m_pAnimContext->GetInstance()->GetMasterCharacter();
	if (pMasterCharacter)
	{
		//pMasterCharacter->m_Morphing.m_fMorphVertexDrag = 1.0f;
		UnbindProceduralChannels();
	}

}

//////////////////////////////////////////////////////////////////////////
void CFacialAnimSequenceInstance::UnbindProceduralChannels()
{
	if (!m_pAnimContext)
		return;
	int numChannels = (int)m_proceduralChannels.size();
	for (int i = 0; i < numChannels; i++)
	{
		ChannelInfo& chinfo = m_proceduralChannels[i];
		if (chinfo.nChannelId != INVALID_CHANNEL_ID)
		{
			m_pAnimContext->RemoveChannel(chinfo.nChannelId);
		}
	}
	m_proceduralChannels.clear();
}

void CFacialAnimSequenceInstance::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(m_channels);
	pSizer->AddObject(m_proceduralChannels);
	pSizer->AddObject(m_balanceChannelStateIndices);
	pSizer->AddObject(m_balanceChannelEntries);
	pSizer->AddObject(m_nValidateID);
	pSizer->AddObject(m_pAnimContext);
	pSizer->AddObject(m_nCurrentPhoneme);
	pSizer->AddObject(m_nCurrentPhonemeChannelId);
}
//////////////////////////////////////////////////////////////////////////
CProceduralChannelSet::CProceduralChannelSet()
{
	CFacialAnimation* pFaceAnim = g_pCharacterManager->GetFacialAnimation();
	m_channels[ChannelTypeHeadUpDown].SetEffectorIdentifier(pFaceAnim->CreateIdentifierHandle("Neck_up_down"));
	m_channels[ChannelTypeHeadRightLeft].SetEffectorIdentifier(pFaceAnim->CreateIdentifierHandle("Neck_left_right"));
}
