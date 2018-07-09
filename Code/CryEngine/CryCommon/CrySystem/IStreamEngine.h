// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! \cond INTERNAL

// Some excerpts explaining basic ideas behind streaming design here:

/*
 * The idea is that the data loaded is ready for usage and ideally doesn't need further transformation,
 * therefore the client allocates the buffer (to avoid extra copy). All the data transformations should take place in the Resource Compiler. If you have to allocate a lot of small memory objects, you should revise this strategy in favor of one big allocation (again, that will be read directly from the compiled file).
 * Anyway, we can negotiate that the streaming engine allocates this memory.
 * In the end, it could make use of a memory pool, and copying data is not the bottleneck in our engine
 *
 * The client should take care of all fast operations. Looking up file size should be fast on the virtual
 * file system in a pak file, because the directory should be preloaded in memory
 */

#pragma once

#include <list>
#include <CryCore/smartptr.h>
#include <CrySystem/File/ICryPak.h> // <> required for Interfuscator

#include "IStreamEngineDefs.h"

class IStreamCallback;
class ICrySizer;

#define STREAM_TASK_TYPE_AUDIO_ALL ((1 << eStreamTaskTypeSound) | (1 << eStreamTaskTypeFSBCache))

//! This is used as parameter to the asynchronous read function.
//! All the unnecessary parameters go here, because there are many of them.
struct StreamReadParams
{
public:
	StreamReadParams()
	{
		memset(this, 0, sizeof(*this));
		ePriority = estpNormal;
	}

	StreamReadParams(
	  DWORD_PTR _dwUserData,
	  EStreamTaskPriority _ePriority = estpNormal,
	  unsigned _nLoadTime = 0,
	  unsigned _nMaxLoadTime = 0,
	  unsigned _nOffset = 0,
	  unsigned _nSize = 0,
	  void* _pBuffer = NULL,
	  unsigned _nFlags = 0
	  ) :
		dwUserData(_dwUserData),
		ePriority(_ePriority),
		nPerceptualImportance(0),
		nLoadTime(_nLoadTime),
		nMaxLoadTime(_nMaxLoadTime),
		pBuffer(_pBuffer),
		nOffset(_nOffset),
		nSize(_nSize),
		eMediaType(eStreamSourceTypeUnknown),
		nFlags(_nFlags)
	{
	}

	// File name.
	//const char* szFile;

	// The callback.
	//IStreamCallback* pAsyncCallback;

	//! User data that'll be used to call the callback.
	DWORD_PTR dwUserData;

	//! Priority of this read.
	EStreamTaskPriority ePriority;

	//! Value from 0-255 of the perceptual importance of the task (used for debugging task sheduling).
	uint8 nPerceptualImportance;

	//! Desirable loading time, in milliseconds, from the time of call.
	//! 0 means as fast as possible (desirably in this frame).
	unsigned nLoadTime;

	//! The maximum load time, in milliseconds.
	//! 0 means forever. If the read lasts longer, it can be discarded.
	//! \warning Avoid too small max times, like 1-10 ms, because many loads will be discarded in this case.
	unsigned nMaxLoadTime;

	//! The buffer into which to read the file or the file piece.
	//! If this is NULL, the streaming engine will supply the buffer.
	//! \warning Do not use this buffer during read operations! Do not read from it, as this can lead to memory corruption!
	void* pBuffer;

	//! Offset in the file to read.
	//! If this is not 0 then the file read occurs beginning with the specified offset in bytes.
	//! The callback interface receives the size of already read data as nSize
	//! and generally behaves as if the piece of file would be a file of its own.
	unsigned nOffset;

	//! Number of bytes to read.
	//! If this is 0, then the whole file is read.
	//! If nSize == 0 and nOffset != 0, then the file from the offset to the end is read.
	//! If nSize != 0, then the file piece from nOffset is read, at most nSize bytes
	//! (if less, an error is reported). So, from nOffset byte to nOffset + nSize - 1 byte in the file.
	unsigned nSize;

	//! Media type to use when starting file request - if wrong, the request may take longer to complete.
	EStreamSourceMediaType eMediaType;

	//! The combination of one or several flags from the stream engine general purpose flags.
	unsigned nFlags;
};

struct StreamReadBatchParams
{
	StreamReadBatchParams()
		: tSource((EStreamTaskType)0)
		, szFile(NULL)
		, pCallback(NULL)
	{
	}

