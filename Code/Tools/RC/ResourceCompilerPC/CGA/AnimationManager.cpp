// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FileUtil.h"
#include "AnimationManager.h"
#include "AnimationCompiler.h" // for SEndiannessWrapper

static bool HeaderLessFuncAIM(const GlobalAnimationHeaderAIM* lhs, const GlobalAnimationHeaderAIM* rhs)
{
	return lhs->m_FilePath < rhs->m_FilePath;
}

static bool HeaderLessFuncCAF(const GlobalAnimationHeaderCAF* lhs, const GlobalAnimationHeaderCAF* rhs)
{
	return lhs->m_FilePath < rhs->m_FilePath;
}

// Checks if the GlobalAnimationHeaderAIMs are empty, 
// which indicates that calls to SaveAIMImage can be skipped for this instance.
bool CAnimationManager::CanBeSkipped()
{
	return m_arrGlobalAIM.empty();
}

bool CAnimationManager::SaveAIMImage(const char* name, FILETIME timeStamp, bool bigEndianOutput)
{
	CChunkFile chunkFile;
	CSaverCGF cgfSaver(chunkFile);

	// Sort headers by filename. This is not required by Engine, but makes it easier to compare RC-s output.
	std::vector<GlobalAnimationHeaderAIM*> sortedHeaders;
	size_t numHeaders = m_arrGlobalAIM.size();
	sortedHeaders.reserve(numHeaders);
	for (size_t i = 0; i < numHeaders; ++i)
	{
		sortedHeaders.push_back(&m_arrGlobalAIM[i]);
	}
	std::sort(sortedHeaders.begin(), sortedHeaders.end(), HeaderLessFuncAIM);

	for (size_t i = 0; i < numHeaders; ++i)
	{
		GlobalAnimationHeaderAIM* header = sortedHeaders[i];
		if (!header->IsValid())
		{
			RCLogError("AIM animation header contains invalid values (NANs). Animation skipped: '%s'.",
								 header->GetFilePath());
			continue;
		}
		header->SaveToChunkFile(&chunkFile, bigEndianOutput);
	}

	SetFileAttributes( name,FILE_ATTRIBUTE_ARCHIVE );
	chunkFile.Write( name );

	FileUtil::SetFileTimes(name, timeStamp);
	const __int64 fileSize = FileUtil::GetFileSize(name);
	if (fileSize < 0)
	{
		RCLogError("Failed to get file size of '%s'", name);
		return false;
	}

	if (fileSize > 0xffFFffFFU)
	{
		RCLogError( "Unexpected huge file '%s' found", name);
		return false;
	}
	return true;
}

//---------------------------------------------------------------
//---------------------------------------------------------------
//---------------------------------------------------------------
bool CAnimationManager::SaveCAFImage(const char* name, FILETIME timeStamp, bool bigEndianOutput)
{
	SEndiannessSwapper swap(bigEndianOutput);

	CChunkFile chunkFile;
	CSaverCGF cgfSaver(chunkFile);

	// Sort headers by filename. This is not required by Engine, but makes it easier to compare RC-s output.
	std::vector<GlobalAnimationHeaderCAF*> sortedHeaders;
	size_t numHeaders = m_arrGlobalAnimations.size();
	sortedHeaders.reserve(numHeaders);
	for (size_t i = 0; i < numHeaders; ++i)
	{
		sortedHeaders.push_back(&m_arrGlobalAnimations[i]);
	}
	std::sort(sortedHeaders.begin(), sortedHeaders.end(), &HeaderLessFuncCAF);

	uint32 numGAH = m_arrGlobalAnimations.size();
	for (size_t i = 0; i < numHeaders; ++i)
	{
		GlobalAnimationHeaderCAF* header = sortedHeaders[i];

		if (header->IsAimpose())
		{
			continue;
		}
		header->SaveToChunkFile(&chunkFile, bigEndianOutput);
	}


	SetFileAttributes( name,FILE_ATTRIBUTE_ARCHIVE );
	chunkFile.Write( name );

	FileUtil::SetFileTimes(name, timeStamp);
	const __int64 fileSize = FileUtil::GetFileSize(name);
	if (fileSize < 0)
	{
		RCLogError("Failed to get file size of '%s'", name);
		return false;
	}

	if (fileSize > 0xffFFffFFU)
	{
		RCLogError( "Unexpected huge file '%s' found", name);
		return false;
	}
	return true;
}

bool CAnimationManager::AddAIMHeaderOnly(const GlobalAnimationHeaderAIM& header)
{
	if (header.m_FilePathCRC32 == 0)
	{
		RCLogError("AnimationManager: Adding GlobalAnimationHeaderAIM with m_FilePathCRC32 == 0.");
		return false;
	}

	GlobalAnimationHeaderAIM newHeader = header;
	uint32 numControllers = newHeader.m_arrController.size();
	for (size_t i = 0; i < numControllers; ++i)
	{
		newHeader.m_arrController[i].reset(); // release controllers
	}

	ThreadUtils::AutoLock lock(m_lockAIMs);
	if (HasAIM(header.m_FilePathCRC32))
		return false;
	m_arrGlobalAIM.push_back(header);
	return true;
}

bool CAnimationManager::AddCAFHeaderOnly(const GlobalAnimationHeaderCAF& header)
{
	GlobalAnimationHeaderCAF newHeader = header;
	newHeader.m_arrController.clear();

	if (newHeader.m_FilePathCRC32 == 0)
	{
		RCLogError("AnimationManager: Adding GlobalAnimationHeaderCAF with null m_FilePathCRC32.");
		return false;
	}

	ThreadUtils::AutoLock lock(m_lockCAFs);
	if (HasCAF(header.m_FilePathCRC32))
	{
		return false;
	}

	m_arrGlobalAnimations.push_back(newHeader);
	return true;
}

bool CAnimationManager::HasAIMHeader(const GlobalAnimationHeaderAIM& gah) const
{
	return HasAIM(gah.m_FilePathCRC32);
}

bool CAnimationManager::HasAIM(uint32 pathCRC) const
{
	int32 numAIM = m_arrGlobalAIM.size();
	for (int32 i = 0; i < numAIM; ++i)
	{
		if(m_arrGlobalAIM[i].m_FilePathCRC32 == pathCRC)
		{
			return true;
		}
	}
	return false;
}

bool CAnimationManager::HasCAFHeader(const GlobalAnimationHeaderCAF& gah) const
{
	return HasCAF(gah.m_FilePathCRC32);
}

bool CAnimationManager::HasCAF(uint32 pathCRC) const
{
	int32 numAIM = m_arrGlobalAnimations.size();
	for (int32 i = 0; i < numAIM; ++i)
	{
		if(m_arrGlobalAnimations[i].m_FilePathCRC32 == pathCRC)
		{
			return true;
		}
	}
	return false;
}
