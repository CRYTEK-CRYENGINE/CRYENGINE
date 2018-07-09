// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2008.
-------------------------------------------------------------------------
$Id: TextureSplitter.cpp,v 1.0 2008/01/17 15:14:13 AntonKaplanyan Exp wwwrun $
$DateTime$
Description:  Routine for creation of streaming pak
							file from file with list of resources
-------------------------------------------------------------------------
History:
- 17:1:2008   10:31 : Created by Anton Kaplanyan

*************************************************************************/

#include "StdAfx.h"
#include <IConfig.h>
#include "TextureSplitter.h"
#include "../../ResourceCompiler/IResCompiler.h"
#include <CryCore/BitFiddling.h>
#include <Cry3DEngine/ImageExtensionHelper.h>
#include <CrySystem/IConsole.h>

#include <CryCore/Platform/CryWindows.h>
#include <DbgHelp.h>

#include "FileUtil.h"
#include "IRCLog.h"
#include "IAssetManager.h"
#include "IResCompiler.h"
#include "StringHelpers.h"
#include "UpToDateFileHelpers.h"
#include "Util.h"

#include "Formats/DDS.h"
#include "Formats/TIFF.h"
#include "ImageObject.h"
#include "ImageProperties.h"

//#define DEBUG_DISABLE_TEXTURE_SPLITTING
//#define DEBUG_FILE_SIZE_LIMIT 140000

#define CRY_MAKEFOURCC(ch0, ch1, ch2, ch3)                              \
				((uint32)(uint8)(ch0) | ((uint32)(uint8)(ch1) << 8) |       \
				((uint32)(uint8)(ch2) << 16) | ((uint32)(uint8)(ch3) << 24 ))

// global convention for naming files and directories inside pak. 
namespace TextureHelper
{

	static bool IsBlockCompressed(const ETEX_Format eTF) { return CImageExtensionHelper::IsBlockCompressed(eTF); }
	static int BytesPerBlock(const ETEX_Format eTF) { return CImageExtensionHelper::BytesPerBlock(eTF); }
	static int BytesPerPixel(const ETEX_Format eTF) { return CImageExtensionHelper::BitsPerPixel(eTF) / 8; }

	int TextureDataSize(int nWidth, int nHeight, int nDepth, int nMips, const ETEX_Format eTF)
	{
		if (eTF == eTF_Unknown)
			return 0;

		if (nMips <= 0)
			nMips = 1;
		int nSize = 0;
		int nM = 0;
		while (nWidth || nHeight || nDepth)
		{
			if (!nWidth)
				nWidth = 1;
			if (!nHeight)
				nHeight = 1;
			if (!nDepth)
				nDepth = 1;
			nM++;

			const bool bIsBlockCompressed = IsBlockCompressed(eTF);
			size_t nSingleMipBlocks = nWidth * nHeight * nDepth;
			if (bIsBlockCompressed)
			{
				nSingleMipBlocks = ((nWidth + 3) / 4) * ((nHeight + 3) / 4) * nDepth;
			}

			if (bIsBlockCompressed)
				nSize += nSingleMipBlocks * BytesPerBlock(eTF);
			else
				nSize += nSingleMipBlocks * BytesPerPixel(eTF);

			nWidth >>= 1;
			nHeight >>= 1;
			nDepth >>= 1;
			if (nMips == nM)
				break;
		}

		return nSize;
	}
};

class CTextureDecompressor : public ICompiler
{
public:
	virtual void Release() override
	{
		
	}

	virtual void BeginProcessing(const IConfig* config) override
	{

	}

	virtual void EndProcessing() override
	{
	
	}

	virtual IConvertContext* GetConvertContext() override
	{
		return &m_CC;
	}

	string GetOutputFileNameOnly() const
	{
		const string sourceFileFinal = m_CC.config->GetAsString("overwritefilename", m_CC.sourceFileNameOnly.c_str(), m_CC.sourceFileNameOnly.c_str());
		return PathUtil::ReplaceExtension(sourceFileFinal, "tif");
	}

	string GetOutputPath() const
	{
		return PathUtil::Make(m_CC.GetOutputFolder(), GetOutputFileNameOnly());
	}

	virtual bool Process() override
	{
		const string sInputFile = m_CC.GetSourcePath();
		const string sDestFileName = PathUtil::GetFile(MakeFileName(PathUtil::GetFile(GetOutputPath()), 0, 0));
		const string sFullDestFileName = PathUtil::Make(m_CC.config->GetAsString("targetroot", m_CC.GetOutputFolder(), m_CC.GetOutputFolder()), sDestFileName);
		string dummy;
		std::unique_ptr<ImageObject> image(ImageDDS::LoadByUsingDDSLoader(sInputFile, nullptr, dummy));
		CImageProperties props = CImageProperties();
		props.SetPropsCC(&m_CC, true);
		ImageTIFF::SaveByUsingTIFFSaver(sFullDestFileName, &props, image.get());
		return true;
	}
private:
	ConvertContext m_CC;

};

static void FindUnusedResources(std::vector<string>* filenames, const std::vector<string>& usedFilenames, const string& baseOutputPath, bool bCheckOnDisk)
{
	assert(filenames);

	const size_t maxMipLevel = 9;
	const char* const suffixes[] = { "", "a" };
	const size_t numSuffixes = sizeof(suffixes) / sizeof(suffixes[0]);

	string filename;
	for (size_t i = 0; i < numSuffixes; ++i)
	{
		const char* const suffix = suffixes[i];

		for (size_t mipLevel = 0; mipLevel <= maxMipLevel; ++mipLevel)
		{
			filename.Format("%s.%d%s", baseOutputPath.c_str(), mipLevel, suffix);

			bool used = false;
			for (size_t j = 0; j < usedFilenames.size(); ++j)
			{
				if (StringHelpers::EqualsIgnoreCase(filename, usedFilenames[j]))
				{
					used = true;
					break;
				}
			}

			if (!used && (!bCheckOnDisk || FileUtil::FileExists(filename.c_str())))
			{
				filenames->push_back(filename);
			}
		}
	}
}

