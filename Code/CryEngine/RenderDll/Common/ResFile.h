// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

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
#define RF_NOTSAVED    BIT(0)
#define RF_DUPLICATE   BIT(1)
#define RF_COMPRESS    BIT(2)
#define RF_TEMPDATA    BIT(3)
#define RF_RES_$TOKENS BIT(5)
#define RF_RES_$ (RF_RES_$TOKENS)
#define RF_COMPRESSED  BIT(7)

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
	AUTO_STRUCT_INFO;
};

#define MAX_FILE_NAME 256

#define OFFSET_BIG_POSITIVE 0x20000000

struct SDirEntryOpen
{
	CCryNameTSCRC Name;
	uint32        curOffset;
	void*         pData = nullptr;
	int           nSize = 0;

	void GetMemoryUsage(ICrySizer* pSizer) const {}
};

// Internal file entry
class CDirEntry
{
	friend class CResFile;

private:
	CCryNameTSCRC Name;
	uint32        size  : 24;
	uint32        flags : 8;        // RF_
	uint32        offset = 0;

public:
	CDirEntry() : size(0), flags(0) {}
	CDirEntry(CCryNameTSCRC Name, uint32 size, uint32 flags = 0) : Name(Name), size(size), flags(flags) {}
	CDirEntry(CCryNameTSCRC Name, uint32 size, uint32 offset, uint32 flags) : Name(Name), size(size), flags(flags), offset(offset) {}

	CDirEntry(CDirEntry&&) = default;
	CDirEntry &operator=(CDirEntry&&) = default;
	CDirEntry(const CDirEntry&) = default;
	CDirEntry &operator=(const CDirEntry&) = default;

	void GetMemoryUsage(ICrySizer* pSizer) const {}

	// IsValid() returns true if entry content was flushed
	bool IsValid()     const { return size > 0; }
	bool IsDuplicate() const { return (flags & RF_DUPLICATE) != 0; }

	const CCryNameTSCRC& GetName() const { return Name; }

	uint32 GetSize() const   { return size; }
	uint32 GetFlags() const  { return flags; }
	// Only meaningful for valid entries
	uint32 GetOffset() const 
	{
		return offset;
	}

	void MarkNotSaved()
	{
		flags |= RF_NOTSAVED;
	}
	void MarkTemp()
	{
		flags |= RF_TEMPDATA;
	}

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
typedef std::vector<CDirEntry> ResDir;
typedef ResDir::iterator       ResDirIt;

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
	ResDirOpen m_DirOpen;
	byte*  m_pCompressedDir;
	int    m_typeaccess;
	uint32 m_nNumFilesUnique;
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

	SResFileLookupData* GetLookupData() const;
	void                StoreLookupData(uint32 CRC, float fVersion);

	const char* mfGetError(void);
	void        mfSetError(const char* er, ...);
	const char* mfGetFileName() const {return m_name.c_str(); }
	int         mfGetVersion()  const { return m_version; }
	void        mfDeactivate(bool bReleaseDir);

	void mfTickStreaming();

	int  mfOpen(int type, CResFileLookupDataMan* pMan, SResStreamInfo* pStreamInfo = NULL);
	bool mfClose();
	int  mfFlush();
	int  mfFlushDir(long nSeek);
	bool mfPrepareDir();
	int  mfLoadDir(SResStreamInfo* pStreamInfo);
	void mfReleaseDir();

	int mfGetNumFiles() { return m_Dir.size(); }

	byte* mfFileReadCompressed(CDirEntry* de, uint32& nSizeDecomp, uint32& nSizeComp);

	int mfFileRead(CDirEntry* de);
	int mfFileRead(const char* name);
	int mfFileRead(CCryNameTSCRC name);

	int mfFileWrite(CCryNameTSCRC name, void* data);

	void mfFileRead2(CDirEntry* de, int size, void* buf);
	void mfFileRead2(CCryNameTSCRC name, int size, void* buf);

	void* mfFileGetBuf(CDirEntry* de);
	void* mfFileGetBuf(CCryNameTSCRC name);

	int mfFileSeek(CDirEntry* de, int offs, int type);
	int mfFileSeek(CCryNameTSCRC name, int offs, int type);
	int mfFileSeek(char* name, int offs, int type);

	int mfFileLength(CDirEntry* de);
	int mfFileLength(CCryNameTSCRC name);
	int mfFileLength(char* name);

	int mfFileAdd(CDirEntry* de);

	bool mfIsDirty()        { return m_bDirty; }
	bool mfIsDirStreaming() { return m_bDirStreaming; }

	//int mfFileDelete(CDirEntry *de);
	//int mfFileDelete(CCryNameTSCRC name);
	//int mfFileDelete(char* name);

	bool mfFileExist(CCryNameTSCRC name);
	bool mfFileExist(const char* name);

	int            mfFileClose(const CCryNameTSCRC &Name, uint32 flags);
	bool           mfCloseEntry(const CCryNameTSCRC &Name, uint32 flags, bool bEraseOpenEntry = true);
	SDirEntryOpen* mfOpenEntry(const CCryNameTSCRC &Name);
	SDirEntryOpen* mfGetOpenEntry(const CCryNameTSCRC &Name);
	CDirEntry*     mfGetEntry(const CCryNameTSCRC &name, bool* bAsync = NULL);
	ResDir*        mfGetDirectory() { return &m_Dir; }
	const ResDir*  mfGetDirectory() const { return &m_Dir; }

	FILE* mfGetHandle() { return m_handle; }
	int   mfGetResourceSize();

	uint64 mfGetModifTime();

	int  Size();
	void GetMemoryUsage(ICrySizer* pSizer) const;

	bool RequiresSwapEndianOnRead() const { return m_bSwapEndianRead; }
	bool RequiresSwapEndianOnWrite() const { return m_bSwapEndianWrite; }

	static void Tick();
	static bool IsStreaming();

	static uint32 m_nMaxOpenResFiles;
	static int m_nNumOpenResources;
};

class CResFileOpenScope
{
private:
	CResFile * rf = nullptr;

public:
	CResFileOpenScope(CResFile* rf) : rf(rf) {}

	CResFileOpenScope(const CResFileOpenScope&) = delete;
	CResFileOpenScope& operator=(const CResFileOpenScope&) = delete;

	CResFileOpenScope(CResFileOpenScope&& o) noexcept : rf(o.rf) { o.rf = nullptr; }
	CResFileOpenScope& operator=(CResFileOpenScope&& o) noexcept { 
		rf = o.rf;
		o.rf = nullptr; 

		return *this; 
	}

	template <typename... Ts>
	bool open(Ts&&... args)
	{
		return rf->mfOpen(std::forward<Ts>(args)...) != 0;
	}

	~CResFileOpenScope()
	{
		if (rf)
			rf->mfClose();
	}

	CResFile* getHandle() { return rf; }
	const CResFile* getHandle() const { return rf; }
};

#endif //  __RESFILE_H__
