// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryString/CryString.h>
#include <IAudioConnection.h>
#include "AudioAssets.h"
#include <CrySystem/XML/IXml.h>
#include <ACETypes.h>

namespace ACE
{
class CAudioAssetsManager;

class CAudioControlsLoader
{
public:
	CAudioControlsLoader(CAudioAssetsManager* pAssetsManager);
	std::set<string> GetLoadedFilenamesList();
	void             LoadAll();
	void             LoadControls();
	void             LoadScopes();
	uint             GetErrorCodeMask() const { return m_errorCodeMask; }

private:
	typedef std::vector<const char*> SwitchStates;
	void           LoadAllLibrariesInFolder(const string& folderPath, const string& level);
	void           LoadControlsLibrary(XmlNodeRef pRoot, const string& filepath, const string& level, const string& filename, uint version);
	CAudioControl* LoadControl(XmlNodeRef pNode, Scope scope, uint version, IAudioAsset* pParentItem);

	void           LoadPreloadConnections(XmlNodeRef pNode, CAudioControl* pControl, uint version);
	void           LoadConnections(XmlNodeRef root, CAudioControl* pControl);

	void           CreateDefaultControls();
	void           CreateDefaultSwitch(IAudioAsset* pLibrary, const char* szExternalName, const char* szInternalName, const SwitchStates& states);

	void           LoadScopesImpl(const string& path);

	void           LoadEditorData(XmlNodeRef pEditorDataNode, IAudioAsset* pRootItem);
	void           LoadAllFolders(XmlNodeRef pRootFoldersNode, IAudioAsset* pParentItem);
	void           LoadFolderData(XmlNodeRef pRootFoldersNode, IAudioAsset* pParentItem);

	IAudioAsset*   AddUniqueFolderPath(IAudioAsset* pParent, const QString& path);

	static const string ms_controlsLevelsFolder;
	static const string ms_levelsFolder;
	// TODO: Move these strings to Utils

	CAudioAssetsManager* m_pAssetsManager;
	std::set<string>     m_loadedFilenames;
	uint                 m_errorCodeMask;
};
}
