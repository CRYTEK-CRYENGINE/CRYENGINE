// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "SystemAssets.h"

namespace ACE
{
class CSystemAssetsManager;
struct IEditorImpl;

struct SLibraryScope
{
	SLibraryScope()
		: isDirty(false)
	{
		pNodes[0] = GetISystem()->CreateXmlNode("AudioTriggers");
		pNodes[1] = GetISystem()->CreateXmlNode("AudioRTPCs");
		pNodes[2] = GetISystem()->CreateXmlNode("AudioSwitches");
		pNodes[3] = GetISystem()->CreateXmlNode("AudioEnvironments");
		pNodes[4] = GetISystem()->CreateXmlNode("AudioPreloads");
	}

	XmlNodeRef GetXmlNode(ESystemItemType const type) const
	{
		switch (type)
		{
		case ESystemItemType::Trigger:
			return pNodes[0];
		case ESystemItemType::Parameter:
			return pNodes[1];
		case ESystemItemType::Switch:
			return pNodes[2];
		case ESystemItemType::Environment:
			return pNodes[3];
		case ESystemItemType::Preload:
			return pNodes[4];
		}
		return nullptr;
	}

	XmlNodeRef pNodes[5]; // Trigger, Parameter, Switch, Environment, Preloads
	bool       isDirty;
};

typedef std::map<Scope, SLibraryScope> LibraryStorage;

class CAudioControlsWriter
{
public:

	CAudioControlsWriter(CSystemAssetsManager* pAssetsManager, IEditorImpl* pEditorImpl, std::set<string>& previousLibraryPaths);

private:

	void WriteLibrary(CSystemLibrary& library);
	void WriteItem(CSystemAsset* const pItem, string const& path, LibraryStorage& library);
	void GetScopes(CSystemAsset const* const pItem, std::unordered_set<Scope>& scopes);
	void WriteControlToXML(XmlNodeRef const pNode, CSystemControl* pControl, string const& path);
	void WriteConnectionsToXML(XmlNodeRef const pNode, CSystemControl* const pControl, int const platformIndex = -1);
	void WriteLibraryEditorData(CSystemAsset const& library, XmlNodeRef const pParentNode) const;
	void WriteFolderEditorData(CSystemAsset const& library, XmlNodeRef const pParentNode) const;
	void WriteControlsEditorData(CSystemAsset const& parentAsset, XmlNodeRef const pParentNode) const;

	void CheckOutFile(string const& filepath);
	void DeleteLibraryFile(string const& filepath);

	CSystemAssetsManager* m_pAssetsManager;
	IEditorImpl*          m_pEditorImpl;

	std::set<string>      m_foundLibraryPaths;

	static string const   s_controlsPath;
	static string const   s_levelsFolder;
	static uint const     s_currentFileVersion;
};
} // namespace ACE
