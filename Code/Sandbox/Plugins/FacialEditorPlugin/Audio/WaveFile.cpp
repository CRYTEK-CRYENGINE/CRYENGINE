// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include "WaveFile.h"
//#include "AtgUtil.h"

// This might be defined by WindowsSDK
#undef FOURCC_RIFF

namespace ATG
{

#ifndef MAKEFOURCC
	#define MAKEFOURCC(ch0, ch1, ch2, ch3)              \
	  ((DWORD)(BYTE)(ch0) | ((DWORD)(BYTE)(ch1) << 8) | \
	   ((DWORD)(BYTE)(ch2) << 16) | ((DWORD)(BYTE)(ch3) << 24))
#endif /* defined(MAKEFOURCC) */

//--------------------------------------------------------------------------------------
// FourCC definitions
//--------------------------------------------------------------------------------------
const DWORD FOURCC_RIFF = MAKEFOURCC('R', 'I', 'F', 'F');
const DWORD FOURCC_WAVE = MAKEFOURCC('W', 'A', 'V', 'E');
const DWORD FOURCC_FORMAT = MAKEFOURCC('f', 'm', 't', ' ');
const DWORD FOURCC_DATA = MAKEFOURCC('d', 'a', 't', 'a');
const DWORD FOURCC_WSMP = MAKEFOURCC('w', 's', 'm', 'p');
const DWORD FOURCC_SMPL = MAKEFOURCC('l', 's', 'm', 'p');

//--------------------------------------------------------------------------------------
// RIFF chunk type that contains loop point information
//--------------------------------------------------------------------------------------
struct WAVESAMPLE
{
	DWORD dwSize;
	WORD  UnityNote;
	SHORT FineTune;
	LONG  Gain;
	DWORD dwOptions;
	DWORD dwSampleLoops;
};

//--------------------------------------------------------------------------------------
// Loop point (contained in WSMP chunk)
//--------------------------------------------------------------------------------------
struct WAVESAMPLE_LOOP
{
	DWORD dwSize;
	DWORD dwLoopType;
	DWORD dwLoopStart;
	DWORD dwLoopLength;
};

//--------------------------------------------------------------------------------------
// RIFF chunk that may contain loop point information
//--------------------------------------------------------------------------------------
struct SAMPLER
{
	DWORD dwManufacturer;
	DWORD dwProduct;
	DWORD dwSamplePeriod;
	DWORD dwMIDIUnityNote;
	DWORD dwMIDIPitchFraction;
	DWORD dwSMPTEFormat;
	DWORD dwSMPTEOffset;
	DWORD dwNumSampleLoops;
	DWORD dwSamplerData;
};

//--------------------------------------------------------------------------------------
// Loop point contained in SMPL chunk
//--------------------------------------------------------------------------------------
struct SAMPLER_LOOP
{
	DWORD dwCuePointID;
	DWORD dwType;
	DWORD dwStart;
	DWORD dwEnd;
	DWORD dwFraction;
	DWORD dwPlayCount;
};

//--------------------------------------------------------------------------------------
// Name: RiffChunk()
// Desc: Constructor
//--------------------------------------------------------------------------------------
RiffChunk::RiffChunk()
{
	// Initialize defaults
	m_fccChunkId = 0;
	m_pParentChunk = NULL;
	m_hFile = INVALID_HANDLE_VALUE;
	m_dwDataOffset = 0;
	m_dwDataSize = 0;
	m_dwFlags = 0;
}

//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initializes the Riff chunk for use
//--------------------------------------------------------------------------------------
VOID RiffChunk::Initialize(FOURCC fccChunkId, const RiffChunk* pParentChunk,
                           HANDLE hFile)
{
	m_fccChunkId = fccChunkId;
	m_pParentChunk = pParentChunk;
	m_hFile = hFile;
}

//--------------------------------------------------------------------------------------
// Name: Open()
// Desc: Opens an existing chunk
//--------------------------------------------------------------------------------------
HRESULT RiffChunk::Open()
{
	LONG lOffset = 0;

	// Seek to the first byte of the parent chunk's data section
	if (m_pParentChunk)
	{
		lOffset = m_pParentChunk->m_dwDataOffset;

		// Special case the RIFF chunk
		if (FOURCC_RIFF == m_pParentChunk->m_fccChunkId)
			lOffset += sizeof(FOURCC);
	}

	// Read each child chunk header until we find the one we're looking for
	for (;; )
	{
		if (INVALID_SET_FILE_POINTER == SetFilePointer(m_hFile, lOffset, NULL, FILE_BEGIN))
			return HRESULT_FROM_WIN32(GetLastError());

		RIFFHEADER rhRiffHeader;
		DWORD dwRead;
		if (0 == ReadFile(m_hFile, &rhRiffHeader, sizeof(rhRiffHeader), &dwRead, NULL))
			return HRESULT_FROM_WIN32(GetLastError());
		//rhRiffHeader.dwDataSize = __loadwordbytereverse( 0, &rhRiffHeader.dwDataSize);
		SwapEndian(rhRiffHeader.dwDataSize);

		// Hit EOF without finding it
		if (0 == dwRead)
			return E_FAIL;

		// Check if we found the one we're looking for
		if (m_fccChunkId == rhRiffHeader.fccChunkId)
		{
			// Save the chunk size and data offset
			m_dwDataOffset = lOffset + sizeof(rhRiffHeader);
			m_dwDataSize = rhRiffHeader.dwDataSize;

			// Success
			m_dwFlags |= RIFFCHUNK_FLAGS_VALID;

			return S_OK;
		}

		lOffset += sizeof(rhRiffHeader) + rhRiffHeader.dwDataSize;
	}
}

//--------------------------------------------------------------------------------------
// Name: ReadData()
// Desc: Reads from the file
//--------------------------------------------------------------------------------------
HRESULT RiffChunk::ReadData(LONG lOffset, VOID* pData, DWORD dwDataSize) const
{
	// Seek to the offset
	DWORD dwPosition = SetFilePointer(m_hFile, m_dwDataOffset + lOffset, NULL, FILE_BEGIN);
	if (INVALID_SET_FILE_POINTER == dwPosition)
		return HRESULT_FROM_WIN32(GetLastError());

	// Read from the file
	DWORD dwRead;
	if (0 == ReadFile(m_hFile, pData, dwDataSize, &dwRead, NULL))
		return HRESULT_FROM_WIN32(GetLastError());

	return S_OK;
}

// RiffChunkMemory
//--------------------------------------------------------------------------------------
// Name: RiffChunkMemory()
// Desc: Constructor
//--------------------------------------------------------------------------------------
RiffChunkMemory::RiffChunkMemory()
{
	// Initialize defaults
	m_fccChunkId = 0;
	m_pParentChunk = NULL;
	m_pFileData = NULL;
	m_nFileSize = 0;
	m_dwDataOffset = 0;
	m_dwDataSize = 0;
	m_dwFlags = 0;
}

//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initializes the Riff chunk for use
//--------------------------------------------------------------------------------------
VOID RiffChunkMemory::Initialize(FOURCC fccChunkId, const RiffChunkMemory* pParentChunk, const char* pFileData, uint32 nFileSize)
{
	m_fccChunkId = fccChunkId;
	m_pParentChunk = pParentChunk;
	m_pFileData = pFileData;
	m_nFileSize = nFileSize;
}

//--------------------------------------------------------------------------------------
// Name: Open()
// Desc: Opens an existing chunk
//--------------------------------------------------------------------------------------
HRESULT RiffChunkMemory::Open()
{
	LONG lOffset = 0;
	const char* pLocalPtr = NULL;

	// Seek to the first byte of the parent chunk's data section
	if (m_pParentChunk)
	{
		lOffset = m_pParentChunk->m_dwDataOffset;

		// Special case the RIFF chunk
		if (FOURCC_RIFF == m_pParentChunk->m_fccChunkId)
			lOffset += sizeof(FOURCC);
	}

	// Read each child chunk header until we find the one we're looking for
	while (lOffset < m_nFileSize)
	{
		pLocalPtr = m_pFileData + lOffset;
		//
		//if( INVALID_SET_FILE_POINTER == SetFilePointer( m_hFile, lOffset, NULL, FILE_BEGIN ) )
		//return HRESULT_FROM_WIN32( GetLastError() );

		RIFFHEADER rhRiffHeader;
		//DWORD dwRead;

		memcpy(&rhRiffHeader, pLocalPtr, sizeof(rhRiffHeader));
		//if( 0 == ReadFile( m_hFile, &rhRiffHeader, sizeof(rhRiffHeader), &dwRead, NULL ) )
		//return HRESULT_FROM_WIN32( GetLastError() );
		//rhRiffHeader.dwDataSize = __loadwordbytereverse( 0, &rhRiffHeader.dwDataSize);
		SwapEndian(rhRiffHeader.dwDataSize);
		SwapEndian(rhRiffHeader.fccChunkId);

		// Hit EOF without finding it
		//if( 0 == dwRead )
		//return E_FAIL;

		// Check if we found the one we're looking for
		if (m_fccChunkId == rhRiffHeader.fccChunkId)
		{
			// Save the chunk size and data offset
			m_dwDataOffset = lOffset + sizeof(rhRiffHeader);
			m_dwDataSize = rhRiffHeader.dwDataSize;

			// Success
			m_dwFlags |= RIFFCHUNK_FLAGS_VALID;

			return S_OK;
		}

		lOffset += sizeof(rhRiffHeader) + rhRiffHeader.dwDataSize;
	}
	return E_FAIL;
}

//--------------------------------------------------------------------------------------
// Name: ReadData()
// Desc: Reads from the file
//--------------------------------------------------------------------------------------
HRESULT RiffChunkMemory::ReadData(LONG lOffset, VOID* pData, DWORD dwDataSize) const
{
	// Seek to the offset
	const char* pLocalPtr = m_pFileData + m_dwDataOffset + lOffset;

	//DWORD dwPosition = SetFilePointer( m_hFile, m_dwDataOffset+lOffset, NULL, FILE_BEGIN );
	//if( INVALID_SET_FILE_POINTER == dwPosition )
	//return HRESULT_FROM_WIN32( GetLastError() );

	// Read from the file
	//DWORD dwRead;
	//if( 0 == ReadFile( m_hFile, pData, dwDataSize, &dwRead, NULL ) )
	//return HRESULT_FROM_WIN32( GetLastError() );
	memcpy(pData, pLocalPtr, dwDataSize);

	return S_OK;
}

// WaveFileMemory

//--------------------------------------------------------------------------------------
// Name: WaveFileMemory()
// Desc: Constructor
//--------------------------------------------------------------------------------------
WaveFileMemory::WaveFileMemory()
{
	m_pFileData = NULL;
}

//--------------------------------------------------------------------------------------
// Name: ~WaveFileMemory()
// Desc: Destructor
//--------------------------------------------------------------------------------------
WaveFileMemory::~WaveFileMemory()
{
}

//--------------------------------------------------------------------------------------
// Name: Open()
// Desc: Initializes the object
//--------------------------------------------------------------------------------------
HRESULT WaveFileMemory::Init(const char* pFileData, uint32 nFileSize)
{

	if (!pFileData)
		return ERROR_INVALID_DATA;

	m_pFileData = pFileData;
	m_nFileSize = nFileSize;

	// Initialize the chunk objects
	m_RiffChunk.Initialize(FOURCC_RIFF, NULL, m_pFileData, m_nFileSize);
	m_FormatChunk.Initialize(FOURCC_FORMAT, &m_RiffChunk, m_pFileData, m_nFileSize);
	m_DataChunk.Initialize(FOURCC_DATA, &m_RiffChunk, m_pFileData, m_nFileSize);
	m_WaveSampleChunk.Initialize(FOURCC_WSMP, &m_RiffChunk, m_pFileData, m_nFileSize);
	m_SamplerChunk.Initialize(FOURCC_SMPL, &m_RiffChunk, m_pFileData, m_nFileSize);

	HRESULT hr = m_RiffChunk.Open();
	if (FAILED(hr))
		return hr;

	hr = m_FormatChunk.Open();
	if (FAILED(hr))
		return hr;

	hr = m_DataChunk.Open();
	if (FAILED(hr))
		return hr;

	// Wave Sample and Sampler chunks are not required
	m_WaveSampleChunk.Open();
	m_SamplerChunk.Open();

	// Validate the file type
	FOURCC fccType;
	hr = m_RiffChunk.ReadData(0, &fccType, sizeof(fccType));
	if (FAILED(hr))
		return hr;

	if (FOURCC_WAVE != fccType)
		return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);

