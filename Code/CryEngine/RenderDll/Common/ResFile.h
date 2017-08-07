// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __RESFILE_H__
#define __RESFILE_H__

#include <CryString/CryName.h>

#define IDRESHEADER   (('K' << 24) + ('C' << 16) + ('P' << 8) + 'C')
#define RESVERSION_LZSS  10  // LZSS
#define RESVERSION_LZMA  11  // LZMA
#define RESVERSION_DEBUG  12 // Uncompressed

// defines the current compression for any files written
#define RES_COMPRESSION RESVERSION_LZSS

// Resource files flags
#define RF_NOTSAVED 1
#define RF_COMPRESS 4
#define RF_TEMPDATA 8
#define RF_COMPRESSED 0x80
#define RF_RES_$TOKENS 0x20
#define RF_RES_$ (RF_RES_$TOKENS)

class  CResFileLookupDataMan;
struct SResFileLookupData;
struct SResStreamInfo;

// Resource header
struct SFileResHeader
{
	uint32 hid;
	int ver;
	int num_files;
	//int num_files_ref;
	uint32 ofs_dir;
	uint32 num_files_ref;
	AUTO_STRUCT_INFO;
};

#define MAX_FILE_NAME 256

#define OFFSET_BIG_POSITIVE 0x20000000

struct SDirEntryOpen
{
	CCryNameTSCRC Name;
	uint32 curOffset;
	void*  pData;
	int nSize;

	SDirEntryOpen()
	{
		pData = NULL;
		nSize = 0;
	}

	void GetMemoryUsage(ICrySizer* pSizer) const {}
};

// Internal file entry
struct SDirEntry
{
	CCryNameTSCRC Name;
	uint32 size  : 24;
	uint32 flags : 8;        // RF_
	int32  offset;

	SDirEntry()
	{
		size   = 0;
		flags  = 0;
		offset = 0;
	}

	void GetMemoryUsage(ICrySizer* pSizer) const {}

	AUTO_STRUCT_INFO;
};

struct SDirEntryRef
{
	CCryNameTSCRC Name;
	uint32 ref;

	void GetMemoryUsage(ICrySizer* pSizer) const {}

	AUTO_STRUCT_INFO;
};

//typedef std::map<CCryNameTSCRC, int> ResFilesMap;
//typedef ResFilesMap::iterator ResFilesMapItor;

// Resource access types
#define RA_READ   1
#define RA_WRITE  2
#define RA_CREATE 4
#define RA_ENDIANS 8

// Resource optimize flags
#define RO_HEADERS_IN_BEGIN 1
#define RO_HEADERS_IN_END 2
#define RO_HEADER_FILE 4
#define RO_SORT_ALPHA_ASC 8
#define RO_SORT_ALPHA_DESC 0x10

#define MAX_OPEN_RESFILES 64
typedef std::vector<SDirEntry> ResDir;
typedef ResDir::iterator ResDirIt;

typedef std::vector<SDirEntryRef> ResDirRef;
typedef ResDirRef::iterator ResDirRefIt;

typedef std::vector<SDirEntryOpen> ResDirOpen;
typedef ResDirOpen::iterator ResDirOpenIt;

#define MAX_DIR_SIZE      (2 * 1024 * 1024)       // Directory size in memory (consoles only)

class CResFile
{
	friend class CResStreamCallback;
	friend class CResStreamDirCallback;
	friend class CResFileLookupDataMan;
	friend class CShaderSerialize;

private:
	string m_name;
	char*  m_szAccess;
	FILE*  m_handle;
	ResDir m_Dir;
	ResDirRef  m_DirRef;
	ResDirOpen m_DirOpen;
	byte*  m_pCompressedDir;
	int    m_typeaccess;
	uint32 m_nNumFilesUnique;
	uint32 m_nNumFilesRef;
	uint32 m_nOffsDir;
	uint32 m_nComprDirSize;
	int32  m_nOffset;
	byte   m_bSwapEndianRead  : 1;
	byte   m_bSwapEndianWrite : 1;
	byte   m_bDirty           : 1;
	byte   m_bDirValid        : 1;
	byte   m_bDirStreaming    : 1;
	byte   m_bDirCompressed   : 1;
	byte   m_bActive          : 1;
	uint32 m_nLastTickStreamed;
	string m_ermes;
	int    m_version;
	SResStreamInfo* m_pStreamInfo;

	const SResFileLookupData* m_pLookupData;
	class CResFileLookupDataMan* m_pLookupDataMan;

	static CResFile m_Root;
	static CResFile m_RootStream;
	static uint32 m_nSizeComprDir;
	CResFile* m_Next;
	CResFile* m_Prev;
	CResFile* m_NextStream;
	CResFile* m_PrevStream;

