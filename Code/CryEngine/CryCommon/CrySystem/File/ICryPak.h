// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryCore/smartptr.h>
#include <CryCore/CryEndian.h>
#include <CrySystem/ISystem.h> // <> required for Interfuscator
#include <CrySystem/IStreamEngineDefs.h>

struct IResourceList;
struct _finddata_t;
struct IMemoryBlock;

//! \cond INTERNAL
//! This represents one particular archive filcare.
struct ICryArchive : public _reference_target_t
{
	//! Compression methods.
	enum ECompressionMethods
	{
		METHOD_STORE                = 0,
		METHOD_COMPRESS             = 8,
		METHOD_DEFLATE              = 8,
		METHOD_COMPRESS_AND_ENCRYPT = 11
	};

	//! Compression levels.
	enum ECompressionLevels
	{
		LEVEL_FASTEST = 0,
		LEVEL_FASTER  = 2,
		LEVEL_NORMAL  = 8,
		LEVEL_BETTER  = 8,
		LEVEL_BEST    = 9,
		LEVEL_DEFAULT = -1
	};

	enum EPakFlags
	{
		//! Support for absolute and other complex path specifications.
		//! All paths will be treated relatively to the current directory (normally MasterCD).
		FLAGS_ABSOLUTE_PATHS = 1,

		//! If this is set, the object will only understand relative to the zip file paths.
		//! However, this can give an opportunity to optimize for frequent quick accesses.
		//! FLAGS_SIMPLE_RELATIVE_PATHS and FLAGS_ABSOLUTE_PATHS are mutually exclusive.
		FLAGS_RELATIVE_PATHS_ONLY = 1 << 1,

		//! If this flag is set, the archive update/remove operations will not work.
		//! This is useful when you open a read-only or already opened for reading files.
		//! If FLAGS_OPEN_READ_ONLY | FLAGS_SIMPLE_RELATIVE_PATHS are set, ICryPak
		//! will try to return an object optimized for memory, with long life cycle.
		FLAGS_READ_ONLY = 1 << 2,

		//! If this flag is set, FLAGS_OPEN_READ_ONLY flags are also implied.
		//! The returned object will be optimized for quick access and memory footprint.
		FLAGS_OPTIMIZED_READ_ONLY = (1 << 3),

		//! If this is set, the existing file (if any) will be overwritten.
		FLAGS_CREATE_NEW = 1 << 4,

		//! If this flag is set, and the file is opened for writing, and some files were updated
		//! so that the archive is no more continuous, the archive will nevertheless NOT be compacted
		//! upon closing the archive file. This can be faster if you open/close the archive for writing
		//! multiple times.
		FLAGS_DONT_COMPACT = 1 << 5,

		//! Flag is set when complete pak has been loaded into memory.
		FLAGS_IN_MEMORY      = BIT(6),
		FLAGS_IN_MEMORY_CPU  = BIT(7),
		FLAGS_IN_MEMORY_MASK = FLAGS_IN_MEMORY | FLAGS_IN_MEMORY_CPU,

		//! Store all file names as crc32 in a flat directory structure.
		FLAGS_FILENAMES_AS_CRC32 = BIT(8),

		//! Flag is set when pak is stored on HDD.
		FLAGS_ON_HDD = BIT(9),

		//! Override pak - paks opened with this flag go at the end of the list and contents will be found before other paks.
		//! Used for patching.
		FLAGS_OVERRIDE_PAK = BIT(10),

		//! Disable a pak file without unloading it, this flag is used in combination with patches and multiplayer
		//! to ensure that specific paks stay in the position(to keep the same priority) but being disabled
		//! when running multiplayer.
		FLAGS_DISABLE_PAK = BIT(11),
	};

	typedef void* Handle;

	// <interfuscator:shuffle>
	virtual ~ICryArchive(){}

	struct IEnumerateArchiveEntries
	{
		virtual bool OnEnumArchiveEntry(const char* pFilename, Handle hEntry, bool bIsFolder, int aSize, int64 aModifiedTime) = 0;
		virtual ~IEnumerateArchiveEntries(){};
	};

	//! Enumerate the file entries found in the specified folder.
	//! \return The number of entries.
	virtual int EnumEntries(Handle hFolder, IEnumerateArchiveEntries* pEnum) = 0;

	//! Get archive's root folder.
	virtual Handle GetRootFolderHandle() = 0;

	//! Adds a new file to the zip or update an existing one.
	//! Adds a directory (creates several nested directories if needed)
	//! compression methods supported are METHOD_STORE == 0 (store) and
	//! METHOD_DEFLATE == METHOD_COMPRESS == 8 (deflate) , compression
	//! level is LEVEL_FASTEST == 0 till LEVEL_BEST == 9 or LEVEL_DEFAULT == -1
	//! for default (like in zlib)
	virtual int UpdateFile(const char* szRelativePath, void* pUncompressed, unsigned nSize, unsigned nCompressionMethod = 0, int nCompressionLevel = -1) = 0;

	//! Adds a new file to the zip or update an existing one if it is not compressed - just stored  - start a big file
	//! ( name might be misleading as if nOverwriteSeekPos is used the update is not continuous )
	//! First step for the UpdateFileConinouseSegment
	virtual int StartContinuousFileUpdate(const char* szRelativePath, unsigned nSize) = 0;

