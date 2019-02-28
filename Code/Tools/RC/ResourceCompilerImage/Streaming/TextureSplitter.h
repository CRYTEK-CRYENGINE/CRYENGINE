// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2008.
-------------------------------------------------------------------------
$Id: TextureSplitter.h,v 1.0 2008/01/17 15:14:13 AntonKaplanyan Exp wwwrun $
$DateTime$
Description:  Routine for creation of streaming pak
-------------------------------------------------------------------------
History:
- 17:1:2008   10:31 : Created by Anton Kaplanyan
*************************************************************************/

#pragma once

#ifndef __TEXTURE_SPLITTER_H__
#define __TEXTURE_SPLITTER_H__


#ifndef MAKEFOURCC
#define NEED_UNDEF_MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3)                              \
				((uint32)(uint8)(ch0) | ((uint32)(uint8)(ch1) << 8) |       \
				((uint32)(uint8)(ch2) << 16) | ((uint32)(uint8)(ch3) << 24 ))
#endif

#include "IConverter.h"
#include "../../ResourceCompiler/ConvertContext.h"
#include <list>
#include <set>
#include <Cry3DEngine/ImageExtensionHelper.h>

struct D3DBaseTexture;
struct D3DBaseView;

#if defined(TOOLS_SUPPORT_ORBIS)
namespace sce { namespace Gnm { class Texture; } }
#endif

namespace TextureHelper
{
	int TextureDataSize(int nWidth, int nHeight, int nDepth, int nMips, const ETEX_Format eTF);
}

// creates pak file from file with list of resources
class CTextureSplitter : public IConverter, public ICompiler
{
private:
	string GetOutputFileNameOnly() const;
	string GetOutputPath() const;

public:
	struct STexture;

protected:

	enum CompilerType
	{
		TextureSplitter,
		Decompressor,
	};

	friend string MakeFileName( const string& filePath, const uint32 nChunk, const uint32 nFlags );

	// textures work
	void ParseDDSTexture(const char* fileName, std::vector<STexture>& resources, std::vector<uint8>& fileContent);
	bool BuildDDSChunks(const char* fileName, STexture& resource);
	void AssembleDDSTextureChunk(
		const char* fileName, const STexture& resource,
		const uint32 nChunkNumber, std::vector<uint8>* pOutChunk);

	bool AddResourceToAdditionalList( const char* fileName, const std::vector<uint8>& fileContent );

	// load texture content
	bool LoadTexture( const char* fileName, std::vector<uint8>& fileContent );

	// postprocessing after loading(clamping mips etc.)
	void PostLoadProcessTexture(std::vector<uint8>& fileContent);

	// Arguments:
	//   fileContent - input image, must not be NULL
	// applied to attached images recursively after processing
	// necessary to correctly swap all endians according to the current platform 
	void ProcessPlatformSpecificConversions(std::vector<STexture>& resourcesOut, byte* fileContent, const size_t fileSize );
	void ProcessPlatformSpecificConversions_Orbis(STexture& resource, DWORD dwSides, DWORD dwWidth, DWORD dwHeight, DWORD dwDepth, DWORD dwMips, ETEX_Format format, bool bDXTCompressed, uint32 nBitsPerPixel);
	void ProcessPlatformSpecificConversions_Durango(STexture& resource, DWORD dwSides, DWORD dwWidth, DWORD dwHeight, DWORD dwDepth, DWORD dwMips, ETEX_Format format, bool bDXTCompressed, uint32 nBitsPerPixel);

	// process single texture
	void ProcessResource(
		std::vector<string>* pOutFilenames, 
		const char* inFileName, const STexture& resource,
		const uint32 nChunk);

	// file IO framework
	bool SaveFile(const string& sFileName, const void* pBuffer, const size_t nSize, const FILETIME& fileTime);

public:
	// settings to store textures for streaming
	enum { kNumLastMips = 3 };

public:
	CTextureSplitter();
	~CTextureSplitter();

	virtual void Release();

	virtual void BeginProcessing(const IConfig* config) { }
	virtual void EndProcessing() { }
	virtual IConvertContext* GetConvertContext() { return &m_CC; }
	virtual bool Process();

	virtual void DeInit();
	virtual ICompiler* CreateCompiler();
	virtual bool SupportsMultithreading() const;
	virtual const char* GetExt(int index) const;

	// Used to override reported name of the source file.
	void SetOverrideSourceFileName( const string &srcFilename );
	string GetSourceFilename();

