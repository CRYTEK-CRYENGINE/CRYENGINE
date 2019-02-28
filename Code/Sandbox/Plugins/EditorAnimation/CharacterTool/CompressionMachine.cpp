// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include <CryCore/Platform/platform.h>
#include <CryCore/smartptr.h>
#include "IBackgroundTaskManager.h"
#include "CafCompressionHelper.h"
#include "CompressionMachine.h"
#include "../Shared/AnimSettings.h"
#include <CrySystem/ISystem.h>
#include <CryAnimation/ICryAnimation.h>
#include <IEditor.h>
#include <QtCore/QFile>
#include <QtCore/QDir>
#include <CryString/CryPath.h>
#include "Serialization.h"
#include <CrySerialization/IArchiveHost.h>

static bool IsAimAnimation(ICharacterInstance* pCharacterInstance, const char* animationName)
{
	assert(pCharacterInstance != NULL);

	IAnimationSet* pAnimationSet = pCharacterInstance->GetIAnimationSet();
	assert(pAnimationSet != NULL);

	const int localAnimationId = pAnimationSet->GetAnimIDByName(animationName);
	if (localAnimationId == -1)
	{
		return false;
	}

	const uint32 animationFlags = pAnimationSet->GetAnimationFlags(localAnimationId);
	const bool isAimAnimation = (animationFlags & CA_AIMPOSE);

	return isAimAnimation;
}

static string GetAnimSettingsFilename(const string& animationPath)
{
	const string gameFolderPath = PathUtil::AddSlash(PathUtil::GetGameFolder());
	const string animationFilePath = gameFolderPath + animationPath;

	const string animSettingsFilename = PathUtil::ReplaceExtension(animationFilePath, "animsettings");
	return animSettingsFilename;
}

void AddAnimationToAnimationSet(ICharacterInstance& characterInstance, const char* animationName, const char* animationPath)
{
	if (!animationName)
		return;
	if (!animationPath)
		return;

	IAnimationSet* pAnimationSet = characterInstance.GetIAnimationSet();

	const int localAnimationId = pAnimationSet->GetAnimIDByName(animationName);
	const bool animationExists = (localAnimationId != -1);
	if (animationExists)
	{
		return;
	}

	IDefaultSkeleton& defaultSkeleton = characterInstance.GetIDefaultSkeleton();
	pAnimationSet->AddAnimationByPath(animationName, animationPath, &defaultSkeleton);
}

namespace CharacterTool
{

static bool SetAnimationReferenceByName(SAnimationReference* ref, ICharacterInstance* character, const char* animationName)
{
	IAnimationSet* const animationSet = character->GetIAnimationSet();
	if (!animationSet)
	{
		return false;
	}

	const int localAnimationId = animationSet->GetAnimIDByName(animationName);
	if (localAnimationId < 0)
	{
		return false;
	}

	const bool bIsRegularCaf = !animationSet->IsBlendSpace(localAnimationId)
	                           && !animationSet->IsAimPose(localAnimationId, character->GetIDefaultSkeleton())
	                           && !animationSet->IsLookPose(localAnimationId, character->GetIDefaultSkeleton());

	ref->reset(animationSet->GetFilePathCRCByAnimID(localAnimationId), animationName, bIsRegularCaf);
	return true;
}

static bool SerializedEquivalent(const Serialization::SStruct& a, const Serialization::SStruct& b)
{
	return gEnv->pSystem->GetArchiveHost()->CompareBinary(a, b);
}

void SAnimationReference::reset(uint32 pathCRC, const char* animationName, bool bIsRegularCaf)
{
	if (this->pathCRC != pathCRC)
	{
		if (this->bIsRegularCaf && this->pathCRC != 0)
		{
			gEnv->pCharacterManager->CAF_Release(this->pathCRC);
		}

		this->pathCRC = pathCRC;
		this->bIsRegularCaf = bIsRegularCaf;

		if (this->bIsRegularCaf && this->pathCRC != 0)
		{
			gEnv->pCharacterManager->CAF_AddRef(pathCRC);
		}
	}
	else
	{
		assert(this->bIsRegularCaf == bIsRegularCaf);
	}

	this->animationName = animationName;
}

class CompressionMachine::BackgroundTaskCompressPreview : public IBackgroundTask
{
public:
	BackgroundTaskCompressPreview(CompressionMachine* pEditor, int sessionIndex, const char* animationPath, const char* outputAnimationPath, const SAnimSettings& animSettings, bool ignorePresets)
		: m_pEditor(pEditor)
		, m_animSettings(animSettings)
		, m_animationPath(animationPath)
		, m_outputAnimationPath(outputAnimationPath)
		, m_outputFileSize(0)
		, m_compressedCafSize(0)
		, m_sessionIndex(sessionIndex)
		, m_ignorePresets(ignorePresets)
		, m_failed(false)
	{
		m_description = "Preview for '";
		m_description += m_animationPath;
		;
		m_description += "'";
	}

