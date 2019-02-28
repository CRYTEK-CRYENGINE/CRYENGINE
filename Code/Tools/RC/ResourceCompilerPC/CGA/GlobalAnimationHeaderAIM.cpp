// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GlobalAnimationHeaderAIM.h"
#include "../CryEngine/Cry3DEngine/CGF/ChunkData.h"
#include "../CryEngine/Cry3DEngine/CGF/ChunkFile.h"
#include "GlobalAnimationHeaderCAF.h" // for CAF_ utilities
#include "AnimationCompiler.h" // for SEndiannessSwapper

void GlobalAnimationHeaderAIM::SetFilePath(const string& name)
{ 
	m_FilePath = name; 
	const char* pname = m_FilePath.c_str();
	m_FilePathCRC32 = CCrc32::ComputeLowercase(pname); 
}

bool GlobalAnimationHeaderAIM::LoadAIM(const string& filename, ECAFLoadMode cafLoadMode)
{
	CChunkFile chunkFile;

	if (!chunkFile.Read(filename.c_str()))
	{
		RCLogError("Chunk file error: %s", chunkFile.GetLastError());
		return false;
	}

	int32 lastControllerID;
	if (!LoadChunks(&chunkFile, cafLoadMode, lastControllerID, filename))
	{
		return false;
	}

	//----------------------------------------------------------------------------------------------
	OnAssetCreated();
	OnAssetLoaded();
	return true;
}

bool GlobalAnimationHeaderAIM::ReadTiming(const IChunkFile::ChunkDesc* pChunkDesc, ECAFLoadMode loadMode, const char* logFilename)
{
	if (loadMode == eCAFLoadCompressedOnly)
	{
		RCLogWarning("Uncompressed timing chunk has been found in a compressed animation file: '%s'", logFilename);
		return true;
	}

	switch (pChunkDesc->chunkVersion)
	{

	case TIMING_CHUNK_DESC_0918::VERSION:
		{
			const auto pChunk = static_cast<TIMING_CHUNK_DESC_0918*>(pChunkDesc->data);
			SwapEndian(*pChunk);

			if (pChunk->global_range.end < pChunk->global_range.start)
			{
				RCLogError("Timing chunk with a negative animation duration has been found in animation file: '%s'", logFilename);
				return false;
			}

			const int32 nStartKey = 0;
			const int32 nEndKey = pChunk->global_range.end - pChunk->global_range.start;

			const f32 fSecsPerFrame = SECONDS_PER_TICK * TICKS_PER_FRAME;

			m_fStartSec = nStartKey * fSecsPerFrame;
			m_fEndSec = (std::max)(nEndKey, 1) * fSecsPerFrame;
			m_fTotalDuration = m_fEndSec - m_fStartSec;
		}
		return true;

	case TIMING_CHUNK_DESC_0919::VERSION:
		{
			auto pChunk = static_cast<TIMING_CHUNK_DESC_0919*>(pChunkDesc->data);
			SwapEndian(*pChunk);

			const f32 hardcodedSamplingRate =  float(TICKS_PER_SECOND) / TICKS_PER_FRAME;

			if (pChunk->numberOfSamples <= 0)
			{
				RCLogError("Timing chunk (0x%04x) declares a nonpositive number of samples (%d). File: '%s'", pChunk->VERSION, pChunk->numberOfSamples, logFilename);
				return false;
			}

			if ((std::abs(pChunk->samplesPerSecond - hardcodedSamplingRate) / hardcodedSamplingRate) > std::numeric_limits<float>::epsilon())
			{
				RCLogWarning("Timing chunk (0x%04x) declares a currently unsupported sampling rate (%.2f). Sampling rate will be overridden to %.2f. File: '%s'", pChunk->VERSION, pChunk->samplesPerSecond, hardcodedSamplingRate, logFilename);
			}

			m_fStartSec = 0.0f;
			m_fTotalDuration = (std::max)(pChunk->numberOfSamples - 1, 1) / hardcodedSamplingRate;
			m_fEndSec = m_fStartSec + m_fTotalDuration;
		}
		return true;

	default:
		RCLogWarning("Unknown timing chunk version (0x%04x): '%s'", pChunkDesc->chunkVersion, logFilename);
		return false;
	}
}