static string MakeFileName( const string& filePath, const uint32 nChunk, const uint32 nFlags )
{
	string suffix = "";

	if(nFlags & CTextureSplitter::eStreamableResourceFlags_DDSIsAttachedAlpha)
		suffix = "a";

	string sFullDestFileName;
	if (nChunk >= 1)
		sFullDestFileName.Format("%s.%d%s", filePath, nChunk, suffix.c_str());
	else if (!suffix.empty())
		sFullDestFileName.Format("%s.%s", filePath, suffix.c_str());
	else
		sFullDestFileName = filePath;

	return sFullDestFileName;
}

bool CTextureSplitter::SaveFile( const string& sFileName, const void* pBuffer, const size_t nSize, const FILETIME& fileTime )
{
	m_CC.pRC->AddInputOutputFilePair(GetSourceFilename(), sFileName);

	if (!FileUtil::EnsureDirectoryExists(PathUtil::GetPathWithoutFilename(sFileName).c_str()))
	{
		RCLogError("Failed creating directory for %s", sFileName.c_str());
		return false;
	}

	// create file
	FILE* file = fopen(sFileName, "wb");
	if(!file)
	{
		RCLogError("Error '%s': Failed to create file '%s'\n", strerror(errno), sFileName.c_str());
		return false;
	}

	// save content
	bool bResult = true;
	if(fwrite(pBuffer, nSize, 1, file) != 1)
	{
		bResult = false;
		RCLogError("Error '%s': Failed to write content of file '%s'\n", strerror(errno), sFileName.c_str());
	}

	fclose(file);
	
	if (bResult)
	{
		FileUtil::SetFileTimes(sFileName.c_str(), fileTime);
	}

	return bResult;
}

CTextureSplitter::STexture::SSurface* CTextureSplitter::STexture::TryGetSurface(int nSide, int nMip)
{
	if (m_surfaces.size() <= nSide) return NULL;
	if (m_surfaces[nSide].size() <= nMip) return NULL;
	return &m_surfaces[nSide][nMip];
}

void CTextureSplitter::STexture::AddSurface(const SSurface& surf)
{
	if (m_surfaces.size() <= surf.m_dwSide) m_surfaces.resize(surf.m_dwSide + 1);
	if (m_surfaces[surf.m_dwSide].size() <= surf.m_dwStartMip) m_surfaces[surf.m_dwSide].resize(surf.m_dwStartMip + 1);
	m_surfaces[surf.m_dwSide][surf.m_dwStartMip] = surf;
}

CTextureSplitter::CTextureSplitter( ) : 
		m_refCount(1),
		m_currentEndian(eLittleEndian),
		m_targetType(eTT_None),
		m_bTile(false),
		m_attachedAlphaOffset(0),
		m_compilerType(TextureSplitter)
{
}

CTextureSplitter::~CTextureSplitter( )
{
}

bool CTextureSplitter::BuildDDSChunks(const char* fileName, STexture& resource)
{
	CImageExtensionHelper::DDS_HEADER& header = resource.m_ddsHeader;
	CImageExtensionHelper::DDS_HEADER_DXT10& exthead = resource.m_ddsHeaderExtension;

	if (!header.IsValid())
	{
		RCLogError("Error: Unknown DDS file header: '%s'\n", fileName);
		return false;
	}

	// get number of real MIPs
	// get number of sides
	uint32 nNumMips = header.GetMipCount();
	uint32 nSides = 1U;
	if (header.IsDX10Ext())
	{
		nSides = (header.dwReserved1 & CImageExtensionHelper::EIF_Cubemap) ? exthead.arraySize : exthead.arraySize;
	}
	else
	{
		nSides = (header.dwReserved1 & CImageExtensionHelper::EIF_Cubemap) || ((header.dwSurfaceFlags & DDS_SURFACE_FLAGS_CUBEMAP) != 0 && (header.dwCubemapFlags & DDS_CUBEMAP_ALLFACES) != 0) ? 6 : 1;
	}

	// Compute output side size
	for (DWORD dwMip = 0; dwMip < nNumMips;)
	{
		STexture::SSurface& surface = *resource.TryGetSurface(0, dwMip);
		dwMip += surface.m_dwMips;
	}

	// parse chunks
	int nEstimatedMips = (int)nNumMips;

	int nLastMips = Util::getMax((int)ehiNumLastMips, (int)header.bNumPersistentMips);

	// header file
	for (int currentPhase = 0; nEstimatedMips > 0; ++currentPhase)
	{
		STexture::SChunkDesc chunkDesc;
		chunkDesc.m_nChunkNumber = (uint8)currentPhase;

		// calculate mips to save
		uint32 nStartMip;
		uint32 nEndMip;
		if (currentPhase == 0)
		{
			nStartMip = Util::getMax(0, (int)nNumMips - nLastMips);
			nEndMip = nNumMips;
		}
		else
		{
			nStartMip = Util::getMax(0, ((int)nNumMips - nLastMips) - currentPhase);
			nEndMip = Util::getMin(nNumMips, nStartMip + 1);
		}

		for (int iSide = 0; iSide < nSides; ++iSide)
		{
			for (int iMip = nStartMip; iMip < nEndMip;)
			{
				// add face chunk
				chunkDesc.m_blocks.push_back(STexture::SBlock());
				STexture::SBlock& face = chunkDesc.m_blocks.back();
				STexture::SSurface& surface = *resource.TryGetSurface(iSide, iMip);

				// initialize offset for side
				// add header to the first chunk of the first side
				face.m_pSurfaceData = surface.m_pData;
				face.m_nChunkSize += surface.m_dwSize;

				iMip += surface.m_dwMips;
			}

			//RCLog("Output Mip: %d-%d Size: %d Offset: %d\n", nStartMip, nEndMip, face.m_nChunkSize, face.m_nOriginalOffset);
		}

		const uint32 nNumMipsToStore = nEndMip - nStartMip;
		nEstimatedMips -= nNumMipsToStore;
		assert(nNumMips >= 0);

		// add chunk
		resource.m_chunks.push_back(chunkDesc);
	}

	// sort all chunks in offset ascending order
	resource.m_chunks.sort();

#ifdef DEBUG_DISABLE_TEXTURE_SPLITTING
	resource.m_chunks.clear();
#endif

	return true;
}