	const char* Description() const override  { return m_description.c_str(); }
	const char* ErrorMessage() const override { return m_errorMessage.c_str(); }

	ETaskResult Work() override
	{
		ICVar* ca_useIMG_CAF = gEnv->pSystem->GetIConsole()->GetCVar("ca_useIMG_CAF");
		if (!ca_useIMG_CAF || ca_useIMG_CAF->GetIVal() != 0)
		{
			m_errorMessage = "Failed to create preview, 'ca_UseIMG_CAF' has value other than zero.";
			m_failed = true;
			return eTaskResult_Failed;
		}

		if (!CafCompressionHelper::CompressAnimationForPreview(&m_createdFile, &m_errorMessage, m_animationPath, m_animSettings, m_ignorePresets, m_sessionIndex))
		{
			m_failed = true;
			return eTaskResult_Failed;
		}

		char buffer[ICryPak::g_nMaxPath] = { 0 };
		string filename = gEnv->pCryPak->AdjustFileName(m_createdFile.c_str(), buffer, 0);
		m_outputFileSize = gEnv->pCryPak->GetFileSizeOnDisk(filename.c_str());
		m_compressedCafSize = gEnv->pCryPak->FGetSize(m_animationPath.c_str(), true);
		return eTaskResult_Completed;
	}

	void Delete() { delete this; }

	bool MoveResult(string* outErrorMessage)
	{
		if (!m_createdFile.empty())
			return CafCompressionHelper::MoveCompressionResult(outErrorMessage, m_createdFile.c_str(), m_outputAnimationPath.c_str());

		return true;
	}

	void Finalize() override
	{
		m_pEditor->OnCompressed(this);
		CafCompressionHelper::CleanUpCompressionResult(m_createdFile.c_str());
	}

	bool Failed() const { return m_failed; }

	SAnimSettings                  m_animSettings;
	string                         m_animationPath;

	_smart_ptr<CompressionMachine> m_pEditor;
	uint64                         m_outputFileSize;
	uint64                         m_compressedCafSize;
	string                         m_errorMessage;
	string                         m_description;
	string                         m_createdFile;
	string                         m_outputAnimationPath;
	bool                           m_ignorePresets;
	bool                           m_failed;
	int                            m_sessionIndex;
};

struct CompressionMachine::AnimationSetExtender : IAnimationSetListener
{
	CompressionMachine*               m_machine;
	vector<std::pair<string, string>> m_animations;
	ICharacterInstance*               m_character;

	AnimationSetExtender(CompressionMachine* machine) : m_machine(machine), m_character(0) {}

	void AddAnimation(const char* name, const char* path)
	{
		for (size_t i = 0; i < m_animations.size(); ++i)
		{
			if (m_animations[i].first == name)
			{
				assert(m_animations[i].second == path);
				return;
			}
		}

		m_animations.push_back(std::make_pair(name, path));
		if (m_pIAnimationSet)
			AddAnimationToAnimationSet(*m_character, m_animations.back().first, m_animations.back().second);
	}