	return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: GetFormat()
// Desc: Gets the wave file format.  Since Xbox only supports WAVE_FORMAT_PCM,
//       WAVE_FORMAT_XBOX_ADPCM, and WAVE_FORMAT_EXTENSIBLE, we know any
//       valid format will fit into a WAVEFORMATEXTENSIBLE struct
//--------------------------------------------------------------------------------------
HRESULT WaveFileMemory::GetFormat(WAVEFORMATEXTENSIBLE* pwfxFormat) const
{
	assert(pwfxFormat);
	DWORD dwValidSize = m_FormatChunk.GetDataSize();

	// Anything larger than WAVEFORMATEXTENSIBLE is not a valid Xbox WAV file
	assert(dwValidSize <= sizeof(WAVEFORMATEXTENSIBLE));

	char* buffer = new char[dwValidSize];

	// Read the format chunk into the buffer
	HRESULT hr = m_FormatChunk.ReadData(0, buffer, dwValidSize);
	if (FAILED(hr))
		return hr;

	int len = MIN(dwValidSize, sizeof(WAVEFORMATEXTENSIBLE));
	memcpy(pwfxFormat, buffer, len);

	delete[]buffer;

	// Endianness conversion
	//pwfxFormat->Format.wFormatTag       = __loadshortbytereverse( 0, &pwfxFormat->Format.wFormatTag );
	//pwfxFormat->Format.nChannels        = __loadshortbytereverse( 0, &pwfxFormat->Format.nChannels );
	//pwfxFormat->Format.nSamplesPerSec   = __loadwordbytereverse( 0, &pwfxFormat->Format.nSamplesPerSec );
	//pwfxFormat->Format.nAvgBytesPerSec  = __loadwordbytereverse( 0, &pwfxFormat->Format.nAvgBytesPerSec );
	//pwfxFormat->Format.nBlockAlign      = __loadshortbytereverse( 0, &pwfxFormat->Format.nBlockAlign );
	//pwfxFormat->Format.wBitsPerSample   = __loadshortbytereverse( 0, &pwfxFormat->Format.wBitsPerSample );
	//pwfxFormat->Format.cbSize           = __loadshortbytereverse( 0, &pwfxFormat->Format.cbSize );
	//pwfxFormat->Samples.wReserved       = __loadshortbytereverse( 0, &pwfxFormat->Samples.wReserved );
	//pwfxFormat->dwChannelMask           = __loadwordbytereverse( 0, &pwfxFormat->dwChannelMask );
	//pwfxFormat->SubFormat.Data1         = __loadwordbytereverse( 0, &pwfxFormat->SubFormat.Data1 );
	//pwfxFormat->SubFormat.Data2         = __loadshortbytereverse( 0, &pwfxFormat->SubFormat.Data2 );
	//pwfxFormat->SubFormat.Data3         = __loadshortbytereverse( 0, &pwfxFormat->SubFormat.Data3 );
	SwapEndian(pwfxFormat->Format.wFormatTag);
	SwapEndian(pwfxFormat->Format.nChannels);
	SwapEndian(pwfxFormat->Format.nSamplesPerSec);
	SwapEndian(pwfxFormat->Format.nAvgBytesPerSec);
	SwapEndian(pwfxFormat->Format.nBlockAlign);
	SwapEndian(pwfxFormat->Format.wBitsPerSample);
	SwapEndian(pwfxFormat->Format.cbSize);
	SwapEndian(pwfxFormat->Samples.wReserved);
	SwapEndian(pwfxFormat->dwChannelMask);
	SwapEndian(pwfxFormat->SubFormat.Data1);
	SwapEndian(pwfxFormat->SubFormat.Data2);
	SwapEndian(pwfxFormat->SubFormat.Data3);

	// Data4 is a array of char, not needed to convert

	// Zero out remaining bytes, in case enough bytes were not read
	if (dwValidSize < sizeof(WAVEFORMATEXTENSIBLE))
		ZeroMemory((BYTE*)pwfxFormat + dwValidSize, sizeof(WAVEFORMATEXTENSIBLE) - dwValidSize);

	return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: ReadSample()
// Desc: Reads data from the audio file.
//--------------------------------------------------------------------------------------
HRESULT WaveFileMemory::ReadSample(DWORD dwPosition, VOID* pBuffer,
                                   DWORD dwBufferSize, DWORD* pdwRead) const
{
	// Don't read past the end of the data chunk
	DWORD dwDuration;
	GetDuration(&dwDuration);

	// Check bit size for endinaness conversion.
	WAVEFORMATEXTENSIBLE wfxFormat;
	GetFormat(&wfxFormat);

	if (dwPosition + dwBufferSize > dwDuration)
		dwBufferSize = dwDuration - dwPosition;

	HRESULT hr = S_OK;
	if (dwBufferSize)
		hr = m_DataChunk.ReadData((LONG)dwPosition, pBuffer, dwBufferSize);

	//Endianness conversion
	if (wfxFormat.Format.wFormatTag == WAVE_FORMAT_PCM && wfxFormat.Format.wBitsPerSample == 16)
	{
		SHORT* pBufferShort = (SHORT*)pBuffer;
		for (DWORD i = 0; i < dwBufferSize / sizeof(SHORT); i++)
			//pBufferShort[i]  = __loadshortbytereverse( 0, &pBufferShort[i] );
			SwapEndian(pBufferShort[i]);
	}

	if (pdwRead)
		*pdwRead = dwBufferSize;

	return hr;
}

//--------------------------------------------------------------------------------------
// Name: GetLoopRegion()
// Desc: Gets the loop region, in terms of samples
//--------------------------------------------------------------------------------------
HRESULT WaveFileMemory::GetLoopRegion(DWORD* pdwStart, DWORD* pdwLength) const
{
	assert(pdwStart != NULL);
	assert(pdwLength != NULL);
	HRESULT hr = S_OK;

	*pdwStart = 0;
	*pdwLength = 0;

	// First, look for a MIDI-style SMPL chunk, then for a DLS-style WSMP chunk.
	BOOL bGotLoopRegion = FALSE;
	if (!bGotLoopRegion && m_SamplerChunk.IsValid())
	{
		// Read the SAMPLER struct from the chunk
		SAMPLER smpl;
		hr = m_SamplerChunk.ReadData(0, &smpl, sizeof(SAMPLER));
		if (FAILED(hr))
			return hr;

		//Endianness conversion
		SwapEndian((LONG*)&smpl, sizeof(SAMPLER) / sizeof(LONG));

		// Check if the chunk contains any loop regions
		if (smpl.dwNumSampleLoops > 0)
		{
			SAMPLER_LOOP smpl_loop;
			hr = m_SamplerChunk.ReadData(sizeof(SAMPLER), &smpl_loop, sizeof(SAMPLER_LOOP));
			if (FAILED(hr))
				return E_FAIL;

			//Endianness conversion
			SwapEndian((LONG*)&smpl_loop, sizeof(SAMPLER_LOOP) / sizeof(LONG));

			// Documentation on the SMPL chunk indicates that dwStart and
			// dwEnd are stored as byte-offsets, rather than sample counts,
			// but SoundForge stores sample counts, so we'll go with that.
			*pdwStart = smpl_loop.dwStart;
			*pdwLength = smpl_loop.dwEnd - smpl_loop.dwStart + 1;
			bGotLoopRegion = TRUE;
		}
	}

	if (!bGotLoopRegion && m_WaveSampleChunk.IsValid())
	{
		// Read the WAVESAMPLE struct from the chunk
		WAVESAMPLE ws;
		hr = m_WaveSampleChunk.ReadData(0, &ws, sizeof(WAVESAMPLE));
		if (FAILED(hr))
			return hr;

		// Endianness conversion
		//ws.dwSize        = __loadwordbytereverse( 0, &ws.dwSize );
		//ws.UnityNote     = __loadshortbytereverse( 0, &ws.UnityNote );
		//ws.FineTune      = __loadshortbytereverse( 0, &ws.FineTune );
		//ws.Gain          = __loadwordbytereverse( 0, &ws.Gain );
		//ws.dwOptions     = __loadwordbytereverse( 0, &ws.dwOptions );
		//ws.dwSampleLoops = __loadwordbytereverse( 0, &ws.dwSampleLoops );
		SwapEndian(ws.dwSize);
		SwapEndian(ws.UnityNote);
		SwapEndian(ws.FineTune);
		SwapEndian(ws.Gain);
		SwapEndian(ws.dwOptions);
		SwapEndian(ws.dwSampleLoops);

		// Check if the chunk contains any loop regions
		if (ws.dwSampleLoops > 0)
		{
			// Read the loop region
			WAVESAMPLE_LOOP wsl;
			hr = m_WaveSampleChunk.ReadData(ws.dwSize, &wsl, sizeof(WAVESAMPLE_LOOP));
			if (FAILED(hr))
				return hr;

			//Endianness conversion
			SwapEndian((LONG*)&wsl, sizeof(WAVESAMPLE_LOOP) / sizeof(LONG));

			// Fill output vars with the loop region
			*pdwStart = wsl.dwLoopStart;
			*pdwLength = wsl.dwLoopLength;
			bGotLoopRegion = TRUE;
		}
	}

	// Couldn't find either chunk, or they didn't contain loop points
	if (!bGotLoopRegion)
		return E_FAIL;

	return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: GetLoopRegionBytes()
// Desc: Gets the loop region, in terms of bytes
//--------------------------------------------------------------------------------------
HRESULT WaveFileMemory::GetLoopRegionBytes(DWORD* pdwStart, DWORD* pdwLength) const
{
	assert(pdwStart != NULL);
	assert(pdwLength != NULL);

	// If no loop region is explicitly specified, loop the entire file
	*pdwStart = 0;
	GetDuration(pdwLength);

	// We'll need the wave format to convert from samples to bytes
	WAVEFORMATEXTENSIBLE wfx;
	if (FAILED(GetFormat(&wfx)))
		return E_FAIL;

	// See if the file contains an embedded loop region
	DWORD dwLoopStartSamples;
	DWORD dwLoopLengthSamples;
	if (FAILED(GetLoopRegion(&dwLoopStartSamples, &dwLoopLengthSamples)))
		return S_FALSE;

	// For PCM, multiply by bytes per sample
	DWORD cbBytesPerSample = wfx.Format.nChannels * wfx.Format.wBitsPerSample / 8;
	*pdwStart = dwLoopStartSamples * cbBytesPerSample;
	*pdwLength = dwLoopLengthSamples * cbBytesPerSample;

	return S_OK;
}

} // namespace ATG

