// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include <CryAnimation/ICryAnimation.h>
#include <QApplication>
#include <QClipboard>
#include "FileDialogs/SystemFileDialog.h"
#include "Controls/QuestionDialog.h"
#include <QDir>
#include <QFile>
#include "AnimationList.h"
#include "Serialization.h"
#include <CrySerialization/IArchiveHost.h>
#include <CrySystem/File/IFileChangeMonitor.h>
#include "IEditor.h"
#include "SkeletonList.h"
#include "Expected.h"
#include "Explorer/Explorer.h"
#include "../Cry3DEngine/CGF/CGFLoader.h"
#include "IAnimationCompressionManager.h"
#include <CrySystem/File/ICryPak.h>
#include "AnimEventFootstepGenerator.h"
#include "QPropertyTree/QPropertyDialog.h"
#include "QPropertyTree/ContextList.h"
#include "ListSelectionDialog.h"
#include "Explorer/ExplorerFileList.h"
#include "dll_string.h"
#include "IResourceSelectorHost.h"
#include "CharacterDocument.h"
#include "CharacterToolSystem.h"
#include "IBackgroundTaskManager.h"
#include "CharacterToolSystem.h"
#include "AnimationCompressionManager.h"
#include <CryIcon.h>
#include "FilePathUtil.h"

namespace CharacterTool {

struct UpdateAnimationSizesTask : public IBackgroundTask
{
	System*                 m_system;
	vector<ExplorerEntryId> m_entries;
	vector<string>          m_animationPaths;
	int                     m_column;
	vector<unsigned int>    m_newSizes;
	int                     m_subtree;

	void Delete() override
	{
		delete this;
	}

	ETaskResult Work() override
	{
		m_newSizes.resize(m_animationPaths.size());
		for (size_t i = 0; i < m_animationPaths.size(); ++i)
		{
			const char* animationPath = m_animationPaths[i].c_str();
			if (!EXPECTED(animationPath[0] != '\0'))
			{
				m_newSizes.clear();
				return eTaskResult_Failed;
			}

			unsigned int newSize = 0;
			if (FILE* f = gEnv->pCryPak->FOpen(animationPath, "rb"))
			{
				m_newSizes[i] = gEnv->pCryPak->FGetSize(f);
				gEnv->pCryPak->FClose(f);
			}
			else
				m_newSizes[i] = 0;
		}

		return eTaskResult_Completed;
	}

	UpdateAnimationSizesTask(const vector<ExplorerEntryId>& entries, const vector<string>& animationPaths, int column, System* system, int subtree)
		: m_system(system)
		, m_entries(entries)
		, m_column(column)
		, m_animationPaths(animationPaths)
		, m_subtree(subtree)
	{
	}

	UpdateAnimationSizesTask(const ExplorerEntryId& entryId, const string& animationPath, int column, System* system, int subtree)
		: m_system(system)
		, m_entries(1, entryId)
		, m_column(column)
		, m_animationPaths(1, animationPath)
		, m_subtree(subtree)
	{
	}

