// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   SaveReaderWriter_Memory.cpp
//  Created:     30/4/2010 by Ian Masters.
//  Description: Simple in-memory file system for saving/loading files.
// -------------------------------------------------------------------------
//  History:
//
//  TODO:
//  - Add transfer capability so memory saves can be copied to a real
//    storage device once one is available.
////////////////////////////////////////////////////////////////////////////

#include <StdAfx.h>
#include <CryMemory/STLGlobalAllocator.h>
#include "SaveReaderWriter_Memory.h"
#include "PatternMatcher.h"
#include <CryCore/Platform/CryWindows.h>

////////////////////////////////////////////////////////////////////////////
// CMemoryFile
////////////////////////////////////////////////////////////////////////////

class CMemoryFile : public IMemoryFile
{
	friend class CMemoryFileSystem;

protected:

	bool         m_bDirty;
	unsigned int m_user;
	string       m_filePath;
	long         m_filePtr;
	FILETIME     m_fileTime;

public:
	CMemoryFile(const char* fileName, unsigned int userIndex)
		: m_user(userIndex)
		, m_filePath(fileName)
		, m_filePtr(0)
		, m_bDirty(true)
	{
		m_filePath.replace('\\', '/');
	}

	virtual IPlatformOS::EFileOperationCode AppendBytes(const void* data, size_t length) = 0;

	// Reset file length to zero but do not free reserved data to reduce fragmentation
	virtual void                            Truncate() = 0;

	virtual long                            GetFilePos() const { return m_filePtr; }

	virtual IPlatformOS::EFileOperationCode Seek(long seek, IPlatformOS::ISaveReader::ESeekMode mode)
	{
		long fileSize = static_cast<long>(GetLength());

		switch (mode)
		{
		case IPlatformOS::ISaveReader::ESM_BEGIN:
			break;

		case IPlatformOS::ISaveReader::ESM_CURRENT:
			seek = static_cast<long>(m_filePtr) + seek;
			break;

		case IPlatformOS::ISaveReader::ESM_END:
			seek = fileSize + seek;
			break;

		default:
			assert(!"Invalid seek mode");
			return IPlatformOS::eFOC_Failure;
		}

		if (seek < 0 || seek > fileSize)
			return IPlatformOS::eFOC_Failure;

		m_filePtr = seek;
		return IPlatformOS::eFOC_Success;
	}

	virtual const char* GetFilePath() const
	{
		//size_t offset = m_filePath.rfind('/');
		//if(offset != string::npos)
		//	return &m_filePath[offset + 1];
		return m_filePath.c_str();
	}

	virtual bool IsDirty() const
	{
		return m_bDirty;
	}

	virtual void SetDirty(bool bDirty)
	{
		m_bDirty = bDirty;
	}

	virtual void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddString(m_filePath);
	}

	virtual size_t      Resize(size_t requiredBytes) = 0;

	static inline int64 makeint64(DWORD high, DWORD low)
	{
		return (static_cast<int64>(high) << 32) | static_cast<int64>(low);
	}

	void FillFindData(_finddata_t& fd)
	{
		fd.attrib = FILE_ATTRIBUTE_NORMAL;
		fd.time_access = fd.time_create = fd.time_write = makeint64(m_fileTime.dwHighDateTime, m_fileTime.dwLowDateTime);
		fd.size = GetLength();
		cry_strcpy(fd.name, GetFilePath());
	}

	bool operator<(const CMemoryFile& file) const
	{
		return m_user < file.m_user || strcmp(m_filePath.c_str(), file.m_filePath.c_str()) < 0;
	}

	bool operator==(const CMemoryFile& file) const
	{
		return m_user == file.m_user && !m_filePath.compareNoCase(file.m_filePath.c_str());
	}

	IPlatformOS::EFileOperationCode ReadFromHFile(FILE* hFile, size_t length)
	{
		size_t offset = Resize(length);
		uint8* buffer = (uint8*)GetFileContents();
		size_t count = fread(buffer + offset, 1, length, hFile);
		m_filePtr = offset + count;
		SetDirty(true);
		return count == length ? IPlatformOS::eFOC_Success : IPlatformOS::eFOC_ErrorRead;
	}

};