	EStreamTaskType  tSource;
	const char*      szFile;
	IStreamCallback* pCallback;
	StreamReadParams params;
};

struct IStreamEngineListener
{
	// <interfuscator:shuffle>
	virtual ~IStreamEngineListener() {}

	virtual void OnStreamEnqueue(const void* pReq, const char* filename, EStreamTaskType source, const StreamReadParams& readParams) = 0;
	virtual void OnStreamComputedSortKey(const void* pReq, uint64 key) = 0;
	virtual void OnStreamBeginIO(const void* pReq, uint32 compressSize, uint32 readSize, EStreamSourceMediaType mediaType) = 0;
	virtual void OnStreamEndIO(const void* pReq) = 0;
	virtual void OnStreamBeginInflate(const void* pReq) = 0;
	virtual void OnStreamEndInflate(const void* pReq) = 0;
	virtual void OnStreamBeginDecrypt(const void* pReq) = 0;
	virtual void OnStreamEndDecrypt(const void* pReq) = 0;
	virtual void OnStreamBeginAsyncCallback(const void* pReq) = 0;
	virtual void OnStreamEndAsyncCallback(const void* pReq) = 0;
	virtual void OnStreamDone(const void* pReq) = 0;
	virtual void OnStreamPreempted(const void* pReq) = 0;
	virtual void OnStreamResumed(const void* pReq) = 0;
	// </interfuscator:shuffle>
};

//! The highest level. There is only one StreamingEngine in the application and it controls all I/O streams.
struct IStreamEngine
{
public:

	enum EJobType
	{
		ejtStarted  = 1 << 0,
		ejtPending  = 1 << 1,
		ejtFinished = 1 << 2,
	};

	//! General purpose flags.
	enum EFlags
	{
		//! If this is set only asynchronous callback will be called.
		FLAGS_NO_SYNC_CALLBACK           = BIT(0),
		//! If this is set the file will be read from disc directly, instead of from the pak system.
		FLAGS_FILE_ON_DISK               = BIT(1),
		//! Ignore the tmp out of streaming memory for this request
		FLAGS_IGNORE_TMP_OUT_OF_MEM      = BIT(2),
		//! External buffer is write only
		FLAGS_WRITE_ONLY_EXTERNAL_BUFFER = BIT(3),
	};

	// <interfuscator:shuffle>
	//! Starts asynchronous read from the specified file (the file may be on a
	//! virtual file system, in pak or zip file or wherever).
	//! Reads the file contents into the given buffer, up to the given size.
	//! Upon success, calls success callback. If the file is truncated or for other
	//! reason can not be read, calls error callback. The callback can be NULL (in this case, the client should poll
	//! the returned IReadStream object; the returned object must be locked for that)
	//! \note The error/success/ progress callbacks can also be called from INSIDE this function.
	//! \param pParamsPlaceholder for the future additional parameters (like priority), or really a pointer to a structure that will hold the parameters if there are too many of them.
	//! \return IReadStream is reference-counted and will be automatically deleted if you don't refer to it; if you don't store it immediately in an auto-pointer,
	//! it may be deleted as soon as on the next line of code, because the read operation may complete immediately inside StartRead() and the object is self-disposed as soon as the callback is called.
	//! \note In some implementations disposal of the old pointers happen synchronously (in the main thread) outside StartRead() (it happens in the entity update),
	//! so you're guaranteed that it won't trash inside the calling function. However, this may change in the future
	//! and you'll be required to assign it to IReadStream immediately (StartRead will return IReadStream_AutoPtr then).
	virtual IReadStreamPtr StartRead(const EStreamTaskType tSource, const char* szFile, IStreamCallback* pCallback = NULL, const StreamReadParams* pParams = NULL) = 0;

	virtual size_t         StartBatchRead(IReadStreamPtr* pStreamsOut, const StreamReadBatchParams* pReqs, size_t numReqs) = 0;

	//! Call this method before submitting large number of new requests.
	virtual void BeginReadGroup() = 0;

	//! Call this method after submitting large number of new requests.
	virtual void EndReadGroup() = 0;

	//! Pause/resumes streaming of specific data types.
	//! \param nPauseTypesBitmask Bit mask of data types (ex, 1<<eStreamTaskTypeGeometry).
	virtual void PauseStreaming(bool bPause, uint32 nPauseTypesBitmask) = 0;

	//! Get pause bit mask.
	virtual uint32 GetPauseMask() const = 0;

