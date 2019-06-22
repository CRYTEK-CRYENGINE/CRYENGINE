// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Control.h"

#if defined (USE_BACKWARDS_COMPATIBILITY)
namespace ACE
{
// This file is deprecated and only used for backwards compatibility. It will be removed with CE 5.7.
class CAudioControlsLoader final
{
public:

	CAudioControlsLoader(CAudioControlsLoader const&) = delete;
	CAudioControlsLoader(CAudioControlsLoader&&) = delete;
	CAudioControlsLoader& operator=(CAudioControlsLoader const&) = delete;
	CAudioControlsLoader& operator=(CAudioControlsLoader&&) = delete;

	CAudioControlsLoader() = default;

	FileNames GetLoadedFilenamesList();
	void      LoadAll(bool const loadOnlyDefaultControls = false);
	void      LoadControls(string const& folderPath);

private:

	bool      LoadAllLibrariesInFolder(string const& folderPath, string const& level);
	void      LoadControlsLibrary(XmlNodeRef const& rootNode, string const& filepath, string const& level, string const& filename, uint8 const version);
	CControl* LoadControl(XmlNodeRef const& node, CryAudio::ContextId const contextId, uint8 const version, CAsset* const pParentItem);
	CControl* LoadDefaultControl(XmlNodeRef const& node, CryAudio::ContextId const contextId, CAsset* const pParentItem);

	void      LoadPreloadConnections(XmlNodeRef const& node, CControl* const pControl, uint8 const version);
	void      LoadConnections(XmlNodeRef const& rootNode, CControl* const pControl);

	void      LoadEditorData(XmlNodeRef const& node, CAsset& library);
	void      LoadLibraryEditorData(XmlNodeRef const& node, CAsset& library);
	void      LoadAllFolders(XmlNodeRef const& node, CAsset& library);
	void      LoadFolderData(XmlNodeRef const& node, CAsset& parentAsset);
	void      LoadAllControlsEditorData(XmlNodeRef const& node);
	void      LoadControlsEditorData(XmlNodeRef const& node);

	CAsset*   AddUniqueFolderPath(CAsset* pParent, QString const& path);

	static string const s_controlsLevelsFolder;
	static string const s_assetsFolderPath;
	FileNames           m_loadedFilenames;
	bool                m_loadOnlyDefaultControls = false;

	FileNames           m_defaultTriggerNames { CryAudio::g_szGetFocusTriggerName, CryAudio::g_szLoseFocusTriggerName, CryAudio::g_szMuteAllTriggerName, CryAudio::g_szUnmuteAllTriggerName };
	FileNames           m_defaultParameterNames { "absolute_velocity", "object_speed", "relative_velocity", "object_doppler" };
};
}      // namespace ACE
#endif //  USE_BACKWARDS_COMPATIBILITY