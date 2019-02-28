// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __MANNEQUIN_FILE_CHANGE_WRITER__H__
#define __MANNEQUIN_FILE_CHANGE_WRITER__H__

#include <ICryMannequinEditor.h>
#include "../MannFileManager.h"

enum EFileManagerResponse { eFMR_OK, eFMR_Cancel, eFMR_NoChanges };

struct SFileEntry
{
	XmlNodeRef     xmlNode;
	CString        filename;
	CString        typeDesc;
	EFileEntryType type;
};

class CMannequinFileChangeWriter
	: public IMannequinWriter
{
public:
	CMannequinFileChangeWriter(bool changedFilesMode = false);

	size_t            GetModifiedFilesCount() const;
	const SFileEntry& GetModifiedFileEntry(const size_t index) const;

	void              WriteNewFile(const char* filename);
	void              UndoModifiedFile(const CString& filename);

	bool              SaveFile(const char* filename, XmlNodeRef xmlNode, CString& path);

	// IMannequinWriter
	virtual void SaveFile(const char* filename, XmlNodeRef xmlNode, EFileEntryType fileEntryType);
	virtual void WriteModifiedFiles();
	// ~IMannequinWriter

	void                 SetFilterFilesByControllerDef(bool filterFilesByControllerDef);
	bool                 GetFilterFilesByControllerDef() const;

	void                 SetControllerDef(const SControllerDef* pControllerDef);

	EFileManagerResponse ShowFileManager(bool reexportAll = false);
	void                 RefreshModifiedFiles();

	static bool          UpdateActiveWriter()
	{
		if (sm_pActiveWriter)
		{
			sm_pActiveWriter->RefreshModifiedFiles();
			sm_pActiveWriter->m_fileManager.OnRefresh();

			return true;
		}
		return false;
	}

private:
	void        AddEntry(const CString& filename, XmlNodeRef xmlNode, EFileEntryType fileEntryType);
	SFileEntry* FindEntryByFilename(const CString& filename);
	void        SetModifiedFileType(SFileEntry& fileEntry) const;

private:

	CMannFileManager                   m_fileManager;
	typedef std::vector<SFileEntry> FileEntryVec;
	FileEntryVec                       m_modifiedFiles;
	const SControllerDef*              m_pControllerDef;
	bool                               m_filterFilesByControllerDef;
	bool                               m_changedFilesMode;
	bool                               m_reexportAll;

	static CMannequinFileChangeWriter* sm_pActiveWriter;
};

#endif

