// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __MEMORYFRAGMENTATIONPROFILER_H__
#define __MEMORYFRAGMENTATIONPROFILER_H__

#pragma once

// useful class to investigate memory fragmentation
// every time you call this from the console:
//
// #System.DumpMemoryCoverage()
//
// it adds a line to "MemoryCoverage.bmp" (generated the first time, there is a max line count)
// blue stripes mark some special positions (DLL positions)

// Dependencies: only CryLog()

#include <vector>     // STL vector<>

#if CRY_PLATFORM_WINDOWS

class CMemoryFragmentationProfiler
{
public:

	// constructor - clean file
	CMemoryFragmentationProfiler() : m_dwLine(0xffffffff)    // 0xffffffff means not initialized yet
	{
	}

	// call this if you want to add one line (on first call the file is generated)
	void DumpMemoryCoverage()
	{
		if (m_dwLine == 0xffffffff)
			Init();

		const size_t nMinMemoryPerUnit = 4 * 1024;              // down to a few KB
		const size_t nUnitsPerLine = 1024 * 8;                  // amount of bits, should only occupy a few KB memory

		const size_t nMaxMemoryPerUnit = 0x100000000 / nUnitsPerLine;     // 4GB in total

		static std::vector<bool> vCoverage;

		vCoverage.clear();
		vCoverage.resize(nUnitsPerLine, 0);                  // should occupy nUnitsPerLine/8 bytes (vector<bool> is specialized)

		size_t nAvailableMem = 0, nUsedMem = 0;
		/*

		    _HEAPINFO hinfo;
		    int heapstatus;
		    hinfo._pentry = NULL;
		    while( ( heapstatus = _heapwalk( &hinfo ) ) == _HEAPOK )
		    {
		   //			char str[256];
		   //			cry_sprintf(str,"%6s block at %Fp of size %d\n", ( hinfo._useflag == _USEDENTRY ? "USED" : "FREE" ), hinfo._pentry, hinfo._size );

		   //			OutputDebugString(str);

		      // update coverage (conservative)
		      if(hinfo._useflag == _USEDENTRY)
		      {
		        size_t nStartUnit = ((size_t)hinfo._pentry+nMaxMemoryPerUnit-1)/(nMaxMemoryPerUnit);
		        size_t nEndUnit = ((size_t)hinfo._pentry+hinfo._size-(nMaxMemoryPerUnit-1))/(nMaxMemoryPerUnit);

		        if(nStartUnit>nUnitsPerLine)
		          nStartUnit=nUnitsPerLine;

		        if(nEndUnit>nUnitsPerLine)
		          nEndUnit=nUnitsPerLine;

		        for(size_t i=nStartUnit;i<nEndUnit;++i)
		          vCoverage[i]=1;

		        nUsedMem += hinfo._size;
		      }
		      else if(hinfo._useflag == _FREEENTRY)
		          nAvailableMem += hinfo._size;
		    }
		 */

		const size_t nMallocOverhead = 24;                    // depends on used runtime (debug:32, release:24)

		size_t nCurrentUnitSize = 256 * 1024 * 1024;            // start with 256 MB blocks

		void** pMemoryBlocks = 0;                           // linked list of memory blocks (to free them)

		size_t nUnits = 0;
		uint32 dwAllocCnt = 0, dwFreeCnt = 0;

		while (nCurrentUnitSize >= nMinMemoryPerUnit)
		{
			size_t nLocalUnits = 0;

			for (;; )
			{
				void** pMem = (void**)::malloc(nCurrentUnitSize - nMallocOverhead);

				if (!pMem)
					break;

				++dwAllocCnt;

				// update coverage (conservative)
				{
					size_t nStartUnit = ((size_t)pMem + nMaxMemoryPerUnit - 1) / (nMaxMemoryPerUnit);
					size_t nEndUnit = ((size_t)pMem + nCurrentUnitSize) / (nMaxMemoryPerUnit);

					if (nStartUnit > nUnitsPerLine)
						nStartUnit = nUnitsPerLine;

					if (nEndUnit > nUnitsPerLine)
						nEndUnit = nUnitsPerLine;

					for (size_t i = nStartUnit; i < nEndUnit; ++i)
						vCoverage[i] = 1;
				}

				++nLocalUnits;

				// insert in linked list
				*pMem = pMemoryBlocks;
				pMemoryBlocks = pMem;
			}

			nUnits += nLocalUnits;
			nAvailableMem += nLocalUnits * nCurrentUnitSize;

			//			cry_sprintf(str,"CMemoryFragmentationProfiler nCurrentUnitSize= %7d KB unit size => %6d more units allocated, resulting in %5d MB\n",
			//				(nCurrentUnitSize+1023)/1024,nLocalUnits,(nUnits*nCurrentUnitSize+1024*1024-1)/(1024*1024));

			//			OutputDebugString(str);

			nCurrentUnitSize /= 2;
			nUnits *= 2;
		}

		// free all memory blocks allocated
		while (pMemoryBlocks)
		{
			void* pNext = *pMemoryBlocks;

			::free(pMemoryBlocks);
			++dwFreeCnt;

			pMemoryBlocks = (void**)pNext;
		}

		//		_heapmin();

		CryLog("CMemoryFragmentationProfiler  Y=%d, available memory=%d MB, used memory=%d MB",
		       m_dwLine, (nAvailableMem + 1024 * 1024 - 1) / (1024 * 1024), (nUsedMem + 1024 * 1024 - 1) / (1024 * 1024));

		LogCoverage(vCoverage);

		DumpToRAWCoverage(vCoverage);
	}

private: // ------------------------------------------------------------------