	//! Pause/resumes any IO active from the streaming engine.
	virtual void PauseIO(bool bPause) = 0;

	//! Is the streaming data available on harddisc for fast streaming
	virtual bool IsStreamDataOnHDD() const = 0;

	//! Inform streaming engine that the streaming data is available on HDD
	virtual void SetStreamDataOnHDD(bool bFlag) = 0;

	//! Per frame update ofthe streaming engine, synchronous events are dispatched from this function.
	virtual void Update() = 0;

	//! Per frame update of the streaming engine, synchronous events are dispatched from this function, by particular TypesBitmask.
	virtual void Update(uint32 nUpdateTypesBitmask) = 0;

	//! Waits until all submitted requests are complete. (can abort all reads which are currently in flight)
	virtual void UpdateAndWait(bool bAbortAll = false) = 0;

	//! Puts the memory statistics into the given sizer object.
	//! According to the specifications in interface ICrySizer.
	virtual void GetMemoryStatistics(ICrySizer* pSizer) = 0;

#if defined(STREAMENGINE_ENABLE_STATS)
	//! Returns the streaming statistics collected from the previous call.
	virtual SStreamEngineStatistics& GetStreamingStatistics() = 0;
	virtual void                     ClearStatistics() = 0;

	//! Returns the bandwidth used for the given type of streaming task.
	virtual void GetBandwidthStats(EStreamTaskType type, float* bandwidth) = 0;
#endif

	//! Returns the counts of open streaming requests.
	virtual void        GetStreamingOpenStatistics(SStreamEngineOpenStats& openStatsOut) = 0;

	virtual const char* GetStreamTaskTypeName(EStreamTaskType type) = 0;

#if defined(STREAMENGINE_ENABLE_LISTENER)
	//! Sets up a listener for stream events (used for statoscope).
	virtual void                   SetListener(IStreamEngineListener* pListener) = 0;
	virtual IStreamEngineListener* GetListener() = 0;
#endif

	virtual ~IStreamEngine() {}
	// </interfuscator:shuffle>
};

//! This is the file "handle" that can be used to query the status
//! of the asynchronous operation on the file. The same object may be returned
//! for the same file to multiple clients.
//! \note It will actually represent the asynchronous object in memory, and will be
//! thread-safe reference-counted (both AddRef() and Release() will be virtual
//! and thread-safe, just like the others)
//! Example:
//! USE:
//! IReadStream_AutoPtr pReadStream = pStreamEngine->StartRead ("bla.xxx", this);
//! OR:
//! pStreamEngine->StartRead ("SomeSystem","bla.xxx", this);
class IReadStream
{
public:
	// <interfuscator:shuffle>
	//! Increment ref count, returns new count
	virtual int AddRef() = 0;

	//! Decrement ref count, returns new count
	virtual int Release() = 0;

	//! Returns true if the file read was not successful.
	virtual bool IsError() = 0;

	//! Checks IsError to check if the whole requested file (piece) was read.
	//! \return true if the file read was completed successfully.
	virtual bool IsFinished() = 0;

	//! Returns the number of bytes read so far (the whole buffer size if IsFinished())
	//! \param bWait if true, wait until the pending I/O operation completes.
	//! \return The total number of bytes read (if it completes successfully, returns the size of block being read)
	virtual unsigned int GetBytesRead(bool bWait = false) = 0;

	//! Returns the buffer into which the data has been or will be read
	//! at least GetBytesRead() bytes in this buffer are guaranteed to be already read.
	//! \warning Do not use this buffer during read operations! Do not read from it, as it can lead to memory corruption!
	virtual const void* GetBuffer() = 0;

	//! Returns the transparent DWORD that was passed in the StreamReadParams::dwUserData field of the structure passed in the call to IStreamEngine::StartRead.
	virtual DWORD_PTR GetUserData() = 0;

	//! Set user defined data into stream's params.
	virtual void SetUserData(DWORD_PTR dwUserData) = 0;

	//! Tries to stop reading the stream.
	//! This is advisory and may have no effect but the callback will not be called after this.
	//! If you just destructing object, dereference this object and it will automatically abort and release all associated resources.
	virtual void Abort() = 0;

	//! Tries to stop reading the stream, as long as IO or the async callback is not currently in progress.
	virtual bool TryAbort() = 0;

