// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   AVI_Reader.h
//  Version:     v1.00
//  Description: AVI files reader.
// -------------------------------------------------------------------------
//  History:
//  Created:     28/02/2007 by MarcoC.
//
////////////////////////////////////////////////////////////////////////////

#ifndef __AVI_Reader_h__
#define __AVI_Reader_h__
#pragma once

#include <CrySystem/File/IAVI_Reader.h>

struct tCaptureAVI_VFW;

//////////////////////////////////////////////////////////////////////////
// AVI file reader.
//////////////////////////////////////////////////////////////////////////
class CAVI_Reader : public IAVI_Reader
{
public:
	CAVI_Reader();
	~CAVI_Reader();

	bool OpenFile(const char* szFilename);
	void CloseFile();

	int  GetWidth();
	int  GetHeight();
	int  GetFPS();

	// if no frame is passed, it will retrieve the current one and advance one frame.
	// If a frame is specified, it will get that one.
	// Notice the "const", don't override this memory!
	const unsigned char* QueryFrame(int nFrame = -1);

	int                  GetFrameCount();
	int                  GetAVIPos();
	void                 SetAVIPos(int nFrame);

private:

	void PrintAVIError(int hr);

	void InitCapture_VFW();
	int  OpenAVI_VFW(const char* filename);

	tCaptureAVI_VFW* m_capture;
};

#endif // __AVI_Reader_h__
