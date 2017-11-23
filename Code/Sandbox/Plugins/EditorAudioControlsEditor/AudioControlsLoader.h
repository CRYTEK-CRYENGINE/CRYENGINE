// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "SystemAssets.h"

namespace ACE
{
class CSystemAssetsManager;

class CAudioControlsLoader
{
public:

	CAudioControlsLoader(CSystemAssetsManager* pAssetsManager);
	std::set<string> GetLoadedFilenamesList();
	void             LoadAll();
	void             LoadControls();
	void             LoadScopes();
	EErrorCode       GetErrorCodeMask() const { return m_errorCodeMask; }

private:

	using SwitchStates = std::vector<char const*>;

	void            LoadAllLibrariesInFolder(string const& folderPath, string const& level);
	void            LoadControlsLibrary(XmlNodeRef const pRoot, string const& filepath, string const& level, string const& filename, uint const version);
	CSystemControl* LoadControl(XmlNodeRef const pNode, Scope scope, uint version, CSystemAsset* const pParentItem);

	void            LoadPreloadConnections(XmlNodeRef const pNode, CSystemControl* const pControl, uint const version);
	void            LoadConnections(XmlNodeRef const root, CSystemControl* const pControl);

	void            CreateDefaultControls();
	void            CreateDefaultSwitch(CSystemAsset* const pLibrary, char const* const szExternalName, char const* const szInternalName, SwitchStates const& states);

	void            LoadScopesImpl(string const& path);

	void            LoadEditorData(XmlNodeRef const pEditorDataNode, CSystemAsset& library);
	void            LoadLibraryEditorData(XmlNodeRef const pLibraryNode, CSystemAsset& library);
	void            LoadAllFolders(XmlNodeRef const pFoldersNode, CSystemAsset& library);
	void            LoadFolderData(XmlNodeRef const pFolderNode, CSystemAsset& parentAsset);
	void            LoadAllControlsEditorData(XmlNodeRef const pControlsNode);
	void            LoadControlsEditorData(XmlNodeRef const pParentNode);

	CSystemAsset*   AddUniqueFolderPath(CSystemAsset* pParent, QString const& path);

	static string const s_controlsLevelsFolder;
	static string const s_levelsFolder;
	// TODO: Move these strings to Utils

	CSystemAssetsManager* m_pAssetsManager;
	std::set<string>      m_loadedFilenames;
	EErrorCode            m_errorCodeMask;
};
} // namespace ACE
