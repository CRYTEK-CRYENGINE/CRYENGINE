// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   CGFLoader.h
//  Version:     v1.00
//  Created:     6/11/2004 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CAFLoader_h__
#define __CAFLoader_h__
#pragma once

#include <Cry3DEngine/CGF/IChunkFile.h>
#include <Cry3DEngine/CGF/CGFContent.h>
#include "../CGA/Controller.h"  // IController_AutoPtr

struct IChunkFile;
class CContentCGF;
struct ChunkDesc;
//////////////////////////////////////////////////////////////////////////

typedef DynArray<int> TrackTimes;
typedef DynArray<CryTCB3Key> TrackTCB3Keys;
typedef DynArray<CryTCBQKey> TrackTCBQKeys;


class IController;

class CInternalSkinningInfo : public _reference_target_t
{
public:
	DynArray<TCBFlags> m_TrackVec3Flags;
	DynArray<TCBFlags> m_TrackQuatFlags;
	DynArray<TrackTCB3Keys*> m_TrackVec3;
	DynArray<TrackTCBQKeys*> m_TrackQuat;
	DynArray<CControllerType> m_arrControllers;
	DynArray<string> m_arrBoneNameTable;	//names of bones
	uint32 m_numChunks;
	QuatT m_StartLocation;
	bool m_bNewFormat;
	bool m_bOldFormat;
	bool m_bTCBFormat;
	DWORD m_dwTimeStamp;

	DynArray<IController_AutoPtr> m_pControllers;
	DynArray<uint32> m_arrControllerId;

	int32 m_nStart; // Start time of the animation, expressed in frames (a constant frame rate of 30fps is assumed).
	int32 m_nEnd;   // End time of the animation, expressed in frames (a constant frame rate of 30fps is assumed).
	f32 m_Speed;
	f32 m_Distance;
	f32 m_Slope;
	int m_nAssetFlags;
	f32 m_LHeelStart,m_LHeelEnd;
	f32 m_LToe0Start,m_LToe0End;
	f32 m_RHeelStart,m_RHeelEnd;
	f32 m_RToe0Start,m_RToe0End;
	Vec3 m_MoveDirection;
	DynArray<uint8> m_FootPlantBits;

	CInternalSkinningInfo()
		: m_bNewFormat(false)
		, m_bOldFormat(false)
		, m_bTCBFormat(false)
		, m_dwTimeStamp(0)
		, m_Speed(-1.0f)
		, m_Distance(-1.0f)
		, m_nAssetFlags(0)
		, m_LHeelStart(-10000.0f)
		, m_LHeelEnd(-10000.0f)
		, m_nStart(1)
		, m_nEnd(1)
		, m_LToe0Start(-10000.0f)
		, m_LToe0End(-10000.0f)
		, m_RHeelStart(-10000.0f)
		, m_RHeelEnd(-10000.0f)
		, m_RToe0Start(-10000.0f)
		, m_RToe0End(-10000.0f)
		, m_Slope(0)
	{
	}
};


#endif //__CGFLoader_h__
