// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "MannequinFileChangeWriter.h"

#include <CryGame/IGameFramework.h>
#include "../MannequinDialog.h"
#include "Util/FileUtil.h"

CMannequinFileChangeWriter* CMannequinFileChangeWriter::sm_pActiveWriter = NULL;

//////////////////////////////////////////////////////////////////////////
#pragma warning(push)
#pragma warning(disable:4355) // 'this': used in base member initializer list
CMannequinFileChangeWriter::CMannequinFileChangeWriter(bool changedFilesMode)
	: m_pControllerDef(NULL)
	, m_filterFilesByControllerDef(true)
	, m_changedFilesMode(changedFilesMode)
	, m_reexportAll(false)
	, m_fileManager(*this, changedFilesMode, CMannequinDialog::GetCurrentInstance())
{
}
#pragma warning(pop)

//////////////////////////////////////////////////////////////////////////
EFileManagerResponse CMannequinFileChangeWriter::ShowFileManager(const bool reexportAll /* =false */)
{
	m_reexportAll = reexportAll;
	RefreshModifiedFiles();

	// Abort early if there are no modified files
	if (m_modifiedFiles.empty())
	{
		return eFMR_NoChanges;
	}

	sm_pActiveWriter = this;

	EFileManagerResponse result = eFMR_NoChanges;
	switch (m_fileManager.DoModal())
	{
	case IDOK:
		result = eFMR_OK;
		break;
	case IDCANCEL:
		result = eFMR_Cancel;
		break;
	}

	sm_pActiveWriter = NULL;

	return result;
}