void CTextureSplitter::PostLoadProcessTexture( std::vector<uint8>& fileContent )
{
	CImageExtensionHelper::DDS_FILE_DESC* pFileDesc = (CImageExtensionHelper::DDS_FILE_DESC*)(&fileContent[0]);
	CImageExtensionHelper::DDS_HEADER_DXT10* pFileDescExt = (CImageExtensionHelper::DDS_HEADER_DXT10*)(&fileContent[0] + sizeof(CImageExtensionHelper::DDS_FILE_DESC));

	const ETEX_Format format = DDSFormats::GetFormatByDesc(pFileDesc->header.ddspf, pFileDescExt->dxgiFormat);
	const size_t nHeaderSize = pFileDesc->GetFullHeaderSize();

	const int nOldMips = pFileDesc->header.GetMipCount();
	int nFaces = 1;
	if ((pFileDesc->header.dwSurfaceFlags & DDS_SURFACE_FLAGS_CUBEMAP) != 0 && (pFileDesc->header.dwCubemapFlags & DDS_CUBEMAP_ALLFACES) != 0)
		nFaces = 6;

	// if we have already texture with proper mips, just return
	if( (pFileDesc->header.dwWidth >> nOldMips) >= 4 && (pFileDesc->header.dwHeight >> nOldMips) >= 4
		|| pFileDesc->header.GetMipCount() == 1)
		return;

	// else calculate # mips to drop and drop it
	int nNumMipsToDrop = 0;
	while(pFileDesc->header.GetMipCount() - nNumMipsToDrop > 1)
	{
		int nNewMips = pFileDesc->header.GetMipCount() - nNumMipsToDrop - 1;
		if((pFileDesc->header.dwWidth >> nNewMips) >= 4 && 
			(pFileDesc->header.dwHeight >> nNewMips) >= 4)
			break;
		nNumMipsToDrop++;
	}

	if(!nNumMipsToDrop)
		return;

	const int nTexDataSize = TextureHelper::TextureDataSize(
		pFileDesc->header.dwWidth, pFileDesc->header.dwHeight, 
		Util::getMax(1ul, pFileDesc->header.dwDepth), 
		pFileDesc->header.GetMipCount(), format);

	const int nNewTexDataSize = TextureHelper::TextureDataSize(
		pFileDesc->header.dwWidth, pFileDesc->header.dwHeight, 
		Util::getMax(1ul, pFileDesc->header.dwDepth), 
		pFileDesc->header.GetMipCount() - nNumMipsToDrop, format);

	const int nDiffOffset = nTexDataSize - nNewTexDataSize;
	uint8* pData = &fileContent[nHeaderSize];
	int nEndOffset = nHeaderSize + nTexDataSize;
	int nNewEndOffset = nHeaderSize + nNewTexDataSize;
	for(int iSide = 1;iSide < nFaces;++iSide)
	{
		uint8* pOldSideData = pData + nTexDataSize * iSide;
		uint8* pNewSideData = pData + nNewTexDataSize * iSide;
		assert(nHeaderSize + nTexDataSize + nDiffOffset + nNewTexDataSize <= fileContent.size());
		memmove(pNewSideData, pOldSideData, nNewTexDataSize);
		nEndOffset += nTexDataSize;
		nNewEndOffset += nNewTexDataSize;
	}

	// copy the rest of the file
	int nRest = fileContent.size() - nEndOffset;
	if(nRest)
	{
		assert(nRest > 0);
		memmove(&fileContent[nNewEndOffset], &fileContent[nEndOffset], nRest);
	}

	fileContent.resize(fileContent.size() - nDiffOffset * nFaces);

	// write to the header
	pFileDesc->header.dwMipMapCount -= nNumMipsToDrop;
}

