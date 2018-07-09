// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! \cond INTERNAL

#pragma once

enum EZInflateState
{
	eZInfState_AwaitingInput,   //!< Caller must call Input() to continue.
	eZInfState_Inflating,       //!< Caller must wait.
	eZInfState_ConsumeOutput,   //!< Caller must consume output and then call SetOutputBuffer() to continue.
	eZInfState_Finished,        //!< Caller must call Release().
	eZInfState_Error            //!< Error has occurred and the stream has been closed and will no longer compress.
};

struct IZLibInflateStream
{
protected:
	virtual ~IZLibInflateStream() {}; //!< Use Release().

public:
	struct SStats
	{
		int bytesInput;
		int bytesOutput;
		int curMemoryUsed;
		int peakMemoryUsed;
	};

	//! Specify the output buffer for the inflate operation.
	//! Should be set before providing input.
	//! The specified buffer must remain valid (ie do not free) whilst compression is in progress (state == eZInfState_Inflating).
	virtual void SetOutputBuffer(char* pInBuffer, unsigned int inSize) = 0;

	//! Return the number of bytes from the output buffer that are ready to be consumed. After consuming any output, you should call SetOutputBuffer() again to mark the buffer as available.
	virtual unsigned int GetBytesOutput() = 0;

	//! Begin decompressing the source data pInSource of length inSourceSize to a previously specified output buffer.
	//! Only valid to be called if the stream is in state eZInfState_AwaitingInput.
	//! The specified buffer must remain valid (ie do not free) whilst compression is in progress (state == eZInfState_Inflating).
	virtual void Input(const char* pInSource, unsigned int inSourceSize) = 0;

	//! Finish the compression, causing all data to be flushed to the output buffer.
	//! Once called no more data can be input.
	//! After calling the caller must wait until GetState() reuturns eZInfState_Finished.
	virtual void EndInput() = 0;

	//! Return the state of the stream.
	virtual EZInflateState GetState() = 0;

	//! Get stats on inflate stream, valid to call at anytime.
	virtual void GetStats(SStats* pOutStats) = 0;

	//! Delete the inflate stream. Will assert if stream is in an invalid state to be released (in state eZInfState_Inflating).
	virtual void Release() = 0;
};

struct IZLibDecompressor
{
protected:
	virtual ~IZLibDecompressor()  {};   //!< Use Release().

public:
	//! Creates a inflate stream to decompress data using zlib.
	virtual IZLibInflateStream* CreateInflateStream() = 0;

	virtual void                Release() = 0;
};

//! \endcond