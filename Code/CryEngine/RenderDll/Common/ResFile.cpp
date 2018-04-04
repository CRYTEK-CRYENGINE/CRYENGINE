// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"
#include "ResFile.h"

// Prevent type-clash between LZMA and Scaleform
#define UInt32 UInt32_NoOverwrite
#define UInt64 UInt64_NoOverwrite
#include <lzss/LZSS.H>
#include <lzma/Lzma86.h>
static_assert(sizeof(UInt32) == 4 && sizeof(UInt64) == 8, "Bad type sizes");
#undef  UInt32
#undef  UInt64

CResFile CResFile::m_Root("Root");
CResFile CResFile::m_RootStream("RootStream");
int CResFile::m_nNumOpenResources;
uint32 CResFile::m_nSizeComprDir;
uint32 CResFile::m_nMaxOpenResFiles = MAX_OPEN_RESFILES;

namespace
{
CryCriticalSection g_cResLock;
CryCriticalSection g_cAsyncResLock;
}

bool CResFile::IsStreaming()
{
	if (m_RootStream.m_NextStream &&
	    m_RootStream.m_NextStream != m_RootStream.m_PrevStream)
		return true;

	return false;
}

// Directory garbage collector (must be executed in render thread)
void CResFile::Tick()
{
	if (!m_RootStream.m_NextStream)
	{
		m_RootStream.m_NextStream = &m_RootStream;
		m_RootStream.m_PrevStream = &m_RootStream;
	}
	//return;

	AUTO_LOCK(g_cAsyncResLock);

	int nCurFrame = gRenDev->m_nFrameSwapID;
	uint32 nFrameDif = 300; // Release the directories in 300 frames (approx 10 secs)
	CResFile* pRF, * pNext;
	for (pRF = m_RootStream.m_PrevStream; pRF != &m_RootStream; pRF = pNext)
	{
		pNext = pRF->m_PrevStream;
		assert(pRF->m_pStreamInfo);
		if (!pRF->m_pStreamInfo)
		{
			pRF->UnlinkStream();
			continue;
		}
		if (pRF->m_bDirStreaming || pRF->m_pStreamInfo->m_EntriesQueue.size())
			continue; // Still streaming
		if (nCurFrame - pRF->m_nLastTickStreamed > nFrameDif)
		{
			pRF->UnlinkStream();
			pRF->mfReleaseDir();
		}
	}
}

void CResFile::mfTickStreaming()
{
	m_nLastTickStreamed = gRenDev->m_nFrameSwapID;
	UnlinkStream();
	LinkStream(&m_RootStream);
}

void CResFile::mfDeactivate(bool bReleaseDir)
{
	AUTO_LOCK(g_cResLock); // Not thread safe without this

	if (m_handle)
	{
		mfFlush();
		gEnv->pCryPak->FClose(m_handle);
		m_handle = NULL;
	}

	if (m_bActive)
		m_nNumOpenResources--;
	m_bActive = false;

	Unlink();
}

bool CResFile::mfActivate(bool bFirstTime)
{
	AUTO_LOCK(g_cResLock); // Not thread safe without this

	if (!m_bActive)
	{
		Relink(&m_Root);
		if (m_nNumOpenResources >= (int)m_nMaxOpenResFiles)
		{
			if (m_nNumOpenResources)
			{
				CResFile* rf = m_Root.m_Prev;
				assert(rf && (rf->m_handle || m_pStreamInfo));
				rf->mfDeactivate(false);
			}
		}

		LOADING_TIME_PROFILE_SECTION(iSystem);
		SCOPED_ALLOW_FILE_ACCESS_FROM_THIS_THREAD();

		int nFlags = !m_pLookupDataMan || m_pLookupDataMan->IsReadOnly() ? 0 : ICryPak::FLAGS_NEVER_IN_PAK | ICryPak::FLAGS_PATH_REAL | ICryPak::FOPEN_ONDISK;

		// don't open the file if we are trying to stream the data, defeats the idea of streaming it
		if (!m_pStreamInfo)
		{
			if (!bFirstTime && m_szAccess[0] == 'w')
			{
				char szAcc[16];
				cry_strcpy(szAcc, m_szAccess);
				szAcc[0] = 'r';
				m_handle = gEnv->pCryPak->FOpen(m_name.c_str(), szAcc, nFlags | ICryPak::FOPEN_HINT_DIRECT_OPERATION);
			}
			else
				m_handle = gEnv->pCryPak->FOpen(m_name.c_str(), m_szAccess, nFlags | ICryPak::FOPEN_HINT_DIRECT_OPERATION);
			if (!m_handle)
			{
				mfSetError("CResFile::Activate - Can't open resource file <%s>", m_name.c_str());
				Unlink();
				return false;
			}
		}

		m_nNumOpenResources++;
		m_bActive = true;
	}
	if (!bFirstTime && !m_bDirValid)
		mfPrepareDir();

	return true;
}

CResFile::CResFile(const char* name)
	: m_name(name)
	, m_szAccess(nullptr)
	, m_handle(nullptr)
	, m_pCompressedDir(nullptr)
	, m_typeaccess(0)
	, m_nNumFilesUnique(0)
	, m_nNumFilesRef(0)
	, m_nOffsDir(0)
	, m_nComprDirSize(0)
	, m_nOffset(OFFSET_BIG_POSITIVE)
	, m_bSwapEndianRead(false)
	, m_bSwapEndianWrite(false)
	, m_bDirty(false)
	, m_bDirValid(false)
	, m_bDirStreaming(false)
	, m_bDirCompressed(false)
	, m_bActive(false)
	, m_nLastTickStreamed(0)
	, m_version(0)
	, m_pStreamInfo(nullptr)
	, m_pLookupData(nullptr)
	, m_pLookupDataMan(nullptr)
	, m_Next(nullptr)
	, m_Prev(nullptr)
	, m_NextStream(nullptr)
	, m_PrevStream(nullptr)
{
	if (!m_Root.m_Next)
	{
		m_Root.m_Next = &m_Root;
		m_Root.m_Prev = &m_Root;
		m_RootStream.m_NextStream = &m_RootStream;
		m_RootStream.m_PrevStream = &m_RootStream;
	}
}

CResFile::~CResFile()
{
	if (this != &m_Root && this != &m_RootStream)
	{
		mfClose();
	}
	else
	{
		for (int i = 0; i < 2; ++i)
		{
			const CResFile* pCurrent = i ? m_Next : m_NextStream;
			while (true)
			{
				if (!pCurrent || pCurrent == this)
				{
					break;
				}
				CRY_ASSERT_TRACE(false, ("CResFile - File still open at shutdown: %s (streaming: %s)", pCurrent->mfGetFileName(), i ? "no" : "yes"));
				pCurrent = i ? pCurrent->m_Next : pCurrent->m_NextStream;
			}
		}
	}
}

void CResFile::mfSetError(const char* er, ...)
{
	char buffer[1024];
	va_list args;
	va_start(args, er);
	cry_vsprintf(buffer, er, args);
	m_ermes = buffer;
	va_end(args);
}

SResFileLookupData* CResFile::GetLookupData(bool bCreate, uint32 CRC, float fVersion) const
{
	if (m_pLookupDataMan)
	{
		CCryNameTSCRC name = m_pLookupDataMan->AdjustName(m_name.c_str());
		SResFileLookupData* pData = m_pLookupDataMan->GetData(name);
		uint32 nMinor = (int)(((float)fVersion - (float)(int)fVersion) * 10.1f);
		uint32 nMajor = (int)fVersion;

		if (bCreate && (!pData || (CRC && pData->m_CRC32 != CRC) || pData->m_CacheMinorVer != nMinor || pData->m_CacheMajorVer != nMajor || pData->m_OffsetDir != m_nOffsDir || pData->m_NumOfFilesUnique != m_nNumFilesUnique || pData->m_NumOfFilesRef != m_nNumFilesRef))
		{
			m_pLookupDataMan->AddData(this, CRC);
			pData = m_pLookupDataMan->GetData(name);
			m_pLookupDataMan->MarkDirty(true);
			assert(pData);
		}

		return pData;
	}

	return NULL;
}