bool GlobalAnimationHeaderAIM::ReadMotionParameters(const IChunkFile::ChunkDesc *chunk, ECAFLoadMode loadMode, const char* logFilename)
{
	if (chunk->chunkVersion == SPEED_CHUNK_DESC_2::VERSION)
	{
		if (loadMode == eCAFLoadCompressedOnly)
		{
			RCLogWarning("Old motion-parameters chunk in 'compressed' animation: %s", logFilename);
			return false;
		}
		// TODO check chunk data size

		SPEED_CHUNK_DESC_2* desc = (SPEED_CHUNK_DESC_2*)chunk->data;
		SwapEndian(*desc);

		m_nFlags = desc->AnimFlags;
		return true;
	}
	else if (chunk->chunkVersion == CHUNK_MOTION_PARAMETERS::VERSION)
	{
		if (loadMode == eCAFLoadUncompressedOnly)
		{
			// skip new chunks
			return true;
		}

		if (chunk->bSwapEndian)
		{
			RCLogError("%s: data are stored in non-native endian format: %s", __FUNCTION__, logFilename);
		}
		
		CHUNK_MOTION_PARAMETERS* desc = (CHUNK_MOTION_PARAMETERS*)chunk->data;
		// TODO: SwapEndian(*desc); 

		const f32 fSecsPerFrame = SECONDS_PER_TICK * TICKS_PER_FRAME;
		m_nFlags = desc->mp.m_nAssetFlags;
		m_fStartSec	= 0;
		m_fEndSec	= (desc->mp.m_nEnd - desc->mp.m_nStart) * fSecsPerFrame;
		
		m_fTotalDuration = m_fEndSec - m_fStartSec;
		return true;
	}
	else
	{
		RCLogWarning("Unknown version of motion parameters chunk (0x%08x): %s", chunk->chunkVersion, logFilename);
		return true;
	}
}

bool GlobalAnimationHeaderAIM::ReadGAH(const IChunkFile::ChunkDesc *chunk)
{
	if (chunk->chunkVersion != CHUNK_GAHAIM_INFO::VERSION)
	{
		RCLogError("Unsupported CHUNK_GAHAIM_INFO version");
		return false;
	}

	const CHUNK_GAHAIM_INFO* pChunk = (CHUNK_GAHAIM_INFO*)chunk->data;

	m_nFlags						= pChunk->m_Flags;
	const uint32 nValidFlags = (CA_ASSET_BIG_ENDIAN|CA_ASSET_LOADED|CA_ASSET_CREATED|CA_AIMPOSE);
	if (pChunk->m_Flags & ~nValidFlags)
	{
		RCLogWarning("Badly exported aim-pose: flags: %08x %s",pChunk->m_Flags,pChunk->m_FilePath);
		m_nFlags &= nValidFlags;
	}

	m_FilePath					=	pChunk->m_FilePath;
	m_FilePathCRC32		=	pChunk->m_FilePathCRC32;

	const uint32 nCRC32 = CCrc32::ComputeLowercase( m_FilePath ); 
	if (m_FilePathCRC32 != nCRC32)	
	{
		RCLogError("CRC32 Invalid! Most likely the endian conversion from RC is wrong.");
	}

	m_fStartSec				= 0;
	m_fEndSec				= pChunk->m_fEndSec - pChunk->m_fStartSec;
	m_fTotalDuration		= pChunk->m_fTotalDuration;

	m_AnimTokenCRC32		= pChunk->m_AnimTokenCRC32;
	m_nExist						= pChunk->m_nExist;
	m_MiddleAimPoseRot = pChunk->m_MiddleAimPoseRot;
	if (!m_MiddleAimPoseRot.IsValid()) 
	{
		RCLogWarning("Invalid m_MiddleAimPoseRot in '%s'", m_FilePath.c_str());
	}
	m_MiddleAimPose		= pChunk->m_MiddleAimPose;
	if (!m_MiddleAimPose.IsValid()) 
	{
		RCLogWarning("Invalid m_MiddleAimPose in '%s'", m_FilePath.c_str());
	}

	for (uint32 v=0; v<(CHUNK_GAHAIM_INFO::XGRID*CHUNK_GAHAIM_INFO::YGRID); v++)
	{
		m_PolarGrid[v]=pChunk->m_PolarGrid[v];
	}

	const char* pAimPoseMem = (const char*)&pChunk->m_numAimPoses;
	const uint32 numAimPoses = *(const uint32*)(pAimPoseMem); 	pAimPoseMem+=sizeof(uint32);
	m_arrAimIKPosesAIM.resize(numAimPoses);
	for (uint32 a=0; a<numAimPoses; a++)
	{
		const uint32 numRot = *(uint32*)(pAimPoseMem); pAimPoseMem+=sizeof(uint32);
		m_arrAimIKPosesAIM[a].m_arrRotation.resize(numRot);
		for (uint32 r=0; r<numRot; r++)
		{
			Quat quat = *(Quat*)(pAimPoseMem); pAimPoseMem+=sizeof(Quat);
			assert(quat.IsValid());
			m_arrAimIKPosesAIM[a].m_arrRotation[r]=quat;
		}
		const uint32 numPos = *(const uint32*)(pAimPoseMem); pAimPoseMem+=sizeof(uint32);
		m_arrAimIKPosesAIM[a].m_arrPosition.resize(numPos);
		for (uint32 p=0; p<numPos; p++)
		{
			Vec3 pos = *(const Vec3*)(pAimPoseMem); pAimPoseMem+=sizeof(Vec3);
			assert(pos.IsValid());
			m_arrAimIKPosesAIM[a].m_arrPosition[p]=pos;
		}
	}
	return true;
}