bool CTextureSplitter::LoadTexture( const char* fileName, std::vector<uint8>& fileContent )
{
	// try to open file
	FILE* file = fopen(fileName, "rb");
	if(file == NULL)
	{
		RCLogError("Error: Cannot open texture file: '%s'\n", fileName);
		return false;
	}

	// get file size
	fseek( file,0,SEEK_END );
	const uint32 nOriginalSize = ftell(file);
	fseek( file,0,SEEK_SET );

	// debug helper
#ifdef DEBUG_FILE_SIZE_LIMIT
	if(nOriginalSize > DEBUG_FILE_SIZE_LIMIT)
	{
		fclose(file);
		return false;
	}
#endif

	if(nOriginalSize < sizeof(CImageExtensionHelper::DDS_FILE_DESC))
	{
		RCLogError("Error: Cannot load texture, file too small: '%s'\n", fileName);
		fclose(file);
		return false;
	}

	// allocate memory for file content
	fileContent.resize(nOriginalSize);

	if(fread(&fileContent[0], nOriginalSize, 1, file) != 1)
	{
		fclose(file);
		return false;
	}

	CImageExtensionHelper::DDS_FILE_DESC* pFileDesc = (CImageExtensionHelper::DDS_FILE_DESC*)(&fileContent[0]);
	CImageExtensionHelper::DDS_HEADER_DXT10* pFileDescExt = (CImageExtensionHelper::DDS_HEADER_DXT10*)(&fileContent[0] + sizeof(CImageExtensionHelper::DDS_FILE_DESC));

	// check for valid CryEngine DDS file
	if (pFileDesc->dwMagic != CRY_MAKEFOURCC('D','D','S',' ') || 
		pFileDesc->header.dwSize != sizeof(pFileDesc->header) ||
		pFileDesc->header.ddspf.dwSize != sizeof(pFileDesc->header.ddspf) || 
		pFileDesc->header.dwDepth > 16384 || 
		pFileDesc->header.dwHeight > 16384 ||
		pFileDesc->header.dwWidth > 16384)
	{
		RCLogError("Error: Cannot load texture header: '%s'\n", fileName);
		fclose(file);
		return false;
	}

	const ETEX_Format format = DDSFormats::GetFormatByDesc(pFileDesc->header.ddspf, pFileDescExt->dxgiFormat);
	const size_t nHeaderSize = pFileDesc->GetFullHeaderSize();

	if(format == eTF_Unknown)
	{
		RCLogError("Error: Cannot load texture(unknown format): '%s'\n", fileName);
		fclose(file);
		return false;
	}
	int nFaces = 1;
	if ((pFileDesc->header.dwSurfaceFlags & DDS_SURFACE_FLAGS_CUBEMAP) != 0 && (pFileDesc->header.dwCubemapFlags & DDS_CUBEMAP_ALLFACES) != 0)
		nFaces = 6;
	int nTexDataSize = nFaces * TextureHelper::TextureDataSize(
		pFileDesc->header.dwWidth, pFileDesc->header.dwHeight, Util::getMax(1ul, pFileDesc->header.dwDepth), 
		pFileDesc->header.GetMipCount(), format);

	if(nHeaderSize + nTexDataSize > nOriginalSize)
	{
		RCLogError("Error: Cannot load texture, file content too small: '%s'\n", fileName);
		fclose(file);
		return false;
	}

	fclose(file);

	PostLoadProcessTexture(fileContent);

	return true;
}

void CTextureSplitter::ParseDDSTexture( const char* fileName, std::vector<STexture>& resources, std::vector<uint8>& fileContent )
{
	if(!LoadTexture(fileName, fileContent))
	{
		RCLogError("Error: Cannot load texture: '%s'\n", fileName);
		fileContent.clear();
		return;
	}

	// load header
	CImageExtensionHelper::DDS_HEADER* pDDSHeader = (CImageExtensionHelper::DDS_HEADER*)(&fileContent[0] + sizeof(DWORD));
	CImageExtensionHelper::DDS_HEADER_DXT10* pDDSHeaderExt = (CImageExtensionHelper::DDS_HEADER_DXT10*)(&fileContent[0] + sizeof(DWORD) + sizeof(CImageExtensionHelper::DDS_FILE_DESC));

	const size_t fileContentSize = fileContent.size() - sizeof(DWORD);

	// process texture platform-specific conversions
	ProcessPlatformSpecificConversions(resources, (byte*)pDDSHeader, fileContentSize);

	if (resources.empty())
		return;

	if (m_CC.config->GetAsBool("dont_split", false, true))
	{
		STexture& tex = resources[0];
		STexture::SChunkDesc chunk;
		chunk.m_nChunkNumber = 0;

		DWORD dwMips  = pDDSHeader->GetMipCount();
		DWORD dwSides = 1;
		DWORD dwDepth = 1;
		if (pDDSHeader->IsDX10Ext())
		{
			dwSides = (pDDSHeader->dwReserved1 & CImageExtensionHelper::EIF_Cubemap) ? pDDSHeaderExt->arraySize : pDDSHeaderExt->arraySize;
			dwDepth = (pDDSHeader->dwReserved1 & CImageExtensionHelper::EIF_Volumetexture) || ((pDDSHeaderExt->resourceDimension == 4 /*D3D10_RESOURCE_DIMENSION_TEXTURE3D*/) && (pDDSHeader->dwHeaderFlags & DDS_HEADER_FLAGS_VOLUME)) ? Util::getMax(1ul, pDDSHeader->dwDepth) : 1;
		}
		else
		{
			dwSides = (pDDSHeader->dwReserved1 & CImageExtensionHelper::EIF_Cubemap) || ((pDDSHeader->dwSurfaceFlags & DDS_SURFACE_FLAGS_CUBEMAP) != 0 && (pDDSHeader->dwCubemapFlags & DDS_CUBEMAP_ALLFACES) != 0) ? 6 : 1;
			dwDepth = (pDDSHeader->dwReserved1 & CImageExtensionHelper::EIF_Volumetexture) || (pDDSHeader->dwHeaderFlags & DDS_HEADER_FLAGS_VOLUME) ? Util::getMax(1ul, pDDSHeader->dwDepth) : 1;
		}

		for (DWORD dwSide = 0; dwSide < dwSides; ++ dwSide)
		{
			for (DWORD dwMip = 0; dwMip < dwMips; ++ dwMip)
			{
				STexture::SSurface* pSurf = tex.TryGetSurface(dwSide, dwMip);
				STexture::SBlock block;
				block.m_nChunkSize = pSurf->m_dwSize;
				block.m_pSurfaceData = pSurf->m_pData;
				chunk.m_blocks.push_back(block);
			}
		}

		tex.m_chunks.push_back(chunk);
		tex.m_nFileFlags |= eStreamableResourceFlags_NoSplit;
	}
	else
	{
		// build main mips
		if(!BuildDDSChunks(fileName, resources[0]))
			return;

		DWORD& imageFlags = resources[0].m_ddsHeader.dwReserved1;
		imageFlags |= CImageExtensionHelper::EIF_Splitted;
	}

	// load attached alpha channel
	bool bHasAttachedAlpha = resources.size() > 1;
	if(bHasAttachedAlpha)
	{
		STexture& attachedAlphaResource = resources[1];

		// try to read header of attached alpha channel
		CImageExtensionHelper::DDS_HEADER& alphaDDSHeader = attachedAlphaResource.m_ddsHeader;

		// set flag
		resources[0].m_nFileFlags |= eStreamableResourceFlags_DDSHasAdditionalAlpha;
		attachedAlphaResource.m_nFileFlags |= eStreamableResourceFlags_DDSIsAttachedAlpha;

		// build alpha mips
		if(!BuildDDSChunks(fileName, attachedAlphaResource))
		{
			RCLogError("Error: Cannot load attached alpha channel of texture header: '%s'\n", fileName);
			return;
		}

		DWORD& imageFlags = alphaDDSHeader.dwReserved1;
		imageFlags |= CImageExtensionHelper::EIF_Splitted;
	}
}

