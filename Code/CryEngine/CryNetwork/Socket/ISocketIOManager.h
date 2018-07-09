// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __ISOCKETIOMANAGER_H__
#define __ISOCKETIOMANAGER_H__

#pragma once

#include <CryNetwork/NetAddress.h>
#include <CryMemory/STLGlobalAllocator.h>
#include "Config.h"
#include "SocketError.h"
#include "IDatagramSocket.h"
#include <CryNetwork/CrySocks.h>

// N.B. MAX_USER_MESSAGES_PER_FRAME *MUST* BE A POWER OF 2
#define MAX_USER_MESSAGES_PER_FRAME (4)
#define MAX_USER_MESSAGES_MASK      (MAX_USER_MESSAGES_PER_FRAME - 1)

struct SSocketID
{
	static const uint16 InvalidId = ~uint16(0);

	SSocketID() : id(InvalidId), salt(0) {}
	SSocketID(uint16 i, uint16 s) : id(i), salt(s) {}

	uint16 id;
	uint16 salt;

	ILINE bool operator!() const
	{
		return id == InvalidId;
	}
	typedef uint16 SSocketID::* safe_bool_idiom_type;
	ILINE operator safe_bool_idiom_type() const
	{
		return !!(*this) ? &SSocketID::id : NULL;
	}

	ILINE bool operator!=(const SSocketID& rhs) const
	{
		return !(*this == rhs);
	}
	ILINE bool operator==(const SSocketID& rhs) const
	{
		return id == rhs.id && salt == rhs.salt;
	}
	ILINE bool operator<(const SSocketID& rhs) const
	{
		return id < rhs.id || (id == rhs.id && salt < rhs.salt);
	}
	ILINE bool operator>(const SSocketID& rhs) const
	{
		return id > rhs.id || (id == rhs.id && salt > rhs.salt);
	}

	uint32 AsInt() const
	{
		uint32 x = id;
		x <<= 16;
		x |= salt;
		return x;
	}
	static SSocketID FromInt(uint32 x)
	{
		return SSocketID(x >> 16, x & 0xffffu);
	}

	bool IsLegal() const
	{
		return salt != 0;
	}

	const char* GetText(char* tmpBuf = 0) const
	{
		static char singlebuf[64];
		if (!tmpBuf)
		{
			tmpBuf = singlebuf;
		}
		if (id == InvalidId)
		{
			sprintf(tmpBuf, "<nil>");
		}
		else if (!salt)
		{
			sprintf(tmpBuf, "illegal:%u:%u", id, salt);
		}
		else
		{
			sprintf(tmpBuf, "%u:%u", id, salt);
		}
		return tmpBuf;
	}

	uint32 GetAsUint32() const
	{
		return (uint32(salt) << 16) | id;
	}
};

struct IRecvFromTarget
{
	virtual ~IRecvFromTarget(){}
	virtual void OnRecvFromComplete(const TNetAddress& from, const uint8* pData, uint32 len) = 0;
	virtual void OnRecvFromException(const TNetAddress& from, ESocketError err) = 0;
};

struct ISendToTarget
{
	virtual ~ISendToTarget(){}
	virtual void OnSendToException(const TNetAddress& to, ESocketError err) = 0;
};

struct IConnectTarget
{
	virtual ~IConnectTarget(){}
	virtual void OnConnectComplete() = 0;
	virtual void OnConnectException(ESocketError err) = 0;
};

struct IAcceptTarget
{
	virtual ~IAcceptTarget(){}
	virtual void OnAccept(const TNetAddress& from, CRYSOCKET sock) = 0;
	virtual void OnAcceptException(ESocketError err) = 0;
};

struct IRecvTarget
{
	virtual ~IRecvTarget(){}
	virtual void OnRecvComplete(const uint8* pData, uint32 len) = 0;
	virtual void OnRecvException(ESocketError err) = 0;
};

struct ISendTarget
{
	virtual ~ISendTarget(){}
	virtual void OnSendException(ESocketError err) = 0;
};

enum ESocketIOManagerCaps
{
	eSIOMC_SupportsBackoff = BIT(0),
	eSIOMC_NoBuffering     = BIT(1)
};

struct ISocketIOManager
{
	enum EMessages
	{
		// User messages
		eUM_FIRST = -1, // used to range check user messages