	//! Adds a new file to the zip or update an existing's segment if it is not compressed - just stored.
	//! Adds a directory (creates several nested directories if needed)
	//! \note The name might be misleading as if nOverwriteSeekPos is used the update is not continuous.
	//! \param nOverwriteSeekPos 0xffffffff means the seek pos should not be overwritten (then it needs UpdateFileCRC() to update CRC).
	virtual int UpdateFileContinuousSegment(const char* szRelativePath, unsigned nSize, void* pUncompressed, unsigned nSegmentSize, unsigned nOverwriteSeekPos = 0xffffffff) = 0;

	//! Needed to update CRC if UpdateFileContinuousSegment() was used with nOverwriteSeekPos.
	virtual int UpdateFileCRC(const char* szRelativePath, const uint32 dwCRC) = 0;

	//! Deletes the file from the archive.
	virtual int RemoveFile(const char* szRelativePath) = 0;

	//! Deletes the directory, with all its descendants (files and subdirs).
	virtual int RemoveDir(const char* szRelativePath) = 0;

	//! Deletes all files and directories in the archive.
	virtual int RemoveAll() = 0;

	//! Finds the file; you don't have to close the returned handle.
	//! \return NULL if the file doesn't exist
	virtual Handle FindFile(const char* szPath) = 0;

	//! Get the file size (uncompressed).
	//! \return The size of the file (unpacked) by the handle
	virtual unsigned GetFileSize(Handle) = 0;

	//! Reads the file into the preallocated buffer
	//! \note Must be at least the size returned by GetFileSize.
	virtual int ReadFile(Handle, void* pBuffer) = 0;

	//! Get the full path to the archive file.
	virtual const char* GetFullPath() const = 0;

	//! Get the flags of this object.
	//! The possibles flags are defined in EPakFlags.
	virtual unsigned GetFlags() const = 0;

	//! Sets the flags of this object.
	//! The possibles flags are defined in EPakFlags.
	virtual bool SetFlags(unsigned nFlagsToSet) = 0;

	//! Resets the flags of this object.
	virtual bool ResetFlags(unsigned nFlagsToSet) = 0;

	//! Control if files in this pack can be accessed
	//! \return true if archive state was changed
	virtual bool SetPackAccessible(bool bAccessible) = 0;

	//! Determines if the archive is read only.
	//! \return true if this archive is read-only
	inline bool IsReadOnly() const { return (GetFlags() & FLAGS_READ_ONLY) != 0; }

	//! Get the class id.
	virtual unsigned GetClassId() const = 0;

	//! Collect allocated memory in CrySizer
	virtual void GetMemoryUsage(ICrySizer* pSizer) const = 0;
	// </interfuscator:shuffle>
};

TYPEDEF_AUTOPTR(ICryArchive);

struct ICryPakFileAcesssSink
{
	// <interfuscator:shuffle>
	virtual ~ICryPakFileAcesssSink(){}
	//! \param in 0 if asyncronous read.
	//! \param szFullPath Must not be 0.
	virtual void ReportFileOpen(FILE* in, const char* szFullPath) = 0;
	// </interfuscator:shuffle>
};
//! \endcond

//! This special flag is used for findfirst/findnext routines to mark the files that were actually found in Archive.
enum  : uint32 {_A_IN_CRYPAK = 0x80000000};

//! Interface to the Pak file system.
//! \see CryPak.
struct ICryPak
{
	typedef uint64 FileTime;
	//! Flags used in file path resolution rules.
	enum EPathResolutionRules
	{
		//! If used, the source path will be treated as the destination path and no transformations will be done.
		//! Pass this flag when the path is to be the actual path on the disk/in the packs and doesn't need adjustment
		//! (or after it has come through adjustments already). If this is set, AdjustFileName will not map the input
		//! path into the master folder (Ex: Shaders will not be converted to Game\Shaders).
		FLAGS_PATH_REAL = 1L << 16,

		//! AdjustFileName will always copy the file path to the destination path: regardless of the returned value, szDestpath can be used.
		FLAGS_COPY_DEST_ALWAYS = 1L << 17,

		//! Adds trailing slash to the path.
		FLAGS_ADD_TRAILING_SLASH = 1L << 18,

		//! If this is set, AdjustFileName will not make relative paths into full paths.
		FLAGS_NO_FULL_PATH = 1L << 21,

		//! If this is set, AdjustFileName will redirect path to disc.
		FLAGS_REDIRECT_TO_DISC = 1L << 22,

		//! If this is set, AdjustFileName will not adjust path for writing files.
		FLAGS_FOR_WRITING = 1L << 23,

		//! If this is set, AdjustFileName will not convert the path to low case.
		FLAGS_NO_LOWCASE = 1L << 24,

		//! If this is set, the pak would be stored in memory (gpu).
		FLAGS_PAK_IN_MEMORY = BIT(25),

		//! Store all file names as crc32 in a flat directory structure.
		FLAGS_FILENAMES_AS_CRC32 = BIT(26),

