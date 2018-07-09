// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   IAVI_Reader.h
//  Version:     v1.00
//  Description: AVI files reader Interface.
// -------------------------------------------------------------------------
//  History:
//  Created:     28/02/2007 by MarcoC.
//
////////////////////////////////////////////////////////////////////////////

#ifndef __IAVI_Reader_h__
#define __IAVI_Reader_h__
#pragma once

//////////////////////////////////////////////////////////////////////////
struct IAVI_Reader
{
	virtual ~IAVI_Reader(){}
	virtual bool OpenFile(const char* szFilename) = 0;
	virtual void CloseFile() = 0;

	virtual int  GetWidth() = 0;
	virtual int  GetHeight() = 0;
	virtual int  GetFPS() = 0;

	//! If no frame is passed, it will retrieve the current one and advance one frame.
	//! If a frame is specified, it will get that one.
	//! Notice the "const" - don't override this memory!.
	virtual const unsigned char* QueryFrame(int nFrame = -1) = 0;

	virtual int                  GetFrameCount() = 0;
	virtual int                  GetAVIPos() = 0;
	virtual void                 SetAVIPos(int nFrame) = 0;
};

#endif // __IAVI_Reader_h__
