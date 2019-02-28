// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifndef __WAVEFILE_H__
	#define __WAVEFILE_H__

	#include <CryCore/Platform/CryWindows.h>
//#include <XAudDefs.h>

namespace ATG
{

//--------------------------------------------------------------------------------------
// Misc type definitions
//--------------------------------------------------------------------------------------
typedef DWORD FOURCC, * PFOURCC, * LPFOURCC;

//--------------------------------------------------------------------------------------
// Format tags
//--------------------------------------------------------------------------------------
	#define WAVE_FORMAT_PCM        1
	#define WAVE_FORMAT_EXTENSIBLE 0xFFFE

//--------------------------------------------------------------------------------------
// For parsing WAV files
//--------------------------------------------------------------------------------------

typedef struct tWAVEFORMATEX
{
	WORD  wFormatTag;                         // Format type
	WORD  nChannels;                          // Channel count
	DWORD nSamplesPerSec;                     // Sampling rate
	DWORD nAvgBytesPerSec;                    // Average number of bytes per second
	WORD  nBlockAlign;                        // Block size of data
	WORD  wBitsPerSample;                     // Count of bits per mono sample
	WORD  cbSize;                             // Bytes of extra format information following this structure
} WAVEFORMATEX, * PWAVEFORMATEX, * LPWAVEFORMATEX;

typedef const WAVEFORMATEX* LPCWAVEFORMATEX;

//--------------------------------------------------------------------------------------
// For parsing WAV files
//--------------------------------------------------------------------------------------

typedef struct
{
	WAVEFORMATEX Format;                      // WAVEFORMATEX data

	union
	{
		WORD wValidBitsPerSample;               // Bits of precision
		WORD wSamplesPerBlock;                  // Samples per block of audio data
		WORD wReserved;                         // Unused -- must be 0
	}     Samples;

	DWORD dwChannelMask;                      // Channel usage bitmask
	GUID  SubFormat;                          // Sub-format identifier
} WAVEFORMATEXTENSIBLE, * PWAVEFORMATEXTENSIBLE, * LPWAVEFORMATEXTENSIBLE;

typedef const WAVEFORMATEXTENSIBLE* LPCWAVEFORMATEXTENSIBLE;

//--------------------------------------------------------------------------------------
// For parsing WAV files
//--------------------------------------------------------------------------------------
struct RIFFHEADER
{
	FOURCC fccChunkId;
	DWORD  dwDataSize;
};

	#define RIFFCHUNK_FLAGS_VALID 0x00000001

//--------------------------------------------------------------------------------------
// Name: class RiffChunk
// Desc: RIFF chunk utility class
//--------------------------------------------------------------------------------------
class RiffChunk
{
	FOURCC           m_fccChunkId;         // Chunk identifier
	const RiffChunk* m_pParentChunk;       // Parent chunk
	HANDLE           m_hFile;
	DWORD            m_dwDataOffset;        // Chunk data offset
	DWORD            m_dwDataSize;          // Chunk data size
	DWORD            m_dwFlags;             // Chunk flags

public:
	RiffChunk();

	// Initialization
	VOID    Initialize(FOURCC fccChunkId, const RiffChunk* pParentChunk,
	                   HANDLE hFile);
	HRESULT Open();
	BOOL    IsValid() const { return !!(m_dwFlags & RIFFCHUNK_FLAGS_VALID); }

	// Data
	HRESULT ReadData(LONG lOffset, VOID* pData, DWORD dwDataSize) const;

	// Chunk information
	FOURCC GetChunkId() const  { return m_fccChunkId; }
	DWORD  GetDataSize() const { return m_dwDataSize; }
};

//--------------------------------------------------------------------------------------
// Name: class RiffChunkMemory
// Desc: RIFF chunk utility class
//--------------------------------------------------------------------------------------
class RiffChunkMemory
{
	FOURCC                 m_fccChunkId;        // Chunk identifier
	const RiffChunkMemory* m_pParentChunk;      // Parent chunk
	const char*            m_pFileData;
	uint32                 m_nFileSize;         // File Size
	DWORD                  m_dwDataOffset;      // Chunk data offset
	DWORD                  m_dwDataSize;        // Chunk data size
	DWORD                  m_dwFlags;           // Chunk flags

public:
	RiffChunkMemory();

	// Initialization
	VOID    Initialize(FOURCC fccChunkId, const RiffChunkMemory* pParentChunk, const char* pFileData, uint32 nFileSize);
	HRESULT Open();
	BOOL    IsValid() const { return !!(m_dwFlags & RIFFCHUNK_FLAGS_VALID); }

	// Data
	HRESULT ReadData(LONG lOffset, VOID* pData, DWORD dwDataSize) const;

	// Chunk information
	FOURCC GetChunkId() const  { return m_fccChunkId; }
	DWORD  GetDataSize() const { return m_dwDataSize; }
};

//--------------------------------------------------------------------------------------
// Name: class WaveFile
// Desc: Wave file utility class
//--------------------------------------------------------------------------------------
class WaveFileMemory
{
	const char*     m_pFileData;          // Ptr to File data
	uint32          m_nFileSize;          // Size of Sound file
	RiffChunkMemory m_RiffChunk;          // RIFF chunk
	RiffChunkMemory m_FormatChunk;        // Format chunk
	RiffChunkMemory m_DataChunk;          // Data chunk
	RiffChunkMemory m_WaveSampleChunk;    // Wave Sample chunk
	RiffChunkMemory m_SamplerChunk;       // Sampler chunk

public:
	WaveFileMemory();
	~WaveFileMemory();

	// Initialization
	HRESULT Init(const char* pFileData, uint32 nFileSize);

	// File format
	HRESULT GetFormat(WAVEFORMATEXTENSIBLE* pwfxFormat) const;

	// File data
	HRESULT ReadSample(DWORD dwPosition, VOID* pBuffer, DWORD dwBufferSize,
	                   DWORD* pdwRead) const;

	// Loop region
	HRESULT GetLoopRegion(DWORD* pdwStart, DWORD* pdwLength) const;
	HRESULT GetLoopRegionBytes(DWORD* pdwStart, DWORD* pdwLength) const;

	// File properties
	VOID GetDuration(DWORD* pdwDuration) const { *pdwDuration = m_DataChunk.GetDataSize(); }
};

} // namespace ATG

#endif // __WAVEFILE_H__

