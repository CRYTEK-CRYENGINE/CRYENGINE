// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CryArchive.h
//  Created:     18/08/2010 by Timur.
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CRY_ARCHIVE_HDR__
#define __CRY_ARCHIVE_HDR__

#include "CryPak.h"

struct CryArchiveSortByName
{
	bool operator()(const ICryArchive* left, const ICryArchive* right) const
	{
		return stricmp(left->GetFullPath(), right->GetFullPath()) < 0;
	}
	bool operator()(const char* left, const ICryArchive* right) const
	{
		return stricmp(left, right->GetFullPath()) < 0;
	}
	bool operator()(const ICryArchive* left, const char* right) const
	{
		return stricmp(left->GetFullPath(), right) < 0;
	}
};

template<class Cache>
class TCryArchive : public ICryArchive
{
public:
	TCryArchive(CCryPak* pPak, const string& strBindRoot, Cache* pCache, unsigned nFlags = 0) :
		m_pCache(pCache),
		m_strBindRoot(strBindRoot),
		m_pPak(pPak),
		m_nFlags(nFlags)
	{
		pPak->Register(this);
	}

	~TCryArchive()
	{
		m_pPak->Unregister(this);
	}

	// finds the file; you don't have to close the returned handle
	Handle FindFile(const char* szRelativePath)
	{
		char szFullPath[CCryPak::g_nMaxPath];
		const char* pPath = AdjustPath(szRelativePath, szFullPath);
		if (!pPath)
			return NULL;
		return m_pCache->FindFile(pPath);
	}

	// returns the size of the file (unpacked) by the handle
	unsigned GetFileSize(Handle h)
	{
		assert(m_pCache->IsOwnerOf((ZipDir::FileEntry*)h));
		return ((ZipDir::FileEntry*)h)->desc.lSizeUncompressed;
	}

	// reads the file into the preallocated buffer (must be at least the size of GetFileSize())
	int ReadFile(Handle h, void* pBuffer)
	{
		assert(m_pCache->IsOwnerOf((ZipDir::FileEntry*)h));
		return m_pCache->ReadFile((ZipDir::FileEntry*)h, NULL, pBuffer);
	}

	// returns the full path to the archive file
	const char* GetFullPath() const
	{
		return m_pCache->GetFilePath();
	}

	unsigned GetFlags() const { return m_nFlags; }
	bool     SetFlags(unsigned nFlagsToSet)
	{
		if (nFlagsToSet & FLAGS_RELATIVE_PATHS_ONLY)
			m_nFlags |= FLAGS_RELATIVE_PATHS_ONLY;

		if (nFlagsToSet & FLAGS_ON_HDD)
			m_nFlags |= FLAGS_ON_HDD;

		if (nFlagsToSet & FLAGS_RELATIVE_PATHS_ONLY ||
		    nFlagsToSet & FLAGS_ON_HDD)
		{
			// we don't support changing of any other flags
			return true;
		}
		return false;
	}

	bool ResetFlags(unsigned nFlagsToReset)
	{
		if (nFlagsToReset & FLAGS_RELATIVE_PATHS_ONLY)
			m_nFlags &= ~FLAGS_RELATIVE_PATHS_ONLY;

		if (nFlagsToReset & ~(FLAGS_RELATIVE_PATHS_ONLY))
		{
			// we don't support changing of any other flags
			return false;
		}
		return true;
	}

	bool SetPackAccessible(bool bAccessible)
	{
		if (bAccessible)
		{
			bool bResult = (m_nFlags& ICryArchive::FLAGS_DISABLE_PAK) != 0;
			m_nFlags &= ~ICryArchive::FLAGS_DISABLE_PAK;
			return bResult;
		}
		else
		{
			bool bResult = (m_nFlags& ICryArchive::FLAGS_DISABLE_PAK) == 0;
			m_nFlags |= ICryArchive::FLAGS_DISABLE_PAK;
			return bResult;
		}
	}

	Cache* GetCache() { return m_pCache; }
protected:
	// returns the pointer to the relative file path to be passed
	// to the underlying Cache pointer. Uses the given buffer to construct the path.
	// returns NULL if the file path is invalid
	const char* AdjustPath(const char* szRelativePath, char szFullPathBuf[CCryPak::g_nMaxPath])
	{
		if (!szRelativePath[0])
			return NULL;

		if (m_nFlags & FLAGS_RELATIVE_PATHS_ONLY)
			return szRelativePath;

		if (szRelativePath[1] == ':' || (m_nFlags & FLAGS_ABSOLUTE_PATHS))
		{
			// make the normalized full path and try to match it against the binding root of this object
			const char* szFullPath = m_pPak->AdjustFileName(szRelativePath, szFullPathBuf, ICryPak::FLAGS_PATH_REAL);
			size_t nPathLen = strlen(szFullPath);
			if (nPathLen <= m_strBindRoot.length())
				return NULL;

			// you should access exactly the file under the directly in which the zip is situated
			if (szFullPath[m_strBindRoot.length()] != '/' && szFullPath[m_strBindRoot.length()] != '\\')
				return NULL;
			if (memicmp(szFullPath, m_strBindRoot.c_str(), m_strBindRoot.length()))
				return NULL; // the roots don't match

			return szFullPath + m_strBindRoot.length() + 1;
		}

		return szRelativePath;
	}
protected:
	_smart_ptr<Cache> m_pCache;
	// the binding root may be empty string - in this case, the absolute path binding won't work
	string            m_strBindRoot;
	CCryPak*          m_pPak;
	unsigned          m_nFlags;
};

