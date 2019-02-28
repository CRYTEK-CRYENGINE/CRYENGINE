// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   ANMSaver.h
//  Version:     v1.00
//  Created:     27/9/2007 by Norbert
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __ANMSaver_h__
#define __ANMSaver_h__
#pragma once

#include "AnimSaver.h"

class CSaverANM : public CSaverAnim
{
public:
	CSaverANM(const char* filename,CChunkFile& chunkFile) : CSaverAnim(filename,chunkFile) {}
	virtual void Save(const CContentCGF *pCGF,const CInternalSkinningInfo *pSkinningInfo) override;

private:
	int SaveNode(const CNodeCGF *pNode,int pos_cont_id,int rot_cont_id,int scl_cont_id);
	int SaveTCB3Track(const CInternalSkinningInfo *pSkinningInfo,int trackIndex);
	int SaveTCBQTrack(const CInternalSkinningInfo *pSkinningInfo,int trackIndex);
};

#endif //__ANMSaver_h__