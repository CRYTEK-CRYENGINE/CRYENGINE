// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryString/CryString.h>
#include <IAudioConnection.h>
#include "AudioAssets.h"
#include <CrySystem/XML/IXml.h>
#include <QModelIndex>
#include <CrySystem/ISystem.h>
#include "ACETypes.h"

class QStandardItemModel;

namespace ACE
{
class CAudioAssetsManager;
class IAudioSystemEditor;

struct SLibraryScope
{
	SLibraryScope()
		: bDirty(false)
	{
		pNodes[0] = GetISystem()->CreateXmlNode("AudioTriggers");
		pNodes[1] = GetISystem()->CreateXmlNode("AudioRTPCs");
		pNodes[2] = GetISystem()->CreateXmlNode("AudioSwitches");
		pNodes[3] = GetISystem()->CreateXmlNode("AudioEnvironments");
		pNodes[4] = GetISystem()->CreateXmlNode("AudioPreloads");
	}

	XmlNodeRef GetXmlNode(const EItemType type) const
	{
		switch (type)
		{
		case EItemType::eItemType_Trigger:
			return pNodes[0];
		case EItemType::eItemType_RTPC:
			return pNodes[1];
		case EItemType::eItemType_Switch:
			return pNodes[2];
		case EItemType::eItemType_Environment:
			return pNodes[3];
		case EItemType::eItemType_Preload:
			return pNodes[4];
		}
		return nullptr;
	}

	XmlNodeRef pNodes[5]; // Trigger, RTPC, Switch, Environment, Preloads
	bool       bDirty;
};

typedef std::map<Scope, SLibraryScope> LibraryStorage;

class CAudioControlsWriter
{
public:
	CAudioControlsWriter(CAudioAssetsManager* pAssetsManager, IAudioSystemEditor* pAudioSystemImpl, std::set<string>& previousLibraryPaths);

private:
	void WriteLibrary(CAudioLibrary& library);
	void WriteItem(IAudioAsset* pItem, const string& path, LibraryStorage& library);
	void GetScopes(IAudioAsset const* const pItem, std::unordered_set<Scope>& scopes);
	void WriteControlToXML(XmlNodeRef pNode, CAudioControl* pControl, const string& path);
	void WriteConnectionsToXML(XmlNodeRef pNode, CAudioControl* pControl, const int platformIndex = -1);
	void WriteEditorData(IAudioAsset* pLibrary, XmlNodeRef pParentNode) const;

	void CheckOutFile(const string& filepath);
	void DeleteLibraryFile(const string& filepath);

	CAudioAssetsManager* m_pAssetsManager;
	IAudioSystemEditor*  m_pAudioSystemImpl;

	std::set<string>     m_foundLibraryPaths;

	static const string  ms_controlsPath;
	static const string  ms_levelsFolder;
	static const uint    ms_currentFileVersion;
};
}
