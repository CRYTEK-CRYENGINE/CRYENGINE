// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
//  Crytek Engine Source File
//  Copyright (C) Crytek GmbH
// -------------------------------------------------------------------------
//  File name:   ChunkCompiler.cpp
//  Created:     2010/04/05 by Sergey Sokov
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ConvertContext.h"
#include "ChunkCompiler.h"
#include "../Cry3DEngine/CGF/ChunkFileReaders.h"
#include "../Cry3DEngine/CGF/ChunkFileWriters.h"
#include <Cry3DEngine/CGF/CryHeaders.h>
#include <Cry3DEngine/CGF/IChunkFile.h>
#include "iconfig.h"
#include "ResourceCompiler.h"
#include <CryCore/ToolsHelpers/ResourceCompilerHelper.h>
#include "StringHelpers.h"
#include "UpToDateFileHelpers.h"


static bool writeChunkFile(
	ChunkFile::MemorylessChunkFileWriter::EChunkFileFormat eFormat,
	const char* dstFilename, 
	ChunkFile::IReader* pReader, 
	const char* srcFilename, 
	const std::vector<IChunkFile::ChunkDesc>& chunks)
{
	ChunkFile::OsFileWriter writer;

	if (!writer.Create(dstFilename))
	{
		RCLogError("Failed to create '%s'", dstFilename);
		return false;
	}

	ChunkFile::MemorylessChunkFileWriter wr(eFormat, &writer);

	wr.SetAlignment(4);

	while (wr.StartPass())
	{
		for (uint32 i = 0; i < chunks.size(); ++i)
		{
			wr.StartChunk((chunks[i].bSwapEndian ? eEndianness_NonNative : eEndianness_Native), chunks[i].chunkType, chunks[i].chunkVersion, chunks[i].chunkId);

			uint32 srcSize = chunks[i].size;
			if (srcSize <= 0)
			{
				continue;
			}

			if (!pReader->SetPos(chunks[i].fileOffset) != 0)
			{
				RCLogError("Failed to read (seek) file %s.", srcFilename);
				return false;
			}

			while (srcSize > 0)
			{
				char bf[4 * 1024];
				const uint32 sz = Util::getMin((uint32)sizeof(bf), srcSize);
				srcSize -= sz;

				if (!pReader->Read(bf, sz))
				{
					RCLogError("Failed to read %u byte(s) from file %s.", (uint)sz, srcFilename);
					return false;
				}

				wr.AddChunkData(bf, sz);
			}
		}
	}

	if (!wr.HasWrittenSuccessfully())
	{
		RCLogError("Failed to write %s.", dstFilename);
		return false;
	}
	
	return true;
}


static bool convertChunkFile(const uint32 version, const char* srcFilename, const char* dstFilename)
{
	if (srcFilename == 0 || srcFilename[0] == 0 || dstFilename == 0 || dstFilename[0] == 0)
	{
		RCLogError("Empty name of a chunk file. Contact RC programmer.");
		return false;
	}

	ChunkFile::CryFileReader f;

	if (!f.Open(srcFilename))
	{
		RCLogError("File to open file %s for reading", srcFilename);
		return false;
	}

	std::vector<IChunkFile::ChunkDesc> chunks;

	string s;
	s = ChunkFile::GetChunkTableEntries_0x746(&f, chunks);
	if (!s.empty())
	{
		s = ChunkFile::GetChunkTableEntries_0x744_0x745(&f, chunks);
		if (s.empty())
		{
			s = ChunkFile::StripChunkHeaders_0x744_0x745(&f, chunks);
		}
	}

	if (!s.empty())
	{
		RCLogError("%s", s.c_str());
		return false;
	}

	const ChunkFile::MemorylessChunkFileWriter::EChunkFileFormat eFormat =
		(version == 0x745)
		? ChunkFile::MemorylessChunkFileWriter::eChunkFileFormat_0x745
		: ChunkFile::MemorylessChunkFileWriter::eChunkFileFormat_0x746;

	const bool bOk = writeChunkFile(eFormat, dstFilename, &f, srcFilename, chunks);
	return bOk;
}


//////////////////////////////////////////////////////////////////////////
CChunkCompiler::CChunkCompiler()
{
}


CChunkCompiler::~CChunkCompiler()
{
}


//////////////////////////////////////////////////////////////////////////
// ICompiler methods.
//////////////////////////////////////////////////////////////////////////
string CChunkCompiler::GetOutputFileNameOnly() const
{
	const string sourceFileFinal = m_CC.config->GetAsString("overwritefilename", m_CC.sourceFileNameOnly.c_str(), m_CC.sourceFileNameOnly.c_str());
	return sourceFileFinal;
}


string CChunkCompiler::GetOutputPath() const
{
	return PathUtil::Make(m_CC.GetOutputFolder(), GetOutputFileNameOnly());
}


bool CChunkCompiler::Process()
{
	const string sourceFile = m_CC.GetSourcePath();
	const string outputFile = GetOutputPath();

	if (!m_CC.bForceRecompiling && UpToDateFileHelpers::FileExistsAndUpToDate(GetOutputPath(), m_CC.GetSourcePath()))
	{
		// The file is up-to-date
		m_CC.pRC->AddInputOutputFilePair(m_CC.GetSourcePath(), GetOutputPath());
		return true;
	}

	if (m_CC.config->GetAsBool("SkipMissing", false, true))
	{
		// Skip missing source files.
		const DWORD dwFileSpecAttr = GetFileAttributes(sourceFile.c_str());
		if (dwFileSpecAttr == INVALID_FILE_ATTRIBUTES)
		{
			// Skip missing file instead of reporting it as an error.
			return true;
		}
	}

	bool bOk = false;

	try
	{
		const uint32 version = 
			StringHelpers::EndsWith(m_CC.config->GetAsString("targetversion", "0x746", "0x746"), "745")
			? 0x745
			: 0x746;
		bOk = convertChunkFile(version, sourceFile.c_str(), outputFile.c_str());
	}
	catch (char*)
	{
		Beep(1000, 1000);
		RCLogError("Unexpected failure in processing %s - contact an RC programmer.", sourceFile.c_str());
		return false;
	}

	if (bOk)
	{
		if (!UpToDateFileHelpers::SetMatchingFileTime(GetOutputPath(), m_CC.GetSourcePath()))
		{
			return false;
		}
		m_CC.pRC->AddInputOutputFilePair(m_CC.GetSourcePath(), GetOutputPath());
	}

	return bOk;
}


const char* CChunkCompiler::GetExt(int index) const 
{ 
	return (index == 0) ? "chunk" : 0; 
}
