// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// #define SOURCE_ASSET_SUPPORT
#ifdef SOURCE_ASSET_SUPPORT

	#include "Serialization.h"
	#include "../Shared/SourceAssetScene.h"
	#include "../Shared/SourceAssetSettings.h"
	#include "Explorer/ExplorerFileList.h" // IEntryLoader

namespace CharacterTool
{
struct CreateAssetManifestTask;
struct SourceAssetContent
{
	enum State
	{
		STATE_EMPTY,
		STATE_LOADING,
		STATE_LOADED
	};

	State                    state;

	SourceAsset::Scene       scene;
	SourceAsset::Settings    settings;
	bool                     changingView;

	CreateAssetManifestTask* loadingTask;

	SourceAssetContent()
		: state(STATE_EMPTY)
		, changingView(false)
		, loadingTask()
	{
	}

	void Reset()
	{
		*this = SourceAssetContent();
	}

	void Serialize(IArchive& ar);

};

struct RCAssetLoader : IEntryLoader
{
	RCAssetLoader(ExplorerFileList* fileList_)
		: fileList(fileList_) {}

	bool Load(EntryBase* entry, const char* filename) override;
	bool Save(EntryBase* entry, const char* filename) override { return false; }

	ExplorerFileList* fileList;
};

}

#endif

