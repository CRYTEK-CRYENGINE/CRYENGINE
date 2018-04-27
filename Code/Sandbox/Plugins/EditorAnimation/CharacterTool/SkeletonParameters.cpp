// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include <CryCore/Platform/platform.h>  // just for PathUtil::!
#include "SkeletonParameters.h"
#include <CrySystem/File/CryFile.h> // just for PathUtil::!
#include <CryString/CryPath.h>
#include "Serialization.h"
#include "IEditor.h"

namespace CharacterTool
{

struct Substring
{
	const char* start;
	const char* end;

	bool operator==(const char* str) const { return strncmp(str, start, end - start) == 0; }
	bool Empty() const                     { return start == end; }
};

static Substring GetLevel(const char** outP)
{
	const char* p = *outP;
	while (*p != '/' && *p != '\0')
		++p;
	Substring result;
	result.start = *outP;
	result.end = p;
	if (*p == '/')
		++p;
	*outP = p;
	return result;
}

const bool MatchesWildcard(const Substring& str, const Substring& wildcard)
{
	const char* savedStr;
	const char* savedWild;

	const char* p = str.start;
	const char* w = wildcard.start;

	while (p != str.end && (*w != '*'))
	{
		if ((*w != *p) && (*w != '?'))
			return false;
		++w;
		++p;
	}

	while (p != str.end)
	{
		if (*w == '*')
		{
			++w;
			if (w == wildcard.end)
			{
				return true;
			}
			savedWild = w;
			savedStr = p + 1;
		}
		else if ((*w == *p) || (*w == '?'))
		{
			++w;
			++p;
		}
		else
		{
			w = savedWild;
			p = savedStr++;
		}
	}

	while (*w == '*')
		++w;

	return w == wildcard.end;
}

static bool MatchesPathWildcardNormalized(const char* path, const char* wildcard)
{
	std::vector<std::pair<const char*, const char*>> border;
	border.push_back(std::make_pair(path, wildcard));

	while (!border.empty())
	{
		const char* p = border.front().first;
		const char* w = border.front().second;
		border.erase(border.begin());

		while (true)
		{
			Substring ws = GetLevel(&w);

			Substring ps = GetLevel(&p);

			bool containsWildCardsOnly = !ws.Empty();
			for (const char* c = ws.start; c != ws.end; ++c)
			{
				if (*c != '*')
				{
					containsWildCardsOnly = false;
					break;
				}
			}

			if (containsWildCardsOnly)
			{
				if (*p != '\0')
				{
					border.push_back(std::make_pair(p, ws.start));
				}
				border.push_back(std::make_pair(ps.start, w));
			}

			if (ws.Empty())
			{
				if (ps.Empty())
					return true;
				else
					break;
			}
			if (!MatchesWildcard(ps, ws))
			{
				ps = GetLevel(&p);
				bool matched = false;
				while (!ps.Empty())
				{
					if (MatchesWildcard(ps, ws))
					{
						matched = true;
						break;
					}
					ps = GetLevel(&p);
				}
				if (!matched)
					break;
			}
			if (ps.Empty() && ws.Empty())
				break;
		}
	}
	return false;
}

static bool MatchesPathWildcard(const char* path, const char* wildcard)
{
	stack_string normalizedPath = path;
	normalizedPath.replace('\\', '/');
	normalizedPath.MakeLower();

	stack_string normalizedWildcard = wildcard;
	normalizedWildcard.replace('\\', '/');
	normalizedWildcard.MakeLower();

	return MatchesPathWildcardNormalized(normalizedPath.c_str(), normalizedWildcard.c_str());
}

bool AnimationSetFilter::Matches(const char* cafPath) const
{
	for (size_t i = 0; i < folders.size(); ++i)
	{
		const AnimationFilterFolder& folder = folders[i];
		for (size_t j = 0; j < folder.wildcards.size(); ++j)
		{
			string combinedWildcard = folder.path + "/" + folder.wildcards[j].fileWildcard;
			if (MatchesPathWildcard(cafPath, combinedWildcard.c_str()))
				return true;
		}
	}
	return false;
}

// ---------------------------------------------------------------------------

void SkeletonParameters::Serialize(Serialization::IArchive& ar)
{
	ar(includes, "includes");
	ar(animationSetFilter, "animationSetFilter");
	ar(animationEventDatabase, "animationEventDatabase");
	ar(faceLibFile, "faceLibFile");

	ar(dbaPath, "dbaPath");
	ar(individualDBAs, "dbas");
}

struct XmlUsageTracker
{
	vector<XmlNodeRef> unusedNodes;

