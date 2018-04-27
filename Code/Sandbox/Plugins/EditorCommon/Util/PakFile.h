// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/File/ICryPak.h>

// forward references.
struct ICryArchive;
class CCryMemFile;
class CMemoryBlock;
TYPEDEF_AUTOPTR(ICryArchive);

/*!	CPakFile Wraps game implementation of ICryArchive.
    Used for storing multiple files into zip archive file.
 */
class EDITOR_COMMON_API CPakFile
{
public:
	CPakFile();
	CPakFile(ICryPak* pCryPak);
	~CPakFile();
	//! Opens archive for writing.
	explicit CPakFile(const char* filename);
	//! Opens archive for writing.
	bool Open(const char* filename, bool bAbsolutePath = true);
	//! Opens archive for reading only.
	bool OpenForRead(const char* filename);

	void Close();
	//! Adds or update file in archive.
	bool UpdateFile(const char* filename, CCryMemFile& file, bool bCompress = true);
	//! Adds or update file in archive.
	bool UpdateFile(const char* filename, CMemoryBlock& mem, bool bCompress = true, int nCompressLevel = ICryArchive::LEVEL_BETTER);
	//! Adds or update file in archive.
	bool UpdateFile(const char* filename, void* pBuffer, int nSize, bool bCompress = true, int nCompressLevel = ICryArchive::LEVEL_BETTER);
	//! Remove file from archive.
	bool RemoveFile(const char* filename);
	//! Remove dir from archive.
	bool RemoveDir(const char* directory);

	//! Return archive of this pak file wrapper.
	ICryArchive* GetArchive() { return m_pArchive; };
private:
	ICryArchive_AutoPtr m_pArchive;
	ICryPak*            m_pCryPak;
};