	void Finalize() override
	{
		if (!m_newSizes.empty())
		{
			if (m_system->explorerData.get())
			{
				m_system->explorerData->BeginBatchChange(m_subtree);
				size_t num = min(m_entries.size(), m_newSizes.size());
				for (size_t i = 0; i < m_entries.size(); ++i)
					m_system->explorerData->SetEntryColumn(m_entries[i], m_column, m_newSizes[i], true);
				m_system->explorerData->EndBatchChange(m_subtree);
			}
		}
	}

};

// ---------------------------------------------------------------------------

static vector<string> LoadJointNames(const char* skeletonPath)
{
	CLoaderCGF cgfLoader;

	CChunkFile chunkFile;
	std::auto_ptr<CContentCGF> cgf(cgfLoader.LoadCGF(skeletonPath, chunkFile, 0));
	if (!cgf.get())
		return vector<string>();

	if (const CSkinningInfo* skinningInfo = cgf->GetSkinningInfo())
	{
		vector<string> result;
		result.reserve(skinningInfo->m_arrBonesDesc.size());
		for (size_t i = 0; i < skinningInfo->m_arrBonesDesc.size(); ++i)
		{
			result.push_back(skinningInfo->m_arrBonesDesc[i].m_arrBoneName);
		}
		return result;
	}

	return vector<string>();
}

static bool LoadAnimEvents(ActionOutput* output, AnimEvents* animEvents, const char* animEventsFilename, const char* animationPath)
{
	animEvents->clear();

	XmlNodeRef root = GetIEditor()->GetSystem()->LoadXmlFromFile(animEventsFilename);
	if (!root)
	{
		if (output)
			output->AddError("Failed to load animevents database", animEventsFilename);
		return false;
	}

	XmlNodeRef animationNode;
	{
		int nodeCount = root->getChildCount();
		for (int i = 0; i < nodeCount; ++i)
		{
			XmlNodeRef node = root->getChild(i);
			if (!node->isTag("animation"))
				continue;

			if (stricmp(node->getAttr("name"), animationPath) == 0)
			{
				animationNode = node;
				break;
			}
		}
	}

	if (animationNode)
	{
		for (int i = 0; i < animationNode->getChildCount(); ++i)
		{
			XmlNodeRef eventNode = animationNode->getChild(i);

			AnimEvent ev;
			ev.LoadFromXMLNode(eventNode);
			animEvents->push_back(ev);
		}
	}

	return true;
}

static bool LoadAnimationEntry(SEntry<AnimationContent>* entry, SkeletonList* skeletonList, IAnimationSet* animationSet, const string& defaultSkeleton, const char* animEventsFilename)
{
	string cafPath = entry->path;
	cafPath.MakeLower();

	entry->content.events.clear();

	bool result = true;
	if (animEventsFilename[0] != '\0' && !LoadAnimEvents(0, &entry->content.events, animEventsFilename, cafPath.c_str()))
		result = false;

	if (entry->content.type == AnimationContent::BLEND_SPACE)
	{
		string errorMessage;
		XmlNodeRef node = gEnv->pSystem->LoadXmlFromFile(entry->path.c_str());
		if (node)
		{
			BlendSpace& bspace = entry->content.blendSpace;
			bspace = BlendSpace();
			result = bspace.LoadFromXml(errorMessage, node, animationSet);
		}
		else
		{
			result = false;
		}
	}
	else if (entry->content.type == AnimationContent::COMBINED_BLEND_SPACE)
	{
		string errorMessage;
		XmlNodeRef node = gEnv->pSystem->LoadXmlFromFile(entry->path.c_str());
		if (node)
		{
			CombinedBlendSpace& cbspace = entry->content.combinedBlendSpace;
			cbspace = CombinedBlendSpace();
			result = cbspace.LoadFromXml(errorMessage, node, animationSet);
		}
		else
		{
			result = false;
		}
	}
	else
	{
		string animationSettingsFilename = SAnimSettings::GetAnimSettingsFilename(entry->path);
		if (!gEnv->pCryPak->IsFileExist(animationSettingsFilename.c_str()))
		{
			entry->content.importState = AnimationContent::COMPILED_BUT_NO_ANIMSETTINGS;
		}
		else
		{
			if (entry->content.settings.Load(animationSettingsFilename, vector<string>(), 0, 0))
			{
				if (entry->content.importState == AnimationContent::COMPILED_BUT_NO_ANIMSETTINGS)
					entry->content.importState = AnimationContent::IMPORTED;
			}
			else
			{
				entry->content.settings = SAnimSettings();
				entry->content.settings.build.skeletonAlias = defaultSkeleton;
				entry->content.newAnimationSkeleton = defaultSkeleton;
				result = false;
			}
		}

		if (entry->content.settings.build.compression.m_usesNameContainsInPerBoneSettings)
		{
			string skeletonPath = skeletonList->FindSkeletonPathByName(entry->content.settings.build.skeletonAlias.c_str());
			if (!skeletonPath.empty())
			{
				vector<string> jointNames = LoadJointNames(skeletonPath.c_str());
				if (!jointNames.empty())
				{
					if (!entry->content.settings.Load(animationSettingsFilename, jointNames, 0, 0))
						result = false;
				}
			}
		}
	}

	return result;
}

// ---------------------------------------------------------------------------

AnimationList::AnimationList(System* system)
	: m_system(system)
	, m_animationSet(nullptr)
	, m_character(nullptr)
	, m_importEntriesInitialized(false)
	, m_explorerColumnFrames(-1)
	, m_explorerColumnSize(-1)
	, m_explorerColumnAudio(-1)
	, m_explorerColumnPak(-1)
	, m_explorerColumnNew(-1)
	, m_bReloading(false)
{
	GetIEditor()->GetFileMonitor()->RegisterListener(this, "", "caf");
	GetIEditor()->GetFileMonitor()->RegisterListener(this, "", "i_caf");
	GetIEditor()->GetFileMonitor()->RegisterListener(this, "", "animsettings");
	GetIEditor()->GetFileMonitor()->RegisterListener(this, "", "bspace");
	GetIEditor()->GetFileMonitor()->RegisterListener(this, "", "comb");
}

AnimationList::~AnimationList()
{
	if (GetIEditor() && GetIEditor()->GetFileMonitor())
		GetIEditor()->GetFileMonitor()->UnregisterListener(this);
}

static bool IsInternalAnimationName(const char* name, const char* animationPath)
{
	if (strncmp(name, "InternalPara", 12) == 0)
		return true;
	const char* editorPrefix = "animations/editor/";
	if (animationPath && _strnicmp(animationPath, editorPrefix, strlen(editorPrefix)) == 0)
		return true;
	return false;
}

static bool AnimationHasAudioEvents(const char* animationPath)
{
	IAnimEvents* animEvents = gEnv->pCharacterManager->GetIAnimEvents();
	if (IAnimEventList* animEventList = animEvents->GetAnimEventList(animationPath))
	{
		int count = animEventList->GetCount();
		for (int i = 0; i < count; ++i)
		{
			CAnimEventData& animEventData = animEventList->GetByIndex(i);
			if (IsAudioEventType(animEventData.GetName()))
				return true;
		}
	}
	return false;
}

static int GetAnimationPakState(const char* animationPath)
{
	int result = 0;
	const char* ext = PathUtil::GetExt(animationPath);
	if (stricmp(ext, "caf") == 0)
	{
		static const char* extensions[] = { "i_caf", "caf", "animsettings" };

		int numExtensions = sizeof extensions / sizeof extensions[0];
		for (int i = 0; i < numExtensions; ++i)
		{
			string path = PathUtil::ReplaceExtension(animationPath, extensions[i]);
			result |= ExplorerFileList::GetFilePakState(path);
		}
	}
	else
	{
		// bspace/comb etc.
		result |= ExplorerFileList::GetFilePakState(animationPath);
	}

	// Pak column display values do not include the first "None" state, so they are 1 less than the PakFileState enum
	return result - 1;
}

void AnimationList::Populate(ICharacterInstance* character, const char* defaultSkeletonAlias, const AnimationSetFilter& filter, const char* animEventsFilename)
{
	if (!character)
		return;
	IAnimationSet* animationSet = character->GetIAnimationSet();
	if (animationSet)
		animationSet->RegisterListener(this);

	m_animationSet = animationSet;
	m_animEventsFilename = animEventsFilename;
	m_filter = filter;
	m_defaultSkeletonAlias = defaultSkeletonAlias;
	m_character = character;

	ReloadAnimationList();
}

void AnimationList::SetExplorerData(ExplorerData* explorerData, int subtree)
{
	m_subtree = subtree;
	m_explorerColumnSize = explorerData->AddColumn("Size", ExplorerColumn::KILOBYTES, false);
	m_explorerColumnFrames = explorerData->AddColumn("Frames", ExplorerColumn::INTEGER, false);

	const ExplorerColumnValue audioValues[] = {
		{ "No audio",  ""                                },
		{ "Has audio", "icons:common/animation_audio_event.ico" },
	};
	int audioValueCount = sizeof(audioValues) / sizeof(audioValues[0]);
	m_explorerColumnAudio = explorerData->AddColumn("Audio", ExplorerColumn::ICON, false, audioValues, audioValueCount);
	m_explorerColumnPak = explorerData->FindColumn("Pak");
	m_explorerColumnNew = explorerData->AddColumn("New", ExplorerColumn::ICON, false);
}

void AnimationList::SetAnimationFilterAndScan(const AnimationSetFilter& filter)
{
	m_filter = filter;
	ScanForImportEntries(0, false);
}

void AnimationList::SetAnimationEventsFilePath(const string& animationEventsFilePath)
{
	m_animEventsFilename = animationEventsFilePath;
}

void AnimationList::ReloadAnimationList()
{
	m_animations.Clear();
	m_aliasToId.clear();

	IAnimEvents* animEvents = gEnv->pCharacterManager->GetIAnimEvents();

	std::vector<std::pair<ExplorerEntryId, unsigned int>> audioColumnValues;
	std::vector<std::pair<ExplorerEntryId, int>> pakColumnValues;
	std::vector<std::pair<ExplorerEntryId, int>> framesColumnValues;

	IDefaultSkeleton& skeleton = m_character->GetIDefaultSkeleton();

	std::vector<string> animationPaths;
	std::vector<ExplorerEntryId> entries;

	int numAnimations = m_animationSet ? m_animationSet->GetAnimationCount() : 0;
	for (int i = 0; i < numAnimations; ++i)
	{
		const char* name = m_animationSet->GetNameByAnimID(i);
		const char* animationPath = m_animationSet->GetFilePathByID(i);
		if (IsInternalAnimationName(name, animationPath))
			continue;

		AnimationContent::Type type = AnimationContent::ANIMATION;
		int flags = m_animationSet->GetAnimationFlags(i);
		if (flags & CA_ASSET_LMG)
		{
			if (m_animationSet->IsCombinedBlendSpace(i))
				type = AnimationContent::COMBINED_BLEND_SPACE;
			else
				type = AnimationContent::BLEND_SPACE;
		}
		else if (flags & CA_ASSET_TCB)
		{
			type = AnimationContent::ANM;
		}
		else if (flags & CA_AIMPOSE)
		{
			if (m_animationSet->IsAimPose(i, skeleton))
				type = AnimationContent::AIMPOSE;
			else if (m_animationSet->IsLookPose(i, skeleton))
				type = AnimationContent::LOOKPOSE;
		}

		SEntry<AnimationContent>* entry = m_animations.AddEntry(0, animationPath, name, false);
		entry->content.type = type;
		entry->content.loadedInEngine = true;
		entry->content.loadedAsAdditive = (flags & CA_ASSET_ADDITIVE) != 0;
		entry->content.importState = AnimationContent::IMPORTED;
		entry->content.animationId = i;
		if (entry->content.settings.build.skeletonAlias.empty())
			entry->content.settings.build.skeletonAlias = m_defaultSkeletonAlias;

		ExplorerEntryId entryId(m_subtree, entry->id);
		animationPaths.push_back(animationPath);
		entries.push_back(entryId);

		entry->Reverted(0);
		m_aliasToId[entry->name] = entry->id;

		bool gotAudioEvents = AnimationHasAudioEvents(animationPath);
		audioColumnValues.push_back(std::make_pair(entryId, gotAudioEvents));

		int pakState = GetAnimationPakState(animationPath);
		pakColumnValues.push_back(std::make_pair(entryId, pakState));

		float durationSeconds = m_animationSet->GetDuration_sec(i);
		const uint32 animationSamplingFrequencyHz = 30;
		int frameCount = 1 + uint32(durationSeconds * animationSamplingFrequencyHz + 0.5f);
		framesColumnValues.push_back(std::make_pair(entryId, frameCount));
	}

	UpdateAnimationSizesTask* sizesTask = new UpdateAnimationSizesTask(entries, animationPaths, m_explorerColumnSize, m_system, m_subtree);
	GetIEditor()->GetBackgroundTaskManager()->AddTask(sizesTask, eTaskPriority_BackgroundScan, eTaskThreadMask_IO);

	SignalBeginBatchChange(m_subtree);

	ScanForImportEntries(&pakColumnValues, true);
	SignalSubtreeReset(m_subtree);

	for (size_t i = 0; i < framesColumnValues.size(); ++i)
		m_system->explorerData->SetEntryColumn(framesColumnValues[i].first, m_explorerColumnFrames, framesColumnValues[i].second, true);
	for (size_t i = 0; i < audioColumnValues.size(); ++i)
		m_system->explorerData->SetEntryColumn(audioColumnValues[i].first, m_explorerColumnAudio, audioColumnValues[i].second, true);
	for (size_t i = 0; i < pakColumnValues.size(); ++i)
		m_system->explorerData->SetEntryColumn(pakColumnValues[i].first, m_explorerColumnPak, pakColumnValues[i].second, true);

	SignalEndBatchChange(m_subtree);
}

void AnimationList::ScanForImportEntries(std::vector<std::pair<ExplorerEntryId, int>>* pakColumnValues, bool resetFollows)
{
	typedef std::set<string, stl::less_stricmp<string>> UnusedAnimations;
	UnusedAnimations unusedAnimations;
	vector<unsigned int> idsToRemove;
	for (size_t i = 0; i < m_animations.Count(); ++i)
	{
		SEntry<AnimationContent>* entry = m_animations.GetByIndex(i);
		if (entry->content.importState == AnimationContent::NEW_ANIMATION)
			unusedAnimations.insert(PathUtil::ReplaceExtension(entry->path, ".caf"));
	}

	if (!m_importEntriesInitialized)
	{
		m_importEntries.clear();
		SDirectoryEnumeratorHelper().ScanDirectoryRecursive("", "", "*.i_caf", m_importEntries);

		m_importEntriesInitialized = true;
	}

	for (size_t i = 0; i < m_importEntries.size(); ++i)
	{
		const string& animationFile = m_importEntries[i];

		const string animationPath = PathUtil::ReplaceExtension(animationFile, ".caf");
		if (m_filter.Matches(animationPath.c_str()))
		{
			unusedAnimations.erase(animationPath);

			bool isAdded = false;
			SEntry<AnimationContent>* entry = m_animations.AddEntry(&isAdded, animationPath.c_str(), 0, false);

			if (!resetFollows)
			{
				if (isAdded)
					SignalEntryAdded(m_subtree, entry->id);
				else
				{
					EntryModifiedEvent ev;
					ev.subtree = m_subtree;
					ev.id = entry->id;
					SignalEntryModified(ev);
				}
			}

			UpdateImportEntry(entry);
			entry->Reverted(0);

			if (isAdded)
			{
				assert(entry->content.importState == AnimationContent::NEW_ANIMATION);

				const string animSettingsPath = PathUtil::ReplaceExtension(animationFile, ".animsettings");
				if (gEnv->pCryPak->IsFileExist(animSettingsPath.c_str(), ICryPak::eFileLocation_OnDisk) && m_system->animationCompressionManager)
				{
					// We expect all .i_caf sources with .animsettings scripts to already be registered as IMPORTED at this point.
					// Unfortunately, the registration process is dependent on resource cache system, which is unreliable.
					// The easiest way out of is to trigger recompilation of this source to rebuild cache (it needs to be done at some point anyway).
					entry->content.importState = AnimationContent::IMPORTED;
					m_system->animationCompressionManager->QueueAnimationCompression(animationPath);
					SignalForceRecompile(animationPath);
				}
			}

			int pakState = GetAnimationPakState(animationPath.c_str());
			if (pakColumnValues)
				pakColumnValues->push_back(std::make_pair(ExplorerEntryId(m_subtree, entry->id), pakState));
			else
				m_system->explorerData->SetEntryColumn(ExplorerEntryId(m_subtree, entry->id), m_explorerColumnPak, pakState, true);
			m_system->explorerData->SetEntryColumn(ExplorerEntryId(m_subtree, entry->id), m_explorerColumnNew, IsNewAnimation(entry->id), true);
		}
	}

	for (UnusedAnimations::iterator it = unusedAnimations.begin(); it != unusedAnimations.end(); ++it)
	{
		if (unsigned int id = m_animations.RemoveByPath(it->c_str()))
			SignalEntryRemoved(m_subtree, id);
	}
}

string      GetPath(const char* path);
const char* GetFilename(const char* path);

bool        AnimationList::LoadOrGetChangedEntry(unsigned int id)
{
	if (SEntry<AnimationContent>* entry = m_animations.GetById(id))
	{
		if (entry->content.importState == AnimationContent::NEW_ANIMATION)
		{
			if (UpdateImportEntry(entry))
			{
				EntryModifiedEvent ev;
				ev.subtree = m_subtree;
				ev.id = entry->id;
				SignalEntryModified(ev);
			}
		}
		else if (!entry->loaded)
		{
			EntryModifiedEvent ev;
			entry->dataLostDuringLoading = !LoadAnimationEntry(entry, m_system->compressionSkeletonList, m_animationSet, m_defaultSkeletonAlias, m_animEventsFilename.c_str());
			entry->StoreSavedContent();
			entry->lastContent = entry->savedContent;
		}
		entry->loaded = true;
	}
	return true;
}

SEntry<AnimationContent>* AnimationList::GetEntry(unsigned int id) const
{
	return m_animations.GetById(id);
}

bool AnimationList::IsNewAnimation(unsigned int id) const
{
	if (SEntry<AnimationContent>* entry = m_animations.GetById(id))
	{
		return entry->content.importState == AnimationContent::NEW_ANIMATION;
	}
	return false;
}

SEntry<AnimationContent>* AnimationList::FindEntryByPath(const char* animationPath)
{
	return m_animations.GetByPath(animationPath);
}

unsigned int AnimationList::FindIdByAlias(const char* animationName)
{
	AliasToId::iterator it = m_aliasToId.find(animationName);
	if (it == m_aliasToId.end())
		return 0;
	return it->second;
}

bool AnimationList::ResaveAnimSettings(const char* filePath)
{
	SEntry<AnimationContent> fakeEntry;
	fakeEntry.path = PathUtil::ReplaceExtension(filePath, "caf");
	fakeEntry.name = PathUtil::GetFileName(fakeEntry.path.c_str());

	if (!LoadAnimationEntry(&fakeEntry, m_system->compressionSkeletonList, m_animationSet, m_defaultSkeletonAlias, m_animEventsFilename.c_str()))
		return false;

	{
		char buffer[ICryPak::g_nMaxPath] = "";
		const char* realPath = gEnv->pCryPak->AdjustFileName(filePath, buffer, 0);
		DWORD attribs = GetFileAttributesA(realPath);
		if (attribs != INVALID_FILE_ATTRIBUTES && (attribs & FILE_ATTRIBUTE_READONLY) != 0)
			SetFileAttributesA(realPath, FILE_ATTRIBUTE_NORMAL);
	}

	if (!fakeEntry.content.settings.Save(filePath))
		return false;

	return true;
}

void AnimationList::CheckIfModified(unsigned int id, const char* reason, bool continuousChange)
{
	if (SEntry<AnimationContent>* entry = m_animations.GetById(id))
	{
		EntryModifiedEvent ev;
		ev.continuousChange = continuousChange;
		if (continuousChange || entry->Changed(&ev.previousContent))
		{
			ev.subtree = m_subtree;
			ev.id = entry->id;
			if (reason)
			{
				ev.reason = reason;
				ev.contentChanged = true;
			}
			if (!continuousChange)
			{
				bool gotAudioEvents = entry->content.HasAudioEvents();
				m_system->explorerData->SetEntryColumn(ExplorerEntryId(m_subtree, id), m_explorerColumnAudio,
				                                       gotAudioEvents, false);
			}
			SignalEntryModified(ev);
		}
	}
}

void AnimationList::OnAnimationSetAddAnimation(const char* animationPath, const char* name)
{
	// When the animationset is reloading, we can safely ignore the AddAnimation events, as we will
	// get a Reloaded event later which we use to re-create all the entries anyway.
	if (m_bReloading)
		return;
	if (IsInternalAnimationName(name, animationPath))
		return;
	AnimationContent::Type type = AnimationContent::ANIMATION;
	int id = m_animationSet->GetAnimIDByName(name);
	if (id < 0)
		return;
	int flags = m_animationSet->GetAnimationFlags(id);
	if (flags & CA_ASSET_LMG)
	{
		if (m_animationSet->IsCombinedBlendSpace(id))
			type = AnimationContent::COMBINED_BLEND_SPACE;
		else
			type = AnimationContent::BLEND_SPACE;
	}

	bool newEntry;
	bool modified = false;
	SEntry<AnimationContent>* entry = m_animations.AddEntry(&newEntry, animationPath, name, false);
	entry->content.loadedInEngine = true;
	if (entry->content.importState == AnimationContent::WAITING_FOR_CHRPARAMS_RELOAD)
		modified = true;
	entry->content.importState = AnimationContent::IMPORTED;

	m_aliasToId[name] = entry->id;

	UpdateAnimationSizesTask* task = new UpdateAnimationSizesTask(ExplorerEntryId(m_subtree, id), animationPath, m_explorerColumnSize, m_system, m_subtree);
	GetIEditor()->GetBackgroundTaskManager()->AddTask(task, eTaskPriority_BackgroundScan, eTaskThreadMask_IO);

	EntryModifiedEvent ev;
	if (entry->Reverted(&ev.previousContent))
		modified = true;
	if (newEntry)
		SignalEntryAdded(m_subtree, entry->id);
	else if (modified)
	{
		ev.subtree = m_subtree;
		ev.id = entry->id;
		ev.reason = "Reload";
		ev.contentChanged = true;
		SignalEntryModified(ev);
	}
}

void AnimationList::OnAnimationSetAboutToBeReloaded()
{
	assert(!m_bReloading);
	m_bReloading = true;
}

void AnimationList::OnAnimationSetReloaded()
{
	assert(m_bReloading);
	ReloadAnimationList();
	m_bReloading = false;
}

struct EventSerializer
{
	AnimEvents& events;