		eUM_QUIT  = eUM_FIRST,
		eUM_WAKE  = -2,
		eUM_SLEEP = -3,

		eUM_LAST  = -3, // used to range check user messages

		// System messages
		eSM_COMPLETEDIO               = 1,
		eSM_COMPLETE_SUCCESS_OK       = 2,
		eSM_COMPLETE_EMPTY_SUCCESS_OK = 3,
		eSM_COMPLETE_FAILURE_OK       = 4,

		// Error messages
		eEM_UNSPECIFIED     = -666,
		eEM_UNBOUND_SOCKET1 = -667,
		eEM_UNBOUND_SOCKET2 = -668,
		eEM_NO_TARGET       = -670,
		eEM_NO_REQUEST      = -671,
		eEM_REQUEST_DEAD    = -680
	};

	const uint32 caps;

	ISocketIOManager(uint32 c) : caps(c) {}
	virtual ~ISocketIOManager() {}

	virtual const char* GetName() = 0;

	virtual bool        PollWait(uint32 waitTime) = 0;
	virtual int         PollWork(bool& performedWork) = 0;

	virtual SSocketID   RegisterSocket(CRYSOCKET sock, int protocol) = 0;
	virtual void        SetRecvFromTarget(SSocketID sockid, IRecvFromTarget* pTarget) = 0;
	virtual void        SetConnectTarget(SSocketID sockid, IConnectTarget* pTarget) = 0;
	virtual void        SetSendToTarget(SSocketID sockid, ISendToTarget* pTarget) = 0;
	virtual void        SetAcceptTarget(SSocketID sockid, IAcceptTarget* pTarget) = 0;
	virtual void        SetRecvTarget(SSocketID sockid, IRecvTarget* pTarget) = 0;
	virtual void        SetSendTarget(SSocketID sockid, ISendTarget* pTarget) = 0;
	virtual void        RegisterBackoffAddressForSocket(const TNetAddress& addr, SSocketID sockid) = 0;
	virtual void        UnregisterBackoffAddressForSocket(const TNetAddress& addr, SSocketID sockid) = 0;
	virtual void        UnregisterSocket(SSocketID sockid) = 0;

	virtual bool        RequestRecvFrom(SSocketID sockid) = 0;
	virtual bool        RequestSendTo(SSocketID sockid, const TNetAddress& addr, const uint8* pData, size_t len) = 0;
	virtual bool        RequestSendVoiceTo(SSocketID sockid, const TNetAddress& addr, const uint8* pData, size_t len) = 0;

	virtual bool        RequestConnect(SSocketID sockid, const TNetAddress& addr) = 0;
	virtual bool        RequestAccept(SSocketID sock) = 0;
	virtual bool        RequestSend(SSocketID sockid, const uint8* pData, size_t len) = 0;
	virtual bool        RequestRecv(SSocketID sockid) = 0;

	virtual void        PushUserMessage(int msg) = 0;

	virtual bool        HasPendingData() = 0;

#if LOCK_NETWORK_FREQUENCY
	virtual void ForceNetworkStart() = 0;
	virtual bool NetworkSleep() = 0;
#endif

	virtual IDatagramSocketPtr CreateDatagramSocket(const TNetAddress& addr, uint32 flags) = 0;
	virtual void               FreeDatagramSocket(IDatagramSocketPtr pSocket) = 0;
};

class CSocketIOManager : public ISocketIOManager
{
public:
	CSocketIOManager(uint32 c) : ISocketIOManager(c)  {}

	virtual IDatagramSocketPtr CreateDatagramSocket(const TNetAddress& addr, uint32 flags) override;
	virtual void               FreeDatagramSocket(IDatagramSocketPtr pSocket) override;

#if NET_MINI_PROFILE || NET_PROFILE_ENABLE
	void RecordPacketSendStatistics(const uint8* pData, size_t len);
#endif

private:
	struct SDatagramSocketData
	{
		TNetAddress        addr;
		IDatagramSocketPtr pSocket;
		uint32             flags;
		uint32             refCount;
	};

	typedef std::vector<SDatagramSocketData, stl::STLGlobalAllocator<SDatagramSocketData>> TDatagramSockets;

	TDatagramSockets m_datagramSockets;
};

bool CreateSocketIOManager(int ncpus, ISocketIOManager** ppExternal, ISocketIOManager** ppInternal);

#endif
