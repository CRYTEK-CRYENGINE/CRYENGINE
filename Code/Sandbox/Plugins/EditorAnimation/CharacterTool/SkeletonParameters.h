// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <vector>
#include "Serialization.h"

class XmlNodeRef;

namespace CharacterTool
{

using std::vector;

struct AnimationFilterWildcard
{
	string renameMask;
	string fileWildcard;

	AnimationFilterWildcard()
		: renameMask("*")
		, fileWildcard("*/*.caf")
	{
	}

	void Serialize(IArchive& ar)
	{
		ar(renameMask, "renameMask", "^");
		ar.doc(
		  "Rename mask: Specifies by which name you want to refer to the animation in the game, sometimes also called the 'animation alias'. (does not change the animation filename).\n"
		  "'*' will be replaced by the filename without extension.\n"
		  "\n"
		  "e.g. '*' to simply use the filename without its extension as an animation alias. The alias for 'walk.caf' would become 'walk'.\n"
		  "e.g. 'cine_*' to prefix the filename (without extension) with 'cine_' in the animation alias. The alias for 'walk.caf' would become 'cine_walk'.");
		if (renameMask.empty())
		{
			ar.warning(renameMask, "Rename mask missing.");
		}
		ar(fileWildcard, "fileWildcard", "^ <- ");
		ar.doc(
		  "File Wildcard: Specify the animation file(s) you want to include in this set, relative to the folder specified above.\n"
		  "'*' wildcards will be matched by any string of characters.\n"
		  "Use '*/' to match all subfolders.\n"
		  "\n"
		  "e.g. 'idle.caf' will include the single file 'idle.caf' in this folder.\n"
		  "e.g. 'locomotion/idle.caf' will include the single file 'idle.caf' in the 'locomotion' subfolder of this folder.\n"
		  "e.g. '*.caf' will include all files with names ending in '.caf' in this folder.\n"
		  "e.g. 'slide_*.caf' will include all .caf files with filenames beginning with 'slide_' in this folder.\n"
		  "e.g. '*/*.caf' will include all files with names ending in '.caf' in this folder and subfolders.");
		if (fileWildcard.empty())
		{
			ar.warning(fileWildcard, "File wildcard missing.");
		}
	}

	bool operator==(const AnimationFilterWildcard& rhs) const
	{
		return renameMask == rhs.renameMask && fileWildcard == rhs.fileWildcard;
	}
	bool operator!=(const AnimationFilterWildcard& rhs) const { return !operator==(rhs); }
};

struct AnimationFilterFolder
{
	string                          path;
	vector<AnimationFilterWildcard> wildcards;

	AnimationFilterFolder()
	{
		wildcards.resize(3);
		wildcards[0].fileWildcard = "*/*.caf";
		wildcards[1].fileWildcard = "*/*.bspace";
		wildcards[2].fileWildcard = "*/*.comb";
	}

	bool operator==(const AnimationFilterFolder& rhs) const
	{
		if (path != rhs.path)
			return false;
		if (wildcards.size() != rhs.wildcards.size())
			return false;
		for (size_t i = 0; i < wildcards.size(); ++i)
			if (wildcards[i] != rhs.wildcards[i])
				return false;
		return true;
	}
	bool operator!=(const AnimationFilterFolder& rhs) const { return !operator==(rhs); }

	void Serialize(IArchive& ar)
	{
		ar(ResourceFolderPath(path), "path", "^");
		ar.doc("The folder that will be used to search for animations. (e.g. 'animations/human/male')");
		if (ar.isEdit() && ar.isOutput())
		{
			if (path.empty())
				ar.warning(path, "Specify a folder above to search for animations. (e.g. 'animations/human/male')");
		}

		ar(wildcards, "wildcards", "^");
		if (ar.isEdit() && ar.isOutput())
		{
			if (wildcards.empty())
				ar.warning(wildcards, "Add wildcards to specify which animation files to include in this folder.");
		}
	}
};

struct AnimationSetFilter
{
	std::vector<AnimationFilterFolder> folders;

	bool                               IsEmpty() const
	{
		return folders.empty();
	}

	void Serialize(IArchive& ar)
	{
		ar(folders, "folders", "^");
	}

	bool Matches(const char* cafPath) const;

	bool operator==(const AnimationSetFilter& rhs) const
	{
		if (folders.size() != rhs.folders.size())
			return false;
		for (size_t i = 0; i < folders.size(); ++i)
			if (folders[i] != rhs.folders[i])
				return false;
		return true;
	}
	bool operator!=(const AnimationSetFilter& rhs) const { return !operator==(rhs); }
};

struct SkeletonParametersInclude
{
	string filename;
};

inline bool Serialize(IArchive& ar, SkeletonParametersInclude& ref, const char* name, const char* label)
{
	const bool returnValue = ar(SkeletonParamsPath(ref.filename), name, label);
	if (ar.isEdit() && ar.isOutput())
	{
		if (ref.filename.empty())
		{
			ar.warning(ref.filename, "Specify a chrparams file to include its animation set filter.");
		}
	}
	return returnValue;
}

struct SkeletonParametersDBA
{
	string filename;
	bool   persistent;

	SkeletonParametersDBA()
		: persistent(false)
	{
	}

	void Serialize(IArchive& ar)
	{
		ar(ResourceFilePath(filename, "Animation Databases (.dba)|*.dba"), "filename", "^");
		ar(persistent, "persistent", "^Persistent");
	}
};

struct SkeletonParameters
{
	vector<XmlNodeRef>                unknownNodes;

	AnimationSetFilter                animationSetFilter;
	vector<SkeletonParametersInclude> includes;
	string                            animationEventDatabase;
	string                            faceLibFile;

	string                            dbaPath;
	vector<SkeletonParametersDBA>     individualDBAs;

	bool       LoadFromXMLFile(const char* filename, bool* dataLost = 0);
	bool       LoadFromXML(XmlNodeRef xml, bool* dataLost = 0);
	XmlNodeRef SaveToXML();
	bool       SaveToMemory(vector<char>* buffer);

	void       Serialize(Serialization::IArchive& ar);
};

}

