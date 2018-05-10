// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ResFileLookupDataMan.h"
#include "ResFile.h"

#include "Shaders/Shader.h"

//////////////////////////////////////////////////////////////////////////

SResFileLookupDataDisk::SResFileLookupDataDisk(const struct SResFileLookupData& inLookup)
{
	m_NumOfFilesUnique = inLookup.m_NumOfFilesUnique;
	m_OffsetDir = inLookup.m_OffsetDir;
	m_CRC32 = inLookup.m_CRC32;
	m_CacheMajorVer = inLookup.m_CacheMajorVer;
	m_CacheMinorVer = inLookup.m_CacheMinorVer;
}

#ifdef USE_PARTIAL_ACTIVATION
unsigned int SResFileLookupData::GetDirOffset(
  const CCryNameTSCRC& dirEntryName) const
{
	if (m_resdirlookup.size() == 0) return 0;

	/*
	   if (m_resdirlookup.size() > 0)
	   {
	   char acTmp[1024];
	   cry_sprintf(acTmp, "dir values: ");
	   OutputDebugString(acTmp);
	   for (unsigned int ui = 0; ui < m_resdirlookup.size(); ++ui)
	   {
	    cry_sprintf(acTmp, " %u ", m_resdirlookup[ui]);
	    OutputDebugString(acTmp);
	   }
	   cry_sprintf(acTmp, " \n");
	   OutputDebugString(acTmp);
	   }
	 */

	unsigned int uiOffset = 0;
	for (; uiOffset < m_resdirlookup.size() - 1; ++uiOffset)
	{
		if (m_resdirlookup[uiOffset + 1] > dirEntryName)
			break;
	}

	return uiOffset;
}
#endif

//////////////////////////////////////////////////////////////////////////

CResFileLookupDataMan::CResFileLookupDataMan() : m_TotalDirStored(0)
{
	m_bDirty = false;
	m_bReadOnly = true;

	m_VersionInfo.m_ResVersion = RES_COMPRESSION;

	float fVersion = (float)FX_CACHE_VER;
	cry_sprintf(m_VersionInfo.m_szCacheVer, "Ver: %.1f", fVersion);
}

CResFileLookupDataMan::~CResFileLookupDataMan()
{
	if (m_bDirty)
		Flush();
	Clear();
}

CCryNameTSCRC CResFileLookupDataMan::AdjustName(const char* szName)
{
	char acTmp[1024];
	int userPathSize = gRenDev->m_cEF.m_szUserPath.size();
	int enginePathSize = strlen("%ENGINE%/");

	if (!strnicmp(szName, gRenDev->m_cEF.m_szUserPath.c_str(), userPathSize))
		cry_strcpy(acTmp, &szName[userPathSize]);
	else if (!strnicmp(szName, "%ENGINE%/", enginePathSize))
		cry_strcpy(acTmp, &szName[enginePathSize]);
	else if (!strnicmp(szName, "Levels", 6))
	{
		const char* acNewName = strstr(szName, "ShaderCache");
		assert(acNewName);
		PREFAST_ASSUME(acNewName);
		cry_strcpy(acTmp, acNewName);
	}
	else
		cry_strcpy(acTmp, szName);

	return acTmp;
}

//////////////////////////////////////////////////////////////////////////

void CResFileLookupDataMan::Clear()
{
	m_Data.clear();
}

void CResFileLookupDataMan::Flush()
{
	if (!m_bDirty)
		return;
	SaveData(m_Path.c_str(), CParserBin::m_bEndians);
	m_bDirty = false;
}

