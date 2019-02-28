// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:
   Provides interface for TimeDemo (CryAction)
   Can attach listener to perform game specific stuff on Record/Play
   -------------------------------------------------------------------------
   History:
   - May 06, 2015:	Created by Pascal Kross

*************************************************************************/

#ifndef __ITIMEDEMORECORDER_H__
#define __ITIMEDEMORECORDER_H__

#pragma once

struct STimeDemoFrameRecord
{
	Vec3 playerPosition;
	Quat playerRotation;
	Quat playerViewRotation;
	Vec3 hmdPositionOffset;
	Quat hmdViewRotation;
};

struct ITimeDemoListener
{
	virtual ~ITimeDemoListener(){}
	virtual void OnRecord(bool bEnable) = 0;
	virtual void OnPlayback(bool bEnable) = 0;
	virtual void OnPlayFrame() = 0;
};

struct ITimeDemoRecorder
{
	virtual ~ITimeDemoRecorder(){}

	virtual bool IsRecording() const = 0;
	virtual bool IsPlaying() const = 0;

	virtual void RegisterListener(ITimeDemoListener* pListener) = 0;
	virtual void UnregisterListener(ITimeDemoListener* pListener) = 0;
	virtual void GetCurrentFrameRecord(STimeDemoFrameRecord& record) const = 0;
};

#endif //__ITIMEDEMORECORDER_H__