		//! If this is set, AdjustFileName will try to find the file under any mod paths we know about.
		FLAGS_CHECK_MOD_PATHS = BIT(27),

		//! If this is set, AdjustFileName will always check the filesystem/disk and not check inside open paks.
		FLAGS_NEVER_IN_PAK = BIT(28),

		//! Used by the resource compiler to pass the real file name.
		//! \return Existing file name from the local data or existing cache file name.
		FLAGS_RESOLVE_TO_CACHE = BIT(29),

		//! If this is set, the pak would be stored in memory (cpu).
		FLAGS_PAK_IN_MEMORY_CPU = BIT(30),
	};

	//! Used for widening FOpen functionality. They're ignored for the regular File System files.
	enum EFOpenFlags
	{
		//! If possible, will prevent the file from being read from memory.
		FOPEN_HINT_DIRECT_OPERATION = BIT(0),

		//! Will prevent a "missing file" warnings to be created.
		FOPEN_HINT_QUIET = BIT(1),

		//! File should be on disk.
		FOPEN_ONDISK = BIT(2),

		//! Open is done by the streaming thread.
		FOPEN_FORSTREAMING = BIT(3),

		//! On supported platforms, file is open in 'locked' mode.
		FOPEN_LOCKED_OPEN = BIT(4),
	};

	enum ERecordFileOpenList
	{
		RFOM_Disabled,              //!< File open are not recorded (fast, no extra memory).
		RFOM_EngineStartup,         //!< Before a level is loaded.
		RFOM_Level,                 //!< During level loading till export2game -> resourcelist.txt, used to generate the list for level2level loading.
		RFOM_NextLevel              //!< Used for level2level loading.
	};

	//! The size of the buffer that receives the full path to the file.
	enum {g_nMaxPath = 0x800};

	//! File location enum used in isFileExist to control where the pak system looks for the file.
	enum EFileSearchLocation
	{
		eFileLocation_Any = 0,
		eFileLocation_OnDisk,
		eFileLocation_InPak,
	};

	enum EInMemoryPakLocation
	{
		eInMemoryPakLocale_Unload = 0,
		eInMemoryPakLocale_CPU,
		eInMemoryPakLocale_GPU,
	};

	struct PakInfo
	{
		struct Pak
		{
			const char* szFilePath;
			const char* szBindRoot;
			size_t      nUsedMem;
		};

		//! The number of packs in the arrPacks array.
		unsigned numOpenPaks;

		//! The packs.
		Pak arrPaks[1];
	};

	typedef int64 SignedFileSize;

	// <interfuscator:shuffle>
	virtual ~ICryPak(){}

	//! Given the source relative path, constructs the full path to the file according to the flags.
	//! \return Pointer to the constructed path (can be either szSourcePath, or szDestPath, or NULL in case of error.
	virtual const char* AdjustFileName(const char* src, char dst[g_nMaxPath], unsigned nFlags) = 0;

	virtual bool        Init(const char* szBasePath) = 0;
	virtual void        Release() = 0;

	//! \return true if given pak path is installed to HDD.
	//! If no file path is given it will return true if whole application is installed to HDD.
	virtual bool IsInstalledToHDD(const char* acFilePath = 0) const = 0;

	//! After this call, the pak file will be searched for files when they aren't on the OS file system.
	//! \param pName Must not be 0.
	virtual bool OpenPack(const char* pName, unsigned nFlags = FLAGS_PATH_REAL, IMemoryBlock* pData = 0, CryFixedStringT<ICryPak::g_nMaxPath>* pFullPath = 0) = 0;

	//! After this call, the pak file will be searched for files when they aren't on the OS file system.
	virtual bool OpenPack(const char* pBindingRoot, const char* pName, unsigned nFlags = FLAGS_PATH_REAL, IMemoryBlock* pData = 0, CryFixedStringT<ICryPak::g_nMaxPath>* pFullPath = 0) = 0;

	//! After this call, the file will be unlocked and closed, and its contents won't be used to search for files.
	virtual bool ClosePack(const char* pName, unsigned nFlags = FLAGS_PATH_REAL) = 0;

	//! Opens pack files by the path and wildcard.
	virtual bool OpenPacks(const char* pWildcard, unsigned nFlags = FLAGS_PATH_REAL, std::vector<CryFixedStringT<ICryPak::g_nMaxPath>>* pFullPaths = NULL) = 0;

	//! Opens pack files by the path and wildcard.
	virtual bool OpenPacks(const char* pBindingRoot, const char* pWildcard, unsigned nFlags = FLAGS_PATH_REAL, std::vector<CryFixedStringT<ICryPak::g_nMaxPath>>* pFullPaths = NULL) = 0;

	//! Closes pack files by the path and wildcard.
	virtual bool ClosePacks(const char* pWildcard, unsigned nFlags = FLAGS_PATH_REAL) = 0;

	//! \return true if a pak exists matching the wildcard.
	virtual bool FindPacks(const char* pWildcardIn) = 0;

	//! Set access status of a pak files with a wildcard.
	virtual bool SetPacksAccessible(bool bAccessible, const char* pWildcard, unsigned nFlags = FLAGS_PATH_REAL) = 0;