bool CResFileLookupDataMan::LoadData(
  const char* acFilename, bool bSwapEndianRead, bool bReadOnly)
{
	m_Path = acFilename;

	m_bReadOnly = bReadOnly;

	int nFlags = bReadOnly ? 0 : ICryPak::FLAGS_NEVER_IN_PAK | ICryPak::FLAGS_PATH_REAL | ICryPak::FOPEN_ONDISK;

	FILE* handle = gEnv->pCryPak->FOpen(acFilename, "rb", nFlags);
	if (handle == 0)
		return false;

	uint32 hid;

	SVersionInfo versionInfo;

	gEnv->pCryPak->FReadRaw(&hid, sizeof(uint32), 1, handle);
	gEnv->pCryPak->FReadRaw(&versionInfo, sizeof(SVersionInfo), 1, handle);

	if (bSwapEndianRead)
	{
		SwapEndian(hid, eBigEndian);
		SwapEndian(versionInfo, eBigEndian);
	}

	if (hid != IDRESHEADER)
	{
		gEnv->pCryPak->FClose(handle);
		return false;
	}
	if (versionInfo.m_ResVersion != RESVERSION_LZSS && versionInfo.m_ResVersion != RESVERSION_LZMA && versionInfo.m_ResVersion != RESVERSION_DEBUG)
	{
		gEnv->pCryPak->FClose(handle);
		return false;
	}

	float fVersion = (float)FX_CACHE_VER;
	char cacheVer[16];
	cry_sprintf(cacheVer, "Ver: %.1f", fVersion);
	if (strcmp(cacheVer, versionInfo.m_szCacheVer))
	{
		gEnv->pCryPak->FClose(handle);
		return false;
	}

	m_VersionInfo = versionInfo;

	unsigned int uiCount;
	gEnv->pCryPak->FReadRaw(&uiCount, sizeof(unsigned int), 1, handle);

	if (bSwapEndianRead)
	{
		SwapEndian(uiCount, eBigEndian);
	}

	CCryNameTSCRC name;
	CDirEntry dirEntry;
	CCryNameTSCRC dirEntryName;
	unsigned int ui;
	for (ui = 0; ui < uiCount; ++ui)
	{
		gEnv->pCryPak->FReadRaw(&name, sizeof(CCryNameTSCRC), 1, handle);
		if (bSwapEndianRead)
		{
			SwapEndian(name, eBigEndian);
		}

		SResFileLookupDataDisk dirDataDisk;

#ifndef USE_PARTIAL_ACTIVATION

		gEnv->pCryPak->FReadRaw(&dirDataDisk, sizeof(SResFileLookupDataDisk), 1, handle);
		if (bSwapEndianRead)
		{
			SwapEndian(dirDataDisk, eBigEndian);
		}
		SResFileLookupData dirData(dirDataDisk);

#else

		gEnv->pCryPak->FReadRaw(&dirData.m_NumOfFilesUnique, sizeof(int), 1, handle);
		gEnv->pCryPak->FReadRaw(&dirData.m_NumOfFilesRef, sizeof(int), 1, handle);
		gEnv->pCryPak->FReadRaw(&dirData.m_OffsetDir, sizeof(uint32), 1, handle);
		gEnv->pCryPak->FReadRaw(&dirData.m_CRC32, sizeof(uint32), 1, handle);

		if (bSwapEndianRead)
		{
			SwapEndian(dirData.m_NumOfFilesUnique, eBigEndian);
			SwapEndian(dirData.m_NumOfFilesRef, eBigEndian);
			SwapEndian(dirData.m_OffsetDir, eBigEndian);
			SwapEndian(dirData.m_CRC32, eBigEndian);
		}

		gEnv->pCryPak->FReadRaw(&dirData.m_ContainsResDir, sizeof(bool), 1, handle);
		unsigned int uiDirSize;
		gEnv->pCryPak->FReadRaw(&uiDirSize, sizeof(unsigned int), 1, handle);
		if (bSwapEndianRead)
		{
			SwapEndian(uiDirSize, eBigEndian);
		}
		if (dirData.m_ContainsResDir)
		{
			dirData.m_resdir.reserve(uiDirSize);
			for (unsigned int uj = 0; uj < uiDirSize; ++uj)
			{
				gEnv->pCryPak->FReadRaw(&dirEntry, sizeof(CDirEntry), 1, handle);
				if (bSwapEndianRead)
				{
					SwapEndian(dirEntry, eBigEndian);
				}
				dirData.m_resdir.push_back(dirEntry);
			}
		}
		else
		{
			dirData.m_resdirlookup.reserve(uiDirSize);
			for (unsigned int uj = 0; uj < uiDirSize; ++uj)
			{
				gEnv->pCryPak->FReadRaw(&dirEntryName, sizeof(CCryNameTSCRC), 1, handle);
				if (bSwapEndianRead)
				{
					SwapEndian(dirEntryName, eBigEndian);
				}
				dirData.m_resdirlookup.push_back(dirEntryName);
			}
		}
#endif

		m_Data[name] = dirData;
	}

	gEnv->pCryPak->FReadRaw(&uiCount, sizeof(unsigned int), 1, handle);
	if (bSwapEndianRead)
		SwapEndian(uiCount, eBigEndian);
	for (ui = 0; ui < uiCount; ++ui)
	{
		gEnv->pCryPak->FReadRaw(&name, sizeof(CCryNameTSCRC), 1, handle);
		if (bSwapEndianRead)
			SwapEndian(name, eBigEndian);

		SCFXLookupData CFXData;
		gEnv->pCryPak->FReadRaw(&CFXData, sizeof(CFXData), 1, handle);
		if (bSwapEndianRead)
			SwapEndian(CFXData, eBigEndian);

		m_CFXData[name] = CFXData;
	}

	gEnv->pCryPak->FClose(handle);

	return true;
}

