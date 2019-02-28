// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QtCore/QObject>

#include <memory>
#include <vector>
#include <map>
#include "../Shared/AnimSettings.h"
#include <CrySerialization/Forward.h>
#include <CrySystem/File/IFileChangeMonitor.h>
#include <CryAnimation/ICryAnimation.h>
#include "Explorer/ExplorerDataProvider.h"
#include "Explorer/EntryList.h"
#include "AnimationContent.h"
#include "SkeletonParameters.h"

struct IAnimationSet;
struct ICharacterInstance;

namespace Explorer
{
struct ActionOutput;
struct ActionContext;
}

namespace CharacterTool
{
using namespace Explorer;

struct System;

class AnimationList : public IExplorerEntryProvider, public IFileChangeListener, public IAnimationSetListener
{
	Q_OBJECT
public:
	AnimationList(System* system);
	~AnimationList();

	void                      Populate(ICharacterInstance* character, const char* defaultSkeletonAlias, const AnimationSetFilter& filter, const char* animEventsFilename);
	void                      SetAnimationFilterAndScan(const AnimationSetFilter& filter);
	void                      SetAnimationEventsFilePath(const string& animationEventsFilePath);

	void                      RemoveImportEntry(const char* animationPath);

	bool                      IsLoaded(unsigned int id) const;
	bool                      ImportAnimation(string* errorMessage, unsigned int id);
	SEntry<AnimationContent>* GetEntry(unsigned int id) const;
	bool                      IsNewAnimation(unsigned int id) const;
	SEntry<AnimationContent>* FindEntryByPath(const char* animationPath);
	unsigned int              FindIdByAlias(const char* animationName);
	bool                      ResaveAnimSettings(const char* filePath);

	bool                      SaveAnimationEntry(ActionOutput* output, unsigned int id, bool notifyOfChange);

	// IExplorerDataProvider:
	int          GetEntryCount() const override;
	unsigned int GetEntryIdByIndex(int index) const override;
	bool         GetEntrySerializer(Serialization::SStruct* out, unsigned int id) const override;
	void         UpdateEntry(ExplorerEntry* entry) override;
	void         RevertEntry(unsigned int id) override;
	bool         SaveEntry(ActionOutput* output, unsigned int id) override;
	string       GetSaveFilename(unsigned int id) override;
	bool         SaveAll(ActionOutput* output) override;
	void         CheckIfModified(unsigned int id, const char* reason, bool continuousChange) override;
	void         GetEntryActions(vector<ExplorerAction>* actions, unsigned int id, ExplorerData* explorerData) override;
	bool         LoadOrGetChangedEntry(unsigned int id) override;
	void         SetExplorerData(ExplorerData* explorerData, int subtree) override;
	// ^^^

signals:
	void         SignalForceRecompile(const char* filepath);
private:
	void         ActionImport(ActionContext& x);
	void         ActionForceRecompile(ActionContext& x);
	void         ActionGenerateFootsteps(ActionContext& x);
	void         ActionExportHTR(ActionContext& x);
	void         ActionComputeVEG(ActionContext& x);
	void         ActionCopyExamplePaths(ActionContext& x);

	unsigned int MakeNextId();
	void         ReloadAnimationList();

	bool         UpdateImportEntry(SEntry<AnimationContent>* entry);
	void         UpdateAnimationEntryByPath(const char* filename);
	void         ScanForImportEntries(std::vector<std::pair<ExplorerEntryId, int>>* pakColumnValues, bool resetFollows);
	void         SetEntryState(ExplorerEntry* entry, const vector<char>& state);
	void         GetEntryState(vector<char>* state, ExplorerEntry* entry);

	//////////////////////////////////////////////////////////
	// IAnimationSetListener implementation
	virtual void OnAnimationSetAddAnimation(const char* animationPath, const char* animationName) override;
	virtual void OnAnimationSetAboutToBeReloaded() override;
	virtual void OnAnimationSetReloaded() override;
	//////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////
	// IFileChangeListener implementation
	virtual void OnFileChange(const char* filename, EChangeType eType) override;
	//////////////////////////////////////////////////////////

	System*                      m_system;
	IAnimationSet*               m_animationSet;
	ICharacterInstance*          m_character;
	string                       m_defaultSkeletonAlias;
	string                       m_animEventsFilename;

	CEntryList<AnimationContent> m_animations;
	bool                         m_importEntriesInitialized;
	std::vector<string>          m_importEntries;
	typedef std::map<string, unsigned int, stl::less_stricmp<string>> AliasToId;
	AliasToId                    m_aliasToId;

	AnimationSetFilter           m_filter;

	int                          m_explorerColumnFrames;
	int                          m_explorerColumnAudio;
	int                          m_explorerColumnSize;
	int                          m_explorerColumnPak;
	int                          m_explorerColumnNew;

	string                       m_commonPrefix;

	bool                         m_bReloading;
};

}

