// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include "Explorer/ExplorerFileList.h"
#include "CharacterDocument.h"
#include "Serialization/Decorators/INavigationProvider.h"
#include "IResourceSelectorHost.h"
#include "CharacterToolSystem.h"
#include "SceneContent.h"
#include "AnimationList.h"
#include <IEditor.h>
#include "FileDialogs/EngineFileDialog.h"

#include "AssetSystem/Asset.h"
#include "AssetSystem/AssetResourceSelector.h"

namespace CharacterTool
{

class ExplorerNavigationProvider : public Serialization::INavigationProvider
{
public:
	ExplorerNavigationProvider(System* system)
		: m_system(system)
	{}

	bool IsRegistered(const char* typeName) const override
	{
		return ReferenceTypeToProvider(typeName) != 0;
	}

	ExplorerEntry* FindEntry(const char* type, const char* path) const
	{
		ExplorerEntry* entry = 0;
		IExplorerEntryProvider* provider = ReferenceTypeToProvider(type);
		if (strcmp(type, "AnimationAlias") == 0)
		{
			unsigned int entryId = m_system->animationList->FindIdByAlias(path);
			if (entryId)
				entry = m_system->explorerData->FindEntryById(&*m_system->animationList, entryId);
		}
		else
		{
			entry = m_system->explorerData->FindEntryByPath(provider, path);
		}

		if (!entry && provider)
		{
			string canonicalPath = provider->GetCanonicalPath(path);
			if (!canonicalPath.empty())
				entry = m_system->explorerData->FindEntryByPath(provider, canonicalPath.c_str());
		}
		return entry;
	}

	bool CanSelect(const char* type, const char* path, int index) const override
	{
		if (IsRegistered(type))
		{
			if (index >= 0)
				return true;
		}
		return FindEntry(type, path) != 0;
	}

	IExplorerEntryProvider* ReferenceTypeToProvider(const char* type) const
	{
		if (strcmp(type, "Character") == 0) return m_system->characterList.get();
		if (strcmp(type, "Animation") == 0) return m_system->animationList.get();
		if (strcmp(type, "AnimationOrBSpace") == 0) return m_system->animationList.get();
		if (strcmp(type, "AnimationAlias") == 0) return m_system->animationList.get();
		if (strcmp(type, "Skeleton") == 0) return m_system->skeletonList.get();
		if (strcmp(type, "SkeletonOrCga") == 0) return m_system->skeletonList.get();
		if (strcmp(type, "SkeletonParams") == 0) return m_system->skeletonList.get();
		if (strcmp(type, "CharacterPhysics") == 0) return m_system->physicsList.get();
		if (strcmp(type, "CharacterRig") == 0) return m_system->rigList.get();
		return 0;
	}

	const char* GetIcon(const char* type, const char* path) const override
	{
		ExplorerEntry* entry = path && path[0] ? FindEntry(type, path) : 0;
		return m_system->explorerData->IconForEntry(entry);
	}

	static const char* GetMaskForType(const char* type)
	{
		return strcmp(type, "Character") == 0 ? "Character Definition (cdf)|*.cdf" :
		       strcmp(type, "Animation") == 0 ? "Animation, BlendSpace (caf, i_caf, bspace, comb)|*.caf;*.i_caf;*.bspace;*.comb" :
			   strcmp(type, "AnimationOrBSpace") == 0 ? "Animation, BlendSpace (caf, i_caf, bspace, comb)|*.caf;*.i_caf;*.bspace;*.comb" :
		       strcmp(type, "Skeleton") == 0 ? "Skeleton (skel, chr, cga)|*.skel;*.chr;*.cga" :
			   strcmp(type, "SkeletonOrCga") == 0 ? "Skeleton (skel, chr, cga)|*.skel;*.chr;*.cga" :
		       strcmp(type, "SkeletonParams") == 0 ? "Skeleton Parameters (chrparams)|*.chrparams" :
		       strcmp(type, "CharacterPhysics") == 0 ? "Character Physics (phys)|*.phys" :
		       strcmp(type, "CharacterRig") == 0 ? "Character Rig (rig)|*.rig" :
		       "";
	}

	static CExtensionFilter GetExtensionFilterForType(const char* type)
	{
		return strcmp(type, "Character") == 0 ? CExtensionFilter("Character Definition (cdf)", "cdf") :
			   strcmp(type, "Animation") == 0 ? CExtensionFilter("Animation, BlendSpace (caf, i_caf, bspace, comb)", QStringList() << "caf" << "i_caf" << "bspace" << "comb") :
		       strcmp(type, "AnimationOrBSpace") == 0 ? CExtensionFilter("Animation, BlendSpace (caf, i_caf, bspace, comb)", QStringList() << "caf" << "i_caf" << "bspace" << "comb") :
		       strcmp(type, "Skeleton") == 0 ? CExtensionFilter("Skeleton (skel, chr, cga)", "skel", "chr", "cga") :
			   strcmp(type, "SkeletonOrCga") == 0 ? CExtensionFilter("Skeleton (skel, chr, cga)", "skel", "chr", "cga") :
		       strcmp(type, "SkeletonParams") == 0 ? CExtensionFilter("Skeleton Parameters (chrparams)", "chrparams") :
		       strcmp(type, "CharacterPhysics") == 0 ? CExtensionFilter("Character Physics (phys)", "phys") :
		       strcmp(type, "CharacterRig") == 0 ? CExtensionFilter("Character Rig (rig)", "rig") :
		       CExtensionFilter();
	}