void CResFileLookupDataMan::SaveData(
  const char* acFilename, bool bSwapEndianWrite) const
{
	// ignore invalid file access for lookupdata because it shouldn't be written
	// when shaders no compile is set anyway
	SCOPED_ALLOW_FILE_ACCESS_FROM_THIS_THREAD();

	FILE* handle = gEnv->pCryPak->FOpen(acFilename, "w+b");
	if (handle == 0)
		return;

	uint32 hid = IDRESHEADER;

	SVersionInfo versionInfo;
	versionInfo.m_ResVersion = RES_COMPRESSION;

	float fVersion = (float)FX_CACHE_VER;
	cry_sprintf(versionInfo.m_szCacheVer, "Ver: %.1f", fVersion);

	if (bSwapEndianWrite)
	{
		SwapEndian(hid, eBigEndian);
		SwapEndian(versionInfo, eBigEndian);
	}

	gEnv->pCryPak->FWrite(&hid, sizeof(uint32), 1, handle);
	gEnv->pCryPak->FWrite(&versionInfo, sizeof(SVersionInfo), 1, handle);

	unsigned int uiCount = m_Data.size();
	if (bSwapEndianWrite)
	{
		SwapEndian(uiCount, eBigEndian);
	}
	gEnv->pCryPak->FWrite(&uiCount, sizeof(unsigned int), 1, handle);

	for (TFileResDirDataMap::const_iterator it = m_Data.begin(); it != m_Data.end(); ++it)
	{
		CCryNameTSCRC name = it->first;
		if (bSwapEndianWrite)
		{
			SwapEndian(name, eBigEndian);
		}
		gEnv->pCryPak->FWrite(&name, sizeof(CCryNameTSCRC), 1, handle);

#ifndef USE_PARTIAL_ACTIVATION
		SResFileLookupDataDisk header(it->second);
		if (bSwapEndianWrite)
		{
			SwapEndian(header, eBigEndian);
		}
		gEnv->pCryPak->FWrite(&header, sizeof(SResFileLookupDataDisk), 1, handle);
#else
		const SResFileLookupData& header = it->second;
		int numOfFilesUnique = header.m_NumOfFilesUnique;
		int numOfFilesRef = header.m_NumOfFilesRef;
		uint32 offsetDir = header.m_OffsetDir;
		uint32 crc = header.m_CRC32;
		if (bSwapEndianWrite)
		{
			SwapEndian(numOfFilesUnique, eBigEndian);
			SwapEndian(numOfFilesRef, eBigEndian);
			SwapEndian(offsetDir, eBigEndian);
			SwapEndian(crc, eBigEndian);
		}
		gEnv->pCryPak->FWrite(&numOfFilesUnique, sizeof(int), 1, handle);
		gEnv->pCryPak->FWrite(&numOfFilesRef, sizeof(int), 1, handle);
		gEnv->pCryPak->FWrite(&offsetDir, sizeof(uint32), 1, handle);
		gEnv->pCryPak->FWrite(&crc, sizeof(uint32), 1, handle);

		gEnv->pCryPak->FWrite(&header.m_ContainsResDir, sizeof(bool), 1, handle);

		if (header.m_ContainsResDir)
		{
			unsigned int uiDirSize = header.m_resdir.size();
			if (bSwapEndianWrite)
			{
				SwapEndian(uiDirSize, eBigEndian);
			}
			gEnv->pCryPak->FWrite(&uiDirSize, sizeof(unsigned int), 1, handle);

			for (ResDir::const_iterator it2 = header.m_resdir.begin(); it2 != header.m_resdir.end(); ++it2)
			{
				CDirEntry dirEntry = *it2;
				if (bSwapEndianWrite)
				{
					SwapEndian(dirEntry, eBigEndian);
				}
				gEnv->pCryPak->FWrite(&dirEntry, sizeof(CDirEntry), 1, handle);
			}
		}
		else
		{
			unsigned int uiDirSize = header.m_resdirlookup.size();
			if (bSwapEndianWrite)
			{
				SwapEndian(uiDirSize, eBigEndian);
			}
			gEnv->pCryPak->FWrite(&uiDirSize, sizeof(unsigned int), 1, handle);

			for (TResDirNames::const_iterator it2 = header.m_resdirlookup.begin(); it2 != header.m_resdirlookup.end(); ++it2)
			{
				CCryNameTSCRC dirEntryName = *it2;
				if (bSwapEndianWrite)
				{
					SwapEndian(dirEntryName, eBigEndian);
				}
				gEnv->pCryPak->FWrite(&dirEntryName, sizeof(CCryNameTSCRC), 1, handle);
			}
		}
#endif
	}

	uiCount = m_CFXData.size();
	if (bSwapEndianWrite)
		SwapEndian(uiCount, eBigEndian);
	gEnv->pCryPak->FWrite(&uiCount, sizeof(unsigned int), 1, handle);
	for (TFileCFXDataMap::const_iterator it = m_CFXData.begin(); it != m_CFXData.end(); ++it)
	{
		CCryNameTSCRC name = it->first;
		if (bSwapEndianWrite)
			SwapEndian(name, eBigEndian);
		gEnv->pCryPak->FWrite(&name, sizeof(CCryNameTSCRC), 1, handle);
		SCFXLookupData CFXData = it->second;
		if (bSwapEndianWrite)
			SwapEndian(CFXData, eBigEndian);
		gEnv->pCryPak->FWrite(&CFXData, sizeof(SCFXLookupData), 1, handle);
	}

	gEnv->pCryPak->FClose(handle);
}