	XmlUsageTracker(const XmlNodeRef& root)
	{
		int numChildren = root->getChildCount();
		for (int i = 0; i < numChildren; ++i)
			unusedNodes.push_back(root->getChild(i));
	}

	XmlNodeRef UseNode(const char* name)
	{
		for (size_t i = 0; i < unusedNodes.size(); ++i)
			if (unusedNodes[i]->isTag(name))
			{
				XmlNodeRef result = unusedNodes[i];
				unusedNodes.erase(unusedNodes.begin() + i);
				return result;
			}
		return XmlNodeRef();
	}
};

bool SkeletonParameters::LoadFromXML(XmlNodeRef topRoot, bool* dataLost)
{
	if (!topRoot)
		return false;

	AnimationFilterFolder* filterFolder = 0;
	animationSetFilter.folders.clear();
	includes.clear();
	individualDBAs.clear();

	XmlUsageTracker xml(topRoot);

	if (XmlNodeRef calNode = xml.UseNode("AnimationList"))
	{
		uint32 count = calNode->getChildCount();
		for (uint32 i = 0; i < count; ++i)
		{
			XmlNodeRef assignmentNode = calNode->getChild(i);

			if (stricmp(assignmentNode->getTag(), "Animation") != 0)
				continue;

			const int BITE = 512;
			CryFixedStringT<BITE> line;

			CryFixedStringT<BITE> key;
			CryFixedStringT<BITE> value;

			key.append(assignmentNode->getAttr("name"));
			line.append(assignmentNode->getAttr("path"));

			int32 pos = 0;
			value = line.Tokenize(" \t\n\r=", pos);

			// now only needed for aim poses
			char buffer[BITE];
			cry_strcpy(buffer, line.c_str());

			if (value.empty() || value.at(0) == '?')
			{
				continue;
			}

			if (0 == stricmp(key, "#filepath"))
			{
				animationSetFilter.folders.push_back(AnimationFilterFolder());
				filterFolder = &animationSetFilter.folders.back();
				filterFolder->wildcards.clear();
				filterFolder->path = PathUtil::ToUnixPath(value.c_str());
				filterFolder->path.TrimRight('/');   // delete the trailing slashes
				continue;
			}

			if (0 == stricmp(key, "#ParseSubFolders"))
			{
				bool parseSubFolders = stricmp(value, "true") == 0 ? true : false; // if it's false, stricmp return 0
				if (parseSubFolders != true)
				{
					if (dataLost)
						*dataLost = true;
				}
				continue;
			}

			// remove first '\' and replace '\' with '/'
			value.replace('\\', '/');
			value.TrimLeft('/');

			// process the possible directives
			if (key.c_str()[0] == '$')
			{
				if (!stricmp(key, "$AnimationDir") || !stricmp(key, "$AnimDir") || !stricmp(key, "$AnimationDirectory") || !stricmp(key, "$AnimDirectory"))
				{
					if (dataLost)
						*dataLost = true;
				}
				else if (!stricmp(key, "$AnimEventDatabase"))
				{
					if (animationEventDatabase.empty())
						animationEventDatabase = value.c_str();
					else
						*dataLost = true;
				}
				else if (!stricmp(key, "$TracksDatabase"))
				{
					int wildcardPos = value.find('*');
					if (wildcardPos != string::npos)
					{
						//--- Wildcard include

						dbaPath = PathUtil::ToUnixPath(value.Left(wildcardPos)).c_str();
					}
					else
					{
						CryFixedStringT<BITE> flags;
						flags.append(assignmentNode->getAttr("flags"));

						SkeletonParametersDBA dba;

						// flag handling
						if (!flags.empty())
						{
							if (strstr(flags.c_str(), "persistent"))
								dba.persistent = true;
						}

						dba.filename = PathUtil::ToUnixPath(value).c_str();
						individualDBAs.push_back(dba);
					}
				}
				else if (!stricmp(key, "$Include"))
				{
					SkeletonParametersInclude include;
					include.filename = value.c_str();
					includes.push_back(include);
				}
				else if (!stricmp(key, "$FaceLib"))
				{
					faceLibFile = value.c_str();
				}
				else
				{
					if (dataLost)
						*dataLost = true;
					//Warning(paramFileName, VALIDATOR_ERROR, "Unknown directive in '%s'", key.c_str());
				}
			}
			else
			{
				AnimationFilterWildcard wildcard;
				wildcard.renameMask = key.c_str();
				wildcard.fileWildcard = value.c_str();
				if (!filterFolder)
				{
					animationSetFilter.folders.push_back(AnimationFilterFolder());
					filterFolder = &animationSetFilter.folders.back();
					filterFolder->wildcards.clear();
				}
				filterFolder->wildcards.push_back(wildcard);
			}
		}
	}

	unknownNodes = xml.unusedNodes;

	return true;
}

bool SkeletonParameters::LoadFromXMLFile(const char* filename, bool* dataLost)
{
	XmlNodeRef root = gEnv->pSystem->LoadXmlFromFile(filename);
	if (!root)
		return false;

	if (!LoadFromXML(root, dataLost))
		return false;

	return true;
}

static void AddAnimation(const XmlNodeRef& dest, const char* name, const char* path, const char* flags = 0)
{
	XmlNodeRef child = dest->createNode("Animation");
	child->setAttr("name", name);
	child->setAttr("path", path);
	if (flags)
		child->setAttr("flags", flags);
	dest->addChild(child);
}

XmlNodeRef SkeletonParameters::SaveToXML()
{
	XmlNodeRef root = gEnv->pSystem->CreateXmlNode("Params");

	XmlNodeRef animationList = root->findChild("AnimationList");
	if (animationList)
		animationList->removeAllChilds();
	else
	{
		animationList = root->createNode("AnimationList");
		root->addChild(animationList);
	}

	for (size_t i = 0; i < includes.size(); ++i)
	{
		const string& filename = includes[i].filename;
		AddAnimation(animationList, "$Include", filename.c_str());
	}

	if (!animationEventDatabase.empty())
		AddAnimation(animationList, "$AnimEventDatabase", animationEventDatabase.c_str());

	if (!dbaPath.empty())
	{
		string path = dbaPath;
		if (!path.empty() && path[path.size() - 1] != '/' && path[path.size() - 1] != '\\')
			path += "/";
		path += "*.dba";
		AddAnimation(animationList, "$TracksDatabase", path.c_str());
	}

	for (size_t i = 0; i < individualDBAs.size(); ++i)
		AddAnimation(animationList, "$TracksDatabase", individualDBAs[i].filename.c_str(), individualDBAs[i].persistent ? "persistent" : 0);

	for (size_t i = 0; i < animationSetFilter.folders.size(); ++i)
	{
		AnimationFilterFolder& folder = animationSetFilter.folders[i];

		AddAnimation(animationList, "#filepath", folder.path);
		for (size_t j = 0; j < folder.wildcards.size(); ++j)
		{
			AnimationFilterWildcard& w = folder.wildcards[j];
			AddAnimation(animationList, w.renameMask.c_str(), w.fileWildcard.c_str());
		}
	}

	for (size_t i = 0; i < unknownNodes.size(); ++i)
		root->addChild(unknownNodes[i]);

	return root;
}

bool SkeletonParameters::SaveToMemory(vector<char>* buffer)
{
	XmlNodeRef root = SaveToXML();
	if (!root)
		return false;

	_smart_ptr<IXmlStringData> str(root->getXMLData());
	buffer->assign(str->GetString(), str->GetString() + str->GetStringLength());
	return true;
}

}

