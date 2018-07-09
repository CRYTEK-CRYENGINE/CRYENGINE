// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   ANMSaver.cpp
//  Version:     v1.00
//  Created:     27/9/2007 by Norbert
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ANMSaver.h"
#include "../CryEngine/Cry3DEngine/CGF/ChunkData.h"



void CSaverANM::Save(const CContentCGF *const pCGF,const CInternalSkinningInfo *const pSkinningInfo)
{
	assert(pCGF);
	assert(pSkinningInfo);

	SaveExportFlags(pCGF);
	SaveTiming(pSkinningInfo);
	
	for (int i = 0; i < pCGF->GetNodeCount(); ++i)
	{
		int positionChunkId = -1;
		int rotationChunkId = -1;
		int scaleChunkId = -1;

		if (pSkinningInfo->m_arrControllers[i * 3 + 0].m_controllertype == 0x55)
		{
			positionChunkId = SaveTCB3Track(pSkinningInfo, pSkinningInfo->m_arrControllers[i * 3 + 0].m_index);
		}

		if (pSkinningInfo->m_arrControllers[i * 3 + 1].m_controllertype == 0xaa)
		{
			rotationChunkId = SaveTCBQTrack(pSkinningInfo, pSkinningInfo->m_arrControllers[i * 3 + 1].m_index);
		}

		if (pSkinningInfo->m_arrControllers[i * 3 + 2].m_controllertype == 0x55)
		{
			scaleChunkId = SaveTCB3Track(pSkinningInfo, pSkinningInfo->m_arrControllers[i * 3 + 2].m_index);
		}

		if (positionChunkId != -1 || rotationChunkId != -1 || scaleChunkId != -1)
		{
			SaveNode(pCGF->GetNode(i), positionChunkId, rotationChunkId, scaleChunkId);
		}
	}
}

#define SCALE_TO_CGF 100.0f

int CSaverANM::SaveNode(const CNodeCGF *const pNode,int pos_cont_id,int rot_cont_id,int scl_cont_id)
{
	NODE_CHUNK_DESC_0824 chunk;
	ZeroStruct(chunk);

	cry_strcpy(chunk.name, pNode->name);

	chunk.ObjectID = -1;
	chunk.ParentID = -1;
	chunk.nChildren = 0;
	chunk.MatID = -1;

	float *pMat = (float*)&chunk.tm;
	pMat[0] = pNode->localTM(0,0);
	pMat[1] = pNode->localTM(1,0);
	pMat[2] = pNode->localTM(2,0);
	pMat[4] = pNode->localTM(0,1);
	pMat[5] = pNode->localTM(1,1);
	pMat[6] = pNode->localTM(2,1);
	pMat[8] = pNode->localTM(0,2);
	pMat[9] = pNode->localTM(1,2);
	pMat[10]= pNode->localTM(2,2);
	pMat[12] = pNode->localTM.GetTranslation().x * SCALE_TO_CGF;
	pMat[13] = pNode->localTM.GetTranslation().y * SCALE_TO_CGF;
	pMat[14] = pNode->localTM.GetTranslation().z * SCALE_TO_CGF;
	
	chunk.pos_cont_id = pos_cont_id;
	chunk.rot_cont_id = rot_cont_id;
	chunk.scl_cont_id = scl_cont_id;

	chunk.PropStrLen = 0;

	CChunkData chunkData;
	chunkData.Add(chunk);

	return m_pChunkFile->AddChunk(
		ChunkType_Node,
		NODE_CHUNK_DESC_0824::VERSION,
		eEndianness_Native,
		chunkData.GetData(), chunkData.GetSize());
}

int CSaverANM::SaveTCB3Track(const CInternalSkinningInfo *const pSkinningInfo,int trackIndex)
{
	return CSaverAnim::SaveTCB3Track(m_pChunkFile, pSkinningInfo, trackIndex);
}

int CSaverANM::SaveTCBQTrack(const CInternalSkinningInfo *const pSkinningInfo,int trackIndex)
{
	return CSaverAnim::SaveTCBQTrack(m_pChunkFile, pSkinningInfo, trackIndex);
}