	//! Set access status of a pack file.
	virtual bool SetPackAccessible(bool bAccessible, const char* pName, unsigned nFlags = FLAGS_PATH_REAL) = 0;

	//! This marks as Accessible all paks required for specific level and vice versa for all other paks.
	//! This is useful in case the game has per level split assets.
	virtual void SetPacksAccessibleForLevel(const char* sLevelName) = 0;

	//! Load or unload pak file completely to memory.
	virtual bool LoadPakToMemory(const char* pName, EInMemoryPakLocation eLoadToMemory, IMemoryBlock* pMemoryBlock = NULL) = 0;
	virtual void LoadPaksToMemory(int nMaxPakSize, bool bLoadToMemory) = 0;

	//! Adds a mod to the list.
	virtual void AddMod(const char* szMod) = 0;

	//! Removes a mod from the list.
	virtual void RemoveMod(const char* szMod) = 0;

	//! \return Indexed mod path, or NULL if out of range.
	virtual const char* GetMod(int index) = 0;

	//! Processes an alias command line containing multiple aliases.
	virtual void ParseAliases(const char* szCommandLine) = 0;

	//! Adds or removes an alias from the list.
	virtual void SetAlias(const char* szName, const char* szAlias, bool bAdd) = 0;

	//! Gets an alias from the list, if any exist.
	//! If bReturnSame==true, it will return the input name if an alias doesn't exist. Otherwise returns NULL.
	virtual const char* GetAlias(const char* szName, bool bReturnSame = true) = 0;

	//! Lock all the operations.
	virtual void Lock() = 0;
	virtual void Unlock() = 0;

	//! Add a lock operation around the read operations to be sure they only happen from one thread.
	virtual void LockReadIO(bool bValue) = 0;

	//! Set and Get "Game" folder (/Game, /Game04, ...).
	virtual void        SetGameFolder(const char* szFolder) = 0;
	virtual const char* GetGameFolder() const = 0;

	//! Set and Get the localization folder name (Languages, Localization, ...).
	virtual void              SetLocalizationFolder(char const* const sLocalizationFolder) = 0;
	virtual char const* const GetLocalizationFolder() const = 0;

	//! Only returns useful results on a dedicated server at present - and only if the pak is already opened.
	virtual void GetCachedPakCDROffsetSize(const char* szName, uint32& offset, uint32& size) = 0;

	//! \return Array of PackInfo structures inside OpenPacks structure. You MUST call FreeOpenPackInfo.
	virtual ICryPak::PakInfo* GetPakInfo() = 0;
	virtual void              FreePakInfo(PakInfo*) = 0;

	//! Open file handle, file can be on disk or in PAK file.
	//! Possible mode is r,b,x.
	//! Example:
	//! FILE *f = FOpen( "test.txt","rbx" );.
	//! Mode x is a direct access mode, when used file reads will go directly into the low level file system without any internal data caching.
	//! Text mode is not supported for files in PAKs.
	//! \see ICryPak::EFOpenFlags.
	virtual FILE* FOpen(const char* pName, const char* mode, unsigned nFlags = 0) = 0;
	virtual FILE* FOpen(const char* pName, const char* mode, char* szFileGamePath, int nLen) = 0;

	//! Just a wrapper for fopen function. For loading sampler unification.
	virtual FILE* FOpenRaw(const char* pName, const char* mode) = 0;

	//! Read raw data from file, no endian conversion.
	virtual size_t FReadRaw(void* data, size_t length, size_t elems, FILE* handle) = 0;

	//! Read all file contents into the provided memory, nSizeOfFile must be the same as returned by GetFileSize(handle).
	//! Current seek pointer is ignored and reseted to 0. No endian conversion.
	virtual size_t FReadRawAll(void* data, size_t nFileSize, FILE* handle) = 0;

	//! Get pointer to the internally cached, loaded data of the file.
	//! \note Requesting cached file data of the another file will invalidate previously retrieved pointer.
	virtual void* FGetCachedFileData(FILE* handle, size_t& nFileSize) = 0;

	//! Write file data, cannot be used for writing into the PAK.
	//! Use ICryArchive interface for writing into the pak files.
	virtual size_t FWrite(const void* data, size_t length, size_t elems, FILE* handle) = 0;

	//! virtual int FScanf(FILE *, const char *, ...) SCANF_PARAMS(2, 3) =0;
	virtual int    FPrintf(FILE* handle, const char* format, ...) PRINTF_PARAMS(3, 4) = 0;
	virtual char*  FGets(char*, int, FILE*) = 0;
	virtual int    Getc(FILE*) = 0;
	virtual size_t FGetSize(FILE* f) = 0;
	virtual size_t FGetSize(const char* pName, bool bAllowUseFileSystem = false) = 0;
	virtual int    Ungetc(int c, FILE*) = 0;
	virtual bool   IsInPak(FILE* handle) = 0;
	virtual bool   RemoveFile(const char* pName) = 0;                //!< Remove file from FS (if supported).
	virtual bool   RemoveDir(const char* pName, bool bRecurse) = 0;  //!< Remove directory from FS (if supported).
	virtual bool   IsAbsPath(const char* pPath) = 0;                 //!< Determines if pPath is an absolute or relative path.