const char* CResFile::mfGetError(void)
{
	if (m_ermes.size())
		return m_ermes.c_str();
	else
		return NULL;
}

int CResFile::mfGetResourceSize()
{
	if (!m_handle)
		return 0;

	AUTO_LOCK(g_cResLock); // Not thread safe without this

	gEnv->pCryPak->FSeek(m_handle, 0, SEEK_END);
	int length = gEnv->pCryPak->FTell(m_handle);
	gEnv->pCryPak->FSeek(m_handle, 0, SEEK_SET);

	return length;
}

uint64 CResFile::mfGetModifTime()
{
	if (!mfActivate(false))
		return 0;

	if (!m_handle)
		return 0;

	return gEnv->pCryPak->GetModificationTime(m_handle);
}

bool CResFile::mfFileExist(CCryNameTSCRC name)
{
	SDirEntry* de = mfGetEntry(name);
	if (!de)
		return false;
	assert(name == de->Name);
	return true;
}

bool CResFile::mfFileExist(const char* name)
{
	return mfFileExist(CCryNameTSCRC(name));
}

void CResStreamDirCallback::StreamAsyncOnComplete(IReadStream* pStream, unsigned nError)
{
	/*
	   SResStreamInfo *pStreamInfo = (SResStreamInfo*)pStream->GetUserData();
	   assert(pStreamInfo);
	   if (!pStreamInfo)
	   return;
	   SShaderCache *pCache = pStreamInfo->m_pCache;
	   CResFile *pRes = pStreamInfo->m_pRes;
	   IF(!pRes,0)return;
	   assert(pRes->m_bDirStreaming);

	   if (nError == )
	   pStreamInfo->m_nRequestCount--;

	   // are all requests processed ?
	   if (pStreamInfo->m_nRequestCount == 0)
	   {
	   // check if both requests were valid !
	   pRes->m_bDirValid = true;
	   }
	 */
}

void CResStreamDirCallback::StreamOnComplete(IReadStream* pStream, unsigned nError)
{
	SResStreamInfo* pStreamInfo = (SResStreamInfo*)pStream->GetUserData();
	assert(pStreamInfo);
	if (!pStreamInfo)
		return;

	CryAutoLock<CryCriticalSection> lock(pStreamInfo->m_StreamLock);

	CResFile* pRes = pStreamInfo->m_pRes;
	IF (!pRes, 0) return;
	assert(pRes->m_bDirStreaming);

	if (!nError)
		pStreamInfo->m_nDirRequestCount--;

	for (std::vector<IReadStreamPtr>::iterator it = pStreamInfo->m_dirReadStreams.begin();
	     it != pStreamInfo->m_dirReadStreams.end(); ++it)
	{
		if (pStream == *it)
		{
			pStreamInfo->m_dirReadStreams.erase(it);
			break;
		}
	}

	// all requests processed ?
	if (pStreamInfo->m_dirReadStreams.size() == 0)
	{
		// were all requests processed successfully
		if (pStreamInfo->m_nDirRequestCount == 0)
		{
			// check if both requests were valid !
			pRes->m_bDirValid = true;
		}

		pRes->m_bDirStreaming = false;
	}

	SShaderCache* pCache = pStreamInfo->m_pCache;
	pCache->Release();
}

int CResFile::mfLoadDir(SResStreamInfo* pStreamInfo)
{
	int nRes = 1;
	if (pStreamInfo)
	{
		// if we are streaming the data, we need the lookup data to be valid!
		if (m_pLookupData == NULL)
			return -1;

		mfTickStreaming();
		if (m_bDirStreaming)
			return -1;
		m_bDirStreaming = true;

		int nSizeDir = m_nNumFilesUnique * sizeof(SDirEntry);
		int nSizeDirRef = m_nNumFilesRef * sizeof(SDirEntry);

		if (nSizeDir)
		{
			m_Dir.resize(m_nNumFilesUnique);

			StreamReadParams StrParams;
			StrParams.nFlags = 0;
			StrParams.dwUserData = (DWORD_PTR)pStreamInfo;
			StrParams.nLoadTime = 1;
			StrParams.nMaxLoadTime = 4;
			StrParams.pBuffer = &m_Dir[0];
			StrParams.nOffset = m_nOffsDir;
			StrParams.nSize = nSizeDir;
			pStreamInfo->m_pCache->AddRef();

			CryAutoLock<CryCriticalSection> lock(pStreamInfo->m_StreamLock);
			pStreamInfo->m_dirReadStreams.push_back(iSystem->GetStreamEngine()->StartRead(
			                                          eStreamTaskTypeShader, m_name.c_str(), &pStreamInfo->m_CallbackDir, &StrParams));
			pStreamInfo->m_nDirRequestCount++;
		}

		if (nSizeDirRef)
		{
			m_DirRef.resize(m_nNumFilesRef);

			StreamReadParams StrParams;
			StrParams.nFlags = 0;
			StrParams.dwUserData = (DWORD_PTR)pStreamInfo;
			StrParams.nLoadTime = 1;
			StrParams.nMaxLoadTime = 4;
			StrParams.pBuffer = &m_DirRef[0];
			StrParams.nOffset = m_nOffsDir + nSizeDir;
			StrParams.nSize = nSizeDirRef;
			pStreamInfo->m_pCache->AddRef();

			CryAutoLock<CryCriticalSection> lock(pStreamInfo->m_StreamLock);
			pStreamInfo->m_dirReadStreams.push_back(iSystem->GetStreamEngine()->StartRead(
			                                          eStreamTaskTypeShader, m_name.c_str(), &pStreamInfo->m_CallbackDir, &StrParams));
			pStreamInfo->m_nDirRequestCount++;
		}

		nRes = -1;
	}
	else
	{
		if (gEnv->pCryPak->FSeek(m_handle, m_nOffsDir, SEEK_SET) > 0)
		{
			mfSetError("Open - Directory reading error");
			return 0;
		}

		int nSize = m_nNumFilesUnique * sizeof(SDirEntry);
		//int nSizeDir = m_nComprDirSize ? m_nComprDirSize : nSize;
		if (m_nNumFilesUnique)
		{
			m_Dir.resize(m_nNumFilesUnique);
			if (gEnv->pCryPak->FReadRaw(&m_Dir[0], 1, nSize, m_handle) != nSize)
			{
				mfSetError("Open - Directory reading error");
				m_Dir.clear();
				return 0;
			}
		}

		if (m_nNumFilesRef)
		{
			nSize = m_nNumFilesRef * sizeof(SDirEntryRef);
			//int nSizeDir = m_nComprDirSize ? m_nComprDirSize : nSize;
			m_DirRef.resize(m_nNumFilesRef);
			if (gEnv->pCryPak->FReadRaw(&m_DirRef[0], 1, nSize, m_handle) != nSize)
			{
				mfSetError("Open - Directory reading error");
				m_DirRef.clear();
				return 0;
			}
		}
	}
	m_bDirValid = false;
	if (!m_nComprDirSize && nRes == 1)
	{
		m_bDirValid = true;
		if (m_bSwapEndianRead)
		{
			if (m_nNumFilesUnique)
				SwapEndian(&m_Dir[0], (size_t)m_nNumFilesUnique, eBigEndian);
			if (m_nNumFilesRef)
				SwapEndian(&m_DirRef[0], (size_t)m_nNumFilesRef, eBigEndian);
		}
	}
	return nRes;
}