//////////////////////////////////////////////////////////////////////////
static void SaveTagDefinitionAndImports(IMannequinEditorManager* const pMannequinEditorManager, CMannequinFileChangeWriter* const pWriter, const CTagDefinition* const pTagdefinition)
{
	assert(pMannequinEditorManager);
	assert(pWriter);
	assert(pTagdefinition);

	pMannequinEditorManager->SaveTagDefinition(pWriter, pTagdefinition);

	DynArray<CTagDefinition*> includedTagDefs;
	pMannequinEditorManager->GetIncludedTagDefs(pTagdefinition, includedTagDefs);

	for (int i = 0; i < includedTagDefs.size(); ++i)
	{
		assert(includedTagDefs[i]);
		pMannequinEditorManager->SaveTagDefinition(pWriter, includedTagDefs[i]);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMannequinFileChangeWriter::RefreshModifiedFiles()
{
	CMannequinDialog* const pMannequinDialog = CMannequinDialog::GetCurrentInstance();
	if (!pMannequinDialog)
	{
		return;
	}

	m_modifiedFiles.clear();

	IMannequin& mannequinSys = gEnv->pGameFramework->GetMannequinInterface();
	IMannequinEditorManager* pMannequinEditorManager = mannequinSys.GetMannequinEditorManager();

	if (m_reexportAll)
	{
		// Save all
		pMannequinEditorManager->SaveAll(this);
	}
	else
	{
		// Only save files directly referenced by the current project
		const SMannequinContexts& mannequinContexts = *pMannequinDialog->Contexts();
		if (mannequinContexts.m_controllerDef)
		{
			// Controller def, fragment definition and global tags
			pMannequinEditorManager->SaveControllerDef(this, mannequinContexts.m_controllerDef);
			SaveTagDefinitionAndImports(pMannequinEditorManager, this, &mannequinContexts.m_controllerDef->m_fragmentIDs);
			SaveTagDefinitionAndImports(pMannequinEditorManager, this, &mannequinContexts.m_controllerDef->m_tags);

			// Fragment specific tags
			const TagID numFragmentIDs = mannequinContexts.m_controllerDef->m_fragmentIDs.GetNum();
			for (TagID itFragmentID = 0; itFragmentID < numFragmentIDs; ++itFragmentID)
			{
				const CTagDefinition* pFragmentTagDef = mannequinContexts.m_controllerDef->GetFragmentTagDef(itFragmentID);
				if (pFragmentTagDef)
				{
					SaveTagDefinitionAndImports(pMannequinEditorManager, this, pFragmentTagDef);
				}
			}

			// Databases
			const uint32 numScopeContexts = mannequinContexts.m_contextData.size();
			for (uint32 itScopeContexts = 0; itScopeContexts != numScopeContexts; ++itScopeContexts)
			{
				const SScopeContextData& scopeContextData = mannequinContexts.m_contextData[itScopeContexts];
				if (scopeContextData.database)
				{
					pMannequinEditorManager->SaveDatabase(this, scopeContextData.database);
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
size_t CMannequinFileChangeWriter::GetModifiedFilesCount() const
{
	return m_modifiedFiles.size();
}

//////////////////////////////////////////////////////////////////////////
const SFileEntry& CMannequinFileChangeWriter::GetModifiedFileEntry(const size_t index) const
{
	assert(index < m_modifiedFiles.size());

	return m_modifiedFiles[index];
}

//////////////////////////////////////////////////////////////////////////
void CMannequinFileChangeWriter::SetModifiedFileType(SFileEntry& fileEntry) const
{
	switch (fileEntry.type)
	{
	case eFET_Database:
		fileEntry.typeDesc = "Animation Database";
		break;
	case eFET_ControllerDef:
		fileEntry.typeDesc = "Controller Definition";
		break;
	case eFET_TagDef:
		fileEntry.typeDesc = "Tag Definition";
		break;
	default:
		fileEntry.typeDesc = "Unknown Type";
	}
}

//////////////////////////////////////////////////////////////////////////
void CMannequinFileChangeWriter::WriteModifiedFiles()
{
	const size_t modifiedFilesCount = m_modifiedFiles.size();
	for (size_t i = 0; i < modifiedFilesCount; ++i)
	{
		SFileEntry& entry = m_modifiedFiles[i];

		entry.xmlNode->saveToFile(entry.filename);
	}
	m_modifiedFiles.clear();
}

//////////////////////////////////////////////////////////////////////////
void CMannequinFileChangeWriter::WriteNewFile(const char* filename)
{
	for (FileEntryVec::iterator it = m_modifiedFiles.begin(); it != m_modifiedFiles.end(); ++it)
	{
		if (it->filename == filename)
		{
			SFileEntry& entry = *it;
			entry.xmlNode->saveToFile(entry.filename);
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMannequinFileChangeWriter::UndoModifiedFile(const CString& filename)
{
	for (FileEntryVec::iterator it = m_modifiedFiles.begin(); it != m_modifiedFiles.end(); ++it)
	{
		if (it->filename == filename)
		{
			SFileEntry& entry = *it;

			IMannequinEditorManager* manager = gEnv->pGameFramework->GetMannequinInterface().GetMannequinEditorManager();

			switch (entry.type)
			{
			case eFET_Database:
				manager->RevertDatabase(entry.filename);
				break;
			case eFET_ControllerDef:
				manager->RevertControllerDef(entry.filename);
				break;
			case eFET_TagDef:
				manager->RevertTagDef(entry.filename);
				break;
			}

			m_modifiedFiles.erase(it);
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMannequinFileChangeWriter::AddEntry(const CString& filename, XmlNodeRef xmlNode, EFileEntryType fileEntryType)
{
	//if ( m_filterFilesByControllerDef )
	//{
	//	if ( m_pControllerDef )
	//	{
	//		IMannequin &mannequinSys = gEnv->pGameFramework->GetMannequinInterface();
	//		const bool fileUsedByControllerDef = mannequinSys.GetMannequinEditorManager()->IsFileUsedByControllerDef( *m_pControllerDef, filename );
	//		if ( ! fileUsedByControllerDef )
	//		{
	//			return;
	//		}
	//	}
	//}

	SFileEntry* pEntry = FindEntryByFilename(filename);
	if (pEntry)
	{
		pEntry->xmlNode = xmlNode;
		pEntry->type = fileEntryType;
		SetModifiedFileType(*pEntry);
		return;
	}

	SFileEntry entry;
	entry.filename = filename;
	entry.xmlNode = xmlNode;
	entry.type = fileEntryType;
	SetModifiedFileType(entry);

	m_modifiedFiles.push_back(entry);
}

//////////////////////////////////////////////////////////////////////////
SFileEntry* CMannequinFileChangeWriter::FindEntryByFilename(const CString& filename)
{
	const size_t modifiedFilesCount = m_modifiedFiles.size();
	for (size_t i = 0; i < modifiedFilesCount; ++i)
	{
		SFileEntry& entry = m_modifiedFiles[i];
		const bool filenamesMatch = (filename == entry.filename);
		if (filenamesMatch)
		{
			return &entry;
		}
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
bool CMannequinFileChangeWriter::SaveFile(const char* filename, XmlNodeRef xmlNode, CString& path)
{
	assert(filename);
	assert(xmlNode);

	path = filename;
	PathUtil::ToUnixPath(path.GetBuffer());

	const XmlNodeRef xmlNodeOnDisk = GetISystem()->LoadXmlFromFile(path);
	if (!xmlNodeOnDisk)
	{
		return true;
	}

	IXmlUtils* pXmlUtils = GetISystem()->GetXmlUtils();
	assert(pXmlUtils);

	const CString hashXmlNode = pXmlUtils->HashXml(xmlNode);
	const CString hashXmlNodeOnDisk = pXmlUtils->HashXml(xmlNodeOnDisk);
	const bool fileContentsMatch = (hashXmlNode == hashXmlNodeOnDisk);

	return !fileContentsMatch;
}

//////////////////////////////////////////////////////////////////////////
void CMannequinFileChangeWriter::SaveFile(const char* filename, XmlNodeRef xmlNode, EFileEntryType fileEntryType)
{
	CString path;
	if (SaveFile(filename, xmlNode, path))
		AddEntry(path, xmlNode, fileEntryType);
}

//////////////////////////////////////////////////////////////////////////
void CMannequinFileChangeWriter::SetFilterFilesByControllerDef(bool filterFilesByControllerDef)
{
	m_filterFilesByControllerDef = filterFilesByControllerDef;
}

//////////////////////////////////////////////////////////////////////////
bool CMannequinFileChangeWriter::GetFilterFilesByControllerDef() const
{
	return m_filterFilesByControllerDef;
}

//////////////////////////////////////////////////////////////////////////
void CMannequinFileChangeWriter::SetControllerDef(const SControllerDef* pControllerDef)
{
	m_pControllerDef = pControllerDef;
}

