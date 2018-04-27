// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include "ISocketIOManager.h"

class CSocketIOManagerNull : public ISocketIOManager
{
public:
	CSocketIOManagerNull() : ISocketIOManager(0) {}

	virtual bool        PollWait(uint32 waitTime) override                                                                     { return false; }
	virtual int         PollWork(bool& performedWork) override                                                                 { performedWork = false; return eSM_COMPLETEDIO; }
	virtual const char* GetName() override                                                                                     { return "null"; }

	virtual SSocketID   RegisterSocket(CRYSOCKET sock, int protocol) override                                                  { return SSocketID(1, 2); }
	virtual void        SetRecvFromTarget(SSocketID sockid, IRecvFromTarget* pTarget) override                                 {}
	virtual void        SetConnectTarget(SSocketID sockid, IConnectTarget* pTarget) override                                   {}
	virtual void        SetSendToTarget(SSocketID sockid, ISendToTarget* pTarget) override                                     {}
	virtual void        SetAcceptTarget(SSocketID sockid, IAcceptTarget* pTarget) override                                     {}
	virtual void        SetRecvTarget(SSocketID sockid, IRecvTarget* pTarget) override                                         {}
	virtual void        SetSendTarget(SSocketID sockid, ISendTarget* pTarget) override                                         {}
	virtual void        RegisterBackoffAddressForSocket(const TNetAddress& addr, SSocketID sockid) override                    {}
	virtual void        UnregisterBackoffAddressForSocket(const TNetAddress& addr, SSocketID sockid) override                  {}
	virtual void        UnregisterSocket(SSocketID sockid) override                                                            {}

	virtual bool        RequestRecvFrom(SSocketID sockid) override                                                             { return false; }
	virtual bool        RequestSendTo(SSocketID sockid, const TNetAddress& addr, const uint8* pData, size_t len) override      { return false; }
	virtual bool        RequestSendVoiceTo(SSocketID sockid, const TNetAddress& addr, const uint8* pData, size_t len) override { return false; }

	virtual bool        RequestConnect(SSocketID sockid, const TNetAddress& addr) override                                     { return false; }
	virtual bool        RequestAccept(SSocketID sock) override                                                                 { return false; }
	virtual bool        RequestSend(SSocketID sockid, const uint8* pData, size_t len) override                                 { return false; }
	virtual bool        RequestRecv(SSocketID sockid) override                                                                 { return false; }

	virtual void        PushUserMessage(int msg) override                                                                      {}

	virtual bool        HasPendingData() override                                                                              { return false; }

#if LOCK_NETWORK_FREQUENCY
	virtual void ForceNetworkStart() override {}
	virtual bool NetworkSleep() override      { return true; }
#endif

	virtual IDatagramSocketPtr CreateDatagramSocket(const TNetAddress& addr, uint32 flags) override { return NULL; }
	virtual void               FreeDatagramSocket(IDatagramSocketPtr pSocket) override              {}
};