bool GlobalAnimationHeaderAIM::LoadChunks(IChunkFile *chunkFile, ECAFLoadMode loadMode, int32& lastControllerID, const char* logFilename)
{
	if (!CAF_EnsureLoadModeApplies(chunkFile, loadMode, logFilename))
	{
		return false;
	}

	bool headerLoaded = false;

	int numChunk = chunkFile->NumChunks();
	m_arrController.reserve(numChunk);	

	bool strippedFrames = false;

	for (int i = 0; i < numChunk; ++i)
	{
		IChunkFile::ChunkDesc* const pChunkDesc = chunkFile->GetChunk(i);
		switch (pChunkDesc->chunkType)
		{
		case ChunkType_Controller:
		{
			IController_AutoPtr controller;
			if (CAF_ReadController(pChunkDesc, loadMode, controller, lastControllerID, logFilename, strippedFrames))
			{
				if(controller)
				{
					m_arrController.push_back(controller);
				}
			}
			else
			{
				return false;
			}
			break;
		}
		case ChunkType_MotionParameters:
			if(!ReadMotionParameters(pChunkDesc, loadMode, logFilename))
			{
				return false;
			}
			break;
		case ChunkType_Timing:
			if (!ReadTiming(pChunkDesc, loadMode, logFilename))
			{
				return false;
			}
			break;
		case ChunkType_GlobalAnimationHeaderAIM:
			if (loadMode == eCAFLoadUncompressedOnly)
			{
				break;
			}
			if (!ReadGAH(pChunkDesc))
			{
				return false;
			}
			headerLoaded = true;
			break;
		}

	}


	if (strippedFrames)
	{
		RCLogWarning("Stripping last incomplete frame of animation. Please adjust animation range to be a multiple of the frame duration and re-export animation: %s", logFilename);
	}

	if (loadMode == eCAFLoadCompressedOnly && !headerLoaded)
	{
		RCLogError("Compressed animation doesn't contain expected GAHAIM: %s", logFilename);
		return false;
	}

	return true;
}