	virtual bool   CopyFileOnDisk(const char* source, const char* dest, bool bFailIfExist) = 0;

	virtual size_t FSeek(FILE* handle, long seek, int mode) = 0;
	virtual long   FTell(FILE* handle) = 0;
	virtual int    FClose(FILE* handle) = 0;
	virtual int    FEof(FILE* handle) = 0;
	virtual int    FError(FILE* handle) = 0;
	virtual int    FGetErrno() = 0;
	virtual int    FFlush(FILE* handle) = 0;

	//! Return pointer to pool if available.
	virtual void* PoolMalloc(size_t size) = 0;

	//! Free pool.
	virtual void PoolFree(void* p) = 0;

	//! \param sUsage indicates for what usage this memory was requested.
	//! \return Interface to the Memory Block allocated on the File Pool memory.
	virtual IMemoryBlock* PoolAllocMemoryBlock(size_t nSize, const char* sUsage, size_t nAlign = 1) = 0;

	//! \param nFlags A combination of EPathResolutionRules flags.
	virtual intptr_t FindFirst(const char* pDir, _finddata_t* fd, unsigned int nFlags = 0, bool bAllOwUseFileSystem = false) = 0;
	virtual int      FindNext(intptr_t handle, _finddata_t* fd) = 0;
	virtual int      FindClose(intptr_t handle) = 0;
	// virtual bool IsOutOfDate(const char * szCompiledName, const char * szMasterFile)=0;

	//! \return File modification time.
	virtual ICryPak::FileTime GetModificationTime(FILE* f) = 0;

	//! \return true if specified file exists in filesystem.
	virtual bool IsFileExist(const char* sFilename, EFileSearchLocation = eFileLocation_Any) = 0;

	//! \return true if path is a folder.
	virtual bool                    IsFolder(const char* sPath) = 0;

	virtual ICryPak::SignedFileSize GetFileSizeOnDisk(const char* filename) = 0;

	//! \return true if file is compressed inside a pak.
	virtual bool IsFileCompressed(const char* filename) = 0;

	//! Creates a directory.
	virtual bool MakeDir(const char* szPath, bool bGamePathMapping = false) = 0;

	//! Open the physical archive file - creates if it doesn't exist.
	//! nFlags is a combination of flags from EPakFlags enum.
	//! \return NULL if it's invalid or can't open the file.
	//! \see EPakFlags
	virtual ICryArchive* OpenArchive(const char* szPath, unsigned int nFlags = 0, IMemoryBlock* pData = 0) = 0;

	//! Returns the path to the archive in which the file was opened.
	//! \return NULL if the file is a physical file, and "" if the path to archive is unknown (shouldn't ever happen).
	virtual const char* GetFileArchivePath(FILE* f) = 0;

	//! Compresses the raw data into raw data. The buffer for compressed data itself with the heap passed. Uses method 8 (deflate).
	//! \return One of the Z_* errors (Z_OK upon success).
	//! \note Multithreaded-safe.
	virtual int RawCompress(const void* pUncompressed, unsigned long* pDestSize, void* pCompressed, unsigned long nSrcSize, int nLevel = -1) = 0;

	//! Uncompresses raw (without wrapping) data that is compressed with method 8 (deflated) in the Zip file.
	//! This function just mimics the standard uncompress (with modification taken from unzReadCurrentFile) with 2 differences: there are no
	//! 16-bit checks, and it initializes the inflation to start without waiting for compression method byte, as this is the way it's stored into zip file.
	//! \return One of the Z_* errors (Z_OK upon success).
	virtual int RawUncompress(void* pUncompressed, unsigned long* pDestSize, const void* pCompressed, unsigned long nSrcSize) = 0;

	// Files collector.

	//! Turn on/off recording of filenames of opened files.
	virtual void RecordFileOpen(const ERecordFileOpenList eList) = 0;

	//! Record this file if recording is enabled.
	//! \param in - 0 if asyncronous read.
	virtual void RecordFile(FILE* in, const char* szFilename) = 0;

	//! Get resource list of all recorded files, the next level, ...
	virtual IResourceList* GetResourceList(const ERecordFileOpenList eList) = 0;
	virtual void           SetResourceList(const ERecordFileOpenList eList, IResourceList* pResourceList) = 0;

	//! Get the current mode, can be set by RecordFileOpen().
	virtual ICryPak::ERecordFileOpenList GetRecordFileOpenList() = 0;

	//! Computes CRC (zip compatible) for a file.
	//! Useful if a huge uncompressed file is generation in non continuous way.
	//! Good for big files - low memory overhead (1MB).
	//! \param szPath Must not be 0.
	//! \return Error code.
	virtual uint32 ComputeCRC(const char* szPath, uint32 nFileOpenFlags = 0) = 0;

	//! Computes MD5 checksum for a file.
	//! Good for big files - low memory overhead (1MB).
	//! \param szPath - must not be 0.
	//! \param md5 - destination array of unsigned char [16].
	//! \return true on success, false on failure.
	virtual bool ComputeMD5(const char* szPath, unsigned char* md5, uint32 nFileOpenFlags = 0) = 0;

