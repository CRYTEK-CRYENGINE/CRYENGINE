// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   CAFSaver.cpp
//  Version:     v1.00
//  Created:     27/9/2007 by Norbert
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "CAFSaver.h"
#include "../CryEngine/Cry3DEngine/CGF/ChunkData.h"


void CSaverCAF::Save(const CContentCGF *const pCGF,const CInternalSkinningInfo *const pSkinningInfo)
{
	assert(pCGF);
	assert(pSkinningInfo);

	SaveExportFlags(pCGF);
	SaveTiming(pSkinningInfo);
	
	for (int i=0; i<pSkinningInfo->m_pControllers.size(); i++)
		SaveController(pSkinningInfo,i);

	SaveBoneNameList(pSkinningInfo);
}

int CSaverCAF::SaveController(const CInternalSkinningInfo *const pSkinningInfo,int ctrlIndex)
{
	const CControllerPQLog* pController  = dynamic_cast<CControllerPQLog*>(pSkinningInfo->m_pControllers[ctrlIndex].get()); // TODO: PQLogS

	assert(pController->m_arrTimes.size() == pController->m_arrKeys.size());

	const size_t numKeys = pController->m_arrTimes.size();
	const uint32 nControllerId = pController->m_nControllerId;

	std::vector<CryKeyPQS> frames(numKeys);
	for (size_t i = 0; i < numKeys; ++i)
	{
		const PQLogS& pqs = pController->m_arrKeys[i];

		frames[i].position = pqs.position;
		frames[i].rotation = !Quat::exp(pqs.rotationLog);
		frames[i].scale = pqs.scale;
	}

	return SaveController(m_pChunkFile, frames, nControllerId);
}

int CSaverCAF::SaveController(CChunkFile *pChunkFile, const std::vector<CryKeyPQS>& frames, const uint32 nControllerId)
{
	const size_t numKeys = frames.size();

	CONTROLLER_CHUNK_DESC_0833 chunk;
	ZeroStruct(chunk);
	chunk.numKeys = numKeys;
	chunk.controllerId = nControllerId;

	CChunkData chunkData;
	chunkData.Add(chunk);
	chunkData.AddData(frames.data(), frames.size() * sizeof(frames[0]));

	return pChunkFile->AddChunk(
		ChunkType_Controller,
		chunk.VERSION,
		eEndianness_Native,
		chunkData.GetData(), chunkData.GetSize());
}

int CSaverCAF::SaveBoneNameList(const CInternalSkinningInfo *const pSkinningInfo)
{
	BONENAMELIST_CHUNK_DESC_0745 hdr;
	ZeroStruct(hdr);

	int numBones = pSkinningInfo->m_arrBoneNameTable.size();
	hdr.numEntities = numBones;
	int dataSize = 0;
	for (int i=0; i<numBones; i++)
	{
		int len = pSkinningInfo->m_arrBoneNameTable[i].length();
		dataSize += (len + 1);
	}
	dataSize++;			// the last string terminated with double 0

	char *pData = new char[dataSize];
	char *p = pData;

	for (int i=0; i<numBones; i++)
	{
		strcpy(p,pSkinningInfo->m_arrBoneNameTable[i].c_str());
		int len = pSkinningInfo->m_arrBoneNameTable[i].length();
		p += (len + 1);
	}
	*p = 0;
	p++;

	CChunkData chunkData;
	chunkData.Add(hdr);
	chunkData.AddData(pData, dataSize);
	delete[] pData;

	return m_pChunkFile->AddChunk(
		ChunkType_BoneNameList,
		BONENAMELIST_CHUNK_DESC_0745::VERSION,
		eEndianness_Native,
		chunkData.GetData(), chunkData.GetSize());
}