bool GlobalAnimationHeaderAIM::SaveToChunkFile(IChunkFile* chunkFile, bool bigEndianOutput)
{
	SEndiannessSwapper swap(bigEndianOutput);

	size_t nSizeOfPoses = 0;
	size_t numAimPoses  = m_arrAimIKPosesAIM.size();
	for (size_t a = 0; a < numAimPoses; ++a)
	{
		nSizeOfPoses += sizeof(uint32); // numRot
		nSizeOfPoses += m_arrAimIKPosesAIM[a].m_arrRotation.size() * sizeof(Quat);
		nSizeOfPoses += sizeof(uint32); // numPos
		nSizeOfPoses += m_arrAimIKPosesAIM[a].m_arrPosition.size() * sizeof(Vec3);
	}
	if (nSizeOfPoses % 4 != 0)
	{
		nSizeOfPoses |= 3;
		nSizeOfPoses += 1;
	}

	size_t nChunkSize = sizeof(CHUNK_GAHAIM_INFO) + nSizeOfPoses;
	CHUNK_GAHAIM_INFO* pGAH_Info = (CHUNK_GAHAIM_INFO*)malloc( nChunkSize ); 
	memset(pGAH_Info, 0, nChunkSize);

	swap(pGAH_Info->m_numAimPoses);
	uint8* parrAimPoses = (uint8*)(&pGAH_Info->m_numAimPoses + 1);
	memset( parrAimPoses, 0x20, nSizeOfPoses );

	pGAH_Info->m_Flags	=	m_nFlags;
	swap(pGAH_Info->m_Flags);

	memset(pGAH_Info->m_FilePath,0,sizeof(pGAH_Info->m_FilePath));
	uint32 num=m_FilePath.size();
	for (int32 s=0; s<num; s++)
	{
		pGAH_Info->m_FilePath[s]=m_FilePath[s];
	}
	pGAH_Info->m_FilePathCRC32		= m_FilePathCRC32;
	swap(pGAH_Info->m_FilePathCRC32);

	pGAH_Info->m_fStartSec				=	m_fStartSec;									//asset-feature: Start time in seconds.
	swap(pGAH_Info->m_fStartSec);
	pGAH_Info->m_fEndSec					=	m_fEndSec;										//asset-feature: End time in seconds.
	swap(pGAH_Info->m_fEndSec);
	pGAH_Info->m_fTotalDuration		=	m_fEndSec-m_fStartSec;	//asset-feature: asset-feature: total duration in seconds.
	swap(pGAH_Info->m_fTotalDuration);

	pGAH_Info->m_AnimTokenCRC32		=	m_AnimTokenCRC32;
	swap(pGAH_Info->m_AnimTokenCRC32);

	pGAH_Info->m_nExist		        =	m_nExist;
	swap(pGAH_Info->m_nExist);

	pGAH_Info->m_MiddleAimPoseRot =	m_MiddleAimPoseRot;
	swap(pGAH_Info->m_MiddleAimPoseRot.w);
	swap(pGAH_Info->m_MiddleAimPoseRot.v.x);
	swap(pGAH_Info->m_MiddleAimPoseRot.v.y);
	swap(pGAH_Info->m_MiddleAimPoseRot.v.z);

	pGAH_Info->m_MiddleAimPose    =	m_MiddleAimPose;
	swap(pGAH_Info->m_MiddleAimPose.w);
	swap(pGAH_Info->m_MiddleAimPose.v.x);
	swap(pGAH_Info->m_MiddleAimPose.v.y);
	swap(pGAH_Info->m_MiddleAimPose.v.z);

	for (uint32 v=0; v<(CHUNK_GAHAIM_INFO::XGRID*CHUNK_GAHAIM_INFO::YGRID); v++)
	{
		pGAH_Info->m_PolarGrid[v]=m_PolarGrid[v];
		swap(pGAH_Info->m_PolarGrid[v].v0);
		swap(pGAH_Info->m_PolarGrid[v].v1);
		swap(pGAH_Info->m_PolarGrid[v].v2);
		swap(pGAH_Info->m_PolarGrid[v].v3);
	}

	pGAH_Info->m_numAimPoses=numAimPoses;
	swap(pGAH_Info->m_numAimPoses);

	for(size_t a=0; a<numAimPoses; ++a)
	{
		uint32 numRot = m_arrAimIKPosesAIM[a].m_arrRotation.size();
		*((uint32*)parrAimPoses)=numRot; 
		swap(*((uint32*)parrAimPoses));		parrAimPoses+=sizeof(uint32);
		for (uint32 r=0; r<numRot; r++)
		{
			*((Quat*)parrAimPoses) = m_arrAimIKPosesAIM[a].m_arrRotation[r]; 
			swap(*((f32*)parrAimPoses));	parrAimPoses+=sizeof(f32);
			swap(*((f32*)parrAimPoses));	parrAimPoses+=sizeof(f32);
			swap(*((f32*)parrAimPoses));	parrAimPoses+=sizeof(f32);
			swap(*((f32*)parrAimPoses));	parrAimPoses+=sizeof(f32);
		}

		uint32 numPos = m_arrAimIKPosesAIM[a].m_arrPosition.size();
		*((uint32*)parrAimPoses)=numPos; 
		swap(*((uint32*)parrAimPoses));		parrAimPoses+=sizeof(uint32);
		for (uint32 p=0; p<numPos; p++)
		{
			*((Vec3*)parrAimPoses) = m_arrAimIKPosesAIM[a].m_arrPosition[p]; 
			swap(*((f32*)parrAimPoses));	parrAimPoses+=sizeof(f32);
			swap(*((f32*)parrAimPoses));	parrAimPoses+=sizeof(f32);
			swap(*((f32*)parrAimPoses));	parrAimPoses+=sizeof(f32);
		}
	}

	CChunkData chunkData;
	chunkData.AddData(pGAH_Info, nChunkSize);

	chunkFile->AddChunk(
		ChunkType_GlobalAnimationHeaderAIM,
		CHUNK_GAHAIM_INFO::VERSION,
		(bigEndianOutput ? eEndianness_Big : eEndianness_Little),
		chunkData.GetData(), chunkData.GetSize());

	free(pGAH_Info);
	return true;
}
