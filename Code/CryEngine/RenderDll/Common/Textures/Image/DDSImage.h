// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef DDSIMAGE_H
#define DDSIMAGE_H

#include <Cry3DEngine/ImageExtensionHelper.h>
#include "CImage.h"
#include <CryMemory/IMemory.h>

/**
 * An ImageFile subclass for reading DDS files.
 */

namespace DDSSplitted
{
enum
{
	etexNumLastMips     = 3,
	etexLowerMipMaxSize = (1 << (etexNumLastMips + 2)), // + 2 means we drop all the mips that are less than 4x4(two mips: 2x2 and 1x1)
};

struct DDSDesc
{
	DDSDesc()
	{
		memset(this, 0, sizeof(*this));
	}

	const char*   pName;
	uint32        nBaseOffset;

	uint32        nWidth;
	uint32        nHeight;
	uint32        nDepth;
	uint32        nSides;
	uint32        nMips;
	uint32        nMipsPersistent;
	ETEX_Format   eFormat;
	ETEX_TileMode eTileMode;
	uint32        nFlags;
};

typedef CryStackStringT<char, 192> TPath;

struct ChunkInfo
{
	TPath  fileName;
	uint32 nOffsetInFile;
	uint32 nSizeInFile;
	uint32 nMipLevel;
	uint32 nSideDelta;
};

struct RequestInfo
{
	string fileName;
	void*  pOut;
	uint32 nOffs;
	uint32 nRead;
};

struct FileWrapper
{
	explicit FileWrapper(CCryFile& file) : m_bInMem(false), m_pFile(&file) {}
	FileWrapper(const void* p, size_t nLen) : m_bInMem(true), m_pRaw((const char*)p), m_nRawLen(nLen), m_nSeek(0) {}

	bool IsValid() const
	{
		return m_bInMem
		       ? m_pRaw != NULL
		       : m_pFile != NULL && m_pFile->GetHandle() != NULL;
	}

	size_t GetLength() const
	{
		return m_bInMem
		       ? m_nRawLen
		       : m_pFile->GetLength();
	}

	size_t ReadRaw(void* p, size_t amount)
	{
		if (m_bInMem)
		{
			amount = min(m_nRawLen - m_nSeek, amount);
			memcpy(p, m_pRaw + m_nSeek, amount);
			m_nSeek += amount;

			return amount;
		}
		else
		{
			return m_pFile->ReadRaw(p, amount);
		}
	}

	void Seek(size_t offs)
	{
		if (m_bInMem)
		{
			m_nSeek = min(offs, m_nRawLen);
		}
		else
		{
			m_pFile->Seek(offs, SEEK_SET);
		}
	}

	size_t Tell()
	{
		if (m_bInMem)
		{
			return m_nSeek;
		}
		else
		{
			return m_pFile->GetPosition();
		}
	}

	bool m_bInMem;
	union
	{
		CCryFile* m_pFile;
		struct
		{
			const char* m_pRaw;
			size_t      m_nRawLen;
			size_t      m_nSeek;
		};
	};
};

typedef std::vector<ChunkInfo> Chunks;

TPath& MakeName(TPath& sOut, const char* sOriginalName, const uint32 nChunk, const uint32 nFlags);

size_t GetFilesToRead(ChunkInfo* pFiles, size_t nFilesCapacity, const DDSDesc& desc, uint32 nStartMip, uint32 nEndMip);

bool   SeekToAttachedImage(FileWrapper& file);

size_t LoadMipRequests(RequestInfo* pReqs, size_t nReqsCap, const DDSDesc& desc, byte* pBuffer, uint32 nStartMip, uint32 nEndMip);
size_t LoadMipsFromRequests(const RequestInfo* pReqs, size_t nReqs);
size_t LoadMips(byte* pBuffer, const DDSDesc& desc, uint32 nStartMip, uint32 nEndMip);

int    GetNumLastMips(const int nWidth, const int nHeight, const int nNumMips, const int nSides, ETEX_Format eTF, const uint32 nFlags);
};

class CImageDDSFile : public CImageFile
{
	friend class CImageFile;  // For constructor
public:
	CImageDDSFile(const string& filename);
	CImageDDSFile(const string& filename, uint32 nFlags);

	bool                         Stream(uint32 nFlags, IImageFileStreamCallback* pStreamCallback);

	virtual DDSSplitted::DDSDesc mfGet_DDSDesc() const;

private: // ------------------------------------------------------------------------------
	/// Read the DDS file from the file.
	bool LoadFromFile(DDSSplitted::FileWrapper& file, uint32 nFlags, DDSSplitted::RequestInfo* pConts, size_t& nConts, size_t nContsCap);

	/// Read the DDS file from the file.
	bool Load(const string& filename, uint32 nFlags);

	void StreamAsyncOnComplete(IReadStream* pStream, unsigned nError);

	bool SetHeaderFromMemory(byte* pFileStart, byte* pFileAfterHeader, uint32 nFlags);
	int  AdjustHeader();

	bool PostLoad();

	void AdjustFirstFileName(uint32& nFlags, const char* pFileName, DDSSplitted::TPath& adjustedFileName);

	CImageExtensionHelper::DDS_HEADER       m_DDSHeader;
	CImageExtensionHelper::DDS_HEADER_DXT10 m_DDSHeaderExtension;

	_smart_ptr<IMemoryBlock>                m_pFileMemory;
};

void WriteDDS(byte* dat, int wdt, int hgt, int dpth, int Size, char* name, ETEX_Format eF, int NumMips, ETEX_Type eTT);

#endif