////////////////////////////////////////////////////////////////////////////
// CMemoryFileStandard
////////////////////////////////////////////////////////////////////////////

class CMemoryFileStandard : public CMemoryFile
{
	enum { ePAGE_SIZE = 64 * 1024 };

	std::vector<unsigned char, stl::STLGlobalAllocator<unsigned char>> m_fileBytes;

public:

	CMemoryFileStandard(const char* fileName, unsigned int userIndex) : CMemoryFile(fileName, userIndex)
	{
		m_fileBytes.reserve(ePAGE_SIZE);
		Truncate();
	}

	virtual IPlatformOS::EFileOperationCode AppendBytes(const void* data, size_t length)
	{
		size_t offset = m_fileBytes.size();
		if (offset + length > m_fileBytes.capacity())
			m_fileBytes.reserve(m_fileBytes.capacity() + ePAGE_SIZE);
		m_fileBytes.resize(offset + length);
		memcpy(&m_fileBytes[offset], data, length);
		m_filePtr = offset + length;
		SetDirty(true);
		return IPlatformOS::eFOC_Success;
	}

	virtual size_t GetLength() const
	{
		return m_fileBytes.size();
	}

	virtual void Truncate()
	{
		m_fileBytes.resize(0);
		m_filePtr = 0;

		time_t unixtime;
		time(&unixtime);
		// Borrowed from UnixTimeToFileTime in LocalizedStringManager.cpp
#ifndef Int32x32To64
	#define Int32x32To64(a, b) ((uint64)((uint64)(a)) * (uint64)((uint64)(b)))
#endif
		LONGLONG longlong = Int32x32To64(unixtime, 10000000) + 116444736000000000LL;
		m_fileTime.dwLowDateTime = (DWORD)longlong;
		m_fileTime.dwHighDateTime = (DWORD)(longlong >> 32);
	}

	virtual const void* GetFileContents() const
	{
		return &m_fileBytes.front();
	}

	virtual void GetMemoryUsage(ICrySizer* pSizer) const
	{
		CMemoryFile::GetMemoryUsage(pSizer);
		pSizer->Add(*this);
		pSizer->AddContainer(m_fileBytes);
	}

	virtual size_t Resize(size_t requiredBytes)
	{
		const size_t offset = m_fileBytes.size();
		m_fileBytes.resize(offset + requiredBytes);
		return offset;
	}
};

////////////////////////////////////////////////////////////////////////////
// CMemoryFileSystem
////////////////////////////////////////////////////////////////////////////

class CMemoryFileSystem : public IMemoryFileSystem
{
protected:

	typedef std::vector<IMemoryFilePtr, stl::STLGlobalAllocator<IMemoryFilePtr>> FileList;
	FileList           m_files;
	CryCriticalSection m_mutex;

	struct mem_finddata_t
	{
		FileList::const_iterator it;
		unsigned int             user;
		string                   filePattern;
	};

protected:

	virtual ~CMemoryFileSystem() {}

public:

	CMemoryFileSystem()
	{
		m_files.reserve(8);
	}

	virtual IMemoryFilePtr CreateFile(const char* fileName, unsigned int userIndex)
	{
		Lock();
		IMemoryFilePtr file(new CMemoryFileStandard(fileName, userIndex));
		LinkFile(file);
		Unlock();
		return file;
	}

	virtual void LinkFile(IMemoryFilePtr file)
	{
		Lock();
		// If the file already exists, delete the old one and overwrite
		FileList::iterator it = std::find(m_files.begin(), m_files.end(), file);
		if (it != m_files.end())
			m_files.erase(it);
		m_files.push_back(file);
		Unlock();
	}

	virtual bool UnlinkFile(IMemoryFilePtr file)
	{
		Lock();
		bool result = stl::find_and_erase(m_files, file);
		Unlock();
		return result;
	}

	virtual IMemoryFilePtr GetFile(const char* fileName, unsigned int user)
	{
		Lock();
		IMemoryFilePtr fileptr;
		string fname(fileName);
		fname.replace('\\', '/');
		for (FileList::const_iterator it = m_files.begin(); it != m_files.end(); ++it)
		{
			CMemoryFile* file = static_cast<CMemoryFile*>(it->get());
			if (file->m_user == user && !file->m_filePath.compareNoCase(fname.c_str()))
			{
				fileptr = *it;
				break;
			}
		}
		Unlock();
		return fileptr;
	}