	void SetCharacterInstance(ICharacterInstance* character)
	{
		m_character = character;
		IAnimationSet* animationSet = character ? character->GetIAnimationSet() : 0;
		if (animationSet)
			animationSet->RegisterListener(this);
		else if (m_pIAnimationSet)
			m_pIAnimationSet->UnregisterListener(this);

		OnAnimationSetReloaded();
	}

	void OnAnimationSetReloaded() override
	{
		if (m_pIAnimationSet)
		{
			assert(m_character);
			for (size_t i = 0; i < m_animations.size(); ++i)
				AddAnimationToAnimationSet(*m_character, m_animations[i].first, m_animations[i].second);

			m_machine->StartPreview(false, false);
		}
	}
};

static void FormatReferenceName(string* name, string* path, int index)
{
	if (name)
		name->Format("_CompressionEditor_Reference_%d", index);
	if (path)
		path->Format("animations/editor/reference_preview_%d.caf", index);
}

static void FormatPreviewName(string* name, string* path, int index)
{
	if (name)
		name->Format("_CompressionEditor_Preview_%d", index);
	if (path)
		path->Format("animations/editor/compression_preview_%d.caf", index);
}

// ---------------------------------------------------------------------------

CompressionMachine::CompressionMachine()
	: m_state(eState_Idle)
	, m_showOriginalAnimation(false)
	, m_previewReloadListener(new AnimationSetExtender(this))
	, m_referenceReloadListener(new AnimationSetExtender(this))
	, m_normalizedStartTime(0.0f)
	, m_playbackSpeed(1.0f)
	, m_compressionSessionIndex(0)
	, m_compressedCharacter(0)
	, m_uncompressedCharacter(0)
{
}

void CompressionMachine::SetCharacters(ICharacterInstance* uncompressedCharacter, ICharacterInstance* compressedCharacter)
{
	m_compressedCharacter = compressedCharacter;
	m_uncompressedCharacter = uncompressedCharacter;

	m_previewReloadListener->SetCharacterInstance(compressedCharacter);
	m_referenceReloadListener->SetCharacterInstance(uncompressedCharacter);

	SetState(eState_Idle);
}

static bool EquivalentAnimSettings(const vector<SAnimSettings>& a, const vector<SAnimSettings>& b)
{
	if (a.size() != b.size())
		return false;
	for (size_t i = 0; i < a.size(); ++i)
		if (!SerializedEquivalent(Serialization::SStruct(a[i]), Serialization::SStruct(b[i])))
			return false;
	return true;
}

void CompressionMachine::PreviewAnimation(const PlaybackLayers& layers, const vector<bool>& isModified, bool showOriginalAnimation, const vector<SAnimSettings>& animSettings, float normalizedTime, bool forceRecompile, bool expectToReloadChrparams)
{
	if (m_compressedCharacter == 0)
		return;

	bool hasNonEmptyLayers = false;
	for (size_t i = 0; i < layers.layers.size(); ++i)
		if (!layers.layers[i].animation.empty())
			hasNonEmptyLayers = true;

	if (!hasNonEmptyLayers)
	{
		Reset();
		return;
	}

	m_normalizedStartTime = normalizedTime;

	if (!forceRecompile && layers.ContainsSameAnimations(m_playbackLayers) &&
	    EquivalentAnimSettings(m_animSettings, animSettings) &&
	    showOriginalAnimation == m_showOriginalAnimation &&
	    (m_state == eState_PreviewAnimation || m_state == eState_PreviewCompressedOnly))
	{
		// make sure we don't recompress animations for all layer parameters changes
		m_playbackLayers = layers;
		for (size_t i = 0; i < layers.layers.size(); ++i)
			m_animations[i].enabled = layers.layers[i].enabled && !layers.layers[i].animation.empty();

		Play(normalizedTime);
		return;
	}

	m_showOriginalAnimation = showOriginalAnimation;
	m_playbackLayers = layers;
	m_layerAnimationsModified = isModified;
	m_animSettings = animSettings;

	StartPreview(forceRecompile, expectToReloadChrparams);
}

void CompressionMachine::StartPreview(bool forceRecompile, bool expectToReloadChrparams)
{
	m_animations.clear();
	m_animations.resize(m_playbackLayers.layers.size());

	if (!m_compressedCharacter || !m_compressedCharacter->GetISkeletonAnim())
		return;

	if (m_uncompressedCharacter)
		m_uncompressedCharacter->GetISkeletonAnim()->StopAnimationsAllLayers();

	m_compressedCharacter->GetISkeletonAnim()->StopAnimationsAllLayers();
	SetState(eState_Waiting);

	for (size_t i = 0; i < m_playbackLayers.layers.size(); ++i)
	{
		Animation& animation = m_animations[i];
		const PlaybackLayer& layer = m_playbackLayers.layers[i];
		animation.path = layer.path;
		animation.name = layer.animation;
		animation.state = eAnimationState_Compression;

		if (!layer.enabled || animation.name.empty())
			animation.enabled = false;

		if (animation.name.empty())
			continue;

		FormatPreviewName(&animation.compressedAnimationName, &animation.compressedAnimationPath, int(i));
		if (m_uncompressedCharacter)
			FormatReferenceName(&animation.uncompressedAnimationName, &animation.uncompressedAnimationPath, int(i));
		animation.type = animation.UNKNOWN;

		const char* extension = PathUtil::GetExt(animation.path.c_str());
		if (stricmp(extension, "bspace") == 0 || stricmp(extension, "comb") == 0)
		{
			animation.compressedAnimationName = animation.name;
			animation.uncompressedAnimationName = animation.name;
			SetAnimationReferenceByName(&animation.compressedCaf, m_compressedCharacter, animation.name);
			if (m_uncompressedCharacter)
				SetAnimationReferenceByName(&animation.uncompressedCaf, m_uncompressedCharacter, animation.name);
			animation.state = eAnimationState_Ready;
			animation.type = animation.BLEND_SPACE;
			animation.hasSourceFile = true;
			AnimationStateChanged();
			continue;
		}
		else if (stricmp(extension, "anm") == 0 || stricmp(extension, "cga") == 0)
		{
			animation.compressedAnimationName = animation.name;
			animation.uncompressedAnimationName = animation.name;
			SetAnimationReferenceByName(&animation.compressedCaf, m_compressedCharacter, animation.name);
			if (m_uncompressedCharacter)
				SetAnimationReferenceByName(&animation.uncompressedCaf, m_uncompressedCharacter, animation.name);
			animation.state = eAnimationState_Ready;
			animation.type = animation.ANM;
			animation.hasSourceFile = true;
			AnimationStateChanged();
			continue;
		}
		else if (stricmp(extension, "caf") == 0)
		{
			string iCafPath = PathUtil::ReplaceExtension(animation.path, "i_caf");
			animation.hasSourceFile = gEnv->pCryPak->IsFileExist(iCafPath.c_str(), ICryPak::eFileLocation_OnDisk);

			if (IsAimAnimation(m_uncompressedCharacter, animation.name.c_str()))
			{
				animation.type = animation.AIMPOSE;
				animation.compressedAnimationName = animation.name;
				animation.compressedAnimationPath = animation.path;
				animation.uncompressedAnimationName = animation.name;
				animation.uncompressedAnimationPath = animation.path;

				SetAnimationReferenceByName(&animation.compressedCaf, m_compressedCharacter, animation.compressedAnimationName);

				if (m_showOriginalAnimation && m_uncompressedCharacter)
					SetAnimationReferenceByName(&animation.uncompressedCaf, m_uncompressedCharacter, animation.uncompressedAnimationName);

				animation.state = eAnimationState_Ready;
			}
			else
			{
				animation.type = animation.CAF;
				if ((!m_showOriginalAnimation && !forceRecompile && !m_layerAnimationsModified[i]) || !animation.hasSourceFile)
				{
					animation.uncompressedCaf.reset(0, nullptr, false);
					animation.compressedAnimationName = animation.name;
					if (SetAnimationReferenceByName(&animation.compressedCaf, m_compressedCharacter, animation.compressedAnimationName))
						animation.state = eAnimationState_Ready;
					else if (expectToReloadChrparams)
					{
						animation.state = eAnimationState_WaitingToReloadChrparams;
						SetState(eState_Waiting);
					}
					else
					{
						animation.state = eAnimationState_Failed;
						animation.failMessage = "Animation is missing from animation set";
						SetState(eState_Failed);
					}

					AnimationStateChanged();
					continue;
				}

				if (m_layerAnimationsModified[i])
				{
					animation.previewTask = new BackgroundTaskCompressPreview(this, m_compressionSessionIndex++, animation.path.c_str(), animation.compressedAnimationPath.c_str(), m_animSettings[i], false);
					GetIEditor()->GetBackgroundTaskManager()->AddTask(animation.previewTask, eTaskPriority_RealtimePreview, eTaskThreadMask_IO);
				}
				else
				{
					animation.compressedAnimationName = animation.name;
					animation.compressedAnimationPath = animation.path;
					SetAnimationReferenceByName(&animation.compressedCaf, m_compressedCharacter, animation.compressedAnimationName);
					animation.hasPreviewCompressed = true;
				}

				SAnimSettings zeroAnimSettings = m_animSettings[i];
				zeroAnimSettings.build.compression = SCompressionSettings();
				zeroAnimSettings.build.compression.SetZeroCompression();

				animation.referenceTask = new BackgroundTaskCompressPreview(this, m_compressionSessionIndex++, animation.path.c_str(), animation.uncompressedAnimationPath.c_str(), zeroAnimSettings, true);
				GetIEditor()->GetBackgroundTaskManager()->AddTask(animation.referenceTask, eTaskPriority_RealtimePreview, eTaskThreadMask_IO);
				// TODO: cache recompressed reference animations
				//
				// if (!SetCafReferenceByName(&animation.uncompressedCaf, m_uncompressedCharacter, animation.referenceAnimationName))
				// {
				//  animation.state = eAnimationState_Failed;
				//  animation.failMessage =  "Missing animation in animation set";
				// }
			}
			continue;
		}
		else
		{
			animation.failMessage = "Unknown animation format";
			animation.state = eAnimationState_Failed;
			animation.type = animation.UNKNOWN;
			continue;
		}
	}

	AnimationStateChanged();
}

bool CompressionMachine::IsPlaying() const
{
	return m_state == eState_PreviewAnimation || m_state == eState_PreviewCompressedOnly;
}

void CompressionMachine::Reset()
{
	if (m_uncompressedCharacter)
		m_uncompressedCharacter->GetISkeletonPose()->SetDefaultPose();
	if (m_compressedCharacter)
		m_compressedCharacter->GetISkeletonPose()->SetDefaultPose();
	SetState(eState_Idle);
}

void CompressionMachine::OnCompressed(BackgroundTaskCompressPreview* pTask)
{
	Animation* animation = 0;
	for (size_t i = 0; i < m_animations.size(); ++i)
	{
		if (m_animations[i].previewTask == pTask)
		{
			animation = &m_animations[i];
			animation->previewTask = 0;
		}
		else if (m_animations[i].referenceTask == pTask)
		{
			animation = &m_animations[i];
			animation->referenceTask = 0;
		}
	}

	if (!animation)
		return;

	if (pTask->IsCanceled())
	{
		animation->state = eAnimationState_Canceled;
		AnimationStateChanged();
		return;
	}

	if (pTask->Failed())
	{
		animation->failMessage = pTask->ErrorMessage();
		animation->state = eAnimationState_Failed;
		AnimationStateChanged();
		return;
	}

	if (!pTask->MoveResult(&animation->failMessage))
	{
		animation->state = eAnimationState_Failed;
		AnimationStateChanged();
		return;
	}

	if (pTask->m_ignorePresets) // for reference
	{
		animation->compressionInfo.uncompressedSize = pTask->m_outputFileSize;
		animation->compressionInfo.compressedCafSize = pTask->m_compressedCafSize;
		animation->hasReferenceCompressed = true;

		ICharacterManager* pCharacterManager = GetISystem()->GetIAnimationSystem();
		m_previewReloadListener->AddAnimation(animation->uncompressedAnimationName.c_str(), animation->uncompressedAnimationPath.c_str());
		if (pCharacterManager->ReloadCAF(animation->uncompressedAnimationPath) == CR_RELOAD_SUCCEED)
		{
			SetAnimationReferenceByName(&animation->uncompressedCaf, m_uncompressedCharacter, animation->uncompressedAnimationName);

			if (animation->hasReferenceCompressed && animation->hasPreviewCompressed)
				animation->state = eAnimationState_Ready;
		}
		else
		{
			animation->failMessage = "Failed to reload animation";
			animation->state = eAnimationState_Failed;
		}

		AnimationStateChanged();
	}
	else
	{
		animation->compressionInfo.compressedPreviewSize = pTask->m_outputFileSize;
		animation->compressionInfo.compressedCafSize = pTask->m_compressedCafSize;
		animation->path = pTask->m_animationPath;

		if (m_state == eState_Waiting)
		{
			if (pTask->m_outputFileSize != 0 && !animation->path.empty())
			{
				m_previewReloadListener->AddAnimation(animation->compressedAnimationName.c_str(), animation->compressedAnimationPath.c_str());
				SetAnimationReferenceByName(&animation->compressedCaf, m_compressedCharacter, animation->compressedAnimationName);

				ICharacterManager* pCharacterManager = GetISystem()->GetIAnimationSystem();
				if (!pCharacterManager->ReloadCAF(animation->compressedAnimationPath))
				{
					animation->state = eAnimationState_Failed;
					animation->failMessage = "Reloading failed";
				}
				else
					animation->hasPreviewCompressed = true;

				if (animation->hasReferenceCompressed && animation->hasPreviewCompressed)
					animation->state = eAnimationState_Ready;

				AnimationStateChanged();
			}
			else
			{
				SetState(eState_Failed);
				animation->failMessage = pTask->m_errorMessage;
			}
		}
	}
}

void CompressionMachine::AnimationStateChanged()
{
	if (m_state == eState_Waiting)
	{
		bool hasCanceled = false;
		bool hasFailed = false;
		bool hasIncomplete = false;
		bool hasCompressedOnly = false;

		for (size_t i = 0; i < m_animations.size(); ++i)
		{
			const Animation& a = m_animations[i];
			if (!a.enabled)
				continue;
			if (a.state == eAnimationState_Canceled)
				hasCanceled = true;
			else if (a.state == eAnimationState_Failed)
				hasFailed = true;
			if (a.state != eAnimationState_Ready)
				hasIncomplete = true;
			if (!a.hasSourceFile)
				hasCompressedOnly = true;
		}

		if (hasFailed)
			SetState(eState_Failed);
		else if (hasCanceled)
			SetState(eState_Idle);
		else if (!hasIncomplete)
		{
			if (hasCompressedOnly)
				SetState(eState_PreviewCompressedOnly);
			else
				SetState(eState_PreviewAnimation);

			Play(m_normalizedStartTime);
		}
	}
}

void CompressionMachine::SetState(State state)
{
	m_state = state;

	if (m_state == eState_Failed)
	{
		if (m_uncompressedCharacter)
			m_uncompressedCharacter->GetISkeletonPose()->SetDefaultPose();
		if (m_compressedCharacter)
			m_compressedCharacter->GetISkeletonPose()->SetDefaultPose();
	}
}

static void PlayLayer(ICharacterInstance* character, const PlaybackLayer& layer, const char* animationName,
                      float normalizedTime, float playbackSpeed, bool loop)
{
	ISkeletonAnim& skeletonAnim = *character->GetISkeletonAnim();
	int localAnimID = character->GetIAnimationSet()->GetAnimIDByName(animationName);
	if (localAnimID < 0)
		return;
	character->SetPlaybackScale(playbackSpeed);

	CryCharAnimationParams params;
	params.m_fPlaybackSpeed = 1.0f;
	params.m_fTransTime = 0;
	params.m_fPlaybackWeight = layer.weight;
	params.m_nLayerID = layer.layerId;
	params.m_fKeyTime = normalizedTime;
	params.m_nFlags = CA_FORCE_SKELETON_UPDATE | CA_ALLOW_ANIM_RESTART;
	if (loop)
		params.m_nFlags |= CA_LOOP_ANIMATION;
	else
		params.m_nFlags |= CA_REPEAT_LAST_KEY;
	skeletonAnim.StartAnimationById(localAnimID, params);
}

void CompressionMachine::Play(float normalizedTime)
{
	if (m_state != eState_PreviewAnimation && m_state != eState_PreviewCompressedOnly)
		return;

	for (size_t i = 0; i < m_playbackLayers.layers.size(); ++i)
	{
		const PlaybackLayer& layer = m_playbackLayers.layers[i];
		Animation& animation = m_animations[i];
		if (!animation.enabled)
		{
			m_compressedCharacter->GetISkeletonAnim()->StopAnimationInLayer(layer.layerId, 0.0f);
			if (m_uncompressedCharacter)
				m_uncompressedCharacter->GetISkeletonAnim()->StopAnimationInLayer(layer.layerId, 0.0f);
		}
		else
		{
			PlayLayer(m_compressedCharacter, layer, animation.compressedAnimationName.c_str(), normalizedTime, m_playbackSpeed, m_loop);
			if (animation.uncompressedCaf.PathCRC())
				PlayLayer(m_uncompressedCharacter, layer, animation.uncompressedAnimationName.c_str(), normalizedTime, m_playbackSpeed, m_loop);
		}
	}

	SignalAnimationStarted();
}

void CompressionMachine::SetLoop(bool loop)
{
	if (m_loop != loop)
	{
		m_loop = loop;
		if (IsPlaying())
		{
			int layer = 0; // TODO add layers supports
			ISkeletonAnim& skeletonAnim = *m_compressedCharacter->GetISkeletonAnim();
			float normalizedTime = skeletonAnim.GetAnimationNormalizedTime(&skeletonAnim.GetAnimFromFIFO(layer, 0));
			Play(normalizedTime);
		}
	}
}

void CompressionMachine::SetPlaybackSpeed(float playbackSpeed)
{
	if (m_playbackSpeed != playbackSpeed)
	{
		m_playbackSpeed = playbackSpeed;
		if (IsPlaying())
		{
			int layer = 0; // TODO add layers supports
			ISkeletonAnim& skeletonAnim = *m_compressedCharacter->GetISkeletonAnim();
			float normalizedTime = skeletonAnim.GetAnimationNormalizedTime(&skeletonAnim.GetAnimFromFIFO(layer, 0));
			Play(normalizedTime);
		}
	}
}

void CompressionMachine::GetAnimationStateText(vector<StateText>* lines, bool compressedCharacter)
{
	lines->clear();

	if (compressedCharacter)
	{
		ICVar* ca_useIMG_CAF = gEnv->pSystem->GetIConsole()->GetCVar("ca_useIMG_CAF");
		if (ca_useIMG_CAF->GetIVal() != 0)
		{
			StateText line;
			line.type = line.WARNING;
			line.text = "ca_useIMG_CAF has value other than 0.";
			lines->push_back(line);
			line.text = " Only animations compiled during the build can be previewed";
			lines->push_back(line);
		}

		if (m_state == eState_Waiting)
		{
			StateText line;
			line.text = "Compressing...";
			line.type = line.PROGRESS;

			for (size_t i = 0; i < m_animations.size(); ++i)
			{
				if (m_animations[i].state == eAnimationState_WaitingToReloadChrparams)
				{
					line.text = "Reloading ChrParams...";
					break;
				}
			}
			lines->push_back(line);
		}
		else if (m_state == eState_Failed)
		{
			for (size_t i = 0; i < m_animations.size(); ++i)
			{
				const Animation& animation = m_animations[i];
				if (!animation.enabled)
					continue;
				if (animation.state == eAnimationState_Failed)
				{
					StateText line;
					line.type = line.FAIL;
					line.animation = animation.name;
					line.text = animation.failMessage;
					lines->push_back(line);
				}
			}
		}
		else if (m_state == eState_PreviewAnimation)
		{
			for (size_t i = 0; i < m_animations.size(); ++i)
			{
				StateText line;
				line.type = line.INFO;

				const Animation& animation = m_animations[i];
				if (!animation.enabled)
					continue;

				switch (animation.type)
				{
				case animation.AIMPOSE:
					{
						line.animation = animation.name;
						line.text = "AIM/Look Pose";
						lines->push_back(line);
						break;
					}
				case animation.BLEND_SPACE:
					{
						line.animation = animation.name;
						line.text = "BlendSpace";
						lines->push_back(line);
						break;
					}
				case animation.ANM:
					{
						line.animation = animation.name;
						line.text = "ANM format does not support compression";
						lines->push_back(line);
						break;
					}
				case animation.CAF:
					{
						if (m_showOriginalAnimation)
						{
							if (animation.hasSourceFile)
							{
								uint64 compressedSize = animation.compressionInfo.compressedPreviewSize;
								if (compressedSize == 0)
									compressedSize = animation.compressionInfo.compressedCafSize;
								if (animation.compressionInfo.uncompressedSize > 0)
								{
									float percent = float(compressedSize) * 100.0f / float(animation.compressionInfo.uncompressedSize);
									line.animation = animation.name;
									line.text.Format("Compressed %.0f->%.0fKB (%.1f%%)",
									                 animation.compressionInfo.uncompressedSize / 1024.0f,
									                 compressedSize / 1024.0f,
									                 percent);
									lines->push_back(line);
								}
								else
								{
									line.animation = animation.name;
									line.text = "Compressed";
									lines->push_back(line);
								}
							}
							else
							{
								line.animation = animation.name;
								line.text = "Precompiled animation (missing source i_caf)";
								lines->push_back(line);
							}
						}
						break;
					}
				}
			}
		}
	}
	else
	{
		if (m_state == eState_PreviewAnimation || m_state == eState_PreviewCompressedOnly)
		{
			bool hasMissingSources = false;

			for (size_t i = 0; i < m_animations.size(); ++i)
			{
				const Animation& animation = m_animations[i];
				if (!animation.hasSourceFile)
				{
					StateText line;
					line.type = line.WARNING;
					line.animation = animation.name;
					line.text = "Missing source i_caf";
					hasMissingSources = true;
					lines->push_back(line);
				}
			}

			if (!hasMissingSources)
			{
				StateText line;
				line.type = line.INFO;
				line.text = "Original";
				lines->push_back(line);
			}

			for (size_t i = 0; i < m_animations.size(); ++i)
			{
				const Animation& animation = m_animations[i];
				if (animation.type == animation.BLEND_SPACE)
				{
					StateText line;
					line.type = line.WARNING;
					line.animation = animation.name;
					line.text = "BlendSpace (compressed)";
					lines->push_back(line);
				}
			}
		}
	}
}

const char* CompressionMachine::AnimationPathConsideringPreview(const char* inputCaf) const
{
	for (size_t i = 0; i < m_animations.size(); ++i)
		if (m_animations[i].type == Animation::CAF && m_animations[i].hasSourceFile && m_animations[i].path == inputCaf)
			return m_animations[i].compressedAnimationPath.c_str();
	return inputCaf;
}
}

