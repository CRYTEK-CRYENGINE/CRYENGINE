// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id: GameRealtimeRemoveUpdate.h,v 1.1 23/09/2009 Johnmichael Quinlan
Description:  This is the header file for the game module specific Realtime remote update. 
							The purpose of this module is to allow data update to happen remotely inside 
							the game layer so that you can, for example, edit the terrain and see 
							the changes in the console.
-------------------------------------------------------------------------
History:
- 23/09/2009   10:45: Created by Johnmichael Quinlan
*************************************************************************/

#ifndef GameRealtimeRemoteUpdate_h__
#define GameRealtimeRemoteUpdate_h__

#pragma once

#include <CryLiveCreate/IRealtimeRemoteUpdate.h>

class CGameRealtimeRemoteUpdateListener : public IRealtimeUpdateGameHandler
{
protected:
	CGameRealtimeRemoteUpdateListener();
	virtual ~CGameRealtimeRemoteUpdateListener();

public:
	static CGameRealtimeRemoteUpdateListener& GetGameRealtimeRemoteUpdateListener();
	bool Enable(bool boEnable);

	bool Update();
protected:
	
	virtual bool UpdateGameData(XmlNodeRef oXmlNode, unsigned char * auchBinaryData);
	virtual void UpdateCamera(XmlNodeRef oXmlNode);
	virtual void CloseLevel();
	virtual void CameraSync();
	virtual void EndCameraSync();
private:
	bool m_bCameraSync;
	int  m_nPreviousFlyMode;
	Vec3 m_Position;
	Vec3 m_ViewDirection;
	Vec3 m_headPositionDelta;


	XmlNodeRef	m_CameraUpdateInfo;
};

#endif //GameRealtimeRemoteUpdate_h__