bool CResFile::mfPrepareDir()
{
	if (m_bDirValid)
		return true;
	assert(!m_Dir.size());
	SDirEntry* pFileDir = NULL;
	if (m_pCompressedDir)
	{
		assert(!m_Dir.size());
		pFileDir = new SDirEntry[m_nNumFilesUnique];
		if (m_version == RESVERSION_LZSS)
		{
			if (!Decodem(m_pCompressedDir, (byte*)pFileDir, m_nComprDirSize, sizeof(SDirEntry) * m_nNumFilesUnique))
			{
				CryFatalError("Compressed res file directory corrupt!");
			}
		}
		else if (m_version == RESVERSION_LZMA)
		{
			SizeT size = sizeof(SDirEntry) * m_nNumFilesUnique;
			SizeT inSize = m_nComprDirSize;
			int res = Lzma86_Decode((byte*)pFileDir, &size, m_pCompressedDir, &inSize);
		}
		else if (m_version == RESVERSION_DEBUG)
		{
			memcpy(pFileDir, m_pCompressedDir, sizeof(SDirEntry) * m_nNumFilesUnique);
		}
		else
		{
			CryFatalError("Bad Version: %d!", m_version);
		}
		m_Dir.resize(m_nNumFilesUnique);
		for (uint32 i = 0; i < m_nNumFilesUnique; i++)
		{
			SDirEntry* deS = &m_Dir[i];
			SDirEntry& fdent = pFileDir[i];
			if (m_bSwapEndianRead)
				SwapEndian(fdent, eBigEndian);
			deS->Name = fdent.Name;
			deS->size = fdent.size;
			deS->offset = fdent.offset;
			deS->flags = fdent.flags;
		}
		m_nSizeComprDir += m_nComprDirSize;
		m_bDirValid = true;
	}
	else
	{
		int nRes = mfLoadDir(m_pStreamInfo);
		assert(nRes);
	}

	return true;
}

void CResFile::mfReleaseDir()
{
	// Never unload directory which wasn't flushed yet
	if (m_bDirty)
		return;
	if (m_bDirStreaming)
		return;
	if (m_pStreamInfo && m_pStreamInfo->m_EntriesQueue.size())
		return;

	if (m_bDirValid)
	{
		uint32 i;
		for (i = 0; i < m_Dir.size(); i++)
		{
			SDirEntry* de = &m_Dir[i];
			assert(!(de->flags & RF_NOTSAVED));
			mfCloseEntry(de, false);
		}

		m_DirOpen.clear();
		//m_Dir.clear();
		ResDir r;
		m_Dir.swap(r);

		m_bDirValid = false;
	}
	else
	{
		assert(!m_Dir.size());
	}
}

int CResFile::mfOpen(int type, CResFileLookupDataMan* pMan, SResStreamInfo* pStreamInfo)
{
	SFileResHeader frh;
	SDirEntry fden;

	PROFILE_FRAME(Resource_Open);

	if (m_name.c_str()[0] == 0)
	{
		mfSetError("Open - No Resource name");
		return 0;
	}
	int nRes = 1;
	m_bSwapEndianWrite = (type & RA_ENDIANS) != 0;
	m_bSwapEndianRead = m_bSwapEndianWrite;
	m_pLookupDataMan = pMan;
	type &= ~RA_ENDIANS;
	if (type == RA_READ)
		m_szAccess = "rb";
	else if (type == (RA_WRITE | RA_READ))
		m_szAccess = "r+b";
	else if (type & RA_CREATE)
		m_szAccess = "w+b";
	else
	{
		mfSetError("Open - Wrong access mode");
		return 0;
	}
	m_typeaccess = type;

	if (type & RA_READ)
		m_pStreamInfo = pStreamInfo;

	mfActivate(true);

	AUTO_LOCK(g_cResLock); // Not thread safe without this

	if (!m_bActive)
	{
		if (type & (RA_WRITE | RA_CREATE))
		{
			char name[256];
			gEnv->pCryPak->AdjustFileName(m_name.c_str(), name, 0);
			FILE* statusdst = fopen(name, "rb");
			if (statusdst)
			{
				/*
				   #if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
				        struct stat64 st;
				   #else
				        struct __stat64 st;
				   #endif
				        int result = _fstat64(fileno(statusdst), &st);
				        if (result == 0)
				        {
				   #if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
				          if (st.st_mode & FILE_ATTRIBUTE_READONLY)
				            chmod(name, 0x777);//set to full access
				   #else
				          if (!(st.st_mode & _S_IWRITE))
				            _chmod(name, _S_IREAD | _S_IWRITE);
				   #endif
				        }
				 */
				fclose(statusdst);
				m_ermes.clear();

				CrySetFileAttributes(name, FILE_ATTRIBUTE_ARCHIVE);

				mfActivate(true);
			}
		}
		if (!m_handle)
		{
			mfSetError("Open - Can't open resource file <%s>", m_name.c_str());
			return 0;
		}
	}

	if (type & RA_READ)
	{
		// check the preloaded dir data, to see if we can get the dir data from there
		CCryNameTSCRC name = m_pLookupDataMan->AdjustName(m_name.c_str());
		if (m_pLookupDataMan)
			m_pLookupData = m_pLookupDataMan->GetData(name);
		if (m_pLookupData)
		{
			m_version = m_pLookupDataMan->GetResVersion();
			m_nNumFilesUnique = m_pLookupData->m_NumOfFilesUnique;
			m_nNumFilesRef = m_pLookupData->m_NumOfFilesRef;
			m_nOffsDir = m_pLookupData->m_OffsetDir;
			m_nComprDirSize = 0;//m_pResDirData->size_dir;
		}
		else
		{
			// make sure lookupdata is available when we are streaming the data
			if (!m_handle)
			{
				mfSetError("Open - no file handle (lookupdata not found, while streaming data?)");
				return 0;
			}

			// Detect file endianness automatically.
			if (gEnv->pCryPak->FReadRaw(&frh, 1, sizeof(frh), m_handle) != sizeof(frh))
			{
				mfSetError("Open - Reading fault");
				return 0;
			}
			if (m_bSwapEndianRead)
				SwapEndian(frh, eBigEndian);
			if (frh.hid != IDRESHEADER)
			{
				mfSetError("Open - Wrong header MagicID");
				return 0;
			}
			if (frh.ver != RESVERSION_LZSS && frh.ver != RESVERSION_LZMA && frh.ver != RESVERSION_DEBUG)
			{
				mfSetError("Open - Wrong version number");
				return 0;
			}
			m_version = frh.ver;
			if (!frh.num_files)
			{
				mfSetError("Open - Empty resource file");
				return 0;
			}

			m_nNumFilesUnique = frh.num_files;
			m_nNumFilesRef = frh.num_files_ref;
			m_nOffsDir = frh.ofs_dir;
			m_nComprDirSize = 0; //frh.size_dir;
		}

		if (pStreamInfo)
			m_pStreamInfo->m_pRes = this;

		m_bDirCompressed = false;
		nRes = mfLoadDir(pStreamInfo);
	}
	else
	{
		frh.hid = IDRESHEADER;
		int ver = RES_COMPRESSION;
		frh.ver = ver;
		frh.num_files = 0;
		frh.ofs_dir = -1;
		frh.num_files_ref = 0;
		m_version = ver;
		m_nOffsDir = sizeof(frh);
		SFileResHeader frhTemp, * pFrh;
		pFrh = &frh;
		if (m_bSwapEndianWrite)
		{
			frhTemp = frh;
			SwapEndian(frhTemp, eBigEndian);
			pFrh = &frhTemp;
		}
		if (gEnv->pCryPak->FWrite(pFrh, 1, sizeof(frh), m_handle) != sizeof(frh))
		{
			mfSetError("Open - Writing fault");
			return 0;
		}
		m_nComprDirSize = 0;
		m_bDirCompressed = false;
		m_nNumFilesUnique = 0;
		m_nNumFilesRef = 0;
		m_pCompressedDir = NULL;
		m_bDirValid = true;
	}

	return nRes;
}

bool CResFile::mfClose(void)
{
	AUTO_LOCK(g_cResLock); // Not thread safe without this

	assert(!m_bDirStreaming);
	assert(!m_pStreamInfo || !m_pStreamInfo->m_EntriesQueue.size());

	//if (m_bDirStreaming || (m_pStreamInfo && m_pStreamInfo->m_EntriesQueue.size()))
	//  Warning("Warning: CResFile::mfClose: Streaming task is in progress!");
	UnlinkStream();

	if (m_typeaccess != RA_READ)
		mfFlush();

	// Close the handle and release directory
	mfDeactivate(true);
	assert(!m_bDirty);
	mfReleaseDir();

	SAFE_DELETE_ARRAY(m_pCompressedDir);

	return true;
}