#ifndef OPTIMIZED_READONLY_ZIP_ENTRY
class CryArchiveRW : public TCryArchive<ZipDir::CacheRW>
{
public:

	CryArchiveRW(CCryPak* pPak, const string& strBindRoot, ZipDir::CacheRW* pCache, unsigned nFlags = 0) :
		TCryArchive<ZipDir::CacheRW>(pPak, strBindRoot, pCache, nFlags)
	{
	}

	~CryArchiveRW()
	{
	}

	int    EnumEntries(Handle hFolder, IEnumerateArchiveEntries* pEnum);
	Handle GetRootFolderHandle();

	// Adds a new file to the zip or update an existing one
	// adds a directory (creates several nested directories if needed)
	// compression methods supported are 0 (store) and 8 (deflate) , compression level is 0..9 or -1 for default (like in zlib)
	int UpdateFile(const char* szRelativePath, void* pUncompressed, unsigned nSize, unsigned nCompressionMethod = 0, int nCompressionLevel = -1);

	//   Adds a new file to the zip or update an existing one if it is not compressed - just stored  - start a big file
	int StartContinuousFileUpdate(const char* szRelativePath, unsigned nSize);

	// Adds a new file to the zip or update an existing's segment if it is not compressed - just stored
	// adds a directory (creates several nested directories if needed)
	// Arguments:
	//   nOverwriteSeekPos - 0xffffffff means the seek pos should not be overwritten
	int         UpdateFileContinuousSegment(const char* szRelativePath, unsigned nSize, void* pUncompressed, unsigned nSegmentSize, unsigned nOverwriteSeekPos);

	virtual int UpdateFileCRC(const char* szRelativePath, const uint32 dwCRC);

	// deletes the file from the archive
	int RemoveFile(const char* szRelativePath);

	// deletes the directory, with all its descendants (files and subdirs)
	int RemoveDir(const char* szRelativePath);

	int RemoveAll();

	enum {gClassId = 1};
	unsigned GetClassId() const { return gClassId; }

	void     GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
	}
};
#endif //#ifndef OPTIMIZED_READONLY_ZIP_ENTRY

class CryArchive : public TCryArchive<ZipDir::Cache>
{
public:
	CryArchive(CCryPak* pPak, const string& strBindRoot, ZipDir::Cache* pCache, unsigned nFlags) :
		TCryArchive<ZipDir::Cache>(pPak, strBindRoot, pCache, nFlags)
	{}

	~CryArchive(){}

	int    EnumEntries(Handle hFolder, IEnumerateArchiveEntries* pEnum) { return 0; };
	Handle GetRootFolderHandle()                                        { return NULL; };

	// Adds a new file to the zip or update an existing one
	// adds a directory (creates several nested directories if needed)
	// compression methods supported are METHOD_STORE == 0 (store) and
	// METHOD_DEFLATE == METHOD_COMPRESS == 8 (deflate) , compression
	// level is LEVEL_FASTEST == 0 till LEVEL_BEST == 9 or LEVEL_DEFAULT == -1
	// for default (like in zlib)
	int UpdateFile(const char* szRelativePath, void* pUncompressed, unsigned nSize, unsigned nCompressionMethod = 0, int nCompressionLevel = -1) { return ZipDir::ZD_ERROR_INVALID_CALL; }

	//   Adds a new file to the zip or update an existing one if it is not compressed - just stored  - start a big file
	int StartContinuousFileUpdate(const char* szRelativePath, unsigned nSize) { return ZipDir::ZD_ERROR_INVALID_CALL; }

	// Adds a new file to the zip or update an existing's segment if it is not compressed - just stored
	// adds a directory (creates several nested directories if needed)
	int         UpdateFileContinuousSegment(const char* szRelativePath, unsigned nSize, void* pUncompressed, unsigned nSegmentSize, unsigned nOverwriteSeekPos) { return ZipDir::ZD_ERROR_INVALID_CALL; }

	virtual int UpdateFileCRC(const char* szRelativePath, const uint32 dwCRC)                                                                                   { return ZipDir::ZD_ERROR_INVALID_CALL; }

	// deletes the file from the archive
	int RemoveFile(const char* szRelativePath) { return ZipDir::ZD_ERROR_INVALID_CALL; }
	int RemoveAll()                            { return ZipDir::ZD_ERROR_INVALID_CALL; }

	// deletes the directory, with all its descendants (files and subdirs)
	int      RemoveDir(const char* szRelativePath) { return ZipDir::ZD_ERROR_INVALID_CALL; }
	enum {gClassId = 2};
	unsigned GetClassId() const                    { return gClassId; }

	void     GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
	}
};
#endif //__CRY_ARCHIVE_HDR__
