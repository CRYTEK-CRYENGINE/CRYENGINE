// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CharacterToolSystem.h"
#include "Explorer/EntryList.h"
#include "Explorer/ExplorerFileList.h"
#include "SkeletonContent.h"
#include "SkeletonList.h"
#include "Serialization/Decorators/EditorActionButton.h"
#include "FileDialogs/EngineFileDialog.h"
#include "Controls/QuestionDialog.h"
#include <QDir>

namespace CharacterTool
{

void SkeletonContent::Serialize(IArchive& ar)
{
	ar(skeletonParameters.includes, "includes", "+Includes");
	ar.doc("Used to include the animation set from other chrparams files.\nYou can use this to reuse (include) the same animation set on different skeletons.");
	if (ar.isEdit() && ar.isOutput())
	{
		System* system = ar.context<System>();
		UpdateIncludedAnimationSet(system->skeletonList.get());
		if (!includedAnimationSetFilter.IsEmpty())
		{
			ar(includedAnimationSetFilter, "includedAnimationSetFilter", "+!Included Animation Set Filter (read only)");
			ar.doc("Preview of the animation set filters included from other chrparams files.");
		}
	}

	ar(skeletonParameters.animationSetFilter, "animationSetFilter", "+[+]Animation Set Filter");
	ar.doc("Specifies a set of animations and how to name them in the game.");
	if (ar.isEdit() && ar.isOutput())
	{
		if (includedAnimationSetFilter.IsEmpty())
		{
			if (skeletonParameters.animationSetFilter.IsEmpty())
			{
				ar.warning(skeletonParameters.animationSetFilter, "Add children to the animation set filter (or include a filter) to tell the skeleton where to find its animations.");
			}
		}
	}

	ar(ResourceFilePath(skeletonParameters.animationEventDatabase, "Animation Events File (.animevents)|*.animevents"), "animationEventDatabase", "<Animation Events File");
	ar.doc("Path to the Animation Events File. The specified file must exist if you want to add animation events.\nTypically called events.animevents, and stored alongside the skeletons animations.");

	if (ar.isEdit() && ar.isOutput())
	{
		if (!includedAnimationEventDatabase.empty())
		{
			ar(includedAnimationEventDatabase, "includedAnimEventDatabase", "Included Animation Events File (read only)");
			ar.doc("Preview of the Animation Events File included from other chrparams files");

			if (!skeletonParameters.animationEventDatabase.empty())
			{
				ar.warning(includedAnimationEventDatabase, "Included Animation Events File is ignored as we specified an Animation Events File in this skeleton as well.");
			}
		}
	}

	if (ar.isEdit() && ar.isOutput())
	{
		const string& finalAnimationEventDatabase = skeletonParameters.animationEventDatabase.empty() ? includedAnimationEventDatabase : skeletonParameters.animationEventDatabase;

		bool bShowCreateAnimEventsButton = false;
		if (finalAnimationEventDatabase.empty())
		{
			ar.warning(skeletonParameters.animationEventDatabase, "Animation Events File is not specified.\nEither include a chrparams file that has one specified, or create a new one.");
			bShowCreateAnimEventsButton = true;
		}
		else
		{
			CCryFile xmlFile;
			if (!xmlFile.Open(finalAnimationEventDatabase.c_str(), "rb"))
			{
				const string warningMessage = "Animation Events File '" + finalAnimationEventDatabase + "' cannot be opened. Maybe it doesn't exist?";
				ar.warning(skeletonParameters.animationEventDatabase, warningMessage);
				bShowCreateAnimEventsButton = true;
			}
		}

		if (bShowCreateAnimEventsButton)
		{
			auto pEntry = ar.context<EntryBase>();
			if (pEntry)
			{
				const string path = pEntry->path;
				auto pSystem = ar.context<System>();
				ar(Serialization::ActionButton(
				     [this, pSystem, path]
					{
						assert(pSystem);
						OnNewAnimEvents(*pSystem, path);
				  }
				     ), "newAnimationEventsFile", "Create New Animation Events File...");
			}
		}
	}

	ar(ResourceFolderPath(skeletonParameters.dbaPath, "Animations"), "dbaPath", "<DBA Path");
	ar(skeletonParameters.individualDBAs, "individualDBAs", "Individual DBAs");
}

// Returns a suggestion for the animation events file path, relative to the working folder
string SkeletonContent::GetSuggestedAnimationEventsFilePath(System& system) const
{
	// By default it's the gameroot folder
	string suggestedFolder;

	// Take the first folder we find in the animation set filter
	auto completeAnimationSetFilter = AnimationSetFilter();
	auto dummy = string();
	ComposeCompleteAnimationSetFilter(&completeAnimationSetFilter, &dummy, system.skeletonList.get());

	if (!completeAnimationSetFilter.IsEmpty())
	{
		for (const auto& folder : completeAnimationSetFilter.folders)
		{
			if (!folder.path.empty())
			{
				suggestedFolder = folder.path; // assumes folder.path is a gameroot-relative path
			}
		}
	}

	return PathUtil::AddSlash(suggestedFolder) + "events.animevents";
}

// Return animation events file path relative to the game folder (or "" on failure)
string SkeletonContent::AskUserForAnimationEventsFilePath(System& system) const
{
	const string suggestedRelativePath = GetSuggestedAnimationEventsFilePath(system);

	CEngineFileDialog::RunParams runParams;
	runParams.title = QStringLiteral("Create Animation Events File...");
	runParams.buttonLabel = QStringLiteral("Create");
	runParams.initialDir = suggestedRelativePath;
	runParams.extensionFilters << CExtensionFilter("Animation Events File", "animevents");
	auto qFilename = CEngineFileDialog::RunGameSave(runParams, nullptr);
	return qFilename.toLocal8Bit().data();
}

void SkeletonContent::OnNewAnimEvents(System& system, const string& skeletonPath)
{
	const string relativePath = AskUserForAnimationEventsFilePath(system);
	if (relativePath.empty())
	{
		return;
	}

	char realPath[ICryPak::g_nMaxPath];
	gEnv->pCryPak->AdjustFileName(relativePath.c_str(), realPath, ICryPak::FLAGS_FOR_WRITING);

	// Create folders on disk if needed
	{
		string path;
		string filename;
		PathUtil::Split(realPath, path, filename);
		QDir().mkpath(QString::fromLocal8Bit(path.c_str()));
	}

	// Create the new animation events file on disk
	{
		XmlNodeRef root = gEnv->pSystem->CreateXmlNode("anim_event_list");
		if (!root->saveToFile(realPath))
		{
			CQuestionDialog::SCritical("Error", "Failed to save Animation Events File");
			return;
		}
	}

	// Update the property in the skeletonParameters
	{
		skeletonParameters.animationEventDatabase = relativePath;

		// Tell the system we (potentially) modified a property
		auto pEntry = system.skeletonList->GetEntryByPath<SkeletonContent>(skeletonPath.c_str());
		if (pEntry)
		{
			system.skeletonList->CheckIfModified(pEntry->id, 0, false);
		}
	}
}

void SkeletonContent::GetDependencies(vector<string>* deps) const
{
	for (size_t i = 0; i < skeletonParameters.includes.size(); ++i)
		deps->push_back(skeletonParameters.includes[i].filename);
}

static void ExpandIncludes(AnimationSetFilter* outFilter, string* outAnimEventDatabase, const std::vector<string>& includeStack, const vector<SkeletonParametersInclude>& includes, ExplorerFileList* skeletonList)
{
	std::vector<string> stack = includeStack;
	std::vector<AnimationFilterFolder> includedFolders;
	for (size_t i = 0; i < includes.size(); ++i)
	{
		const string& originalFilename = includes[i].filename;
		string filename = originalFilename;
		filename.replace('\\', '/');

		if (stl::find(includeStack, filename))
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Recursive inclusion of chrparams: '%s'", includes[i].filename.c_str());
			continue;
		}

		SEntry<SkeletonContent>* entry = skeletonList->GetEntryByPath<SkeletonContent>(filename.c_str());
		if (entry)
		{
			skeletonList->LoadOrGetChangedEntry(entry->id);

			includedFolders.insert(includedFolders.end(),
			                       entry->content.skeletonParameters.animationSetFilter.folders.begin(),
			                       entry->content.skeletonParameters.animationSetFilter.folders.end());

			// the first animEventDatabase we encounter in our depth first search 'wins'
			if (outAnimEventDatabase->empty())
			{
				*outAnimEventDatabase = entry->content.skeletonParameters.animationEventDatabase;
			}

			AnimationSetFilter filter;

			stack.push_back(filename);
			ExpandIncludes(&filter, outAnimEventDatabase, stack, entry->content.skeletonParameters.includes, skeletonList);
			stack.pop_back();
			includedFolders.insert(includedFolders.end(),
			                       filter.folders.begin(), filter.folders.end());
		}
	}

	outFilter->folders.insert(outFilter->folders.begin(),
	                          includedFolders.begin(), includedFolders.end());
}

void SkeletonContent::UpdateIncludedAnimationSet(ExplorerFileList* skeletonList)
{
	includedAnimationSetFilter = AnimationSetFilter();
	includedAnimationEventDatabase = string();

	ExpandIncludes(&includedAnimationSetFilter, &includedAnimationEventDatabase, std::vector<string>(), skeletonParameters.includes, skeletonList);
}

void SkeletonContent::ComposeCompleteAnimationSetFilter(AnimationSetFilter* outFilter, string* outAnimEventDatabase, ExplorerFileList* skeletonList) const
{
	*outFilter = skeletonParameters.animationSetFilter;
	*outAnimEventDatabase = skeletonParameters.animationEventDatabase;

	ExpandIncludes(&*outFilter, outAnimEventDatabase, vector<string>(), skeletonParameters.includes, skeletonList);
}

}