struct ResDirSortByName
{
	bool operator()(const SDirEntry& left, const SDirEntry& right) const
	{
		return left.Name < right.Name;
	}
	bool operator()(const CCryNameTSCRC left, const SDirEntry& right) const
	{
		return left < right.Name;
	}
	bool operator()(const SDirEntry& left, CCryNameTSCRC right) const
	{
		return left.Name < right;
	}
};
struct ResDirRefSortByName
{
	bool operator()(const SDirEntryRef& left, const SDirEntryRef& right) const
	{
		return left.Name < right.Name;
	}
	bool operator()(const CCryNameTSCRC left, const SDirEntryRef& right) const
	{
		return left < right.Name;
	}
	bool operator()(const SDirEntryRef& left, CCryNameTSCRC right) const
	{
		return left.Name < right;
	}
};
struct ResDirOpenSortByName
{
	bool operator()(const SDirEntryOpen& left, const SDirEntryOpen& right) const
	{
		return left.Name < right.Name;
	}
	bool operator()(const CCryNameTSCRC left, const SDirEntryOpen& right) const
	{
		return left < right.Name;
	}
	bool operator()(const SDirEntryOpen& left, CCryNameTSCRC right) const
	{
		return left.Name < right;
	}
};

SDirEntryOpen* CResFile::mfGetOpenEntry(SDirEntry* de)
{
	ResDirOpenIt it = std::lower_bound(m_DirOpen.begin(), m_DirOpen.end(), de->Name, ResDirOpenSortByName());
	if (it != m_DirOpen.end() && de->Name == (*it).Name)
		return &(*it);
	return NULL;
}
SDirEntryOpen* CResFile::mfOpenEntry(SDirEntry* de)
{
	SDirEntryOpen* pOE = NULL;
	ResDirOpenIt it = std::lower_bound(m_DirOpen.begin(), m_DirOpen.end(), de->Name, ResDirOpenSortByName());
	if (it == m_DirOpen.end() || de->Name != (*it).Name)
	{
		AUTO_LOCK(g_cAsyncResLock);
		SDirEntryOpen OE;
		OE.Name = de->Name;
		OE.curOffset = 0;
		OE.pData = NULL;
		m_DirOpen.insert(it, OE);
		it = std::lower_bound(m_DirOpen.begin(), m_DirOpen.end(), de->Name, ResDirOpenSortByName());
		return &(*it);
	}
	pOE = &(*it);
	pOE->curOffset = 0;
	//assert(pOE->pData);
	//SAFE_DELETE_ARRAY(pOE->pData);

	return pOE;
}
bool CResFile::mfCloseEntry(SDirEntry* de, bool bEraseOpenEntry)
{
	ResDirOpenIt it = std::lower_bound(m_DirOpen.begin(), m_DirOpen.end(), de->Name, ResDirOpenSortByName());
	if (it == m_DirOpen.end() || de->Name != (*it).Name)
		return false;
	SDirEntryOpen& OE = (*it);
	OE.curOffset = 0;
	if (de->flags & RF_TEMPDATA)
	{
		if (OE.pData)
		{
			delete[] (char*)OE.pData;
			OE.pData = NULL;
		}
	}
	if (bEraseOpenEntry)
	{
		AUTO_LOCK(g_cAsyncResLock);
		m_DirOpen.erase(it);
	}

	return true;
}

SDirEntry* CResFile::mfGetEntry(CCryNameTSCRC name, bool* pAsync)
{
	if (pAsync)
	{
		*pAsync = m_bDirStreaming;
		if (m_bDirStreaming)
			return 0;
	}

	if (!m_Dir.size() || m_bDirStreaming)
	{
		if (!mfActivate(false))
			return NULL;
		if (!m_Dir.size() || m_bDirStreaming)
		{
			if (pAsync && m_bDirStreaming)
				*pAsync = true;
			return NULL;
		}
	}

	ResDirIt it = std::lower_bound(m_Dir.begin(), m_Dir.end(), name, ResDirSortByName());
	if (it != m_Dir.end() && name == (*it).Name)
	{
		assert(m_bDirValid);
		return &(*it);
	}
	ResDirRefIt itRef = std::lower_bound(m_DirRef.begin(), m_DirRef.end(), name, ResDirRefSortByName());
	if (itRef != m_DirRef.end() && name == (*itRef).Name)
	{
		assert(m_bDirValid);
		SDirEntryRef& rref = (*itRef);
		return &m_Dir[rref.ref];
	}
	return NULL;
}

int CResFile::mfFileClose(SDirEntry* de)
{
	if (!(de->flags & RF_NOTSAVED))
	{
		mfCloseEntry(de);
	}

	return 0;
}
int CResFile::mfFileAdd(SDirEntry* de)
{
	AUTO_LOCK(g_cResLock); // Not thread safe without this

	assert(!m_pStreamInfo);

	if (m_typeaccess == RA_READ)
	{
		mfSetError("FileAdd - wrong access mode");
		return 0;
	}
	CCryNameTSCRC name = de->Name;
	ResDirIt it = std::lower_bound(m_Dir.begin(), m_Dir.end(), name, ResDirSortByName());
	if (it != m_Dir.end() && name == (*it).Name)
		return m_Dir.size();

	if (de->offset == 0)
		de->offset = m_nOffset++;

	if (de->size != 0)
	{
		if (!m_Dir.size())
			mfActivate(false);

		it = std::lower_bound(m_Dir.begin(), m_Dir.end(), name, ResDirSortByName());
		if (it != m_Dir.end() && name == (*it).Name)
			return m_Dir.size();

		SDirEntry newDE;
		newDE = *de;
		newDE.flags |= RF_NOTSAVED;
		m_Dir.insert(it, newDE);
		m_bDirty = true;
	}
	return m_Dir.size();
}

// Lets unpack the entry asynchronously
void CResStreamCallback::StreamAsyncOnComplete(IReadStream* pStream, unsigned nError)
{
	SResStreamEntry* pEntry = (SResStreamEntry*)pStream->GetUserData();
	SResStreamInfo* pStreamInfo = pEntry->m_pParent;
	assert(pStreamInfo);
	if (!pStreamInfo)
		return;
	//CryHeapCheck();
	CResFile* pRes = pStreamInfo->m_pRes;
	assert(pRes->m_bDirValid);

	if (nError)
	{
		pRes->mfSetError("FileRead - Error during streaming data");
		return;
	}

	SDirEntry* pDE = pRes->mfGetEntry(pEntry->m_Name);
	byte* pBuf = (byte*)pStream->GetBuffer();
	assert(pBuf);
	byte* pData = NULL;

	int32 size;
	if (pDE->flags & RF_COMPRESS)
	{
		assert(pRes->m_version == RESVERSION_LZSS);
		size = *(int32*)pBuf;
		if (size < 10000000)
		{
			pData = new byte[size];
			if (!pData)
				pRes->mfSetError("FileRead - Allocation fault");
			else if (!Decodem(&pBuf[4], (byte*)pData, pDE->size - 4, size))
				pRes->mfSetError("FileRead - Decode fault");
		}
	}
	else
	{
		size = pDE->size;
		pData = new byte[size];
		if (!pData)
			pRes->mfSetError("FileRead - Allocation fault");
		else
			memcpy(pData, pBuf, size);
	}
	{
		AUTO_LOCK(g_cAsyncResLock);

		SDirEntryOpen* pOE = pRes->mfGetOpenEntry(pDE);
		if (pOE)
		{
			pDE->flags |= RF_TEMPDATA;
			pOE->nSize = size;
			pOE->pData = pData;
		}
		else
		{
			CryWarning(VALIDATOR_MODULE_RENDERER, VALIDATOR_ERROR, "mfGetOpenEntry() returned NULL, possibly because r_shadersAllowCompilation=1 and r_shadersAsyncActivation=1. Try r_shadersAsyncActivation=0 in your user.cfg.");
		}
	}
}
// Release the data synchronously
void CResStreamCallback::StreamOnComplete(IReadStream* pStream, unsigned nError)
{
	SResStreamEntry* pEntry = (SResStreamEntry*)pStream->GetUserData();
	SResStreamInfo* pStreamInfo = pEntry->m_pParent;
	assert(pStreamInfo);
	if (!pStreamInfo)
		return;
	SShaderCache* pCache = pStreamInfo->m_pCache;
	uint32 i;
	bool bFound = false;

	CryAutoLock<CryCriticalSection> lock(pStreamInfo->m_StreamLock);

	//CryHeapCheck();
	if (!nError)
	{
		for (i = 0; i < pStreamInfo->m_EntriesQueue.size(); i++)
		{
			SResStreamEntry* pE = pStreamInfo->m_EntriesQueue[i];
			if (pE == pEntry)
			{
				SAFE_DELETE(pE);
				pStreamInfo->m_EntriesQueue.erase(pStreamInfo->m_EntriesQueue.begin() + i);
				bFound = true;
				break;
			}
		}
		assert(bFound);
	}
	pCache->Release();
}

