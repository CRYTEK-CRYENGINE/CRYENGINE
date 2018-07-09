// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Control.h"

#include <CrySystem/ISystem.h>

namespace ACE
{
class CLibrary;

static constexpr char* s_szLibraryNodeTag = "Library";
static constexpr char* s_szFoldersNodeTag = "Folders";
static constexpr char* s_szControlsNodeTag = "Controls";
static constexpr char* s_szFolderTag = "Folder";
static constexpr char* s_szPathAttribute = "path";
static constexpr char* s_szDescriptionAttribute = "description";

struct SLibraryScope final
{
	SLibraryScope()
		: isDirty(false)
	{
		pNodes[0] = GetISystem()->CreateXmlNode(CryAudio::s_szTriggersNodeTag);
		pNodes[1] = GetISystem()->CreateXmlNode(CryAudio::s_szParametersNodeTag);
		pNodes[2] = GetISystem()->CreateXmlNode(CryAudio::s_szSwitchesNodeTag);
		pNodes[3] = GetISystem()->CreateXmlNode(CryAudio::s_szEnvironmentsNodeTag);
		pNodes[4] = GetISystem()->CreateXmlNode(CryAudio::s_szPreloadsNodeTag);
	}

	XmlNodeRef GetXmlNode(EAssetType const type) const
	{
		XmlNodeRef pNode;

		switch (type)
		{
		case EAssetType::Trigger:
			pNode = pNodes[0];
			break;
		case EAssetType::Parameter:
			pNode = pNodes[1];
			break;
		case EAssetType::Switch:
			pNode = pNodes[2];
			break;
		case EAssetType::Environment:
			pNode = pNodes[3];
			break;
		case EAssetType::Preload:
			pNode = pNodes[4];
			break;
		default:
			pNode = nullptr;
			break;
		}

		return pNode;
	}

	XmlNodeRef pNodes[5]; // Trigger, Parameter, Switch, Environment, Preload
	bool       isDirty;
};

using LibraryStorage = std::map<Scope, SLibraryScope>;

class CFileWriter final
{
public:

	explicit CFileWriter(FileNames& previousLibraryPaths);

	CFileWriter() = delete;

	void WriteAll();

private:

	void WriteLibrary(CLibrary& library);
	void WriteItem(CAsset* const pAsset, string const& path, LibraryStorage& library);
	void GetScopes(CAsset const* const pAsset, std::unordered_set<Scope>& scopes);
	void WriteControlToXML(XmlNodeRef const pNode, CControl* const pControl, string const& path);
	void WriteConnectionsToXML(XmlNodeRef const pNode, CControl* const pControl, int const platformIndex = -1);
	void WriteLibraryEditorData(CAsset const& library, XmlNodeRef const pParentNode) const;
	void WriteFolderEditorData(CAsset const& library, XmlNodeRef const pParentNode) const;
	void WriteControlsEditorData(CAsset const& parentAsset, XmlNodeRef const pParentNode) const;
	void DeleteLibraryFile(string const& filepath);

	FileNames&          m_previousLibraryPaths;
	FileNames           m_foundLibraryPaths;

	static uint32 const s_currentFileVersion;
};
} // namespace ACE