void CTextureSplitter::AssembleDDSTextureChunk(
	const char* fileName, const STexture& resource, 
	const uint32 nChunkNumber, std::vector<uint8>* pOutChunk )
{
	assert(resource.m_chunks.size() > nChunkNumber);
	assert(pOutChunk);

	// offset and size of chunk
	std::list<STexture::SChunkDesc>::const_iterator it = resource.m_chunks.begin();
	for(uint32 i=0;i<resource.m_chunks.size();++i)
	{
		if(it->m_nChunkNumber == nChunkNumber)
		{
			break;
		}
		++it;
	}
	assert(it!= resource.m_chunks.end());
	const STexture::SChunkDesc& currentChunk = *it;

	// get DDS header for compression
	const CImageExtensionHelper::DDS_HEADER* pDDSHeader = &resource.m_ddsHeader;
	const CImageExtensionHelper::DDS_HEADER_DXT10* pDDSExtHeader = &resource.m_ddsHeaderExtension;
	int dataHeaderOffset = 0;

	if (nChunkNumber == 0)
	{
		dataHeaderOffset = pDDSHeader->GetFullHeaderSize();

		// attached alpha are HEADER, the others FILE_DESC, the difference is a DWORD
		if (!(resource.m_nFileFlags & eStreamableResourceFlags_DDSIsAttachedAlpha))
		{
			dataHeaderOffset += sizeof(DWORD);
		}
	}

	// allocate space for it
	size_t nTotalSize = dataHeaderOffset;
	for(STexture::TBlocks::const_iterator it = currentChunk.m_blocks.begin();it != currentChunk.m_blocks.end();++it)
		nTotalSize += it->m_nChunkSize;
	pOutChunk->resize(nTotalSize);

	// header file
	if(nChunkNumber == 0)
	{
		size_t offset = 0;

		// copy header to head of array
		if(!(resource.m_nFileFlags & eStreamableResourceFlags_DDSIsAttachedAlpha))
		{
			*reinterpret_cast<DWORD*>(&(*pOutChunk)[0]) = CRY_MAKEFOURCC('D','D','S',' ');
			offset = sizeof(DWORD);
		}

		memcpy(&(*pOutChunk)[offset], pDDSHeader, sizeof(*pDDSHeader));
		offset += sizeof(*pDDSHeader);

		if (pDDSHeader->IsDX10Ext())
		{
			memcpy(&(*pOutChunk)[offset], pDDSExtHeader, sizeof(*pDDSExtHeader));
		}
	}

	// copy mips to head of array
	size_t destOffset = dataHeaderOffset;
	for(STexture::TBlocks::const_iterator it = currentChunk.m_blocks.begin();it != currentChunk.m_blocks.end();++it)
	{
		memcpy(&(*pOutChunk)[destOffset], it->m_pSurfaceData, it->m_nChunkSize);
		destOffset += it->m_nChunkSize;
	}
}

void CTextureSplitter::ProcessResource(
	std::vector<string>* pOutFilenames,
	const char* inFileName, const STexture& resource,
	const uint32 nChunk )
{
	string sDestFileName;
	if (resource.m_nFileFlags & eStreamableResourceFlags_NoSplit)
		sDestFileName = PathUtil::GetFile(GetOutputPath());
	else
		sDestFileName = PathUtil::GetFile( MakeFileName( PathUtil::GetFile(GetOutputPath()), nChunk, resource.m_nFileFlags) );

	const string sFullDestFileName = PathUtil::Make( m_CC.GetOutputFolder(), sDestFileName );
	if (pOutFilenames)
	{
		pOutFilenames->push_back(sFullDestFileName);
	}

	// Compare time stamp of output file.
	if (!m_CC.bForceRecompiling && UpToDateFileHelpers::FileExistsAndUpToDate(sFullDestFileName, inFileName))
	{
		if (m_CC.pRC->GetVerbosityLevel() > 0)
		{
			RCLog("Skipping %s: file %s is up to date", inFileName, sFullDestFileName.c_str());
		}
		return;
	}
	
	// get file chunk
	std::vector<uint8> vecChunkToWrite;
	AssembleDDSTextureChunk(inFileName, resource, nChunk, &vecChunkToWrite);

	assert(!vecChunkToWrite.empty());

	// adds file to pak
	if (!SaveFile(sFullDestFileName, &vecChunkToWrite[0], vecChunkToWrite.size(), FileUtil::GetLastWriteFileTime(inFileName) ) )
	{
		RCLogError("Error: Cannot save file '%s' (source file is '%s')\n", sFullDestFileName, inFileName);
	}
}