	void Init()
	{
		FILE* out = fopen("MemoryCoverage.bmp", "wb");

		if (out)
		{
			BITMAPFILEHEADER pHeader;
			BITMAPINFOHEADER pInfoHeader;

			memset(&pHeader, 0, sizeof(BITMAPFILEHEADER));
			memset(&pInfoHeader, 0, sizeof(BITMAPINFOHEADER));

			pHeader.bfType = 0x4D42;
			pHeader.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + m_nPixelsPerLine * m_nLineCount * 3;
			pHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

			pInfoHeader.biSize = sizeof(BITMAPINFOHEADER);
			pInfoHeader.biWidth = m_nPixelsPerLine;
			pInfoHeader.biHeight = m_nLineCount;
			pInfoHeader.biPlanes = 1;
			pInfoHeader.biBitCount = 24;
			pInfoHeader.biCompression = 0;
			pInfoHeader.biSizeImage = m_nPixelsPerLine * m_nLineCount;

			fwrite(&pHeader, 1, sizeof(BITMAPFILEHEADER), out);
			fwrite(&pInfoHeader, 1, sizeof(BITMAPINFOHEADER), out);

			for (int y = 0; y < m_nLineCount; y++)                   // amount of lines
				for (int x = 0; x < m_nPixelsPerLine; x++)
				{
					size_t nAddress = x * (0x100000000 / m_nPixelsPerLine);

					if (nAddress == 0x30000000
					    || nAddress == 0x30500000
					    || nAddress == 0x31000000
					    || nAddress == 0x31500000
					    || nAddress == 0x32000000
					    || nAddress == 0x32500000
					    || nAddress == 0x33500000
					    || nAddress == 0x34000000
					    || nAddress == 0x35000000
					    || nAddress == 0x35500000
					    || nAddress == 0x36000000
					    || nAddress == 0x36500000
					    || nAddress == 0x38000000
					    || nAddress == 0x39000000)
						putc((unsigned char)100, out);   // blue   DLL start
					else
						putc((unsigned char)0, out);     // black

					putc((unsigned char)0, out);
					putc((unsigned char)0, out);
				}

			fclose(out);
			m_dwLine = 0;
		}
	}

	void LogCoverage(std::vector<bool>& vCov)
	{
		const size_t nCharPerLine = 128;            // readable amount

		char szResult[nCharPerLine + 1], * pCursor = szResult;

		szResult[nCharPerLine] = 0;   // zero termination

		size_t nSize = vCov.size();
		size_t nUnitsPerChar = nSize / nCharPerLine;

		for (size_t i = 0; i < nSize; )
		{
			unsigned int nLocalCov = 0;

			for (size_t e = 0; e < nUnitsPerChar; ++e, ++i)
				if (vCov[i])
					++nLocalCov;

			if (nLocalCov == 0)
				*pCursor++ = '#';             // occupied
			else if (nLocalCov == nUnitsPerChar)
				*pCursor++ = '.';             // free
			else
				*pCursor++ = '+';             // partly
		}

		CryLog("         Coverage=%s", szResult);
	}

	void DumpToRAWCoverage(std::vector<bool>& vCov)
	{
		FILE* out = fopen("MemoryCoverage.bmp", "rb+");

		if (!out)
			return;

		if (fseek(out, sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + 3 * (m_nLineCount - 1 - m_dwLine) * m_nPixelsPerLine, SEEK_SET) != 0)
		{
			fclose(out);
			return;
		}

		size_t nSize = vCov.size();
		size_t nUnitsPerChar = nSize / m_nPixelsPerLine;

		for (size_t i = 0; i < nSize; )
		{
			unsigned int nLocalCov = 0;

			for (size_t e = 0; e < nUnitsPerChar; ++e, ++i)
				if (vCov[i])
					++nLocalCov;

			size_t Val = 256 - (256 * nLocalCov) / nUnitsPerChar;

			if (Val > 0)
				Val = 127 + Val / 2;

			putc((unsigned char)Val, out);
			putc((unsigned char)Val, out);
			putc((unsigned char)Val, out);                                                              // grey
		}

		fclose(out);

		++m_dwLine;
	}

	unsigned int        m_dwLine;                               // [0..m_nLineCount-1], m_nLineCount means bitmap is full, 0xffffffff means not initialized yet
	static const size_t m_nPixelsPerLine = 1024;                // bitmap width
	static const size_t m_nLineCount = 128;                     //
};

#else // CRY_PLATFORM_WINDOWS

class CMemoryFragmentationProfiler
{
public:
	void DumpMemoryCoverage() {}
};

#endif // CRY_PLATFORM_WINDOWS

#endif // __MEMORYFRAGMENTATIONPROFILER_H__
