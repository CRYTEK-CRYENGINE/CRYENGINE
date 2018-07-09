// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CryBufferedFileReader.h
//  Version:     v1.00
//  Created:     12/6/2010 by Jeremy Gross.
//  Compilers:   Visual Studio.NET
//  Description: File wrapper.
// -------------------------------------------------------------------------
//  History:
//
//	This file is created as a patch for the ai system which is reading huge files a few bytes at a time.
//	It is not a perfect implementation, and I am implementing just what is needed to patch this class in
//	between the system and the CryFile.  Atm I have no aim to completely mirror all functionaly of cryfile,
//	or 'properly' make this wrap an existing instance of CryFile java style.  Feel free to expand the functionality
//	to do that in the future.  But at least this leaves a recorded explanation of why this file probably sucks
//	(in addition the many hacks made by god knows who in between) for whoever is reading it 20 years from now and cursing
//	up a storm about it's idiocy which is adding to their stress as they crunch their balls (or ovaries) off. ;-)
////////////////////////////////////////////////////////////////////////////

#ifndef __CryBufferedFileReader_h__
#define __CryBufferedFileReader_h__
#pragma once

#include "CryFile.h"

class CCryBufferedFileReader
{
public:
	CCryBufferedFileReader()
		: m_maxbuffersize(64 * 1024)
		, m_file_offset(0)
		, m_file_size(0)
		, m_buffer_offset(0)
	{
	}
	inline bool Open(const char* filename, const char* mode, int nOpenFlagsEx = 0)
	{
		bool success;
		success = m_file.Open(filename, mode, nOpenFlagsEx);
		if (success)
		{
			m_file_size = m_file.GetLength();
			m_file_offset = 0;
			m_buffer_offset = 0;

			StreamData();
		}
		return success;
	}
	inline void Close()
	{
		m_file.Close();
	}

	size_t ReadRaw(void* lpBuf, size_t nSize)
	{
		size_t total_size_to_read = nSize;
		size_t dest_offset = 0;

		//we might be trying to read past what is buffered.  Copy what is buffered, then read more and copy that too if needed
		do
		{
			size_t size_to_read = min(m_buffer.size() - m_buffer_offset, total_size_to_read);
			if (size_to_read)
			{
				memcpy(&(reinterpret_cast<uint8*>(lpBuf)[dest_offset]), &m_buffer[m_buffer_offset], size_to_read);
				m_buffer_offset += size_to_read;
				dest_offset += size_to_read;
				total_size_to_read -= size_to_read;
			}

			if (total_size_to_read != 0)
				StreamData();
		}
		while (total_size_to_read);

		return nSize - total_size_to_read;
	}

	template<class T>
	inline size_t ReadType(T* pDest, size_t nCount = 1)
	{
		size_t nRead = ReadRaw(pDest, sizeof(T) * nCount);
		SwapEndian(pDest, nCount);
		return nRead / sizeof(T);
	}
private:

	void StreamData()
	{
		size_t total_size_to_read = min(m_maxbuffersize, m_file_size - m_file_offset);
		m_buffer.resize(total_size_to_read);
		m_file_offset += total_size_to_read;

		size_t read = 0;
		do
		{
			read += m_file.ReadRaw(&m_buffer[read], total_size_to_read - read);
		}
		while (read < total_size_to_read);
		assert(read == total_size_to_read);

		m_buffer_offset = 0;
	}

	CCryFile           m_file;
	std::vector<uint8> m_buffer;
	const size_t       m_maxbuffersize;
	size_t             m_file_size;
	size_t             m_buffer_offset;
	size_t             m_file_offset;
};

#endif
