// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   CAFSaver.h
//  Version:     v1.00
//  Created:     27/9/2007 by Norbert
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CAFSaver_h__
#define __CAFSaver_h__
#pragma once

#include "AnimSaver.h"

class CSaverCAF : public CSaverAnim
{
public:
	CSaverCAF(const char* filename,CChunkFile& chunkFile) : CSaverAnim(filename,chunkFile) {}
	virtual void Save(const CContentCGF *pCGF,const CInternalSkinningInfo *pSkinningInfo) override;

	static int SaveController(CChunkFile *pChunkFile, const std::vector<CryKeyPQS>& frames, const uint32 nControllerId);

private:
	int SaveController(const CInternalSkinningInfo *pSkinningInfo,int ctrlIndex);
	int SaveBoneNameList(const CInternalSkinningInfo *pSkinningInfo);
};

#endif //__CAFSaver_h__