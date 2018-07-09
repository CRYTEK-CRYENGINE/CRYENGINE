// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <CryLiveCreate/ILiveCreateHost.h>
#include <CryLiveCreate/ILiveCreateManager.h>

namespace LiveCreate
{

// Null implementation of the new manager
class CNullManagerEx : public IManager
{
public:
	CNullManagerEx() {};
	virtual ~CNullManagerEx() {};
	virtual bool                     IsNullImplementation() const                                                                               { return true; }
	virtual bool                     CanSend() const                                                                                            { return false; }
	virtual bool                     IsEnabled() const                                                                                          { return false; }
	virtual void                     SetEnabled(bool bIsEnabled)                                                                                {};
	virtual bool                     IsLogEnabled() const                                                                                       { return false; }
	virtual void                     SetLogEnabled(bool bIsEnabled)                                                                             {};
	virtual bool                     RegisterListener(IManagerListenerEx* pListener)                                                            { return false; }
	virtual bool                     UnregisterListener(IManagerListenerEx* pListener)                                                          { return false; }
	virtual bool                     FindMachineAddressByName(const char* pTargetMachineName, char* pOutIpAddress, uint32 aMaxOutIpAddressSize) { return false; }
	virtual uint                     GetNumHosts() const                                                                                        { return 0; }
	virtual struct IHostInfo*        GetHost(const uint index) const                                                                            { return NULL; }
	virtual uint                     GetNumPlatforms() const                                                                                    { return false; }
	virtual IPlatformHandlerFactory* GetPlatformFactory(const uint index) const                                                                 { return NULL; }
	virtual bool                     RemoveHost(IHostInfo* pHost)                                                                               { return false; }
	virtual struct IHostInfo* CreateHost(IPlatformHandler* pPlatform, const char* szValidIpAddress)                                      { return NULL; };
	virtual void              RescanForNewHosts()                                                                                        {};
	virtual void              Update()                                                                                                   {};
	virtual bool              SendCommand(const IRemoteCommand& command)                                                                 { return false; }
	virtual void              LogMessage(ELogMessageType aType, const char* pMessage)                                                    {}
};

// Null implementation of LiveCreate host
class CNullHost : public IHost
{
public:
	CNullHost() {};
	virtual ~CNullHost() {};
	virtual bool Initialize(const uint16, const uint16, const uint16) { return false; }
	virtual void Close()                                              {};
	virtual void SuppressCommandExecution()                           {}
	virtual void ResumeCommandExecution()                             {}
	virtual void ExecuteCommands()                                    {};
	virtual void DrawOverlay()                                        {};
};

}
