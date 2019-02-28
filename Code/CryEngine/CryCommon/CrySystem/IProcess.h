// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
//	File:IProcess.h
//	Description: Process common interface
//
//	History:
//	-September 03,2001:Created by Marco Corbetta
//
//////////////////////////////////////////////////////////////////////

#ifndef IPROCESS_H
#define IPROCESS_H

#if _MSC_VER > 1000
	#pragma once
#endif

// forward declaration
struct    SRenderingPassInfo;
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
struct IProcess
{
	// <interfuscator:shuffle>
	virtual ~IProcess(){}
	virtual bool Init() = 0;
	virtual void Update() = 0;
	virtual void RenderWorld(const int nRenderFlags, const SRenderingPassInfo& passInfo, const char* szDebugName) = 0;
	virtual void ShutDown() = 0;
	virtual void SetFlags(int flags) = 0;
	virtual int  GetFlags(void) = 0;
	// </interfuscator:shuffle>
};

#endif
