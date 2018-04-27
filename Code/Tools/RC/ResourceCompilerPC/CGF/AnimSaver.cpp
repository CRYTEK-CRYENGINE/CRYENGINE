// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   AnimSaver.cpp
//  Version:     v1.00
//  Created:     27/9/2007 by Norbert
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "AnimSaver.h"
#include "../CryEngine/Cry3DEngine/CGF/ChunkData.h"
#include "StringHelpers.h"


CSaverAnim::CSaverAnim(const char *filename, CChunkFile &chunkFile)
{
	m_filename = filename;
	m_pChunkFile = &chunkFile;
}

int CSaverAnim::SaveExportFlags(const CContentCGF *const pCGF)
{
	EXPORT_FLAGS_CHUNK_DESC chunk;
	ZeroStruct(chunk);

	const CExportInfoCGF *pExpInfo = pCGF->GetExportInfo();
	if (pExpInfo->bMergeAllNodes)
		chunk.flags |= EXPORT_FLAGS_CHUNK_DESC::MERGE_ALL_NODES;
	if (pExpInfo->bUseCustomNormals)
		chunk.flags |= EXPORT_FLAGS_CHUNK_DESC::USE_CUSTOM_NORMALS;
	if (pExpInfo->bWantF32Vertices)
		chunk.flags |= EXPORT_FLAGS_CHUNK_DESC::WANT_F32_VERTICES;
	if (pExpInfo->b8WeightsPerVertex)
		chunk.flags |= EXPORT_FLAGS_CHUNK_DESC::EIGHT_WEIGHTS_PER_VERTEX;
	if (pExpInfo->bMakeVCloth)
		chunk.flags |= EXPORT_FLAGS_CHUNK_DESC::MAKE_VCLOTH;

	if (pExpInfo->bFromColladaXSI)
		chunk.assetAuthorTool |= EXPORT_FLAGS_CHUNK_DESC::FROM_COLLADA_XSI;
	if (pExpInfo->bFromColladaMAX)
		chunk.assetAuthorTool |= EXPORT_FLAGS_CHUNK_DESC::FROM_COLLADA_MAX;
	if (pExpInfo->bFromColladaMAYA)
		chunk.assetAuthorTool |= EXPORT_FLAGS_CHUNK_DESC::FROM_COLLADA_MAYA;

	enum {NUM_RC_VERSION_ELEMENTS = sizeof(pExpInfo->rc_version) / sizeof(pExpInfo->rc_version[0])};
	std::copy(pExpInfo->rc_version, pExpInfo->rc_version + NUM_RC_VERSION_ELEMENTS, chunk.rc_version);
	StringHelpers::SafeCopy(chunk.rc_version_string, sizeof(chunk.rc_version_string), pExpInfo->rc_version_string);

	return m_pChunkFile->AddChunk(
		ChunkType_ExportFlags,
		EXPORT_FLAGS_CHUNK_DESC::VERSION,
		eEndianness_Native,
		&chunk, sizeof(chunk));
}

int CSaverAnim::SaveTiming(const CInternalSkinningInfo *const pSkinningInfo)
{
	return SaveTiming(m_pChunkFile, pSkinningInfo->m_nStart, pSkinningInfo->m_nEnd);
}

int CSaverAnim::SaveTCB3Track(CChunkFile *pChunkFile, const CInternalSkinningInfo *const pSkinningInfo, int trackIndex)
{
	CONTROLLER_CHUNK_DESC_0826 chunk;
	ZeroStruct(chunk);

	chunk.type = CTRL_TCB3;
	chunk.nKeys = pSkinningInfo->m_TrackVec3[trackIndex]->size();

	CChunkData chunkData;
	chunkData.Add(chunk);
	chunkData.AddData(&(*pSkinningInfo->m_TrackVec3[trackIndex])[0],chunk.nKeys*sizeof(CryTCB3Key));

	return pChunkFile->AddChunk(
		ChunkType_Controller,
		CONTROLLER_CHUNK_DESC_0826::VERSION,
		eEndianness_Native,
		chunkData.GetData(), chunkData.GetSize());
}

int CSaverAnim::SaveTCBQTrack(CChunkFile *pChunkFile, const CInternalSkinningInfo *const pSkinningInfo, int trackIndex)
{
	CONTROLLER_CHUNK_DESC_0826 chunk;
	ZeroStruct(chunk);

	chunk.type = CTRL_TCBQ;
	chunk.nKeys = pSkinningInfo->m_TrackQuat[trackIndex]->size();

	CChunkData chunkData;
	chunkData.Add(chunk);
	chunkData.AddData(&(*pSkinningInfo->m_TrackQuat[trackIndex])[0],chunk.nKeys*sizeof(CryTCBQKey));

	return pChunkFile->AddChunk(
		ChunkType_Controller,
		CONTROLLER_CHUNK_DESC_0826::VERSION,
		eEndianness_Native,
		chunkData.GetData(), chunkData.GetSize());
}

int CSaverAnim::SaveTiming(CChunkFile *pChunkFile, int startSample, int endSample)
{
	TIMING_CHUNK_DESC_0919 chunk;
	ZeroStruct(chunk);

	chunk.numberOfSamples = endSample - startSample + 1;
	chunk.samplesPerSecond = float(TICKS_PER_SECOND) / TICKS_PER_FRAME;
	chunk.startTimeInSeconds = startSample / chunk.samplesPerSecond;

	return pChunkFile->AddChunk(
		ChunkType_Timing,
		chunk.VERSION,
		eEndianness_Native,
		&chunk, sizeof(chunk));
}