int CResFile::mfFileRead(SDirEntry* de)
{
	uint32 size = 0;

	SDirEntryOpen* pOE = mfOpenEntry(de);

	if (pOE->pData)
		return pOE->nSize;

	if (!mfActivate(false))
		return 0;

	AUTO_LOCK(g_cResLock); // Not thread safe without this

	//if (strstr(m_name, "FPVS") && m_Dir.size()==3)
	{
		//int nnn = 0;
	}
	if (m_pStreamInfo)
	{
		mfTickStreaming();
		if (!m_bDirValid)
		{
			assert(m_bDirStreaming);
			return -1;
		}

		CryAutoLock<CryCriticalSection> lock(m_pStreamInfo->m_StreamLock);

		SResStreamEntry* pEntry;
		{
			//AUTO_LOCK(g_cAsyncResLock);
			pEntry = m_pStreamInfo->AddEntry(de->Name);
			if (!pEntry)  // Already processing
				return -1;
		}
		if (pOE->pData)
			return pOE->nSize;
		StreamReadParams StrParams;
		StrParams.nFlags = 0;
		StrParams.dwUserData = (DWORD_PTR)pEntry;
		StrParams.nLoadTime = 1;
		StrParams.nMaxLoadTime = 4;
		//    StrParams.nPriority = 0;
		StrParams.pBuffer = NULL;
		StrParams.nOffset = de->offset;
		StrParams.nSize = de->size;
		m_pStreamInfo->m_pCache->AddRef();
		//Warning("Warning: CResFile::mfFileRead: '%s' Ref: %d", m_name.c_str(), n);
		pEntry->m_readStream = iSystem->GetStreamEngine()->StartRead(eStreamTaskTypeShader, m_name.c_str(), &m_pStreamInfo->m_Callback, &StrParams);
		return -1;
	}
	else if (de->flags & RF_COMPRESS)
	{
		if (gEnv->pCryPak->FSeek(m_handle, de->offset, SEEK_SET) > 0)
		{
			mfSetError("FileRead - Seek error");
			return 0;
		}

		byte* buf = new byte[de->size];
		if (!buf)
		{
			mfSetError("FileRead - Allocation fault");
			return 0;
		}
		if (m_version == RESVERSION_LZSS)
		{
			gEnv->pCryPak->FReadRaw(buf, de->size, 1, m_handle);
			size = *(int32*)buf;
			if (m_bSwapEndianRead)
				SwapEndian(size, eBigEndian);
			if (size >= 10000000)
			{
				SAFE_DELETE_ARRAY(buf);
				return 0;
			}
			pOE->pData = new byte[size];
			de->flags |= RF_TEMPDATA;
			if (!pOE->pData)
			{
				SAFE_DELETE_ARRAY(buf);
				mfSetError("FileRead - Allocation fault");
				return 0;
			}
			if (!Decodem(&buf[4], (byte*)pOE->pData, de->size - 4, size))
			{
				SAFE_DELETE_ARRAY(buf);
				mfSetError("FileRead - Decodem fault");
				return 0;
			}
		}
		else if (m_version == RESVERSION_LZMA)
		{
			SizeT inSize = de->size;
			gEnv->pCryPak->FReadRaw(buf, inSize, 1, m_handle);
			uint64 outSize64;
			if (Lzma86_GetUnpackSize(buf, inSize, &outSize64) != 0)
			{
				delete[] buf;
				mfSetError("FileRead - data error");
				return 0;
			}
			SizeT outSize = (uint32)outSize64;
			if (outSize != 0)
			{
				uint8* outBuffer = (uint8*)MyAlloc(outSize);
				if (outBuffer == 0)
				{
					delete[] buf;
					mfSetError("FileRead - can't allocate");
					return 0;
				}
				SRes res = Lzma86_Decode(outBuffer, &outSize, buf, &inSize);
				if (res != 0)
				{
					delete[] buf;
					mfSetError("FileRead - LzmaDecoder error");
					return 0;
				}
				size = outSize;
				pOE->pData = outBuffer;
				de->flags |= RF_TEMPDATA;
			}
		}
		else if (m_version == RESVERSION_DEBUG)
		{
			gEnv->pCryPak->FReadRaw(buf, de->size, 1, m_handle);
			pOE->pData = new byte[de->size - 20];
			de->flags |= RF_TEMPDATA;
			if (!pOE->pData)
			{
				SAFE_DELETE_ARRAY(buf);
				mfSetError("FileRead - Allocation fault");
				return 0;
			}
			memcpy(pOE->pData, &buf[10], de->size - 20);
		}
		else
		{
			CryFatalError("Bad Version: %d!", m_version);
			return 0;
		}

		delete[] buf;

		pOE->nSize = size;
		return size;
	}

	pOE->pData = new byte[de->size];
	pOE->nSize = de->size;
	de->flags |= RF_TEMPDATA;
	if (!pOE->pData)
	{
		mfSetError("FileRead - Allocation fault");
		return 0;
	}

	if (!m_handle)
	{
		mfSetError("FileRead - Invalid file handle");
		return 0;
	}

	if (gEnv->pCryPak->FSeek(m_handle, de->offset, SEEK_SET) > 0)
	{
		mfSetError("FileRead - Seek error");
		return 0;
	}

	if (gEnv->pCryPak->FReadRaw(pOE->pData, 1, de->size, m_handle) != de->size)
	{
		mfSetError("FileRead - Reading fault");
		return 0;
	}

	//if (strstr(m_name, "FPVS") && m_Dir.size()==3)
	{
		//gEnv->pCryPak->FSeek(m_handle, 0, SEEK_END);
		//int nSize = gEnv->pCryPak->FTell(m_handle);
		//int nnn = 0;
	}

	return de->size;
}

byte* CResFile::mfFileReadCompressed(SDirEntry* de, uint32& nSizeDecomp, uint32& nSizeComp)
{
	if (!mfActivate(false))
		return NULL;

	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Shader, 0, "Shaders File Read Compressed");

	if (!m_handle)
	{
		mfSetError("FileReadCompressed - Invalid file handle");
		return 0;
	}

	if (de->flags & RF_COMPRESS)
	{
		if (de->offset >= 0x10000000)
			return NULL;

		if (gEnv->pCryPak->FSeek(m_handle, de->offset, SEEK_SET) > 0)
		{
			mfSetError("FileReadCompressed - Seek error");
			return 0;
		}

		byte* buf = new byte[de->size];
		if (!buf)
		{
			mfSetError("FileRead - Allocation fault");
			return 0;
		}
		if (m_version == RESVERSION_LZSS)
		{
			int nSize = -1;
			gEnv->pCryPak->FReadRaw(&nSize, 1, sizeof(int), m_handle);
			if (m_bSwapEndianRead)
				SwapEndian(nSize, eBigEndian);
			if (nSize <= 0)
			{
				assert(0);
				SAFE_DELETE_ARRAY(buf);
				return NULL;
			}
			nSizeDecomp = nSize;
			nSizeComp = de->size - 4;
			gEnv->pCryPak->FReadRaw(buf, nSizeComp, 1, m_handle);
		}
		else if (m_version == RESVERSION_LZMA)
		{
			uint32 inSize = de->size;
			gEnv->pCryPak->FReadRaw(buf, inSize, 1, m_handle);
			uint64 outSize64;
			if (Lzma86_GetUnpackSize(buf, inSize, &outSize64) != 0)
			{
				mfSetError("FileRead - data error");
				return 0;
			}
			nSizeDecomp = (uint32)outSize64;
			nSizeComp = inSize;
		}
		else if (m_version == RESVERSION_DEBUG)
		{
			gEnv->pCryPak->FReadRaw(buf, 10, 1, m_handle);
			gEnv->pCryPak->FReadRaw(buf, de->size - 20, 1, m_handle);
			nSizeDecomp = de->size - 20;
			nSizeComp = de->size - 20;
		}
		else
		{
			CryFatalError("Bad Version: %d!", m_version);
			return 0;
		}
		return buf;
	}

	nSizeComp = nSizeDecomp = de->size;
	byte* buf = new byte[de->size];

	if (gEnv->pCryPak->FSeek(m_handle, de->offset, SEEK_SET) > 0)
	{
		mfSetError("FileReadCompressed - Seek error");
		delete[] buf;
		return 0;
	}

	if (gEnv->pCryPak->FReadRaw(buf, 1, de->size, m_handle) != de->size)
	{
		mfSetError("FileRead - Reading fault");
		delete[] buf;
		return 0;
	}
	return buf;
}

