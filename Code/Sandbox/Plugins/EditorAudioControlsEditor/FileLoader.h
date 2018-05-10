// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Control.h"

namespace ACE
{
class CFileLoader final
{
public:

	CFileLoader();
	FileNames  GetLoadedFilenamesList();
	void       CreateInternalControls();
	void       LoadAll();
	void       LoadControls();
	void       LoadScopes();
	EErrorCode GetErrorCodeMask() const { return m_errorCodeMask; }

private:

	using SwitchStates = std::vector<char const*>;

	void      LoadAllLibrariesInFolder(string const& folderPath, string const& level);
	void      LoadControlsLibrary(XmlNodeRef const pRoot, string const& filepath, string const& level, string const& filename, uint32 const version);
	CControl* LoadControl(XmlNodeRef const pNode, Scope const scope, uint32 const version, CAsset* const pParentItem);

	void      LoadPreloadConnections(XmlNodeRef const pNode, CControl* const pControl, uint32 const version);
	void      LoadConnections(XmlNodeRef const root, CControl* const pControl);

	void      CreateInternalSwitch(CAsset* const pLibrary, char const* const szSwitchName, SwitchStates const& StateNames, char const* const szDescription);
	void      CreateDefaultControls();

	void      LoadEditorData(XmlNodeRef const pEditorDataNode, CAsset& library);
	void      LoadLibraryEditorData(XmlNodeRef const pLibraryNode, CAsset& library);
	void      LoadAllFolders(XmlNodeRef const pFoldersNode, CAsset& library);
	void      LoadFolderData(XmlNodeRef const pFolderNode, CAsset& parentAsset);
	void      LoadAllControlsEditorData(XmlNodeRef const pControlsNode);
	void      LoadControlsEditorData(XmlNodeRef const pParentNode);

	CAsset*   AddUniqueFolderPath(CAsset* pParent, QString const& path);

	FileNames  m_loadedFilenames;
	EErrorCode m_errorCodeMask;
};
} // namespace ACE