	virtual int GetNumFiles(unsigned int user)
	{
		Lock();
		int count = 0;
		for (FileList::const_iterator it = m_files.begin(); it != m_files.end(); ++it)
		{
			CMemoryFile* file = static_cast<CMemoryFile*>(it->get());
			if (file->m_user == user)
			{
				++count;
			}
		}
		Unlock();
		return count;
	}

	virtual int GetNumDirtyFiles(unsigned int user)
	{
		Lock();
		int dirty = 0;
		for (FileList::const_iterator it = m_files.begin(); it != m_files.end(); ++it)
		{
			CMemoryFile* file = static_cast<CMemoryFile*>(it->get());
			if (file->m_user == user)
			{
				if (file->IsDirty())
					++dirty;
			}
		}
		Unlock();
		return dirty;
	}

	virtual intptr_t FindFirst(unsigned int userIndex, const char* filePattern, _finddata_t* fd)
	{
		Lock();
		intptr_t result = -1;
		//assert(userIndex != IPlatformOS::Unknown_User);
		assert(fd != NULL);
		mem_finddata_t* find = new mem_finddata_t;
		find->user = userIndex;
		find->filePattern = filePattern;
		for (find->it = m_files.begin(); find->it != m_files.end(); ++find->it)
		{
			CMemoryFile* file = static_cast<CMemoryFile*>(find->it->get());
			if (file->m_user == userIndex)
			{
				int match = PatternMatch(file->m_filePath, filePattern);
				if (match)
				{
					file->FillFindData(*fd);
					result = reinterpret_cast<intptr_t>(find);
					break;
				}
			}
		}
		if (result == -1)
			delete find;
		Unlock();
		return result;
	}

	virtual int FindNext(intptr_t handle, _finddata_t* fd)
	{
		Lock();
		int result = -1;
		assert(fd != NULL);
		mem_finddata_t* find = reinterpret_cast<mem_finddata_t*>(handle);
		for (++find->it; find->it != m_files.end(); ++find->it)
		{
			CMemoryFile* file = static_cast<CMemoryFile*>(find->it->get());
			if (file->m_user == find->user)
			{
				int match = PatternMatch(file->m_filePath, find->filePattern);
				if (match)
				{
					file->FillFindData(*fd);
					result = 0;
					break;
				}
			}
		}
		Unlock();
		return result;
	}

	virtual int FindClose(intptr_t handle)
	{
		Lock();
		mem_finddata_t* find = reinterpret_cast<mem_finddata_t*>(handle);
		delete find;
		Unlock();
		return 0;
	}

	virtual void Format(unsigned int user)
	{
		Lock();
		for (FileList::iterator it = m_files.begin(); it != m_files.end(); )
		{
			CMemoryFile* file = static_cast<CMemoryFile*>(it->get());
			if (file->m_user == user)
				it = m_files.erase(it);
			else
				++it;
		}
		Unlock();
		CryLog("[CSaveWriter_Memory:%x] Formatted", user);
	}

	virtual bool TryLock()
	{
		return m_mutex.TryLock();
	}

	virtual void Lock()
	{
		m_mutex.Lock();
	}

	virtual void Unlock()
	{
		m_mutex.Unlock();
	}