int CResFile::mfFileRead(CCryNameTSCRC name)
{
	SDirEntry* de = mfGetEntry(name);

	if (!de)
	{
		mfSetError("FileRead - Wrong FileId");
		return 0;
	}
	return mfFileRead(de);
}

int CResFile::mfFileRead(const char* name)
{
	return mfFileRead(CCryNameTSCRC(name));
}

int CResFile::mfFileWrite(CCryNameTSCRC name, void* data)
{
	SDirEntry* de = mfGetEntry(name);

	if (!de)
	{
		mfSetError("FileWrite - Wrong FileId");
		return 0;
	}
	if (!data)
	{
		mfSetError("FileWrite - Wrong data");
		return 0;
	}

	if (!mfActivate(false))
		return 0;

	if (de->flags & RF_COMPRESS)
	{
		assert(0);
		return 0;
	}

	if (!m_handle)
	{
		mfSetError("FileWrite - Invalid file handle");
		return 0;
	}

	if (gEnv->pCryPak->FSeek(m_handle, de->offset, SEEK_SET) > 0)
	{
		mfSetError("FileWrite - Seek error");
		return 0;
	}

	if (gEnv->pCryPak->FWrite(data, 1, de->size, m_handle) != de->size)
	{
		mfSetError("FileWrite - Writing fault");
		return 0;
	}

	return de->size;
}

void CResFile::mfFileRead2(SDirEntry* de, int size, void* buf)
{
	if (!buf)
	{
		mfSetError("FileRead - Wrong data");
		return;
	}
	SDirEntryOpen* pOE = mfOpenEntry(de);

	if (pOE->pData)
	{
		memcpy(buf, (byte*)(pOE->pData) + pOE->curOffset, size);
		pOE->curOffset += size;
		return;
	}
	if (!mfActivate(false))
		return;

	if (!m_handle)
	{
		mfSetError("FileRead2 - Invalid file handle");
		return;
	}

	if (gEnv->pCryPak->FSeek(m_handle, de->offset + pOE->curOffset, SEEK_SET) > 0)
	{
		mfSetError("FileRead2 - Seek error");
		return;
	}

	if (gEnv->pCryPak->FReadRaw(buf, 1, size, m_handle) != size)
	{
		mfSetError("FileRead - Reading fault");
		return;
	}
	pOE->curOffset += size;
}

void CResFile::mfFileRead2(CCryNameTSCRC name, int size, void* buf)
{
	SDirEntry* de = mfGetEntry(name);
	if (!de)
	{
		mfSetError("FileRead2 - wrong file id");
		return;
	}
	return mfFileRead2(de, size, buf);
}

void* CResFile::mfFileGetBuf(SDirEntry* de)
{
	SDirEntryOpen* pOE = mfGetOpenEntry(de);
	if (!pOE)
		return NULL;
	return pOE->pData;
}

void* CResFile::mfFileGetBuf(CCryNameTSCRC name)
{
	SDirEntry* de = mfGetEntry(name);
	if (!de)
	{
		mfSetError("FileGetBuf - wrong file id");
		return NULL;
	}
	return mfFileGetBuf(de);
}

int CResFile::mfFileSeek(SDirEntry* de, int ofs, int type)
{
	int m;

	mfActivate(false);

	if (!m_handle)
	{
		mfSetError("FileSeek - Invalid file handle");
		return -1;
	}

	AUTO_LOCK(g_cResLock); // Not thread safe without this

	SDirEntryOpen* pOE = mfOpenEntry(de);

	switch (type)
	{
	case SEEK_CUR:
		pOE->curOffset += ofs;
		m = gEnv->pCryPak->FSeek(m_handle, de->offset + pOE->curOffset, SEEK_SET);
		break;

	case SEEK_SET:
		m = gEnv->pCryPak->FSeek(m_handle, de->offset + ofs, SEEK_SET);
		pOE->curOffset = ofs;
		break;

	case SEEK_END:
		pOE->curOffset = de->size - ofs;
		m = gEnv->pCryPak->FSeek(m_handle, de->offset + pOE->curOffset, SEEK_SET);
		break;

	default:
		mfSetError("FileSeek - wrong seek type");
		return -1;
	}

	return m;
}

int CResFile::mfFileSeek(CCryNameTSCRC name, int ofs, int type)
{
	SDirEntry* de = mfGetEntry(name);

	if (!de)
	{
		mfSetError("FileSeek - invalid file id");
		return -1;
	}
	return mfFileSeek(de, ofs, type);
}

int CResFile::mfFileSeek(char* name, int ofs, int type)
{
	return mfFileSeek(CCryNameTSCRC(name), ofs, type);
}

int CResFile::mfFileLength(SDirEntry* de)
{
	return de->size;
}

int CResFile::mfFileLength(CCryNameTSCRC name)
{
	SDirEntry* de = mfGetEntry(name);

	if (!de)
	{
		mfSetError("FileLength - invalid file id");
		return -1;
	}
	return mfFileLength(de);
}

int CResFile::mfFileLength(char* name)
{
	return mfFileLength(CCryNameTSCRC(name));
}