	//! Unconditionally waits until the callback is called.
	//! If nMaxWaitMillis is not negative wait for the specified ammount of milliseconds then exit.
	//! Example:
	//! If the stream hasn't yet finish, it's guaranteed that the user-supplied callback
	//! is called before return from this function (unless no callback was specified).
	virtual void Wait(int nMaxWaitMillis = -1) = 0;

	//! Return stream params.
	virtual const StreamReadParams& GetParams() const = 0;

	//! Return caller type.
	virtual const EStreamTaskType GetCallerType() const = 0;

	//! Return media type used to satisfy request - only valid once stream has begun read.
	virtual EStreamSourceMediaType GetMediaType() const = 0;

	//! Return pointer to callback routine(can be NULL).
	virtual IStreamCallback* GetCallback() const = 0;

	//! Return IO error #.
	virtual unsigned GetError() const = 0;

	//! Return IO error name
	virtual const char* GetErrorName() const = 0;

	//! Return stream name.
	virtual const char* GetName() const = 0;

	//! Free temporary memory allocated for this stream, when not needed anymore.
	//! Can be called from Async callback, to free memory earlier, not waiting for synchrounus callback.
	virtual void FreeTemporaryMemory() = 0;
	// </interfuscator:shuffle>

protected:
	//! The clients are not allowed to destroy this object directly; only via Release().
	virtual ~IReadStream() {}
};

TYPEDEF_AUTOPTR(IReadStream);

//! CryPak supports asynchronous reading through this interface.
//! The callback is called from the main thread in the frame update loop.
//! The callback receives packets through StreamOnComplete() and
//! StreamOnProgress(). The second one can be used to update the asset based
//! on the partial data that arrived. the callback that will be called by the
//! streaming engine must be implemented by all clients that want to use
//! StreamingEngine services
//! Remarks:
//! the pStream interface is guaranteed to be locked (have reference count > 0)
//! while inside the function, but can vanish any time outside the function.
//! If you need it, keep it from the beginning (after call to StartRead())
//! some or all callbacks MAY be called from inside IStreamEngine::StartRead()
//! Example:
//! \code
//! IStreamEngine *pStreamEngine = g_pISystem->GetStreamEngine();     // get streaming engine
//! IStreamCallback *pAsyncCallback = &MyClass;                       // user
//! StreamReadParams params;
//! params.dwUserData = 0;
//! params.nSize = 0;
//! params.pBuffer = NULL;
//! params.nLoadTime = 10000;
//! params.nMaxLoadTime = 10000;
//! pStreamEngine->StartRead(  .. pAsyncCallback .. params .. );      // registers callback
//! \endcode
class IStreamCallback
{
public:
	// <interfuscator:shuffle>
	virtual ~IStreamCallback(){}

	//! Signals that the file length for the request has been found, and that storage is needed.
	//! Either a pointer to a block of nSize bytes can be returned, into which the file will be
	//! streamed, or NULL can be returned, in which case temporary memory will be allocated
	//! internally by the stream engine (which will be freed upon job completion).
	virtual void* StreamOnNeedStorage(IReadStream* pStream, unsigned nSize, bool& bAbortOnFailToAlloc) { return NULL; }

	//! Signals that reading the requested data has completed (with or without error).
	//! This callback is always called, whether an error occurs or not.
	//! pStream will signal either IsFinished() or IsError() and will hold the (perhaps partially) read data until this interface is released.
	//! GetBytesRead() will return the size of the file (the completely read buffer) in case of successful operation end
	//! or the size of partially read data in case of error (0 if nothing was read).
	//! Pending status is true during this callback, because the callback itself is the part of IO operation.
	//! \param[in] nError 0 indicates success, a non-zero value is an error code.
	virtual void StreamAsyncOnComplete(IReadStream* pStream, unsigned nError) {}

	//! Signals that reading the requested data has completed (with or without error).
	//! This callback is always called, whether an error occurs or not.
	//! pStream will signal either IsFinished() or IsError() and will hold the (perhaps partially) read data until this interface is released.
	//! GetBytesRead() will return the size of the file (the completely read buffer) in case of successful operation end
	//! or the size of partially read data in case of error (0 if nothing was read).
	//! Pending status is true during this callback, because the callback itself is the part of IO operation.
	//! \param[in] nError 0 indicates success, a non-zero value is an error code.
	virtual void StreamOnComplete(IReadStream* pStream, unsigned nError) {}
	// </interfuscator:shuffle>
};

//! \endcond