	static std::vector<string> GetAssetTypesForType(const char* type)
	{
		typedef std::vector<string> v;
		return strcmp(type, "Character") == 0 ? v{ "Character" } :
			strcmp(type, "AnimationOrBSpace") == 0 ? v{ "Animation" } :
			strcmp(type, "Animation") == 0 ? v{ "Animation" } :
			strcmp(type, "Skeleton") == 0 ? v{ "Skeleton" } :
			strcmp(type, "SkeletonOrCga") == 0 ? v{ "Skeleton", "AnimatedMesh" } :
			v();
	}

	const char* GetFileSelectorMaskForType(const char* type) const override { return GetMaskForType(type); }

	bool IsActive(const char* type, const char* path, int index) const override
	{
		if ((strcmp(type, "Animation") == 0 || strcmp(type, "AnimationOrBSpace") == 0) && index != -1)
			return m_system->scene->layers.activeLayer == index;
		return false;
	}

	bool IsSelected(const char* type, const char* path, int index) const override
	{
		if (ExplorerEntry* entry = FindEntry(type, path))
			if (m_system->document->IsExplorerEntrySelected(entry))
				return true;
		if (m_system->scene->layers.activeLayer == index && index != -1)
			if (strcmp(type, "Animation") == 0 || strcmp(type, "AnimationOrBSpace") == 0)
				if (!m_system->document->HasSelectedExplorerEntries())
					return true;
		return false;
	}

	bool IsModified(const char* type, const char* path, int index) const override
	{
		ExplorerEntry* entry = m_system->explorerData->FindEntryByPath(ReferenceTypeToProvider(type), path);
		if (!entry)
			return false;
		return entry->modified;
	}

	bool Select(const char* type, const char* path, int index) const override
	{
		bool selected = false;
		if (strcmp(type, "Animation") == 0 || strcmp(type, "AnimationOrBSpace") == 0)
		{
			if (index != -1)
				m_system->scene->layers.activeLayer = index;
			m_system->scene->LayerActivated();
			m_system->scene->SignalChanged(false);
			selected = true;
		}

		ExplorerEntry* entry = FindEntry(type, path);
		if (!entry)
		{
			m_system->document->SetSelectedExplorerEntries(ExplorerEntries(), 0);
			return false;
		}

		if (!selected)
		{
			ExplorerEntries entries(1, entry);
			m_system->document->SetSelectedExplorerEntries(entries, 0);
		}
		return true;
	}

	bool CanPickFile(const char* type, int index) const override
	{
		if (strcmp(type, "Animation") == 0 && index >= 0)
			return false;
		if (strcmp(type, "AnimationOrBSpace") == 0 && index >= 0)
			return false;
		if (strcmp(type, "AnimationAlias") == 0 && index >= 0)
			return false;
		return true;
	}

	bool CanCreate(const char* type, int index) const override
	{
		return strcmp(type, "CharacterPhysics") == 0 || strcmp(type, "CharacterRig") == 0;
	}

	bool Create(const char* type, const char* path, int index) const override
	{
		if (strcmp(type, "CharacterPhysics") == 0)
			return m_system->physicsList->AddAndSaveEntry(path);
		else if (strcmp(type, "CharacterRig") == 0)
			return m_system->rigList->AddAndSaveEntry(path);
		return false;
	}

private:
	System* m_system;
};

Serialization::INavigationProvider* CreateExplorerNavigationProvider(System* system)
{
	return new ExplorerNavigationProvider(system);
}

dll_string FileSelector(const SResourceSelectorContext& x, const char* previousValue)
{
	CEngineFileDialog::RunParams runParams;
	runParams.initialFile = previousValue;
	runParams.extensionFilters << ExplorerNavigationProvider::GetExtensionFilterForType(x.typeName);
	auto qFilename = CEngineFileDialog::RunGameOpen(runParams, nullptr);
	string filename = qFilename.toLocal8Bit().data();

	if (!filename.empty())
		return filename.c_str();
	else
		return previousValue;
}

static dll_string AnimationResourceSelector(const SResourceSelectorContext& x, const char* previousValue)
{
	const auto assetTypes = ExplorerNavigationProvider::GetAssetTypesForType(x.typeName);
	auto assetPickersEnabled = (EAssetResourcePickerState)GetIEditor()->GetSystem()->GetIConsole()->GetCVar("ed_enableAssetPickers")->GetIVal();
	if (assetPickersEnabled != EAssetResourcePickerState::Disable && !assetTypes.empty())
	{
		return SStaticAssetSelectorEntry::SelectFromAsset(x, assetTypes, previousValue);
	}
	else
	{
		return FileSelector(x, previousValue);
	}
}

REGISTER_RESOURCE_SELECTOR("AnimationOrBSpace", AnimationResourceSelector, "icons:common/assets_animation.ico")
REGISTER_RESOURCE_SELECTOR("SkeletonOrCga", AnimationResourceSelector, "icons:common/animation_skeleton.ico")
REGISTER_RESOURCE_SELECTOR("SkeletonParams", AnimationResourceSelector, "icons:common/animation_skeleton.ico")
REGISTER_RESOURCE_SELECTOR("CharacterRig", AnimationResourceSelector, "icons:common/animation_rig.ico")
REGISTER_RESOURCE_SELECTOR("CharacterPhysics", AnimationResourceSelector, "icons:common/animation_physics.ico")

}