int CResFile::mfFlushDir(long nOffset, bool bOptimise)
{
	uint32 i;
#ifdef _DEBUG
	// Check for sorted array and duplicated values
	ResDir Sorted;
	for (i = 0; i < m_Dir.size(); i++)
	{
		SDirEntry& DE = m_Dir[i];
		ResDirIt it = std::lower_bound(Sorted.begin(), Sorted.end(), DE.Name, ResDirSortByName());
		if (it != Sorted.end() && DE.Name == (*it).Name)
		{
			assert(0);  // Duplicated value
			continue;
		}
		Sorted.insert(it, DE);
	}
	assert(Sorted.size() == m_Dir.size());
	for (i = 0; i < m_Dir.size(); i++)
	{
		SDirEntry& DE1 = m_Dir[i];
		SDirEntry& DE2 = Sorted[i];
		assert(DE1.Name == DE2.Name);
	}

	ResDirRef SortedRef;
	for (i = 0; i < m_DirRef.size(); i++)
	{
		SDirEntryRef& DE = m_DirRef[i];
		ResDirRefIt it = std::lower_bound(SortedRef.begin(), SortedRef.end(), DE.Name, ResDirRefSortByName());
		if (it != SortedRef.end() && DE.Name == (*it).Name)
		{
			assert(0);  // Duplicated value
			continue;
		}
		SortedRef.insert(it, DE);
	}
	assert(SortedRef.size() == m_DirRef.size());
	for (i = 0; i < m_DirRef.size(); i++)
	{
		SDirEntryRef& DE1 = m_DirRef[i];
		SDirEntryRef& DE2 = SortedRef[i];
		assert(DE1.Name == DE2.Name);
	}

	// Make sure references are correct
	if (bOptimise)
	{
		for (i = 0; i < m_DirRef.size(); i++)
		{
			SDirEntryRef& r = m_DirRef[i];
			assert(r.ref < m_Dir.size());
		}
	}
#endif

	TArray<SDirEntry> FDir;
	TArray<SDirEntryRef> FDirRef;
	FDir.ReserveNoClear(m_Dir.size());
	FDirRef.ReserveNoClear(m_DirRef.size());

	for (i = 0; i < (int)m_Dir.size(); i++)
	{
		SDirEntry* de = &m_Dir[i];
		SDirEntry& fden = FDir[i];
		fden.Name = de->Name;
		fden.size = de->size;
		assert(de->offset > 0);
		fden.offset = de->offset;
		fden.flags = de->flags;
		if (m_bSwapEndianWrite)
			SwapEndian(fden, eBigEndian);
	}
	for (i = 0; i < (int)m_DirRef.size(); i++)
	{
		SDirEntryRef* de = &m_DirRef[i];
		SDirEntryRef& fden = FDirRef[i];
		fden.Name = de->Name;
		fden.ref = de->ref;
		assert(de->ref >= 0);
		if (m_bSwapEndianWrite)
			SwapEndian(fden, eBigEndian);
	}
	gEnv->pCryPak->FSeek(m_handle, nOffset, SEEK_SET);
	int sizeUn = FDir.Num() * sizeof(SDirEntry);
	int sizeRef = FDirRef.Num() * sizeof(SDirEntryRef);
	byte* buf = NULL;
	if (m_bDirCompressed)
	{
#if RES_COMPRESSION == RESVERSION_LZSS
		buf = new byte[sizeUn * 2 + 128];
		if (!buf)
		{
			mfSetError("FlushDir - Allocation fault");
			return false;
		}
		sizeUn = Encodem((byte*)&FDir[0], buf, sizeUn);
#elif RES_COMPRESSION == RESVERSION_LZMA
		// we allocate 105% of original size for output buffer
		uint32 outSize = sizeUn / 20 * 21 + (1 << 16);
		buf = (byte*)MyAlloc(outSize);
		if (!buf)
		{
			mfSetError("FlushDir - Allocation fault");
			return false;
		}
		uint32 dict = 1 << 23;
		int res = Lzma86_Encode(buf, &outSize, (byte*)&FDir[0], sizeUn, 5, dict, SZ_FILTER_AUTO);
		if (res != 0)
		{
			mfSetError("FlushDir - Encoder error = %d", res);
			return 1;
		}
		sizeUn = outSize;
#elif RES_COMPRESSION == RESVERSION_DEBUG
		buf = new byte[sizeUn];
		memcpy(buf, (byte*)&FDir[0], sizeUn);
#else
		static_assert(false, "This should never be compiled...");
#endif
	}
	else
	{
		buf = new byte[sizeUn + sizeRef];
		memcpy(buf, &FDir[0], sizeUn);
		if (sizeRef)
			memcpy(&buf[sizeUn], &FDirRef[0], sizeRef);
	}
	if (gEnv->pCryPak->FWrite(buf, 1, sizeUn + sizeRef, m_handle) != sizeUn + sizeRef)
	{
		mfSetError("FlushDir - Writing fault");
		return false;
	}
	m_nOffsDir = nOffset;
	m_nNumFilesUnique = FDir.Num();
	m_nNumFilesRef = FDirRef.Num();
	SAFE_DELETE_ARRAY(m_pCompressedDir);
	if (m_bDirCompressed)
	{
		m_nComprDirSize = sizeUn;
		m_pCompressedDir = new byte[sizeUn];
		memcpy(m_pCompressedDir, buf, sizeUn);
	}
	SAFE_DELETE_ARRAY(buf);
	m_bDirValid = true;

	SResFileLookupData* pLookup = GetLookupData(false, 0, 0);

	if (pLookup)
	{
		pLookup->m_NumOfFilesRef = m_nNumFilesRef;
		pLookup->m_NumOfFilesUnique = m_nNumFilesUnique;
		pLookup->m_OffsetDir = nOffset;
		m_pLookupDataMan->MarkDirty(true);
		m_pLookupDataMan->Flush();
	}
	return sizeUn + sizeRef;
}

int CResFile::mfFlush(bool bOptimise)
{
	SFileResHeader frh;

	PROFILE_FRAME(Resource_Flush);

	// For compatibility with old builds
	bOptimise = false;

	int nSizeDir = m_DirRef.size() * sizeof(SDirEntryRef) + m_Dir.size() * sizeof(SDirEntry);

	if (m_typeaccess == RA_READ)
	{
		mfSetError("Flush - wrong access mode");
		return nSizeDir;
	}
	AUTO_LOCK(g_cResLock); // Not thread safe without this

	if (m_pLookupDataMan)
		m_pLookupDataMan->Flush();

	if (!m_bDirty)
		return nSizeDir;
	m_bDirty = false;
	if (!mfActivate(false))
		return nSizeDir;

	if (!m_handle)
	{
		mfSetError("Flush - Invalid file handle");
		return 0;
	}

	int i;
	uint32 j;

	int nSizeCompr = 0;

	int nUpdate = 0;
	int nSizeUpdate = 0;

	const size_t numDirRefs = m_Dir.size();
	TArray<int>* pRefs = NULL;
	if (!bOptimise)
		pRefs = new TArray<int>[numDirRefs];

	// Make a list of all references
	for (i = 0; i < (int)m_Dir.size(); i++)
	{
		SDirEntry* de = &m_Dir[i];
		if (de->offset < 0)
		{
			//assert(bOptimise);
			assert(de->flags & RF_NOTSAVED);
			bool bFound = false;
			for (j = 0; j < m_Dir.size(); j++)
			{
				if (i == j)
					continue;
				SDirEntry* d = &m_Dir[j];
				if (d->offset == -de->offset)
				{
					if (bOptimise)
					{
						SDirEntryRef r;
						r.Name = de->Name;
						r.ref = d->offset;
						m_DirRef.push_back(r);
						mfCloseEntry(de);
						m_Dir.erase(m_Dir.begin() + i);
						i--;
					}
					else
						pRefs[j].AddElem(i);
					bFound = true;
					break;
				}
			}
			assert(bFound);
		}
	}

	nSizeDir = m_DirRef.size() * sizeof(SDirEntryRef) + m_Dir.size() * sizeof(SDirEntry);

	const int nFiles = m_Dir.size();
	long nSeek = m_nOffsDir;
	for (i = 0; i < nFiles; i++)
	{
		SDirEntry* de = &m_Dir[i];
		//assert(de->offset >= 0);
		if (de->flags & RF_NOTSAVED)
		{
			SDirEntryOpen* pOE = mfGetOpenEntry(de);
			de->flags &= ~RF_NOTSAVED;
			nUpdate++;
			nSizeUpdate += de->size;

			if (de->offset >= 0)
			{
				assert(pOE && pOE->pData);
				if (!pOE || !pOE->pData)
					continue;
				gEnv->pCryPak->FSeek(m_handle, nSeek, SEEK_SET);
				if (de->flags & RF_COMPRESS)
				{
					byte* buf = NULL;
#if RES_COMPRESSION == RESVERSION_LZSS
					if (!(de->flags & RF_COMPRESSED))
					{
						buf = new byte[de->size * 2 + 128];
						if (!buf)
						{
							mfSetError("Flush - Allocation fault");
							return nSizeDir;
						}
						int sizeEnc = Encodem((byte*)pOE->pData, &buf[4], de->size);
						int nS = de->size;
						if (m_bSwapEndianWrite)
							SwapEndian(nS, eBigEndian);
						*(int*)buf = nS;
						de->size = sizeEnc + 4;
					}
					else
					{
						buf = (byte*)pOE->pData;
						if (m_bSwapEndianWrite)
						{
							int nS = *(int*)buf;
							SwapEndian(nS, eBigEndian);
							*(int*)buf = nS;
						}
					}
#elif RES_COMPRESSION == RESVERSION_LZMA
					// we allocate 105% of original size for output buffer
					if (!(de->flags & RF_COMPRESSED))
					{
						uint32 outSize = de->size / 20 * 21 + (1 << 16);
						buf = (byte*)MyAlloc(outSize);
						if (!buf)
						{
							mfSetError("Flush - Allocation fault");
							return nSizeDir;
						}
						uint32 dict = 1 << 23;
						int res = Lzma86_Encode(buf, &outSize, (byte*)pOE->pData, de->size, 5, dict, SZ_FILTER_AUTO);
						if (res != 0)
						{
							mfSetError("Flush - Encoder error = %d", res);
							return nSizeDir;
						}
						de->size = outSize;
					}
					else
						buf = (byte*)pOE->pData;
#elif RES_COMPRESSION == RESVERSION_DEBUG
					buf = new byte[de->size + 20];
					memcpy(buf, ">>rawbuf>>", 10);
					memcpy(buf + 10, (byte*)pOE->pData, de->size);
					memcpy(buf + 10 + de->size, "<<rawbuf<<", 10);
					de->size += 20;
#else
					static_assert(false, "This should never be compiled...");
#endif
					if (gEnv->pCryPak->FWrite(buf, 1, de->size, m_handle) != de->size)
						mfSetError("Flush - Writing fault");
					if (!(de->flags & RF_COMPRESSED))
						delete[] buf;
					nSizeCompr += de->size;
				}
				else if (!pOE->pData || (gEnv->pCryPak->FWrite(pOE->pData, 1, de->size, m_handle) != de->size))
				{
					mfSetError("Flush - Writing fault");
					continue;
				}

				mfCloseEntry(de);
				if (bOptimise)
				{
					for (j = 0; j < m_DirRef.size(); j++)
					{
						SDirEntryRef& r = m_DirRef[j];
						if (r.ref == de->offset)
						{
							nUpdate++;
							r.ref = i;
						}
					}
				}
				de->offset = nSeek;
				nSeek += de->size;
			}
		}
		// Update reference entries
		if (pRefs)
		{
			PREFAST_ASSUME(i < numDirRefs);
			for (j = 0; j < pRefs[i].Num(); j++)
			{
				nUpdate++;
				SDirEntry* d = &m_Dir[pRefs[i][j]];
				d->offset = de->offset;
				d->size = de->size;
				d->flags = de->flags;
				d->flags &= ~RF_NOTSAVED;
			}
		}
	}
	SAFE_DELETE_ARRAY(pRefs);

	if (!nUpdate)
		return nSizeDir;
	m_bDirCompressed = false; //bCompressDir;
	int sizeDir = mfFlushDir(nSeek, bOptimise);
	assert(sizeDir == nSizeDir);

	frh.hid = IDRESHEADER;
	int ver = RES_COMPRESSION;
	frh.ver = ver;
	frh.num_files = m_Dir.size();
	frh.num_files_ref = m_DirRef.size();
	frh.ofs_dir = nSeek;
	//frh.size_dir = sizeDir;
	m_version = ver;
	SFileResHeader frhTemp, * pFrh;
	pFrh = &frh;
	if (m_bSwapEndianWrite)
	{
		frhTemp = frh;
		SwapEndian(frhTemp, eBigEndian);
		pFrh = &frhTemp;
	}
	gEnv->pCryPak->FSeek(m_handle, 0, SEEK_SET);
	if (gEnv->pCryPak->FWrite(pFrh, 1, sizeof(frh), m_handle) != sizeof(frh))
	{
		mfSetError("Flush - Writing fault");
		return nSizeDir;
	}
	gEnv->pCryPak->FFlush(m_handle);
	//if (strstr(m_name, "FPVS") && frh.num_files==4)
	{
		//gEnv->pCryPak->FSeek(m_handle, 0, SEEK_END);
		//int nSize = gEnv->pCryPak->FTell(m_handle);
		//mfDeactivate();
		//int nnn = 0;
	}

	return sizeDir;
}