	virtual void Init(const ConverterInitContext& context) override;

	enum ETargetType
	{
		eTT_None,
		eTT_PC,
		eTT_Orbis,
		eTT_Durango,
	};

protected:

	// settings to store textures for streaming
	enum EHeaderInfo
	{
		ehiNumLastMips = 3,
	};

	// flags for different resource purposes
	enum EStreamableResourceFlags
	{
		eStreamableResourceFlags_DDSHasAdditionalAlpha = 1 << 0,  // indicates that DDS texture has attached alpha A8 texture
		eStreamableResourceFlags_DDSIsAttachedAlpha    = 1 << 1,  // indicates that DDS texture is attached alpha A8 texture
		eStreamableResourceFlags_NoSplit							 = 1 << 2		// indicates that the texture is to be unsplitted
	};

#if defined(TOOLS_SUPPORT_ORBIS)
	class COrbisTexture : public _reference_target<int>
	{
	public:
		COrbisTexture(sce::Gnm::Texture* pTexture, byte* pTextureBits);
		~COrbisTexture();

		sce::Gnm::Texture* GetTexture() const { return m_pTexture; }
		const byte* GetBits() const { return m_pTextureBits; }

	private:
		sce::Gnm::Texture* m_pTexture;
		byte* m_pTextureBits;
	};
#endif

#if defined(TOOLS_SUPPORT_DURANGO)
	class CDurangoTexture : public _reference_target<int>
	{
	public:
		explicit CDurangoTexture(byte* pTextureBits);
		~CDurangoTexture();

		const byte* GetBits() const { return m_pTextureBits; }

	private:
		byte* m_pTextureBits;
	};
#endif

public:
	// structure to store mapping information about interleaved resource 
	struct STexture
	{
		struct SSurface
		{
			SSurface()
				: m_dwSide()
				, m_dwStartMip()
				, m_dwMips()
				, m_dwWidth()
				, m_dwHeight()
				, m_dwDepth()
				, m_dwSize()
				, m_pData()
			{
			}

			uint32 m_dwSide;
			uint32 m_dwStartMip;
			uint32 m_dwMips;
			uint32 m_dwWidth;
			uint32 m_dwHeight;
			uint32 m_dwDepth;
			uint64 m_dwSize;
			byte* m_pData;
		};

		typedef std::vector<SSurface> MipVec;
		typedef std::vector<MipVec> SideVec;

		struct SBlock
		{
			const byte* m_pSurfaceData;
			uint64		m_nChunkSize;															// size of chunk
			SBlock() :	m_pSurfaceData(NULL), m_nChunkSize(0) { }
		};

		typedef std::vector<SBlock>	TBlocks;

		// description of single resource chunk, assembled from multiple blocks
		struct SChunkDesc
		{
			uint8			m_nChunkNumber;														// ordered number of chunk
			TBlocks   m_blocks;
			bool operator <(const SChunkDesc& cd) const { return m_blocks.begin()->m_pSurfaceData > cd.m_blocks.begin()->m_pSurfaceData; }	// decreasing by offset

			SChunkDesc() : m_nChunkNumber(0) { }
		};

		uint8											m_nFileFlags;			// flags for different resource purposes
		CImageExtensionHelper::DDS_HEADER								m_ddsHeader;
		CImageExtensionHelper::DDS_HEADER_DXT10					m_ddsHeaderExtension;
		SideVec										m_surfaces;
		std::list<SChunkDesc>			m_chunks;					// all file chunks

#if defined(TOOLS_SUPPORT_ORBIS)
		_smart_ptr<COrbisTexture> m_pOrbisTexture;
#endif
#if defined(TOOLS_SUPPORT_DURANGO)
		std::vector<_smart_ptr<CDurangoTexture> > m_pDurangoTextures;
#endif

		SSurface* TryGetSurface(int nSide, int nMip);
		void AddSurface(const SSurface& surf);

		STexture() : m_nFileFlags(0) {}
	};

protected:
	volatile long m_refCount;

	EEndian m_currentEndian;
	ETargetType m_targetType;
	bool m_bTile;

	uint32 m_attachedAlphaOffset;

	ConvertContext m_CC;
	CompilerType m_compilerType;
	string m_sOverrideSourceFile;
};

#ifdef NEED_UNDEF_MAKEFOURCC
	#undef MAKEFOURCC
#endif // NEED_UNDEF_MAKEFOURCC

#endif // __TEXTURE_SPLITTER_H__