	virtual void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->Add(*this);
		pSizer->AddContainer(m_files);
		for (FileList::const_iterator it = m_files.begin(); it != m_files.end(); ++it)
			(*it)->GetMemoryUsage(pSizer);
	}

	virtual bool DebugSave(unsigned int user, IPlatformOS::SDebugDump& dump)
	{
		size_t fileNameLen, fileLength;
		bool bOK = true;
		string saveName = "SaveGame.bin";

		if (dump.OpenFile(saveName, false))
		{
			for (FileList::const_iterator it = m_files.begin(); it != m_files.end(); ++it)
			{
				CMemoryFile* file = static_cast<CMemoryFile*>(it->get());

				fileNameLen = strlen(file->GetFilePath());
				bOK &= dump.WriteToFile(&fileNameLen, sizeof(fileNameLen), 1);
				if (!bOK) break;

				bOK &= dump.WriteToFile(file->GetFilePath(), fileNameLen, 1);
				if (!bOK) break;

				fileLength = file->GetLength();
				bOK &= dump.WriteToFile(&fileLength, sizeof(fileLength), 1);
				if (!bOK) break;

				bOK &= dump.WriteToFile(file->GetFileContents(), fileLength, 1);
				if (!bOK) break;
			}

			size_t endMarker = 0;
			bOK &= dump.WriteToFile(&endMarker, sizeof(endMarker), 1);

			dump.CloseFile();
		}
		return bOK;
	}

	static void CorrectUserName(string& fileName)
	{
		string path = fileName;
		path.MakeLower().replace('\\', '/');
		static const char* key = "%user%/profiles/";
		size_t offset = path.find(key);
		if (offset != string::npos)
			fileName.insert(offset + strlen(key), "default/");
	}

	virtual bool DebugLoad(unsigned int user, const char* fileName)
	{
		size_t fileNameLen, fileLength, count;
		bool bOK = true;
		string saveName;
		if (fileName)
		{
			if (!strchr(fileName, '.'))
				saveName += ".bin";
		}

		FILE* hFile = fxopen(saveName, "rb", false);
		if (hFile)
		{
			Format(user);

			for (;; )
			{
				count = fread(&fileNameLen, sizeof(fileNameLen), 1, hFile);
				bOK &= count == 1;
				if (!bOK) break;

				// End of file list?
				if (fileNameLen == 0) break;

				std::vector<char> fname;
				fname.resize(fileNameLen + 1);
				count = fread(&fname.front(), fileNameLen, 1, hFile);
				bOK &= count == 1;
				if (!bOK) break;
				fname[fileNameLen] = 0;

				count = fread(&fileLength, sizeof(fileLength), 1, hFile);
				bOK &= count == 1;
				if (!bOK) break;

				string filename = &fname.front();
				CorrectUserName(filename);

				IMemoryFilePtr file = CreateFile(filename, user);
				CMemoryFile* xfile = static_cast<CMemoryFile*>(file.get());
				bOK &= IPlatformOS::eFOC_Success == xfile->ReadFromHFile(hFile, fileLength);
				if (!bOK) break;
			}

			fclose(hFile);
		}
		else
		{
			bOK = false;
		}
		return bOK;
	}

	virtual void DumpFiles(const char* label)
	{
#ifndef _RELEASE
		Lock();
		CryLog("[IMemoryFileSystem] Dumping files - %s", label);
		for (FileList::const_iterator it = m_files.begin(); it != m_files.end(); ++it)
		{
			const CMemoryFile* file = static_cast<CMemoryFile*>(it->get());
			CryLog("[%u] %s, %s", file->m_user, file->GetFilePath(), file->m_bDirty ? "Dirty" : "Clean");
		}
		Unlock();
#endif // _RELEASE
	}
};

////////////////////////////////////////////////////////////////////////////
// CreateFileSystem
////////////////////////////////////////////////////////////////////////////

IMemoryFileSystem* IMemoryFileSystem::CreateFileSystem()
{
	return new CMemoryFileSystem;
}

////////////////////////////////////////////////////////////////////////////
// CSaveWriter_Memory
////////////////////////////////////////////////////////////////////////////

CSaveWriter_Memory::CSaveWriter_Memory(const IMemoryFileSystemPtr& pFileSystem, const char* fileName, unsigned int userIndex)
	: m_pFileSystem(pFileSystem)
	, m_eLastError(IPlatformOS::eFOC_Success)
	, m_user(userIndex)
	, m_bClosed(false)
{
	//assert(userIndex != IPlatformOS::Unknown_User);
	pFileSystem->Lock();
	m_file = pFileSystem->GetFile(fileName, userIndex);
	if (m_file != NULL)
	{
		CMemoryFile* file = static_cast<CMemoryFile*>(m_file.get());
		file->Truncate();
	}
	else
	{
		m_file = static_cast<CMemoryFileSystem*>(pFileSystem.get())->CreateFile(fileName, userIndex);
	}
	pFileSystem->Unlock();
	CryLog("[CSaveWriter_Memory:%x] \"%s\" opened for write", userIndex, fileName);
}

