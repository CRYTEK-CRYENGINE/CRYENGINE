// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryString/CryString.h>
#include <IAudioConnection.h>
#include "AudioControl.h"
#include <CrySystem/XML/IXml.h>
#include <QModelIndex>
#include <CrySystem/ISystem.h>
#include "ACETypes.h"

class QStandardItemModel;

namespace ACE
{
class CATLControlsModel;
class IAudioSystemEditor;

struct SLibraryScope
{
	SLibraryScope()
		: bDirty(false)
	{
		pNodes[eACEControlType_Trigger] = GetISystem()->CreateXmlNode("AudioTriggers");
		pNodes[eACEControlType_RTPC] = GetISystem()->CreateXmlNode("AudioRTPCs");
		pNodes[eACEControlType_Switch] = GetISystem()->CreateXmlNode("AudioSwitches");
		pNodes[eACEControlType_Environment] = GetISystem()->CreateXmlNode("AudioEnvironments");
		pNodes[eACEControlType_Preload] = GetISystem()->CreateXmlNode("AudioPreloads");
	}
	XmlNodeRef pNodes[eACEControlType_NumTypes];
	bool       bDirty;
};

typedef std::map<Scope, SLibraryScope> TLibraryStorage;

class CAudioControlsWriter
{
public:
	CAudioControlsWriter(CATLControlsModel* pATLModel, QStandardItemModel* pLayoutModel, IAudioSystemEditor* pAudioSystemImpl, std::set<string>& previousLibraryPaths);

private:
	void WriteLibrary(const string& sLibraryName, QModelIndex root);
	void WriteItem(QModelIndex index, const string& sPath, TLibraryStorage& library, bool bParentModified);
	void WriteControlToXML(XmlNodeRef pNode, CATLControl* pControl, const string& sPath);
	void WriteConnectionsToXML(XmlNodeRef pNode, CATLControl* pControl, const int platformIndex = -1);
	bool IsItemModified(QModelIndex index) const;
	void WriteEditorData(const QModelIndex& parentIndex, XmlNodeRef pParentNode) const;

	void CheckOutFile(const string& filepath);
	void DeleteLibraryFile(const string& filepath);

	CATLControlsModel*  m_pATLModel;
	QStandardItemModel* m_pLayoutModel;
	IAudioSystemEditor* m_pAudioSystemImpl;

	std::set<string>    m_foundLibraryPaths;

	static const string ms_sControlsPath;
	static const string ms_sLevelsFolder;
	static const uint   ms_currentFileVersion;
};
}
