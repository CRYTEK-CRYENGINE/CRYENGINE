// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
//	File:SmartGlassManager.h
//
//	History:
//	-Jan 16,2013:Originally Created by Steve Barnett
//
//////////////////////////////////////////////////////////////////////

#ifndef _SMARTGLASSMANAGER_H
#define _SMARTGLASSMANAGER_H

#if _MSC_VER > 1000
	#pragma once
#endif

#if defined(SUPPORT_SMARTGLASS)

	#include "ISmartGlassManager.h"

using Windows::Xbox::SmartGlass::SmartGlassDeviceWatcher;
using Windows::Xbox::SmartGlass::SmartGlassDevice;
using Windows::Xbox::SmartGlass::SmartGlassDirectSurface;
using Windows::Xbox::SmartGlass::QosMetricsChangedEventArgs;

struct CSmartGlassPlayerData
{
	string                                      userId;
	Platform::String^                           deviceId;
	class ISmartGlassContext*                   ctx;
	bool                                        listenerAllocationRequired; // Signal that a listener has not yet been allocated (so constructor and destructor can be called on MT)
	Windows::Foundation::EventRegistrationToken qosChangedCookie;
};

ref class CSmartGlassManagerListenerWrapper;

class CSmartGlassManager : public ISmartGlassManager
{
public:
	CSmartGlassManager();
	virtual ~CSmartGlassManager();

	void         OnDeviceAdded(SmartGlassDeviceWatcher^ sender, SmartGlassDevice^ device);
	void         OnDeviceRemoved(SmartGlassDeviceWatcher^ sender, SmartGlassDevice^ device);
	void         OnQosChanged(SmartGlassDevice^ device, QosMetricsChangedEventArgs^ args);

	virtual void Update();

	virtual void SetFlashPlayer(string userId, struct IFlashPlayer* pFlashPlayer);

	virtual void SetInputListenerFactory(SmartGlassInputListenerFactory* pfnFactory);

private:
	void Lock()   { m_playerInfoLock.Lock(); }
	void Unlock() { m_playerInfoLock.Unlock(); }

	SmartGlassDeviceWatcher^                    m_pWatcher;         // Ref counted
	CSmartGlassManagerListenerWrapper^          m_pListenerWrapper; // Ref counted
	Windows::Foundation::EventRegistrationToken m_deviceAddedCookie;
	Windows::Foundation::EventRegistrationToken m_deviceRemovedCookie;

	SmartGlassInputListenerFactory*             m_pfnInputListenerFactory; // Not owned

	static const int                            MAX_PLAYERS = 4;
	static int          s_nextContextId;

	CryCriticalSection  m_playerInfoLock;
	typedef std::vector<CSmartGlassPlayerData> SmartGlassPlayerVec;
	SmartGlassPlayerVec m_playerInfo;
};

#endif

#endif
