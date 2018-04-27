// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "SkeletonParameters.h"

namespace Explorer
{
class ExplorerFileList;
}

namespace CharacterTool
{
struct System;
using namespace Explorer;

struct System;

struct SkeletonContent
{
	SkeletonParameters skeletonParameters;

	AnimationSetFilter includedAnimationSetFilter;
	string             includedAnimationEventDatabase;

	void               Reset()
	{
		*this = SkeletonContent();
	}

	void UpdateIncludedAnimationSet(ExplorerFileList* skeletonList);
	void ComposeCompleteAnimationSetFilter(AnimationSetFilter* outFilter, string* outAnimEventDatabase, ExplorerFileList* skeletonList) const;

	void GetDependencies(vector<string>* paths) const;
	void Serialize(Serialization::IArchive& ar);

private:
	void   OnNewAnimEvents(System& system, const string& skeletonPath);

	string GetSuggestedAnimationEventsFilePath(System& system) const;
	string AskUserForAnimationEventsFilePath(System& system) const;
};

}

