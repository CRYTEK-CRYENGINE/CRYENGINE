// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include <CryCore/Platform/platform.h>
#include "AnimationFilter.h"
#include <CrySystem/File/CryFile.h>
#include <CryString/CryPath.h>

#include <CrySerialization/SmartPtr.h>
#include <CrySerialization/STL.h>
#include <CrySerialization/ClassFactory.h>
#include <CrySerialization/IArchive.h>
using Serialization::IArchive;

// ---------------------------------------------------------------------------

void IAnimationFilterCondition::Serialize(Serialization::IArchive& ar)
{
	if (not)
	{
		if (!ar(not, "not", "^^Not"))
			not = false;
	}
	else if (ar.isEdit() || ar.isInput() || ar.caps(ar.BINARY))
		ar(not, "not", "^^");
}

SERIALIZATION_CLASS_NULL(IAnimationFilterCondition, "Empty Filter")

// ---------------------------------------------------------------------------

struct SAnimationFilterAnd : IAnimationFilterCondition
{
	bool Check(const SAnimationFilterItem& item) const override
	{
		size_t numConditions = conditions.size();
		size_t conditionsChecked = 0;
		for (size_t i = 0; i < numConditions; ++i)
		{
			if (!conditions[i])
				continue;
			if (!conditions[i]->Check(item))
				return not;
			++conditionsChecked;
		}
		return conditionsChecked != 0 ? !not : not;
	}

	void FindTags(std::vector<string>* tags) const override
	{
		for (size_t i = 0; i < conditions.size(); ++i)
			if (conditions[i])
				conditions[i]->FindTags(tags);
	}

	void Serialize(IArchive& ar)
	{
		IAnimationFilterCondition::Serialize(ar);
		ar(conditions, "conditions", "[<>]^");
	}

	std::vector<_smart_ptr<IAnimationFilterCondition>> conditions;
};

SERIALIZATION_CLASS_NAME(IAnimationFilterCondition, SAnimationFilterAnd, "and", "And")

// ---------------------------------------------------------------------------

struct SAnimationFilterOr : IAnimationFilterCondition
{
	bool Check(const SAnimationFilterItem& item) const override
	{
		size_t numConditions = conditions.size();
		if (numConditions == 0)
			return not;
		for (size_t i = 0; i < numConditions; ++i)
		{
			if (!conditions[i])
				continue;
			if (conditions[i]->Check(item))
				return !not;
		}
		return not;
	}

	void FindTags(std::vector<string>* tags) const override
	{
		for (size_t i = 0; i < conditions.size(); ++i)
			if (conditions[i])
				conditions[i]->FindTags(tags);
	}

	void Serialize(Serialization::IArchive& ar)
	{
		IAnimationFilterCondition::Serialize(ar);
		ar(conditions, "conditions", "[<>]^");
	}

	std::vector<_smart_ptr<IAnimationFilterCondition>> conditions;
};

SERIALIZATION_CLASS_NAME(IAnimationFilterCondition, SAnimationFilterOr, "or", "Or")

// ---------------------------------------------------------------------------

struct SAnimationFilterInFolder : IAnimationFilterCondition
{
	string path;

	SAnimationFilterInFolder()
		: path("animations/")
	{
	}

	bool Check(const SAnimationFilterItem& item) const override
	{
		string normalizedPath = PathUtil::ToUnixPath(item.path);
		normalizedPath.MakeLower();

		string prefix = PathUtil::ToUnixPath(path);
		prefix.MakeLower();
		if (prefix.empty() || prefix[prefix.size() - 1] != '/')
			prefix += "/";

		if (strncmp(normalizedPath, prefix, prefix.size()) == 0)
		{
			//if (context.log)
			//  gEnv->pLog->Log( " + '%s' is located in folder '%s'", item.path.c_str(), m_path.c_str() );
			return !not;
		}
		return not;
	}

	void Serialize(Serialization::IArchive& ar)
	{
		IAnimationFilterCondition::Serialize(ar);
		ar(path, "path", "^<");
	}
};

SERIALIZATION_CLASS_NAME(IAnimationFilterCondition, SAnimationFilterInFolder, "inFolder", "In Folder")

// ---------------------------------------------------------------------------

struct SAnimationFilterHasTags : IAnimationFilterCondition
{
	std::vector<string> tags;

	bool Check(const SAnimationFilterItem& item) const override
	{
		if (tags.empty())
			return not;

		size_t numTags = tags.size();
		for (size_t i = 0; i < numTags; ++i)
		{
			size_t numItemTags = item.tags.size();
			bool hasTag = false;
			for (size_t j = 0; j < numItemTags; ++j)
			{
				if (stricmp(item.tags[j].c_str(), tags[i].c_str()) == 0)
				{
					hasTag = true;
					break;
				}
			}

			if (!hasTag)
				return not;
		}

		return !not;
	}