int CResFile::Size()
{
	int nSize = sizeof(CResFile);

	uint32 i;
	for (i = 0; i < m_Dir.size(); i++)
	{
		SDirEntry& DE = m_Dir[i];
		nSize += sizeof(SDirEntry);
		SDirEntryOpen* pOE = mfGetOpenEntry(&DE);
		if (pOE)
		{
			nSize += sizeof(SDirEntryOpen);
			if (pOE->pData && (DE.flags & RF_TEMPDATA))
				nSize += DE.size;
		}
	}

	return nSize;
}

void CResFile::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(this, sizeof(*this));
	pSizer->AddObject(m_Dir);
	pSizer->AddObject(m_DirOpen);
}

ResDir* CResFile::mfGetDirectory()
{
	return &m_Dir;
}

//======================================================================

void fpStripExtension(const char* const in, char* const out, const size_t bytes)
{
	assert(in && out && (bytes || in == out)); // if this hits, check the call site
	const size_t inlen = strlen(in);
	ptrdiff_t len = inlen - 1;

	if (len <= 1)
	{
		assert(bytes >= 2); // if this hits, bad buffer was passed in
		cry_strcpy(out, bytes, in);
		return;
	}

	while (in[len])
	{
		if (in[len] == '.')
		{
			int n = (int)len;
			while (in[n] != 0)
			{
				if (in[n] == '+')
				{
					assert(bytes > inlen); // if this hits, bad buffer was passed in
					cry_strcpy(out, bytes, in);
					return;
				}
				n++;
			}
			break;
		}
		len--;
		if (!len)
		{
			assert(bytes > inlen); // if this hits, bad buffer was passed in
			cry_strcpy(out, bytes, in);
			return;
		}
	}
	assert(bytes > len); // if this hits, bad buffer was passed in
	cry_strcpy(out, bytes, in, len);
}

const char* fpGetExtension(const char* in)
{
	assert(in); // if this hits, check the call site
	ptrdiff_t len = strlen(in) - 1;
	while (len)
	{
		if (in[len] == '.')
			return &in[len];
		len--;
	}
	return NULL;
}

void fpAddExtension(char* path, const char* extension, size_t bytes)
{
	assert(path && extension && bytes); // if this hits, check the call site
	char* src;

	assert(*path); // if this hits, src underflow
	src = path + strlen(path) - 1;

	while (*src != '/' && src != path)
	{
		if (*src == '.')
			return;                 // it has an extension
		src--;
	}

	assert(bytes > strlen(path) + strlen(extension)); // if this hits, bad buffer was passed in
	strcat(path, extension);
}

void fpConvertDOSToUnixName(char* dst, const char* src, size_t bytes)
{
	assert(dst && src && bytes); // if this hits, check the call site
	assert(bytes > strlen(src)); // if this hits, bad buffer was passed in
	while (*src)
	{
		if (*src == '\\')
			*dst = '/';
		else
			*dst = *src;
		dst++;
		src++;
	}
	*dst = 0;
}

void fpConvertUnixToDosName(char* dst, const char* src, size_t bytes)
{
	assert(dst && src && bytes); // if this hits, check the call site
	assert(bytes > strlen(src)); // if this hits, bad buffer was passed in
	while (*src)
	{
		if (*src == '/')
			*dst = '\\';
		else
			*dst = *src;
		dst++;
		src++;
	}
	*dst = 0;
}

void fpUsePath(const char* name, const char* path, char* dst, size_t bytes)
{
	assert(name && dst && bytes); // if this hits, check the call site

	if (!path)
	{
		assert(bytes > strlen(name)); // if this hits, bad buffer was passed in
		strcpy(dst, name);
		return;
	}

	assert(bytes > strlen(path)); // if this hits, bad buffer was passed in
	strcpy(dst, path);

	assert(*path); // if this hits, path underflow
	char c = path[strlen(path) - 1];
	if (c != '/' && c != '\\')
	{
		assert(bytes > strlen(path) + strlen(name) + 1); // it this hits, bad buffer was passed in
		strcat(dst, "/");
	}
	else
	{
		assert(bytes > strlen(path) + strlen(name)); // it this hits, bad buffer was passed in
	}
	strcat(dst, name);
}

#include <CryCore/TypeInfo_impl.h>
#include "ResFile_info.h"
#ifndef _LIB
	#include <CryString/Name_TypeInfo.h>
#endif
