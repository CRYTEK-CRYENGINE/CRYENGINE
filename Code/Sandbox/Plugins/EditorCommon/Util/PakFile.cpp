// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Util/PakFile.h"
#include "Util/CryMemFile.h"
#include "Util/MemoryBlock.h"

//////////////////////////////////////////////////////////////////////////
CPakFile::CPakFile()
	: m_pArchive(NULL),
	m_pCryPak(NULL)
{
}

//////////////////////////////////////////////////////////////////////////
CPakFile::CPakFile(ICryPak* pCryPak)
	: m_pArchive(NULL),
	m_pCryPak(pCryPak)
{
}

//////////////////////////////////////////////////////////////////////////
CPakFile::~CPakFile()
{
	Close();
}

//////////////////////////////////////////////////////////////////////////
CPakFile::CPakFile(const char* filename)
{
	m_pArchive = NULL;
	Open(filename);
}

//////////////////////////////////////////////////////////////////////////
void CPakFile::Close()
{
	m_pArchive = NULL;
}

//////////////////////////////////////////////////////////////////////////
bool CPakFile::Open(const char* filename, bool bAbsolutePath)
{
	if (m_pArchive)
		Close();

	ICryPak* pCryPak = m_pCryPak ? m_pCryPak : GetIEditor()->GetSystem()->GetIPak();
	if (pCryPak == NULL)
		return false;

	if (bAbsolutePath)
		m_pArchive = pCryPak->OpenArchive(filename, ICryArchive::FLAGS_ABSOLUTE_PATHS);
	else
		m_pArchive = pCryPak->OpenArchive(filename);
	if (m_pArchive)
		return true;
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CPakFile::OpenForRead(const char* filename)
{
	if (m_pArchive)
		Close();
	ICryPak* pCryPak = m_pCryPak ? m_pCryPak : GetIEditor()->GetSystem()->GetIPak();
	if (pCryPak == NULL)
		return false;
	m_pArchive = pCryPak->OpenArchive(filename, ICryArchive::FLAGS_OPTIMIZED_READ_ONLY | ICryArchive::FLAGS_ABSOLUTE_PATHS);
	if (m_pArchive)
		return true;
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CPakFile::UpdateFile(const char* filename, CCryMemFile& file, bool bCompress)
{
	if (m_pArchive)
	{
		int nSize = file.GetLength();

		UpdateFile(filename, file.GetMemPtr(), nSize, bCompress);
		file.Close();
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CPakFile::UpdateFile(const char* filename, CMemoryBlock& mem, bool bCompress, int nCompressLevel)
{
	if (m_pArchive)
	{
		return UpdateFile(filename, mem.GetBuffer(), mem.GetSize(), bCompress, nCompressLevel);
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////
bool CPakFile::UpdateFile(const char* filename, void* pBuffer, int nSize, bool bCompress, int nCompressLevel)
{
	if (m_pArchive)
	{
		if (bCompress)
			return 0 == m_pArchive->UpdateFile(filename, pBuffer, nSize, ICryArchive::METHOD_DEFLATE, nCompressLevel);
		else
			return 0 == m_pArchive->UpdateFile(filename, pBuffer, nSize);
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CPakFile::RemoveFile(const char* filename)
{
	if (m_pArchive)
	{
		return m_pArchive->RemoveFile(filename);
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CPakFile::RemoveDir(const char* directory)
{
	if (m_pArchive)
	{
		return m_pArchive->RemoveDir(directory);
	}
	return false;
}