	void FindTags(std::vector<string>* tags) const override
	{
		tags->insert(tags->end(), this->tags.begin(), this->tags.end());
	}

	void Serialize(IArchive& ar)
	{
		IAnimationFilterCondition::Serialize(ar);
		ar(tags, "tags", "^");
	}
};

SERIALIZATION_CLASS_NAME(IAnimationFilterCondition, SAnimationFilterHasTags, "hasTags", "Has Tags")

// ---------------------------------------------------------------------------

struct SAnimationFilterNameContains : IAnimationFilterCondition
{
	string substring;

	bool Check(const SAnimationFilterItem& item) const override
	{
		if (substring.empty())
			return false;
		string name = PathUtil::GetFile(item.path.c_str());
		name.MakeLower();// = MakeLower(name);
		string substr = substring.c_str();
		substr.MakeLower();
		bool match = name.find(substr) != string::npos;
		//if (match && context.log)
		//	gEnv->pLog->Log( " + name '%s' contains '%s'", name.c_str(), m_nameContains.c_str() );
		if (match)
			return !not;
		else
			return not;
	}

	void Serialize(IArchive& ar)
	{
		IAnimationFilterCondition::Serialize(ar);
		ar(substring, "substring", "^<");
	}
};

SERIALIZATION_CLASS_NAME(IAnimationFilterCondition, SAnimationFilterNameContains, "nameContains", "Contains in Name")

// ---------------------------------------------------------------------------

struct SAnimationFilterPathContains : IAnimationFilterCondition
{
	string path;

	SAnimationFilterPathContains()
		: path("animations/")
	{
	}

	bool Check(const SAnimationFilterItem& item) const override
	{
		string normalizedItemPath = PathUtil::ToUnixPath(item.path);
		normalizedItemPath.MakeLower();

		string normalizedPath = PathUtil::ToUnixPath(path);
		normalizedPath.MakeLower();

		bool match = normalizedItemPath.find(normalizedPath) != string::npos;
		if (match)
			return !not;
		else
			return not;
	}

	void Serialize(Serialization::IArchive& ar)
	{
		IAnimationFilterCondition::Serialize(ar);
		ar(path, "path", "^<");
	}
};

SERIALIZATION_CLASS_NAME(IAnimationFilterCondition, SAnimationFilterPathContains, "pathContains", "Contains in Path")

// ---------------------------------------------------------------------------

struct SAnimationFilterSkeletonAlias : IAnimationFilterCondition
{
	string skeletonAlias;

	bool Check(const SAnimationFilterItem& item) const override
	{
		if (stricmp(item.skeletonAlias.c_str(), skeletonAlias.c_str()) == 0)
			return !not;
		else
			return not;
	}

	void Serialize(IArchive& ar)
	{
		IAnimationFilterCondition::Serialize(ar);
		ar(skeletonAlias, "alias", "^<");
	}
};

SERIALIZATION_CLASS_NAME(IAnimationFilterCondition, SAnimationFilterSkeletonAlias, "skeletonAlias", "Skeleton Alias")

// ---------------------------------------------------------------------------

void SAnimationFilter::Filter(std::vector<SAnimationFilterItem>* filteredItems, const std::vector<SAnimationFilterItem>& items) const
{
	if (!filteredItems)
		return;

	filteredItems->clear();

	if (!condition)
	{
		return;
	}

	size_t numItems = items.size();
	filteredItems->reserve(numItems);
	for (size_t i = 0; i < numItems; ++i)
	{
		const SAnimationFilterItem& item = items[i];
		if (MatchesFilter(item))
		{
			filteredItems->push_back(item);
		}
	}
}

bool SAnimationFilter::MatchesFilter(const SAnimationFilterItem& item) const
{
	if (!condition)
		return false;
	return condition->Check(item);
}

void SAnimationFilter::Serialize(IArchive& ar)
{
	ar(condition, "condition", "+<>");
}

void SAnimationFilter::SetInFolderCondition(const char* folder)
{
	SAnimationFilterInFolder* cond = new SAnimationFilterInFolder();
	cond->path = folder;
	condition.reset(cond);
}

void SAnimationFilter::FindTags(std::vector<string>* tags) const
{
	tags->clear();
	if (condition)
		condition->FindTags(tags);
	tags->erase(std::unique(tags->begin(), tags->end()), tags->end());
}

