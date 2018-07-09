// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   AnimSaver.h
//  Version:     v1.00
//  Created:     27/9/2007 by Norbert
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __AnimSaver_h__
#define __AnimSaver_h__
#pragma once

#include <CryString/CryPath.h>
#include <CryCore/CryTypeInfo.h>
#include "../CryEngine/Cry3DEngine/CGF/ChunkFile.h"
#include <Cry3DEngine/CGF/CGFContent.h>
#include "../CGA/Controller.h"
#include "../CGA/ControllerPQ.h"
#include "../CGA/ControllerPQLog.h"
#include "LoaderCAF.h"


class CSaverAnim
{
public:
	CSaverAnim(const char* filename,CChunkFile& chunkFile);
	virtual void Save(const CContentCGF *pCGF,const CInternalSkinningInfo *pSkinningInfo) = 0;

	static int SaveTCB3Track(CChunkFile *pChunkFile, const CInternalSkinningInfo *pSkinningInfo, int trackIndex);
	static int SaveTCBQTrack(CChunkFile *pChunkFile, const CInternalSkinningInfo *pSkinningInfo, int trackIndex);
	static int SaveTiming(CChunkFile *pChunkFile, int startSample, int endSample);

protected:
	int SaveExportFlags(const CContentCGF *pCGF);
	int SaveTiming(const CInternalSkinningInfo *pSkinningInfo);

	string m_filename;
	CChunkFile* m_pChunkFile;
};

#endif //__AnimSaver_h__