bool CTextureSplitter::AddResourceToAdditionalList( const char* fileName, const std::vector<uint8>& fileContent )
{
	const string sDestFileName = PathUtil::GetFile( GetOutputPath().c_str() );
	const string sFullDestFileName = PathUtil::Make( m_CC.GetOutputFolder(), sDestFileName );

	// Compare time stamp of output file.
	if (!m_CC.bForceRecompiling && UpToDateFileHelpers::FileExistsAndUpToDate(sFullDestFileName, fileName))
	{
		if (m_CC.pRC->GetVerbosityLevel() > 0)
		{
			RCLog("Skipping %s: destination file %s is up to date", fileName, sFullDestFileName.c_str());
		}
		return true;
	}

	const CImageExtensionHelper::DDS_HEADER* const pDDSHeader =
		(const CImageExtensionHelper::DDS_HEADER*)(&fileContent[0] + sizeof(DWORD));
	const CImageExtensionHelper::DDS_HEADER_DXT10* const pDDSExtendedHeader =
		(const CImageExtensionHelper::DDS_HEADER_DXT10*)(&fileContent[0] + sizeof(DWORD) + sizeof(CImageExtensionHelper::DDS_HEADER));

	const uint32 nDDSHeaderSize = sizeof(DWORD) + pDDSHeader->GetFullHeaderSize();
	const ETEX_Format format = DDSFormats::GetFormatByDesc(pDDSHeader->ddspf, pDDSExtendedHeader->dxgiFormat);
	const uint32 nTextureSize = TextureHelper::TextureDataSize(
		pDDSHeader->dwWidth, pDDSHeader->dwHeight, pDDSHeader->dwDepth,
		pDDSHeader->GetMipCount(), format);
	assert(nTextureSize <= fileContent.size() - nDDSHeaderSize);

	// add resource to list of non-streamable resources
	if (!SaveFile(sFullDestFileName, &fileContent[0], fileContent.size(), FileUtil::GetLastWriteFileTime(fileName)))
	{
		return false;
	}

	if (!m_CC.pRC->GetAssetManager()->SaveCryasset(m_CC.config, fileName, { sFullDestFileName }))
	{
		return false;
	}

	RCLog("Resource added to additional pak: '%s'\n", fileName);
	return true;
}

void CTextureSplitter::DeInit( )
{
}

void CTextureSplitter::Release( )
{
	if (InterlockedDecrement(&m_refCount) == 0)
		delete this;
}

bool CTextureSplitter::Process()
{
	// Warning: this one is not multithread-safe (even if we
	// return true in SupportsMultithreading()) if platforms
	// specified for different CTextureSplitter compilers are
	// different.

	const PlatformInfo* const pPlatformInfo = m_CC.pRC->GetPlatformInfo(m_CC.platform);

	m_currentEndian = pPlatformInfo->bBigEndian ? eBigEndian : eLittleEndian;

	if (pPlatformInfo->HasName("orbis"))
	{
		m_targetType = eTT_Orbis;
		m_bTile = true;
	}
	else if (pPlatformInfo->HasName("durango"))
	{
		m_targetType = eTT_Durango;
		m_bTile = true;
	}
	else
	{
		m_targetType = eTT_PC;
		m_bTile = false;
	}

	const string sInputFile = m_CC.GetSourcePath();
	const string sDestFileName = PathUtil::GetFile(MakeFileName(PathUtil::GetFile(GetOutputPath()), 0, 0)); 
	const string sFullDestFileName = PathUtil::Make(m_CC.GetOutputFolder(), sDestFileName); 

	// Compare time stamp of first chunk, all chunk have the same date always
	if (!m_CC.bForceRecompiling && UpToDateFileHelpers::FileExistsAndUpToDate(sFullDestFileName, sInputFile))
	{
		if (m_CC.pRC->GetVerbosityLevel() > 0)
		{
			RCLog("Skipping %s: first chunk is up to date", sInputFile);
		}
		return true;
	}

	std::vector<STexture> resources;
	std::vector<uint8> fileContent;

	// load texture, parse dds chunks
	ParseDDSTexture(sInputFile.c_str(), resources, fileContent);

	if (fileContent.empty())
	{
		RCLogWarning("Failed to parse a texture file '%s'.\n", sInputFile.c_str());
		return true;
	}

	std::vector<string> savedFiles;

	if (!resources[0].m_chunks.empty())
	{
		// Only save chunks that exist in all attached images
		size_t numChunksToSave = resources[0].m_chunks.size();
		for (size_t resIdx = 0; resIdx < resources.size(); ++ resIdx)
		{
			numChunksToSave = (std::min)(numChunksToSave, resources[resIdx].m_chunks.size());
		}

		for (size_t resIdx = 0; resIdx < resources.size(); ++ resIdx)
		{
			const STexture& resource = resources[resIdx];

			// write out dds chunks
			for (int32 nChunk = 0; nChunk < numChunksToSave; ++nChunk)
			{
				// split texture's MIPs regarding to current chunk
				// save it to separate file
				ProcessResource(&savedFiles, sInputFile.c_str(), resource, nChunk);
			}

			RCLog("Added file '%s'\n", sInputFile.c_str());
		}

		if (!m_CC.pRC->GetAssetManager()->SaveCryasset(m_CC.config, sInputFile, savedFiles))
		{
			return false;
		}
	}
	else
	{
		// if failed then this resource is not for streaming
		// add it to additional pak
		if (!AddResourceToAdditionalList(sInputFile.c_str(), fileContent))
		{
			return false;
		}
	}

	{
		std::vector<string> unusedFiles;
		FindUnusedResources(&unusedFiles, savedFiles, GetOutputPath().c_str(), true);
		for (size_t i = 0; i < unusedFiles.size(); ++i)
		{
			m_CC.pRC->MarkOutputFileForRemoval(unusedFiles[i]);
		}
	}


	RCLog("Processing file '%s' finished.\n", sInputFile.c_str());

	m_attachedAlphaOffset = 0;

	return true;
}