	bool mfActivate(bool bFirstTime);

	inline void Relink(CResFile* Before)
	{
		if (m_Next && m_Prev)
		{
			m_Next->m_Prev = m_Prev;
			m_Prev->m_Next = m_Next;
		}
		m_Next                 = Before->m_Next;
		Before->m_Next->m_Prev = this;
		Before->m_Next         = this;
		m_Prev                 = Before;
	}
	inline void Unlink()
	{
		if (!m_Next || !m_Prev)
			return;
		m_Next->m_Prev = m_Prev;
		m_Prev->m_Next = m_Next;
		m_Next         = m_Prev = NULL;
	}
	inline void Link(CResFile* Before)
	{
		if (m_Next || m_Prev)
			return;
		m_Next                 = Before->m_Next;
		Before->m_Next->m_Prev = this;
		Before->m_Next         = this;
		m_Prev                 = Before;
	}

	inline void UnlinkStream()
	{
		if (!m_NextStream || !m_PrevStream)
			return;
		m_NextStream->m_PrevStream = m_PrevStream;
		m_PrevStream->m_NextStream = m_NextStream;
		m_NextStream               = m_PrevStream = NULL;
	}
	inline void LinkStream(CResFile* Before)
	{
		if (m_NextStream || m_PrevStream)
			return;
		m_NextStream = Before->m_NextStream;
		Before->m_NextStream->m_PrevStream = this;
		Before->m_NextStream               = this;
		m_PrevStream = Before;
	}

public:
	CResFile(const char* name);
	~CResFile();

	inline CResFileLookupDataMan* GetLookupMan() const
	{
		return m_pLookupDataMan;
	}

	SResFileLookupData* GetLookupData(bool bCreate, uint32 CRC, float fVersion) const;

	const char* mfGetError(void);
	void        mfSetError(const char* er, ...);
	const char* mfGetFileName() const {return m_name.c_str(); }
	int         mfGetVersion()        { return m_version; }
	void        mfDeactivate(bool bReleaseDir);

	void mfTickStreaming();

	int  mfOpen(int type, CResFileLookupDataMan* pMan, SResStreamInfo* pStreamInfo = NULL);
	bool mfClose();
	int  mfFlush(bool bCompressDir = false);
	int  mfFlushDir(long nSeek, bool bOptimise);
	bool mfPrepareDir();
	int  mfLoadDir(SResStreamInfo* pStreamInfo);
	void mfReleaseDir();

	int mfGetNumFiles() { return m_Dir.size(); }

	byte* mfFileReadCompressed(SDirEntry* de, uint32& nSizeDecomp, uint32& nSizeComp);

	int mfFileRead(SDirEntry* de);
	int mfFileRead(const char* name);
	int mfFileRead(CCryNameTSCRC name);

	int mfFileWrite(CCryNameTSCRC name, void* data);

	void mfFileRead2(SDirEntry* de, int size, void* buf);
	void mfFileRead2(CCryNameTSCRC name, int size, void* buf);

	void* mfFileGetBuf(SDirEntry* de);
	void* mfFileGetBuf(CCryNameTSCRC name);

	int mfFileSeek(SDirEntry* de, int offs, int type);
	int mfFileSeek(CCryNameTSCRC name, int offs, int type);
	int mfFileSeek(char* name, int offs, int type);

	int mfFileLength(SDirEntry* de);
	int mfFileLength(CCryNameTSCRC name);
	int mfFileLength(char* name);

	int mfFileAdd(SDirEntry* de);

	bool mfIsDirty()        { return m_bDirty; }
	bool mfIsDirStreaming() { return m_bDirStreaming; }

	//int mfFileDelete(SDirEntry *de);
	//int mfFileDelete(CCryNameTSCRC name);
	//int mfFileDelete(char* name);

	bool mfFileExist(CCryNameTSCRC name);
	bool mfFileExist(const char* name);

	int            mfFileClose(SDirEntry* de);
	bool           mfCloseEntry(SDirEntry* de, bool bEraseOpenEntry = true);
	SDirEntryOpen* mfOpenEntry(SDirEntry* de);
	SDirEntryOpen* mfGetOpenEntry(SDirEntry* de);
	SDirEntry*     mfGetEntry(CCryNameTSCRC name, bool* bAsync = NULL);
	ResDir*        mfGetDirectory();

	FILE* mfGetHandle() { return m_handle; }
	int   mfGetResourceSize();

	uint64 mfGetModifTime();

	int  Size();
	void GetMemoryUsage(ICrySizer* pSizer) const;

	static void Tick();
	static bool IsStreaming();

	static uint32 m_nMaxOpenResFiles;
	static int m_nNumOpenResources;
};

#endif //  __RESFILE_H__