CSaveWriter_Memory::~CSaveWriter_Memory()
{
	// It should already be closed, but let's be safe
	assert(m_bClosed);
	Close();
}

IPlatformOS::EFileOperationCode CSaveWriter_Memory::AppendBytes(const void* data, size_t length)
{
	CMemoryFile* file = static_cast<CMemoryFile*>(m_file.get());
	return file->AppendBytes(data, length);
}

void CSaveWriter_Memory::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->Add(*this);
	m_file->GetMemoryUsage(pSizer);
}

////////////////////////////////////////////////////////////////////////////
// CSaveReader_Memory
////////////////////////////////////////////////////////////////////////////

CSaveReader_Memory::CSaveReader_Memory(const IMemoryFileSystemPtr& pFileSystem, const char* fileName, unsigned int userIndex)
	: m_pFileSystem(pFileSystem)
	, m_eLastError(IPlatformOS::eFOC_Success)
{
	//assert(userIndex != IPlatformOS::Unknown_User);
	pFileSystem->Lock();
	m_file = pFileSystem->GetFile(fileName, userIndex);
	if (m_file != NULL)
	{
		m_file->Seek(0, ESM_BEGIN);
		CryLog("[CSaveReader_Memory:%x] \"%s\" opened for read", userIndex, fileName);
	}
	else
		m_eLastError = IPlatformOS::eFOC_ErrorOpenRead;
	m_pFileSystem->Unlock();
}

CSaveReader_Memory::~CSaveReader_Memory()
{
}

IPlatformOS::EFileOperationCode CSaveReader_Memory::Seek(long seek, ESeekMode mode)
{
	return m_file->Seek(seek, mode);
}

IPlatformOS::EFileOperationCode CSaveReader_Memory::GetFileCursor(long& fileCursor)
{
	fileCursor = static_cast<long>(m_file->GetFilePos());
	return IPlatformOS::eFOC_Success;
}

IPlatformOS::EFileOperationCode CSaveReader_Memory::ReadBytes(void* data, size_t numBytes)
{
	assert(m_file->GetFilePos() + numBytes <= m_file->GetLength());
	long count = static_cast<long>(numBytes);
	count = min(count, static_cast<long>(m_file->GetLength() - m_file->GetFilePos()));
	if (count < (long)numBytes)
	{
		m_eLastError = IPlatformOS::eFOC_ErrorRead;
		return m_eLastError;
	}
	long filePos = m_file->GetFilePos();
	memcpy(data, &static_cast<const char*>(m_file->GetFileContents())[filePos], count);
	m_file->Seek(count, ESM_CURRENT);
	return IPlatformOS::eFOC_Success;
}

IPlatformOS::EFileOperationCode CSaveReader_Memory::GetNumBytes(size_t& numBytes)
{
	numBytes = m_file->GetLength();
	return IPlatformOS::eFOC_Success;
}

void CSaveReader_Memory::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->Add(*this);
	m_file->GetMemoryUsage(pSizer);
}

CFileFinderMemory::CFileFinderMemory(const IMemoryFileSystemPtr& pFileSystem, unsigned int user)
	: m_pFileSystem(pFileSystem)
	, m_user(user)
{
	if (m_pFileSystem)
		m_pFileSystem->Lock();
}
CFileFinderMemory::~CFileFinderMemory()
{
	if (m_pFileSystem)
		m_pFileSystem->Unlock();
}

intptr_t CFileFinderMemory::FindFirst(const char* filePattern, _finddata_t* fd)
{
	return m_pFileSystem->FindFirst(m_user, filePattern, fd);
}

int CFileFinderMemory::FindNext(intptr_t handle, _finddata_t* fd)
{
	return m_pFileSystem->FindNext(handle, fd);
}

int CFileFinderMemory::FindClose(intptr_t handle)
{
	return m_pFileSystem->FindClose(handle);
}

IPlatformOS::IFileFinder::EFileState CFileFinderMemory::FileExists(const char* path)
{
	IMemoryFilePtr file = m_pFileSystem->GetFile(path, m_user);
	return file ? eFS_File : eFS_NotExist;
}

void CFileFinderMemory::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->Add(*this);
}
