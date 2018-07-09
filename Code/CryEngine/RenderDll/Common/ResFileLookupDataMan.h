// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __RESFILELOOKUPDATAMAN_H__
#define __RESFILELOOKUPDATAMAN_H__

#include "ResFile.h"

typedef std::vector<CCryNameTSCRC> TResDirNames;

struct SResFileLookupDataDisk
{
	int    m_NumOfFilesUnique;
	uint32 m_OffsetDir;
	uint32 m_CRC32;
	uint16 m_CacheMajorVer;
	uint16 m_CacheMinorVer;

	SResFileLookupDataDisk() {}
	SResFileLookupDataDisk(const struct SResFileLookupData& inLookup);

	AUTO_STRUCT_INFO;
};
struct SResFileLookupData
{
	int    m_NumOfFilesUnique;
	uint32 m_OffsetDir;
	uint32 m_CRC32;
	uint16 m_CacheMajorVer;
	uint16 m_CacheMinorVer;

	SResFileLookupData() {}
	SResFileLookupData(const SResFileLookupDataDisk& inLookup)
		: m_NumOfFilesUnique(inLookup.m_NumOfFilesUnique)
		, m_OffsetDir(inLookup.m_OffsetDir)
		, m_CRC32(inLookup.m_CRC32)
		, m_CacheMajorVer(inLookup.m_CacheMajorVer)
		, m_CacheMinorVer(inLookup.m_CacheMinorVer)
#ifdef USE_PARTIAL_ACTIVATION
		, m_ContainsResDir(false)
#endif
	{
	}

#ifdef USE_PARTIAL_ACTIVATION
	bool         m_ContainsResDir;

	TResDirNames m_resdirlookup;
	ResDir       m_resdir;

	unsigned int GetDirOffset(const CCryNameTSCRC& dirEntryName) const;
#endif
};

struct SCFXLookupData
{
	uint32 m_CRC32;
	SCFXLookupData() {}
	AUTO_STRUCT_INFO;
};

#define MAX_DIR_BUFFER_SIZE 300000

typedef std::map<CCryNameTSCRC, SResFileLookupData> TFileResDirDataMap;
typedef std::map<CCryNameTSCRC, SCFXLookupData>     TFileCFXDataMap;

class CResFile;

struct SVersionInfo
{
	SVersionInfo() : m_ResVersion(0)
	{ memset(m_szCacheVer, 0, sizeof(m_szCacheVer)); }

	int  m_ResVersion;
	char m_szCacheVer[16];

	AUTO_STRUCT_INFO;
};

class CResFileLookupDataMan
{
public:
	CResFileLookupDataMan();
	~CResFileLookupDataMan();

	void                Clear();
	void                Flush();

	CCryNameTSCRC       AdjustName(const char* szName);
	int                 GetResVersion() const { return m_VersionInfo.m_ResVersion; }

	bool                LoadData(const char* acFilename, bool bSwapEndianRead, bool bReadOnly);
	void                SaveData(const char* acFilename, bool bSwapEndianWrite) const;

	bool                IsReadOnly() { return m_bReadOnly; }

	void                AddData(const CResFile* pResFile, uint32 CRC);
	void                AddDataCFX(const char* szPath, uint32 CRC);
	void                RemoveData(uint32 CRC);

	SResFileLookupData* GetData(const CCryNameTSCRC& name);
	void                MarkDirty(bool bDirty) { m_bDirty = bDirty; }

protected:

	string             m_Path;
	SVersionInfo       m_VersionInfo;
	TFileResDirDataMap m_Data;
	TFileCFXDataMap    m_CFXData;
	unsigned int       m_TotalDirStored;
	byte               m_bDirty    : 1;
	byte               m_bReadOnly : 1;
};

#endif //  __RESFILELOOKUPDATAMAN_H__