string CTextureSplitter::GetOutputFileNameOnly() const
{
	const string sourceFileFinal = m_CC.config->GetAsString("overwritefilename", m_CC.sourceFileNameOnly.c_str(), m_CC.sourceFileNameOnly.c_str());
	return PathUtil::ReplaceExtension(sourceFileFinal, "dds");
}

string CTextureSplitter::GetOutputPath() const
{
	return PathUtil::Make(m_CC.GetOutputFolder(), GetOutputFileNameOnly());
}

const char* CTextureSplitter::GetExt( int index ) const
{
	return (index == 0) ? "dds" : 0;
}

ICompiler* CTextureSplitter::CreateCompiler()
{
	if (m_compilerType == Decompressor)
	{
		return new CTextureDecompressor();
	}

	if (m_refCount == 1)
	{
		++m_refCount;
		return this;
	}
	CTextureSplitter* pNewSplitter = new CTextureSplitter();
	return pNewSplitter;
}

bool CTextureSplitter::SupportsMultithreading() const
{
	return true;
}

void CTextureSplitter::ProcessPlatformSpecificConversions(std::vector<STexture>& resourcesOut, byte* fileContent, const size_t fileSize)
{
	assert(fileContent);

	resourcesOut.push_back(STexture());
	STexture& resource = resourcesOut.back();

	resource.m_ddsHeader = (CImageExtensionHelper::DDS_HEADER&)*fileContent;
	resource.m_ddsHeaderExtension = (CImageExtensionHelper::DDS_HEADER_DXT10&)*(fileContent + sizeof(CImageExtensionHelper::DDS_HEADER));

	CImageExtensionHelper::DDS_HEADER& header = resource.m_ddsHeader;
	CImageExtensionHelper::DDS_HEADER_DXT10& exthead = resource.m_ddsHeaderExtension;

	if (header.dwTextureStage != CRY_MAKEFOURCC('F','Y','R','C'))
	{
		header.dwReserved1 = 0;
		header.dwTextureStage = CRY_MAKEFOURCC('F','Y','R','C');
	}

	const size_t nHeaderSize = header.GetFullHeaderSize();
	byte* pDataHead = fileContent + nHeaderSize;

	DWORD& imageFlags = header.dwReserved1;

	// get dimensions
	uint32 dwWidth,dwHeight,dwDepth,dwSides,dwMips;
	dwWidth = header.dwWidth;
	dwHeight = header.dwHeight;
	dwMips  = header.GetMipCount();
	dwSides = 1;
	dwDepth = 1;
	if (header.IsDX10Ext())
	{
		dwSides = (header.dwReserved1 & CImageExtensionHelper::EIF_Cubemap) ? exthead.arraySize : exthead.arraySize;
		dwDepth = (header.dwReserved1 & CImageExtensionHelper::EIF_Volumetexture) || ((exthead.resourceDimension == 4 /*D3D10_RESOURCE_DIMENSION_TEXTURE3D*/) && (header.dwHeaderFlags & DDS_HEADER_FLAGS_VOLUME)) ? Util::getMax(1ul, header.dwDepth) : 1;
	}
	else
	{
		dwSides = (header.dwReserved1 & CImageExtensionHelper::EIF_Cubemap) || ((header.dwSurfaceFlags & DDS_SURFACE_FLAGS_CUBEMAP) != 0 && (header.dwCubemapFlags & DDS_CUBEMAP_ALLFACES) != 0) ? 6 : 1;
		dwDepth = (header.dwReserved1 & CImageExtensionHelper::EIF_Volumetexture) || (header.dwHeaderFlags & DDS_HEADER_FLAGS_VOLUME) ? Util::getMax(1ul, header.dwDepth) : 1;
	}

	const ETEX_Format format = DDSFormats::GetFormatByDesc(header.ddspf, exthead.dxgiFormat);
	if (format == eTF_Unknown)
	{
		RCLogError("Unknown DDS format: flags %d (0x%x), fourcc 0x%x", header.ddspf.dwFlags, header.ddspf.dwFlags, header.ddspf.dwFourCC);
		return;
	}

	const bool bBlockCompressed = TextureHelper::IsBlockCompressed(format);
	const uint32 nBitsPerPixel = bBlockCompressed ? (TextureHelper::BytesPerBlock(format) / 2) : (TextureHelper::BytesPerPixel(format) * 8);
	// all formats that don't need endian swapping/byte rearrangement should be here
	const bool bNeedEndianSwapping = (format != eTF_CTX1);

	// Collect surfaces in resource
	uint64 nDataOffset = 0;
	for (uint32 dwSide=0;dwSide<dwSides;++dwSide)
	{
		for (uint32 dwMip=0;dwMip<dwMips;++dwMip)
		{
			STexture::SSurface s;
			s.m_dwSide = dwSide;
			s.m_dwStartMip = dwMip;
			s.m_dwMips = 1;
			s.m_dwWidth = Util::getMax(1u, dwWidth >> dwMip);
			s.m_dwHeight = Util::getMax(1u, dwHeight >> dwMip);
			s.m_dwDepth = Util::getMax(1u, dwDepth >> dwMip);
			s.m_dwSize = TextureHelper::TextureDataSize(s.m_dwWidth, s.m_dwHeight, s.m_dwDepth, 1, format);
			s.m_pData = pDataHead + nDataOffset;

			resource.AddSurface(s);

			nDataOffset += s.m_dwSize;
		}
	}

	const PlatformInfo* const pi = m_CC.pRC->GetPlatformInfo(m_CC.platform);
	if (pi == 0)
	{
		return;
	}

	EEndian currentEndian = eLittleEndian;

	bool bNeedsProcess = false;

	// check if this texture already native-converted
	// mark this texture as native for current platform
	switch (m_targetType)
	{
	case eTT_Orbis:
		if (imageFlags & CImageExtensionHelper::EIF_OrbisNative) return;
		imageFlags |= CImageExtensionHelper::EIF_OrbisNative;
		bNeedsProcess = true;
		break;

	case eTT_Durango:
		if (imageFlags & CImageExtensionHelper::EIF_DurangoNative) return;
		imageFlags |= CImageExtensionHelper::EIF_DurangoNative;
		bNeedsProcess = true;
		break;
	}

	if (bNeedsProcess)
	{
		// Process raw mips
		for (uint32 dwSide=0;dwSide<dwSides;++dwSide)
		{
			for (uint32 dwMip=0;dwMip<dwMips;++dwMip)
			{
				// calc mip's size
				const STexture::SSurface& surface = *resource.TryGetSurface(dwSide, dwMip);
				const uint32 dwLocalWidth = surface.m_dwWidth;
				const uint32 dwLocalHeight = surface.m_dwHeight;
				const uint32 dwLocalDepth = surface.m_dwDepth;
				const uint32 dwBlockHeight = bBlockCompressed ? (dwLocalHeight + 3) / 4 : dwLocalHeight;
				const uint32 dwPitch = TextureHelper::TextureDataSize(dwLocalWidth, 1, 1, 1, format);

				byte* pMem = surface.m_pData;

				for (uint32 d = 0; d < dwDepth; ++d)
				{
					for (uint32 v = 0; v < dwBlockHeight; ++v)	// for each line of blocks
					{
						if (bNeedEndianSwapping)
						{
							// swizzling and endians swapping for specific platforms
							if (bBlockCompressed)
							{
								SwapEndian((uint16*)pMem, dwPitch / sizeof(uint16), currentEndian);
							}
							else if (format == eTF_R16G16F || format == eTF_R16G16S || format == eTF_R16G16)
							{
								SwapEndian((uint32*)pMem, dwPitch / sizeof(uint32), currentEndian);
							}
							else if (format == eTF_R8G8B8A8 || format == eTF_B8G8R8X8 || format == eTF_B8G8R8A8)
							{
								SwapEndian((uint32*)pMem, dwPitch / sizeof(uint32), currentEndian);
							}
							else if (format == eTF_B4G4R4A4)
							{
								SwapEndian((uint32*)pMem, dwPitch / sizeof(uint32), currentEndian);
							}
							else if (format == eTF_B8G8R8 || format == eTF_L8V8U8)
							{
								assert(nBitsPerPixel == 24);
								if ((imageFlags & DDS_RESF1_DSDT) == 0)
								{
									SwapEndian((uint32*)pMem, dwPitch / sizeof(uint32), currentEndian);
								}
								else	// DUDV(L8V8U8)
								{
									for (uint32 i = 0; i < dwLocalWidth; ++i)
										std::swap(pMem[i * 3], pMem[i * 3 + 2]);
								}
							}
							else
							{
								if (nBitsPerPixel == 32)
									SwapEndian((uint32*)pMem, dwPitch / sizeof(uint32), currentEndian);
								else if (nBitsPerPixel == 16)
									SwapEndian((uint16*)pMem, dwPitch / sizeof(uint16), currentEndian);
							}
						}

						pMem += dwPitch;
					}
				}
			}
		}

#if defined(TOOLS_SUPPORT_ORBIS)
		if (m_targetType == eTT_Orbis)
			ProcessPlatformSpecificConversions_Orbis(resource, dwSides, dwWidth, dwHeight, dwDepth, dwMips, format, bBlockCompressed, nBitsPerPixel);
#endif

#if defined(TOOLS_SUPPORT_DURANGO)
		if (m_targetType == eTT_Durango)
			ProcessPlatformSpecificConversions_Durango(resource, dwSides, dwWidth, dwHeight, dwDepth, dwMips, format, bBlockCompressed, nBitsPerPixel);
#endif
	}

	// apply to attached images recursively
	if (nDataOffset + nHeaderSize < fileSize)
	{
		uint32 size;
		CImageExtensionHelper::DDS_HEADER* attachedHeader = CImageExtensionHelper::GetAttachedImage(pDataHead + nDataOffset, &size);
		if (attachedHeader)
		{
			const size_t attachedHeaderSize = attachedHeader->GetFullHeaderSize();

			ProcessPlatformSpecificConversions(resourcesOut, (byte*)attachedHeader, size);

			if (m_attachedAlphaOffset != 0)
			{
				//do we need to support more than one attached texture?
				RCLog("Already have one attached alpha texture for %s\n", GetSourceFilename().c_str());
				assert(0);
			}

			m_attachedAlphaOffset = nDataOffset + attachedHeaderSize;
		}
	}
}

void CTextureSplitter::SetOverrideSourceFileName(const string &srcFilename)
{
	m_sOverrideSourceFile = srcFilename;
}

string CTextureSplitter::GetSourceFilename()
{
	return m_sOverrideSourceFile.empty() ? m_CC.GetSourcePath() : m_sOverrideSourceFile;
}

void CTextureSplitter::Init(const ConverterInitContext& context)
{
	if (context.config->GetAsBool("decompress", false, true))
	{
		m_compilerType = Decompressor;
	}
}
