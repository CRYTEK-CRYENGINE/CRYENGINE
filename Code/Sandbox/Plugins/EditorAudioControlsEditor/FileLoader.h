// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common.h"

class XmlNodeRef;

namespace ACE
{
class CAsset;
class CControl;

class CFileLoader final
{
public:

	CFileLoader(CFileLoader const&) = delete;
	CFileLoader(CFileLoader&&) = delete;
	CFileLoader& operator=(CFileLoader const&) = delete;
	CFileLoader& operator=(CFileLoader&&) = delete;

	CFileLoader() = default;

	FileNames GetLoadedFilenamesList() const { return m_loadedFilenames; }
	void      CreateInternalControls();
	void      Load();

private:

	bool      LoadAllLibrariesInFolder(string const& folderPath, string const& contextName);
	void      LoadControlsLibrary(XmlNodeRef const& rootNode, string const& filepath, string const& contextName, string const& fileName, uint8 const version);
	CControl* LoadControl(XmlNodeRef const& node, CryAudio::ContextId const contextId, CAsset* const pParentItem);

#if defined (USE_BACKWARDS_COMPATIBILITY)
	void LoadControlsBW();
	bool LoadAllLibrariesInFolderBW(string const& folderPath, string const& level);
#endif //  USE_BACKWARDS_COMPATIBILITY

	FileNames m_loadedFilenames;
};
} // namespace ACE
