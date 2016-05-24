// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryString/CryString.h>
#include <IAudioConnection.h>
#include "AudioControl.h"
#include <CrySystem/XML/IXml.h>
#include <ACETypes.h>

class QStandardItemModel;
class QStandardItem;

namespace ACE
{
class CATLControlsModel;
class IAudioSystemEditor;
class QATLTreeModel;

class CAudioControlsLoader
{
public:
	CAudioControlsLoader(CATLControlsModel* pATLModel, QATLTreeModel* pLayoutModel, IAudioSystemEditor* pAudioSystemImpl);
	std::set<string> GetLoadedFilenamesList();
	void             LoadAll();
	void             LoadControls();
	void             LoadScopes();
	uint             GetErrorCodeMask() const { return m_errorCodeMask; }

private:
	void           LoadAllLibrariesInFolder(const string& folderPath, const string& level);
	void           LoadControlsLibrary(XmlNodeRef pRoot, const string& filepath, const string& level, const string& filename, uint version);
	CATLControl*   LoadControl(XmlNodeRef pNode, QStandardItem* pFolder, Scope scope, uint version);

	void           LoadPreloadConnections(XmlNodeRef pNode, CATLControl* pControl, QStandardItem* pItem, uint version);
	void           LoadConnections(XmlNodeRef root, CATLControl* pControl);

	void           CreateDefaultControls();
	void           CreateDefaultSwitch(QStandardItem* pFolder, const char* szExternalName, const char* szInternalName, const std::vector<const char*>& states);

	QStandardItem* AddControl(CATLControl* pControl, QStandardItem* pFolder);

	void           LoadScopesImpl(const string& path);

	void           LoadEditorData(XmlNodeRef pEditorDataNode, QStandardItem* pRootItem);
	void           LoadAllFolders(XmlNodeRef pRootFoldersNode, QStandardItem* pParentItem);
	void           LoadFolderData(XmlNodeRef pRootFoldersNode, QStandardItem* pParentItem);

	QStandardItem* AddUniqueFolderPath(QStandardItem* pParent, const QString& sPath);
	QStandardItem* AddFolder(QStandardItem* pParent, const QString& sName);

	static const string ms_controlsPath;
	static const string ms_controlsLevelsFolder;
	static const string ms_levelsFolder;

	CATLControlsModel*  m_pModel;
	QATLTreeModel*      m_pLayout;
	IAudioSystemEditor* m_pAudioSystemImpl;
	std::set<string>    m_loadedFilenames;
	uint                m_errorCodeMask;
};
}