	virtual int  ComputeCachedPakCDR_CRC(const char* filename, bool useCryFile = true, IMemoryBlock* pData = NULL) = 0;

	//! Useful for gathering file access statistics, assert if it was inserted already but then it does not become insersted.
	//! \param pSink Must not be 0.
	virtual void RegisterFileAccessSink(ICryPakFileAcesssSink* pSink) = 0;
	//! Assert if it was not registered already.
	//! \param pSink Must not be 0.
	virtual void UnregisterFileAccessSink(ICryPakFileAcesssSink* pSink) = 0;

	//! LvlRes can be enabled by command line - then asset resource recording is enabled
	//! and some modules can do more asset tracking work based on that.
	//! \return true=on, false=off
	virtual bool GetLvlResStatus() const = 0;

	// When enabled, files accessed at runtime will be tracked
	virtual void DisableRuntimeFileAccess(bool status) = 0;
	virtual bool DisableRuntimeFileAccess(bool status, threadID threadId) = 0;
	virtual bool CheckFileAccessDisabled(const char* name, const char* mode) = 0;
	virtual void SetRenderThreadId(threadID renderThreadId) = 0;

	//! Gets the current pak priority.
	virtual int GetPakPriority() = 0;

	//! \return Offset in pak file (ideally has to return offset on DVD) for streaming requests sorting.
	virtual uint64 GetFileOffsetOnMedia(const char* szName) = 0;

	//! Return media type for the file.
	virtual EStreamSourceMediaType GetFileMediaType(const char* szName) = 0;

	//! PerfHUD widget for tracking pak file stats.
	virtual void CreatePerfHUDWidget() = 0;
	// </interfuscator:shuffle>

	struct ArchiveEntryInfo
	{
		const char* szName; // pointer managed by ICryPak
		bool        bIsFolder;
		size_t      size;
		uint64      aModifiedTime; // uses FILETIME format (100nsec sinc 1601-1-1 UTC)
	};
	typedef std::function<void (const ArchiveEntryInfo&)> ArchiveEntrySinkFunction;

	//! Calls a callback for all entries in an archive folder
	//! \param szFolderPath contains search masks, if szFolderPath is just a filename or folderpath the callback will be called once for this entry.
	//! \return Returns false if an error occurred.
	virtual bool ForEachArchiveFolderEntry(const char* szArchivePath, const char* szFolderPath, const ArchiveEntrySinkFunction& callback) = 0;

	//! Type-safe endian conversion read.
	template<class T>
	size_t FRead(T* data, size_t elems, FILE* handle, bool bSwapEndian = eLittleEndian)
	{
		size_t count = FReadRaw(data, sizeof(T), elems, handle);
		SwapEndian(data, count, bSwapEndian);
		return count;
	}
	//! Type-independent Write.
	template<class T>
	void FWrite(T* data, size_t elems, FILE* handle)
	{
		FWrite((void*)data, sizeof(T), elems, handle);
	}

	static const ICryPak::SignedFileSize FILE_NOT_PRESENT = -1;
};

//! \cond INTERNAL
//! The IResourceList provides an access to the collection of the resource`s file names.
//! Client can add a new file names to the resource list and check if resource already in the list.
struct IResourceList : public _reference_target_t
{
	// <interfuscator:shuffle>
	//! Adds a new resource to the list.
	virtual void Add(const char* sResourceFile) = 0;

	//! Clears resource list.
	virtual void Clear() = 0;

	//! Checks if specified resource exist in the list.
	virtual bool IsExist(const char* sResourceFile) = 0;

	//! Loads a resource list from the resource list file.
	virtual bool Load(const char* sResourceListFilename) = 0;

	// Enumeration.

	//! \return The file name of the first resource or NULL if resource list is empty.
	virtual const char* GetFirst() = 0;

	//! Client must call GetFirst before calling GetNext.
	//! \return The file name of the next resource or NULL if reached the end.
	virtual const char* GetNext() = 0;

	//! \return Memory usage stats.
	virtual void GetMemoryStatistics(class ICrySizer* pSizer) = 0;
	// </interfuscator:shuffle>
};

#include <CryString/CryPath.h>