void CResFileLookupDataMan::AddData(const CResFile* pResFile, uint32 CRC)
{
	if (pResFile == 0)
		return;

	SResFileLookupData data;

	// store the dir data
	data.m_OffsetDir = pResFile->m_nOffsDir;
	data.m_NumOfFilesUnique = pResFile->m_nNumFilesUnique;

	float fVersion = (float)FX_CACHE_VER;
	uint32 nMinor = (int)(((float)fVersion - (float)(int)fVersion) * 10.1f);
	uint32 nMajor = (int)fVersion;
	data.m_CacheMinorVer = nMinor;
	data.m_CacheMajorVer = nMajor;

	data.m_CRC32 = CRC;

#ifdef USE_PARTIAL_ACTIVATION
	// create the lookup data for the resdir
	if (pResFile->m_Dir.size() < 128)
	{
		data.m_ContainsResDir = true;

		m_TotalDirStored++;

		// just copy the whole vector
		data.m_resdir = pResFile->m_Dir;
		/*
		   data.m_resdir.reserve(pResFile->m_Dir.size());
		   for (unsigned int ui = 0; ui < pResFile->m_Dir.size(); ++ui)
		   {
		   data.m_resdir.push_back(pResFile->m_Dir[0]);
		   }
		   memcpy(&data.m_resdir[0], &pResFile->m_Dir[0], sizeof(CDirEntry) * pResFile->m_Dir.size());
		 */
	}
	else
	{
		data.m_ContainsResDir = false;

		unsigned int entries = 0;
		unsigned int entriesPerSlice = MAX_DIR_BUFFER_SIZE / sizeof(CDirEntry);
		while ((entries * entriesPerSlice) < pResFile->m_Dir.size())
		{
			data.m_resdirlookup.push_back(pResFile->m_Dir[entries * entriesPerSlice].Name);
			entries++;
		}
	}
#endif

	const char* acOrigFilename = pResFile->mfGetFileName();
	AddDataCFX(acOrigFilename, CRC);

	// remove the user info, if available
	CCryNameTSCRC name = AdjustName(acOrigFilename);

	// simply overwrite the data if it was already in here
	m_Data[name] = data;
}

void CResFileLookupDataMan::AddDataCFX(const char* acOrigFilename, uint32 CRC)
{
	char nm[256], nmDir[512];
	_splitpath(acOrigFilename, NULL, nmDir, nm, NULL);
	{
		char* s = strchr(nm, '@');
		//assert(s);
		if (s)
			s[0] = 0;
	}

	CCryNameTSCRC nameCFX = nm;
	SCFXLookupData CFX;
	CFX.m_CRC32 = CRC;
	m_CFXData[nameCFX] = CFX;
}

void CResFileLookupDataMan::RemoveData(uint32 CRC)
{
	{
		TFileResDirDataMap tmpData;
		for (TFileResDirDataMap::iterator it = m_Data.begin(); it != m_Data.end(); ++it)
		{
			SResFileLookupData& data = it->second;
			if (data.m_CRC32 != CRC)
				tmpData[it->first] = data;
		}
		m_Data.swap(tmpData);
	}

	{
		TFileCFXDataMap tmpData;
		for (TFileCFXDataMap::iterator it = m_CFXData.begin(); it != m_CFXData.end(); ++it)
		{
			SCFXLookupData& data = it->second;
			if (data.m_CRC32 != CRC)
				tmpData[it->first] = data;
		}
		m_CFXData.swap(tmpData);
	}
}

SResFileLookupData* CResFileLookupDataMan::GetData(
  const CCryNameTSCRC& name)
{
	TFileResDirDataMap::iterator it = m_Data.find(name);
	if (it == m_Data.end())
		return 0;

	return &it->second;
}

//////////////////////////////////////////////////////////////////////////