	EventSerializer(AnimEvents& events)
		: events(events)
	{
	}

	void Serialize(Serialization::IArchive& ar)
	{
		ar(events, "events");
	}
};

bool PatchAnimEvents(ActionOutput* output, const char* animEventsFilename, const char* animationPath, const AnimEvents& events)
{
	XmlNodeRef root = GetIEditor()->GetSystem()->LoadXmlFromFile(animEventsFilename);
	if (!root)
	{
		if (output)
			output->AddError("Failed to load existing animevents database", animEventsFilename);
		return false;
	}

	XmlNodeRef animationNode;

	int nodeCount = root->getChildCount();
	for (int i = 0; i < nodeCount; ++i)
	{
		XmlNodeRef node = root->getChild(i);
		if (!node->isTag("animation"))
			continue;

		if (stricmp(node->getAttr("name"), animationPath) == 0)
		{
			animationNode = node;
			break;
		}
	}

	if (!animationNode)
	{
		animationNode = root->newChild("animation");
		animationNode->setAttr("name", animationPath);
	}

	AnimEvents existingEvents;
	{
		for (int i = 0; i < animationNode->getChildCount(); ++i)
		{
			XmlNodeRef eventNode = animationNode->getChild(i);

			AnimEvent ev;
			ev.LoadFromXMLNode(eventNode);
			existingEvents.push_back(ev);
		}
	}

	animationNode->removeAllChilds();

	AnimEvents sortedEvents = events;
	std::stable_sort(sortedEvents.begin(), sortedEvents.end());
	for (size_t i = 0; i < sortedEvents.size(); ++i)
	{
		XmlNodeRef eventNode = animationNode->newChild("event");
		CAnimEventData animEventData;
		sortedEvents[i].ToData(&animEventData);
		gEnv->pCharacterManager->GetIAnimEvents()->SaveAnimEventToXml(animEventData, eventNode);
	}

	bool needToSave = false;
	{
		DynArray<char> existingBuffer;
		DynArray<char> newBuffer;
		Serialization::SaveBinaryBuffer(existingBuffer, EventSerializer(existingEvents));
		Serialization::SaveBinaryBuffer(newBuffer, EventSerializer(sortedEvents));

		if (existingBuffer.size() != newBuffer.size())
			needToSave = true;
		else
			needToSave = memcmp(existingBuffer.data(), newBuffer.data(), existingBuffer.size()) != 0;
	}

	if (needToSave)
	{
		char realPath[ICryPak::g_nMaxPath];
		gEnv->pCryPak->AdjustFileName(animEventsFilename, realPath, ICryPak::FLAGS_FOR_WRITING);
		{
			string path;
			string filename;
			PathUtil::Split(realPath, path, filename);
			QDir().mkpath(QString::fromLocal8Bit(path.c_str()));
		}

		if (!root->saveToFile(realPath))
		{
			if (output)
				output->AddError("Failed to save animevents file", realPath);
			return false;
		}
	}
	return true;
}

static void CreateFolderForFile(const char* gameFilename)
{
	char buf[ICryPak::g_nMaxPath];
	gEnv->pCryPak->AdjustFileName(gameFilename, buf, ICryPak::FLAGS_FOR_WRITING);
	QDir().mkpath(QString::fromLocal8Bit(PathUtil::ToUnixPath(PathUtil::GetParentDirectory(buf)).c_str()));
}

bool AnimationList::SaveAnimationEntry(ActionOutput* output, unsigned int id, bool notifyOfChange)
{
	if (SEntry<AnimationContent>* entry = m_animations.GetById(id))
	{
		if (!entry)
			return false;
		string animSettingsFilename = SAnimSettings::GetAnimSettingsFilename(entry->path.c_str());

		bool animEventsSaved = true;

		if (entry->content.importState == AnimationContent::NEW_ANIMATION)
		{
			SAnimSettings settings;
			settings.build.skeletonAlias = entry->content.newAnimationSkeleton;
			if (!settings.Save(animSettingsFilename.c_str()))
				return false;
		}
		else
		{
			std::stable_sort(entry->content.events.begin(), entry->content.events.end());

			if (!m_animEventsFilename.empty())
			{
				if (!PatchAnimEvents(output, m_animEventsFilename.c_str(), entry->path.c_str(), entry->content.events))
					animEventsSaved = false;
			}
			else
			{
				if (output)
					output->AddError("Can't save animevents: Animation Events File is not specified in skeleton parameters (chrparams)", entry->path.c_str());
				animEventsSaved = false;
			}

			if (entry->content.type == AnimationContent::BLEND_SPACE)
			{
				XmlNodeRef root = entry->content.blendSpace.SaveToXml();
				char path[ICryPak::g_nMaxPath] = "";
				gEnv->pCryPak->AdjustFileName(entry->path.c_str(), path, 0);
				if (!root->saveToFile(path))
				{
					if (output)
						output->AddError("Failed to save XML file", path);
					return false;
				}
			}
			else if (entry->content.type == AnimationContent::COMBINED_BLEND_SPACE)
			{
				XmlNodeRef root = entry->content.combinedBlendSpace.SaveToXml();
				char path[ICryPak::g_nMaxPath] = "";
				gEnv->pCryPak->AdjustFileName(entry->path.c_str(), path, 0);
				if (!root->saveToFile(path))
				{
					if (output)
						output->AddError("Failed to save XML file", path);
					return false;
				}
			}
			else if (entry->content.type == AnimationContent::ANIMATION)
			{
				if (entry->content.importState != AnimationContent::COMPILED_BUT_NO_ANIMSETTINGS)
				{
					CreateFolderForFile(animSettingsFilename);
					if (!entry->content.settings.Save(animSettingsFilename.c_str()))
					{
						if (output)
							output->AddError("Failed to save animsettings file", animSettingsFilename.c_str());
						return false;
					}
				}
			}
		}

		entry->Saved();

		if (notifyOfChange)
		{
			EntryModifiedEvent ev;
			ev.subtree = m_subtree;
			ev.id = id;
			SignalEntryModified(ev);
		}

		int pakState = GetAnimationPakState(entry->path.c_str());
		m_system->explorerData->SetEntryColumn(ExplorerEntryId(m_subtree, id), m_explorerColumnPak, pakState, true);
		m_system->explorerData->SetEntryColumn(ExplorerEntryId(m_subtree, id), m_explorerColumnNew, IsNewAnimation(id), true);
		return animEventsSaved;
	}

	return false;
}

bool AnimationList::SaveEntry(ActionOutput* output, unsigned int id)
{
	return SaveAnimationEntry(output, id, true);
}

string AnimationList::GetSaveFilename(unsigned int id)
{
	SEntry<AnimationContent>* entry = GetEntry(id);
	if (!entry)
		return string();

	return PathUtil::ReplaceExtension(entry->path, "animsettings");
}

bool AnimationList::SaveAll(ActionOutput* output)
{
	bool failed = false;
	for (size_t i = 0; i < m_animations.Count(); ++i)
	{
		SEntry<AnimationContent>* entry = m_animations.GetByIndex(i);
		if (entry->modified)
			if (!SaveEntry(output, entry->id))
				failed = true;
	}
	return !failed;
}

void AnimationList::RevertEntry(unsigned int id)
{
	if (SEntry<AnimationContent>* entry = m_animations.GetById(id))
	{
		entry->dataLostDuringLoading = !LoadAnimationEntry(entry, m_system->compressionSkeletonList, m_animationSet, m_defaultSkeletonAlias, m_animEventsFilename.c_str());
		entry->StoreSavedContent();

		EntryModifiedEvent ev;
		if (entry->Reverted(&ev.previousContent))
		{
			ev.subtree = m_subtree;
			ev.id = entry->id;
			ev.contentChanged = ev.previousContent != entry->lastContent;
			ev.reason = "Revert";
			SignalEntryModified(ev);
		}

		int pakState = GetAnimationPakState(entry->path.c_str());
		m_system->explorerData->SetEntryColumn(ExplorerEntryId(m_subtree, id), m_explorerColumnPak, pakState, true);
		m_system->explorerData->SetEntryColumn(ExplorerEntryId(m_subtree, id), m_explorerColumnNew, IsNewAnimation(id), true);
	}
}

bool AnimationList::UpdateImportEntry(SEntry<AnimationContent>* entry)
{
	entry->name = PathUtil::GetFileName(entry->path.c_str());

	if (entry->content.importState == AnimationContent::NOT_SET)
	{
		entry->content.importState = AnimationContent::NEW_ANIMATION;
		entry->content.loadedInEngine = false;
		entry->content.newAnimationSkeleton = m_defaultSkeletonAlias;

		int pakState = GetAnimationPakState(entry->path.c_str());
		m_system->explorerData->SetEntryColumn(ExplorerEntryId(m_subtree, entry->id), m_explorerColumnPak, pakState, true);
		m_system->explorerData->SetEntryColumn(ExplorerEntryId(m_subtree, entry->id), m_explorerColumnNew, true, true);
		return true;
	}

	return false;
}

void AnimationList::UpdateAnimationEntryByPath(const char* filename)
{
	SEntry<AnimationContent>* entry = FindEntryByPath(filename);
	if (entry == 0)
		return;

	UpdateAnimationSizesTask* task = new UpdateAnimationSizesTask(ExplorerEntryId(m_subtree, entry->id), entry->path, m_explorerColumnSize, m_system, m_subtree);
	GetIEditor()->GetBackgroundTaskManager()->AddTask(task, eTaskPriority_BackgroundScan, eTaskThreadMask_IO);
}

static string StripExtension(const char* animationName)
{
	const char* dot = strrchr(animationName, '.');
	if (dot == 0)
		return animationName;
	return string(animationName, dot);
}

bool AnimationList::ImportAnimation(string* errorMessage, unsigned int id)
{
	SEntry<AnimationContent>* importEntry = m_animations.GetById(id);
	if (!importEntry)
	{
		*errorMessage = "Wrong animation entry id";
		return false;
	}

	importEntry->content.settings.build.skeletonAlias = importEntry->content.newAnimationSkeleton;
	importEntry->content.importState = AnimationContent::WAITING_FOR_CHRPARAMS_RELOAD;
	SaveEntry(0, importEntry->id);

	importEntry->name = PathUtil::GetFileName(importEntry->path.c_str());

	UpdateAnimationSizesTask* task = new UpdateAnimationSizesTask(ExplorerEntryId(m_subtree, id), importEntry->path, m_explorerColumnSize, m_system, m_subtree);
	GetIEditor()->GetBackgroundTaskManager()->AddTask(task, eTaskPriority_BackgroundScan, eTaskThreadMask_IO);
	return true;
}

bool AnimationList::IsLoaded(unsigned int id) const
{
	SEntry<AnimationContent>* entry = GetEntry(id);
	if (!entry)
		return false;
	return entry->loaded;
}

void AnimationList::OnFileChange(const char* filename, EChangeType eType)
{
	const string originalExt = PathUtil::GetExt(filename);
	string animationPath;
	const char* pakStateFilename = "";

	if (originalExt == "caf" ||
	    originalExt == "i_caf" ||
	    originalExt == "animsettings")
	{
		string intermediatePath = PathUtil::ReplaceExtension(PathUtil::AbsolutePathToGamePath(filename), "i_caf");
		bool intermediateExists = gEnv->pCryPak->IsFileExist(intermediatePath.c_str(), ICryPak::eFileLocation_OnDisk);

		animationPath = PathUtil::ReplaceExtension(intermediatePath, "caf");
		bool animationExists = gEnv->pCryPak->IsFileExist(animationPath.c_str());

		string animSettingsPath = PathUtil::ReplaceExtension(intermediatePath, "animsettings");
		bool animSettingsExists = gEnv->pCryPak->IsFileExist(animSettingsPath.c_str(), ICryPak::eFileLocation_OnDisk);

		if (animationExists && intermediateExists && animSettingsExists)
		{
			UpdateAnimationEntryByPath(filename);
		}

		if (intermediateExists && !animSettingsExists)
		{
			m_importEntries.push_back(intermediatePath);
			std::sort(m_importEntries.begin(), m_importEntries.end());
			m_importEntries.erase(std::unique(m_importEntries.begin(), m_importEntries.end()), m_importEntries.end());

			if (m_filter.Matches(animationPath.c_str()))
			{
				bool newEntry;
				SEntry<AnimationContent>* entry = m_animations.AddEntry(&newEntry, animationPath, 0, false);
				UpdateImportEntry(entry);

				EntryModifiedEvent ev;
				bool modified = entry->Changed(&ev.previousContent);
				if (newEntry)
					SignalEntryAdded(m_subtree, entry->id);
				else if (modified)
				{
					ev.reason = "Reload";
					ev.subtree = m_subtree;
					ev.id = entry->id;
					ev.contentChanged = true;
					SignalEntryModified(ev);
				}
			}
		}

		if (!intermediateExists)
		{
			if (SEntry<AnimationContent>* entry = m_animations.GetByPath(animationPath.c_str()))
			{
				unsigned int id = entry->id;
				if (entry->content.importState == AnimationContent::NEW_ANIMATION)
				{
					if (m_animations.RemoveById(id))
						SignalEntryRemoved(m_subtree, id);
				}
			}
		}
		pakStateFilename = animationPath.c_str();

		if (originalExt == "animsettings")
		{
			if (SEntry<AnimationContent>* entry = m_animations.GetByPath(animationPath.c_str()))
			{
				if (!entry->modified &&
				    entry->content.importState != AnimationContent::WAITING_FOR_CHRPARAMS_RELOAD)
				{
					unsigned int id = entry->id;
					RevertEntry(id);
				}
			}
		}
	}
	else if (originalExt == "bspace" || originalExt == "comb")
	{
		bool fileExists = gEnv->pCryPak->IsFileExist(filename, ICryPak::eFileLocation_Any);

		if (fileExists)
		{
			ExplorerEntryId entryId;
			SEntry<AnimationContent>* entry = m_animations.GetByPath(filename);

			if (entry)
			{
				entryId = ExplorerEntryId(m_subtree, entry->id);
				LoadAnimationEntry(entry, 0, m_animationSet, 0, m_animEventsFilename.c_str());
				EntryModifiedEvent ev;
				if (entry->Changed(&ev.previousContent))
				{
					ev.subtree = m_subtree;
					ev.id = entry->id;
					ev.reason = "Reload";
					ev.contentChanged = true;
					SignalEntryModified(ev);
				}
			}
		}
		else
		{
			EntryBase* entry = m_animations.GetBaseByPath(filename);
			if (entry && !entry->modified)
			{
				unsigned int id = entry->id;
				if (m_animations.RemoveById(id))
					SignalEntryRemoved(m_subtree, id);
			}
		}
		pakStateFilename = filename;
	}
	else
	{
		pakStateFilename = filename;
	}

	if (EntryBase* entry = m_animations.GetByPath(pakStateFilename))
	{
		int pakState = GetAnimationPakState(pakStateFilename);
		m_system->explorerData->SetEntryColumn(ExplorerEntryId(m_subtree, entry->id), m_explorerColumnPak, pakState, true);
		m_system->explorerData->SetEntryColumn(ExplorerEntryId(m_subtree, entry->id), m_explorerColumnNew, IsNewAnimation(entry->id), true);
	}
}

bool AnimationList::GetEntrySerializer(Serialization::SStruct* out, unsigned int id) const
{
	if (SEntry<AnimationContent>* anim = m_animations.GetById(id))
	{
		*out = Serialization::SStruct(*anim);
		return true;
	}

	return false;
}

static const char* EntryIconByContentType(AnimationContent::Type type, bool isAdditive)
{
	if (isAdditive)
		return "icons:common/animation_additive.ico";
	switch (type)
	{
	case AnimationContent::BLEND_SPACE:
		return "icons:common/animation_blendspace.ico";
	case AnimationContent::COMBINED_BLEND_SPACE:
		return "icons:common/animation_combined.ico";
	case AnimationContent::AIMPOSE:
		return "icons:common/animation_aimpose.ico";
	case AnimationContent::LOOKPOSE:
		return "icons:common/animation_lookpose.ico";
	default:
		return "icons:common/assets_animation.ico";
	}
}

static const char* GetContentTypeName(AnimationContent::Type type)
{
	switch (type)
	{
	case AnimationContent::BLEND_SPACE:
		return "bspace";
	case AnimationContent::COMBINED_BLEND_SPACE:
		return "comb";
	case AnimationContent::AIMPOSE:
		return "aimpose";
	case AnimationContent::LOOKPOSE:
		return "lookpose";
	default:
		return "caf";
	}
}

void AnimationList::UpdateEntry(ExplorerEntry* entry)
{
	if (SEntry<AnimationContent>* anim = m_animations.GetById(entry->id))
	{
		entry->name = anim->name;
		entry->path = anim->path;
		entry->modified = anim->modified;
		entry->icon = EntryIconByContentType(anim->content.type, anim->content.loadedAsAdditive);
		if (anim->content.type == AnimationContent::ANIMATION)
		{
			if (anim->content.importState == AnimationContent::NEW_ANIMATION)
				entry->icon = "icons:common/animation_offline.ico";
		}
	}
}

void AnimationList::ActionImport(ActionContext& x)
{
	for (size_t i = 0; i < x.entries.size(); ++i)
	{
		ExplorerEntry* entry = x.entries[i];
		if (!OwnsAssetEntry(entry))
			return;

		string details;
		if (!ImportAnimation(&details, entry->id))
		{
			details = entry->name + ": " + details;
			x.output->AddError("Unable to import animations", details.c_str());
		}
	}
}

static void CollectExamplePaths(vector<string>* paths, AnimationList* animationList, SEntry<AnimationContent>* entry)
{
	if (entry->content.type == AnimationContent::BLEND_SPACE)
	{
		for (size_t i = 0; i < entry->content.blendSpace.m_examples.size(); ++i)
		{
			const BlendSpaceExample& example = entry->content.blendSpace.m_examples[i];
			unsigned int id = animationList->FindIdByAlias(example.animation.c_str());
			if (!id)
				continue;
			SEntry<AnimationContent>* entry = animationList->GetEntry(id);
			if (entry)
				paths->push_back(entry->path);
		}
	}
	else if (entry->content.type == AnimationContent::COMBINED_BLEND_SPACE)
	{
		for (size_t i = 0; i < entry->content.combinedBlendSpace.m_blendSpaces.size(); ++i)
		{
			SEntry<AnimationContent>* combinedEntry = animationList->FindEntryByPath(entry->content.combinedBlendSpace.m_blendSpaces[i].path.c_str());
			if (!combinedEntry)
				continue;
			animationList->LoadOrGetChangedEntry(combinedEntry->id);
			CollectExamplePaths(&*paths, animationList, combinedEntry);
		}
	}
}

void AnimationList::ActionCopyExamplePaths(ActionContext& x)
{
	std::vector<string> paths;

	for (size_t i = 0; i < x.entries.size(); ++i)
	{
		SEntry<AnimationContent>* entry = m_animations.GetById(x.entries[i]->id);
		if (!entry)
			return;

		CollectExamplePaths(&paths, this, entry);
	}

	QString text;
	for (size_t i = 0; i < paths.size(); ++i)
	{
		text += QString::fromLocal8Bit(paths[i].c_str());
		text += "\n";
	}

	QApplication::clipboard()->setText(text);
}

void AnimationList::GetEntryActions(vector<ExplorerAction>* actions, unsigned int id, ExplorerData* explorerData)
{
	SEntry<AnimationContent>* entry = m_animations.GetById(id);

	bool newAnimation = entry && entry->content.importState == AnimationContent::NEW_ANIMATION;
	bool isBlendSpace = entry && entry->content.type == AnimationContent::BLEND_SPACE;
	bool isCombinedBlendSpace = entry && entry->content.type == AnimationContent::COMBINED_BLEND_SPACE;
	bool isAnimSettingsMissing = entry && entry->content.type == AnimationContent::COMPILED_BUT_NO_ANIMSETTINGS;
	bool isAnm = entry && entry->content.type == AnimationContent::ANM;

	if (isBlendSpace || isCombinedBlendSpace)
	{
		actions->push_back(ExplorerAction("Copy Example Paths", 0, [=](ActionContext& x) { ActionCopyExamplePaths(x); }));
		actions->push_back(ExplorerAction());
	}

	if (newAnimation)
		actions->push_back(ExplorerAction("Import", ACTION_IMPORTANT, [=](ActionContext& x) { ActionImport(x); }, "icons:General/File_Import.ico"));
	else
	{

		actions->push_back(ExplorerAction("Revert", 0, [=](ActionContext& x) { explorerData->ActionRevert(x); }, "icons:General/History_Undo.ico", "Reload file content from the disk, undoing all changes since last save."));
		bool saveEnabled = !isAnm && !isAnimSettingsMissing;
		actions->push_back(ExplorerAction("Save", saveEnabled ? 0 : ACTION_DISABLED, [=](ActionContext& x) { explorerData->ActionSave(x); }, "icons:General/File_Save.ico"));
		if (isBlendSpace)
			actions->push_back(ExplorerAction("Compute and Save VEG", 0, [=](ActionContext& x) { ActionComputeVEG(x); }, "icons:common/animation_force_recompile.ico"));
		if (!isAnm)
		{
			actions->push_back(ExplorerAction());
			actions->push_back(ExplorerAction("Export HTR+I_CAF (Lossy)", 0, [=](ActionContext& x) { ActionExportHTR(x); }));
		}
	}
	actions->push_back(ExplorerAction());
	if (!isBlendSpace && !isCombinedBlendSpace && !isAnm)
		actions->push_back(ExplorerAction("Force Recompile", 0, [=](ActionContext& x) { ActionForceRecompile(x); }, "icons:common/animation_force_recompile.ico"));
	if (!isAnm && !isBlendSpace && !isCombinedBlendSpace)
		actions->push_back(ExplorerAction("Generate Footsteps", ACTION_NOT_STACKABLE, [=](ActionContext& x) { ActionGenerateFootsteps(x); }, "icons:common/animation_footsteps.ico", "Creates AnimEvents based on the animation data."));
	actions->push_back(ExplorerAction());
	actions->push_back(ExplorerAction("Show in Explorer", ACTION_NOT_STACKABLE, [=](ActionContext& x) { explorerData->ActionShowInExplorer(x); }, "icons:General/Show_In_Explorer.ico", "Locates file in Windows Explorer."));
}

int AnimationList::GetEntryCount() const
{
	return m_animations.Count();
}

unsigned int AnimationList::GetEntryIdByIndex(int index) const
{
	if (SEntry<AnimationContent>* entry = m_animations.GetByIndex(index))
		return entry->id;
	return 0;
}

void AnimationList::ActionForceRecompile(ActionContext& x)
{
	if (!m_system->animationCompressionManager)
	{
		CQuestionDialog::SWarning("Warning", "Animation compilation is not possible. Probably 'ca_useIMG_CAF' was not set to zero at load time");
		return;
	}

	for (size_t i = 0; i < x.entries.size(); ++i)
	{
		m_system->animationCompressionManager->QueueAnimationCompression(x.entries[i]->path.c_str());

		SignalForceRecompile(x.entries[i]->path.c_str());
	}
}

void AnimationList::ActionComputeVEG(ActionContext& x)
{
	ExplorerEntry* entry = x.entries.front();

	// Make sure that we don't get save-change notification to reload the
	// BlendSpace. Otherwise virtual examples, that are computed during playback
	// are not going to be ready.
	if (!SaveAnimationEntry(x.output, entry->id, false))
	{
		x.output->AddError("Failed to save blend space.", entry->path.c_str());
		return;
	}

	SEntry<AnimationContent>* animation = GetEntry(entry->id);
	if (!animation)
	{
		x.output->AddError("Missing character entry.", entry->path.c_str());
		return;
	}

	ICharacterInstance* character = m_system->document->CompressedCharacter();
	if (!character || !character->GetISkeletonAnim())
	{
		x.output->AddError("A character should be loaded for VEG computation to work.", "");
		return;
	}

	character->GetISkeletonAnim()->ExportVGrid(entry->name.c_str());
}

void AnimationList::ActionGenerateFootsteps(ActionContext& x)
{
	ExplorerEntry* entry = x.entries.front();

	SEntry<AnimationContent>* animation = GetEntry(entry->id);
	if (!animation)
		return;

	FootstepGenerationParameters parameters;

	QPropertyDialog dialog(0);
	dialog.setSerializer(Serialization::SStruct(parameters));
	dialog.setWindowTitle("Footstep Generator");
	dialog.setWindowStateFilename("CharacterTool/FootstepGenerator.state");
	dialog.setSizeHint(QSize(600, 900));
	dialog.setArchiveContext(m_system->contextList->Tail());
	dialog.setStoreContent(true);

	if (dialog.exec() == QDialog::Accepted)
	{
		string errorMessage;
		if (!GenerateFootsteps(&animation->content, &errorMessage, m_system->document->CompressedCharacter(), animation->name.c_str(), parameters))
		{
			x.output->AddError(errorMessage.c_str(), animation->path.c_str());
		}
		else
		{
			CheckIfModified(entry->id, "Footstep Generation", false);
		}
	}
}

void AnimationList::ActionExportHTR(ActionContext& x)
{
	QString gameFolder = QString::fromLocal8Bit(GetIEditor()->GetSystem()->GetIPak()->GetGameFolder());
	QDir gameFolderDir(QDir::fromNativeSeparators(gameFolder));

	ICharacterInstance* character = m_system->document->CompressedCharacter();
	if (!character)
	{
		x.output->AddError("Character has to be loaded for HTR export to function.", "");
		return;
	}
	IAnimationSet* animationSet = character->GetIAnimationSet();
	if (!animationSet)
	{
		x.output->AddError("Missing animation set", "");
		return;
	}

	for (size_t i = 0; i < x.entries.size(); ++i)
	{
		ExplorerEntry* entry = x.entries[i];

		const char* animationPath = entry->path.c_str();
		const char* animationName = entry->name.c_str();
		string initialPath = gameFolderDir.absoluteFilePath(PathUtil::GetParentDirectory(animationPath).c_str()).toLocal8Bit().data();

		CSystemFileDialog::RunParams runParams;
		runParams.title = tr("Select Export Directory for '%1'").arg(animationName);
		runParams.initialFile = initialPath.c_str();

		QString qExportDirectory = CSystemFileDialog::RunSelectDirectory(runParams, nullptr);
		if (qExportDirectory.isEmpty())
		{
			return;
		}

		string exportDirectory = qExportDirectory.toLocal8Bit().data();
		if (exportDirectory[exportDirectory.size() - 1] != '\\' && exportDirectory[exportDirectory.size() - 1] != '/')
			exportDirectory += "\\";

		int animationId = animationSet->GetAnimIDByName(animationName);
		if (animationId < 0)
		{
			x.output->AddError("Missing animation in animationSet", animationName);
			return;
		}

		SEntry<AnimationContent>* e = GetEntry(entry->id);
		if (!e)
		{
			x.output->AddError("Missing animation entry.", entry->path.c_str());
			return;
		}

		if (e->content.type == AnimationContent::BLEND_SPACE || e->content.type == AnimationContent::COMBINED_BLEND_SPACE)
		{
			if (!gEnv->pCharacterManager->LMG_LoadSynchronously(character->GetIAnimationSet()->GetFilePathCRCByAnimID(animationId), animationSet))
			{
				x.output->AddError("Failed to load blendspace synchronously", animationPath);
				return;
			}
		}
		else
		{
			if (!gEnv->pCharacterManager->CAF_LoadSynchronously(character->GetIAnimationSet()->GetFilePathCRCByAnimID(animationId)))
			{
				x.output->AddError("Failed to load CAF synchronously", animationPath);
				return;
			}
		}

		ISkeletonAnim& skeletonAnimation = *character->GetISkeletonAnim();
		if (!skeletonAnimation.ExportHTRAndICAF(animationName, exportDirectory.c_str()))
		{
			x.output->AddError("Failed to export HTR.", exportDirectory.c_str());
		}
	}
}

// ---------------------------------------------------------------------------

dll_string AnimationAliasSelector(const SResourceSelectorContext& x, const char* previousValue, ICharacterInstance* character)
{
	if (!character)
		return previousValue;

	ListSelectionDialog dialog("CTAnimationAliasSelection", x.parentWidget);
	dialog.setWindowTitle("Animation Alias Selection");
	dialog.setWindowIcon(CryIcon(GetIEditor()->GetResourceSelectorHost()->GetSelector(x.typeName)->GetIconPath()));
	dialog.setModal(true);

	IAnimationSet* animationSet = character->GetIAnimationSet();
	if (!animationSet)
		return previousValue;
	IDefaultSkeleton& skeleton = character->GetIDefaultSkeleton();

	dialog.SetColumnText(0, "Animation Alias");
	dialog.SetColumnText(1, "Asset Type");
	dialog.SetColumnWidth(1, 80);

	int count = animationSet->GetAnimationCount();
	for (int i = 0; i < count; ++i)
	{
		const char* name = animationSet->GetNameByAnimID(i);
		const char* animationPath = animationSet->GetFilePathByID(i);
		if (IsInternalAnimationName(name, animationPath))
			continue;

		AnimationContent::Type type = AnimationContent::ANIMATION;

		int flags = animationSet->GetAnimationFlags(i);
		if (flags & CA_ASSET_LMG)
		{
			if (animationSet->IsCombinedBlendSpace(i))
				type = AnimationContent::COMBINED_BLEND_SPACE;
			else
				type = AnimationContent::BLEND_SPACE;
		}
		else if (flags & CA_AIMPOSE)
		{
			if (animationSet->IsAimPose(i, skeleton))
				type = AnimationContent::AIMPOSE;
			else if (animationSet->IsLookPose(i, skeleton))
				type = AnimationContent::LOOKPOSE;
		}
		bool isAdditive = (flags & CA_ASSET_ADDITIVE) != 0;
		const char* icon = EntryIconByContentType(type, isAdditive);
		const char* typeName = GetContentTypeName(type);

		dialog.AddRow(name, CryIcon(icon));
		dialog.AddRowColumn(typeName);
	}

	return dialog.ChooseItem(previousValue);
}
REGISTER_RESOURCE_SELECTOR("AnimationAlias", AnimationAliasSelector, "icons:common/assets_animation.ico")

}