//! Everybody should use fxopen instead of fopen so it will work both on PC and XBox.
inline FILE* fxopen(const char* file, const char* mode, bool bGameRelativePath = false)
{
	if (gEnv && gEnv->pCryPak)
	{
		gEnv->pCryPak->CheckFileAccessDisabled(file, mode);
	}

	bool hasWriteAccess = false;
	bool hasReadAccess = false;
	bool isAppend = false;
	bool isTextMode = false;
	bool isBinaryMode = false;

	for (const char* s = mode; *s; s++)
	{
		if (*s == 'w' || *s == 'W' || *s == 'a' || *s == 'A' || *s == '+')
		{
			hasWriteAccess = true;
		}
		if (*s == 'r' || *s == 'R' || *s == '+')
		{
			hasReadAccess = true;
		}
		if (*s == 'a' || *s == 'A')
		{
			isAppend = true;
		}
		if (*s == 't') // 'T' == temporary mode
		{
			isBinaryMode = false;
			isTextMode = true;
		}
		if (*s == 'b' || *s == 'B')
		{
			isBinaryMode = true;
			isTextMode = false;
		}
	}

	// This is on windows/xbox/Linux/Mac
	if (gEnv && gEnv->pCryPak)
	{
		int nAdjustFlags = 0;
		if (!bGameRelativePath)
		{
			nAdjustFlags |= ICryPak::FLAGS_PATH_REAL;
		}
		if (hasWriteAccess)
		{
			nAdjustFlags |= ICryPak::FLAGS_FOR_WRITING;
		}
		char path[_MAX_PATH];
		const char* szAdjustedPath = gEnv->pCryPak->AdjustFileName(file, path, nAdjustFlags);

#if !CRY_PLATFORM_LINUX && !CRY_PLATFORM_ANDROID && !CRY_PLATFORM_APPLE
		if (hasWriteAccess)
		{
			// Make sure folder is created.
			gEnv->pCryPak->MakeDir(PathUtil::GetParentDirectory(szAdjustedPath).c_str());
		}
#endif

#if CRY_PLATFORM_WINDOWS
		DWORD winAccessFlags = 0;
		DWORD winCreationMode = 0;
		int cOpenFlags = 0;
		if (hasWriteAccess && hasReadAccess)
		{
			winAccessFlags = (GENERIC_READ | GENERIC_WRITE);
			winCreationMode = CREATE_ALWAYS;
			cOpenFlags |= O_RDWR;
		}
		else if (hasWriteAccess)
		{
			winAccessFlags = GENERIC_WRITE;
			winCreationMode = CREATE_ALWAYS;
			cOpenFlags |= O_WRONLY;
		}
		else if (hasReadAccess)
		{
			winAccessFlags = GENERIC_READ;
			winCreationMode = OPEN_EXISTING;
			cOpenFlags |= O_RDONLY;
		}
		if (isTextMode)
		{
			cOpenFlags |= O_TEXT;
		}
		else if (isBinaryMode)
		{
			cOpenFlags |= O_BINARY;
		}
		if (isAppend)
		{
			winCreationMode = OPEN_ALWAYS;
			cOpenFlags |= O_APPEND;
		}

		HANDLE winFile = CreateFile(szAdjustedPath, winAccessFlags, FILE_SHARE_READ, 0, winCreationMode, FILE_ATTRIBUTE_NORMAL, 0);
		if (winFile == INVALID_HANDLE_VALUE)
		{
			return 0;
		}
		
		int cHandle = _open_osfhandle(reinterpret_cast<intptr_t>(winFile), cOpenFlags);
		if (cHandle == -1)
		{
			return 0;
		}
		return _fdopen(cHandle, mode);
#else
		return fopen(szAdjustedPath, mode);
#endif
	}
	else
	{
		return 0;
	}
}

class CScopedAllowFileAccessFromThisThread
{
public:
#if defined(_RELEASE)
	CScopedAllowFileAccessFromThisThread() = default;
	~CScopedAllowFileAccessFromThisThread()	{}
	void End() {}
#else
	CScopedAllowFileAccessFromThisThread()
	{
		m_threadId = CryGetCurrentThreadId();
		m_oldDisable = gEnv->pCryPak ? gEnv->pCryPak->DisableRuntimeFileAccess(false, m_threadId) : false;
		m_active = true;
	}
	~CScopedAllowFileAccessFromThisThread()
	{
		End();
	}
	void End()
	{
		if (m_active)
		{
			if (gEnv && gEnv->pCryPak)
			{
				gEnv->pCryPak->DisableRuntimeFileAccess(m_oldDisable, m_threadId);
			}
			m_active = false;
		}
	}
protected:
	threadID m_threadId;
	bool     m_oldDisable;
	bool     m_active;
#endif
};

#if defined(_RELEASE)
	#define SCOPED_ALLOW_FILE_ACCESS_FROM_THIS_THREAD()
#else
	#define SCOPED_ALLOW_FILE_ACCESS_FROM_THIS_THREAD() CScopedAllowFileAccessFromThisThread allowFileAccess
#endif

//////////////////////////////////////////////////////////////////////////

class CInMemoryFileLoader
{
public:
	CInMemoryFileLoader(ICryPak* pCryPak) : m_pPak(pCryPak), m_pFile(0), m_pBuffer(0), m_pCursor(0), m_nFileSize(0) {}
	~CInMemoryFileLoader()
	{
		Close();
	}

	bool IsFileExists() const
	{
		return m_pFile != 0;
	}

	FILE* GetFileHandle() const
	{
		return m_pFile;
	}

	bool FOpen(const char* name, const char* mode, bool bImmediateCloseFile = false)
	{
		if (m_pPak)
		{
			assert(!m_pFile);
			m_pFile = m_pPak->FOpen(name, mode);
			if (!m_pFile)
				return false;

			m_nFileSize = m_pPak->FGetSize(m_pFile);
			if (m_nFileSize == 0)
			{
				Close();
				return false;
			}

			m_pCursor = m_pBuffer = (char*)m_pPak->PoolMalloc(m_nFileSize);

			size_t nReaded = m_pPak->FReadRawAll(m_pBuffer, m_nFileSize, m_pFile);
			if (nReaded != m_nFileSize)
			{
				Close();
				return false;
			}

			if (bImmediateCloseFile)
			{
				m_pPak->FClose(m_pFile);
				m_pFile = 0;
			}

			return true;
		}

		return false;
	}

