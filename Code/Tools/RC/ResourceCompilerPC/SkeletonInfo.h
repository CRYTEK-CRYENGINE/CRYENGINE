// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2006.
// -------------------------------------------------------------------------
//  File name:   AnimationInfoLoader.h
//  Version:     v1.00
//  Created:     22/6/2006 by Alexey Medvedev.
//  Compilers:   Visual Studio.NET 2005
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef _SKELETON_INFO
#define _SKELETON_INFO
#pragma once

#include <Cry3DEngine/CGF/CGFContent.h>

class CSkeletonInfo
{
public:
	CSkeletonInfo();
	~CSkeletonInfo();

	bool LoadFromChr(const char* name);
	bool LoadFromCga(const char* name);

public:
	CSkinningInfo m_SkinningInfo;
};

#endif
