// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include "QPropertyTree/ContextList.h"
#include "AnimationList.h"
#include "AnimationTagList.h"
#include "CharacterDocument.h"
#include "CharacterList.h"
#include "CharacterPhysics.h"
#include "CharacterRig.h"
#include "SkeletonList.h"
#include "EditorCompressionPresetTable.h"
#include "EditorDBATable.h"
#include "ExplorerNavigationProvider.h"
#include "FilterAnimationList.h"
#include "GizmoSink.h"
#include "CharacterToolSystem.h"
#include <CryExtension/CryCreateClassInstance.h>
#include <CryMath/Cry_Camera.h>
#include <CryMath/Cry_Geo.h>
#include <CryMath/Cry_Math.h>
#include <CrySystem/ISystem.h>
#include <CryCore/smartptr.h>
#include "SkeletonContent.h"
#include "SceneContent.h"
#include "SourceAssetContent.h"
#include "AnimationCompressionManager.h"
#include "CharacterGizmoManager.h"
#include "Serialization/Decorators/INavigationProvider.h"

namespace CharacterTool
{

System::System()
	: loaded(false)
	, dbaTable(0)
	, compressionPresetTable(0)
	, compressionSkeletonList(0)
{
}

System::~System()
{
}

void System::Initialize()
{
	scene.reset(new SceneContent());
	document.reset(new CharacterDocument(this));

	filterAnimationList.reset(new FilterAnimationList());
	contextList.reset(new Serialization::CContextList());
	explorerData.reset(new ExplorerData());

	explorerData->SetEntryTypeIcon(ENTRY_GROUP, "icons:General/Folder.ico");

	// Add entry types and providers to explorerData
	characterList.reset(new ExplorerFileList());
	characterList->AddPakColumn();
	characterList->AddEntryType<CharacterContent>(new CDFDependencies())
	.AddFormat("cdf", new CDFLoader(), FORMAT_LIST | FORMAT_LOAD | FORMAT_SAVE)
	;
	characterList->AddEntryType<CharacterContent>()
	.AddFormat("cga", new CGALoader(), FORMAT_LIST | FORMAT_LOAD)
	;
	characterList->SetDataIcon("icons:common/animation_character.ico");

	explorerData->AddProvider(characterList.get(), "Characters");

	skeletonList.reset(new ExplorerFileList());
	skeletonList->AddEntryType<SkeletonContent>(new CHRParamsDependencies())
	.AddFormat("chrparams", new CHRParamsLoader(), FORMAT_MAIN | FORMAT_LIST | FORMAT_SAVE | FORMAT_LOAD)
	.AddFormat("chr", 0, FORMAT_LIST)
	;
	skeletonList->SetDataIcon("icons:common/animation_skeleton.ico");
	explorerData->AddProvider(skeletonList.get(), "Skeletons");

#if 0
	// .rig and .phys formats are temporarily disabled
	physicsList.reset(new ExplorerFileList());
	physicsList->AddEntryType<SCharacterPhysicsContent>()
	.AddFormat("phys", new SJSONLoader())
	;
	physicsList->SetDataIcon("icons:common/animation_physics.ico");
	explorerData->AddProvider(physicsList.get(), "Physics");

	rigList.reset(new ExplorerFileList());
	rigList->AddEntryType<SCharacterRigContent>()
	.AddFormat("rig", new SJSONLoader())
	;
	rigList->SetDataIcon("icons:common/animation_rig.ico");
	explorerData->AddProvider(rigList.get(), "Rigs");
#endif

	compressionGlobalList.reset(new ExplorerFileList());
	compressionPresetTable = &compressionGlobalList->AddSingleFile<EditorCompressionPresetTable>("Animations/CompressionPresets.json", "Compression Presets", new SelfLoader<EditorCompressionPresetTable>())->content;
	dbaTable = &compressionGlobalList->AddSingleFile<EditorDBATable>("Animations/DBATable.json", "DBA Table", new SelfLoader<EditorDBATable>())->content;
	compressionSkeletonList = &compressionGlobalList->AddSingleFile<SkeletonList>("Animations/SkeletonList.xml", "Skeleton List", new SelfLoader<SkeletonList>())->content;

	animationTagList.reset(new AnimationTagList(compressionPresetTable, dbaTable));
	animationList.reset(new AnimationList(this));

	explorerData->AddProvider(animationList.get(), "Animations");
	explorerData->AddProvider(compressionGlobalList.get(), "Compression");

#ifdef SOURCE_ASSET_SUPPORT
	// fbx prototype disabled in mainline
	sourceAssetList.reset(new ExplorerFileList());
	sourceAssetList->AddEntryType<SourceAssetContent>()
	.AddFormat("fbx", new RCAssetLoader(sourceAssetList.get()), FORMAT_LIST | FORMAT_LOAD)
	.AddFormat("import", new JSONLoader(), FORMAT_LOAD | FORMAT_SAVE)
	;
	sourceAssetList->SetDataIcon("Animation/Source_Asset.ico");
	explorerData->AddProvider(sourceAssetList.get(), "Source Assets");
#endif

	explorerData->Populate();

	gizmoSink.reset(new GizmoSink());
	characterSpaceProvider.reset(new CharacterSpaceProvider(document.get()));

	explorerNavigationProvider.reset(CreateExplorerNavigationProvider(this));

	document->ConnectExternalSignals();

	contextList->Update(this);
	contextList->Update(document.get());
	contextList->Update(static_cast<ITagSource*>(animationTagList.get()));
	contextList->Update(compressionSkeletonList);
	contextList->Update(compressionPresetTable);
	contextList->Update(dbaTable);
	contextList->Update(explorerNavigationProvider.get());
	contextList->Update<const FilterAnimationList>(filterAnimationList.get());
	characterGizmoManager.reset(new CharacterGizmoManager(this));
}

void System::LoadGlobalData()
{
	if (loaded)
		return;
	characterList->Populate();
	skeletonList->Populate();
	if (physicsList)
		physicsList->Populate();
	if (rigList)
		rigList->Populate();
	explorerData->Populate();

	filterAnimationList->Populate();
	compressionPresetTable->SetFilterAnimationList(filterAnimationList.get());
	compressionPresetTable->Load();
	dbaTable->SetFilterAnimationList(filterAnimationList.get());
	dbaTable->Load();
	compressionSkeletonList->Load();
	compressionGlobalList->Populate();
	if (sourceAssetList)
		sourceAssetList->Populate();
	loaded = true;
}

void System::Serialize(Serialization::IArchive& ar)
{
	ar(*document, "document");
}

}