	void FClose()
	{
		Close();
	}

	size_t FReadRaw(void* data, size_t length, size_t elems)
	{
		ptrdiff_t dist = m_pCursor - m_pBuffer;

		size_t count = length;
		if (dist + count * elems > m_nFileSize)
			count = (m_nFileSize - dist) / elems;

		memmove(data, m_pCursor, count * elems);
		m_pCursor += count * elems;

		return count;
	}

	template<class T>
	size_t FRead(T* data, size_t elems, bool bSwapEndian = eLittleEndian)
	{
		ptrdiff_t dist = m_pCursor - m_pBuffer;

		size_t count = elems;
		if (dist + count * sizeof(T) > m_nFileSize)
			count = (m_nFileSize - dist) / sizeof(T);

		memmove(data, m_pCursor, count * sizeof(T));
		m_pCursor += count * sizeof(T);

		SwapEndian(data, count, bSwapEndian);
		return count;
	}

	size_t FTell()
	{
		ptrdiff_t dist = m_pCursor - m_pBuffer;
		return dist;
	}

	int FSeek(int64 origin, int command)
	{
		int retCode = -1;
		int64 newPos;
		char* newPosBuf;
		switch (command)
		{
		case SEEK_SET:
			newPos = origin;
			if (newPos <= (int64)m_nFileSize)
			{
				m_pCursor = m_pBuffer + newPos;
				retCode = 0;
			}
			break;
		case SEEK_CUR:
			newPosBuf = m_pCursor + origin;
			if (newPosBuf <= m_pBuffer + m_nFileSize)
			{
				m_pCursor = newPosBuf;
				retCode = 0;
			}
			break;
		case SEEK_END:
			newPos = m_nFileSize - origin;
			if (newPos <= (int64)m_nFileSize)
			{
				m_pCursor = m_pBuffer + newPos;
				retCode = 0;
			}
			break;
		default:
			// Not valid disk operation!
			assert(0);
		}
		return retCode;
	}

private:

	void Close()
	{
		if (m_pFile)
			m_pPak->FClose(m_pFile);

		if (m_pBuffer)
			m_pPak->PoolFree(m_pBuffer);

		m_pBuffer = m_pCursor = 0;
		m_nFileSize = 0;
		m_pFile = 0;
	}

private:
	FILE*    m_pFile;
	char*    m_pBuffer;
	ICryPak* m_pPak;
	char*    m_pCursor;
	size_t   m_nFileSize;
};

//////////////////////////////////////////////////////////////////////////

#if !defined(RESOURCE_COMPILER)

// Include File helpers.
	#include "CryFile.h"

//! Helper class that can be used to recusrively scan the directory.
struct SDirectoryEnumeratorHelper
{
public:
	void ScanDirectoryRecursive(const string& root, const string& pathIn, const string& fileSpec, std::vector<string>& files)
	{
		bool anyFound = false;
		string path = PathUtil::AddSlash(pathIn);
		string dir = PathUtil::AddSlash(PathUtil::AddSlash(root) + path);

		ScanDirectoryFiles(root, path, fileSpec, files);

		string findFilter = PathUtil::Make(dir, "*.*");
		ICryPak* pIPak = gEnv->pCryPak;

		// Add all directories.
		_finddata_t fd;
		intptr_t fhandle;

		fhandle = pIPak->FindFirst(findFilter, &fd);
		if (fhandle != -1)
		{
			do
			{
				// Skip back folders.
				if (fd.name[0] == '.')
					continue;
				if (fd.name[0] == '\0')
				{
					CryFatalError("ICryPak FindFirst/FindNext returned empty name while looking for '%s'", findFilter.c_str());
					continue;
				}
				if (fd.attrib & _A_SUBDIR) // skip sub directories.
				{
					ScanDirectoryRecursive(root, PathUtil::AddSlash(path) + fd.name + "/", fileSpec, files);
					continue;
				}
			}
			while (pIPak->FindNext(fhandle, &fd) == 0);
			pIPak->FindClose(fhandle);
		}
	}
private:
	void ScanDirectoryFiles(const string& root, const string& path, const string& fileSpec, std::vector<string>& files)
	{
		string dir = PathUtil::AddSlash(root + path);

		string findFilter = PathUtil::Make(dir, fileSpec);
		ICryPak* pIPak = gEnv->pCryPak;

		_finddata_t fd;
		intptr_t fhandle;

		fhandle = pIPak->FindFirst(findFilter, &fd);
		if (fhandle != -1)
		{
			do
			{
				//  Skip back folders.
				if (fd.name[0] == '.')
					continue;
				if (fd.attrib & _A_SUBDIR) // skip sub directories.
					continue;
				files.push_back(path + fd.name);
			}
			while (pIPak->FindNext(fhandle, &fd) == 0);
			pIPak->FindClose(fhandle);
		}
	}
};
#endif // !RESOURCE_COMPILER

//! \endcond