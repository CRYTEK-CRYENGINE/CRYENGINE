// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Config.h"

#if INTERNET_SIMULATOR

	#include "IDatagramSocket.h"
	#include "NetTimer.h"

class CInternetSimulatorSocket : public IDatagramSocket
{
public:
	CInternetSimulatorSocket(IDatagramSocketPtr pChild);
	~CInternetSimulatorSocket();

	// IDatagramSocket
	virtual void         RegisterListener(IDatagramListener* pListener) override;
	virtual void         UnregisterListener(IDatagramListener* pListener) override;
	virtual void         GetSocketAddresses(TNetAddressVec& addrs) override;
	virtual ESocketError Send(const uint8* pBuffer, size_t nLength, const TNetAddress& to) override;
	virtual ESocketError SendVoice(const uint8* pBuffer, size_t nLength, const TNetAddress& to) override;
	virtual void         Die() override;
	virtual bool         IsDead() override;
	virtual CRYSOCKET    GetSysSocket() override;
	virtual void         RegisterBackoffAddress(const TNetAddress& addr) override   { m_pChild->RegisterBackoffAddress(addr); }
	virtual void         UnregisterBackoffAddress(const TNetAddress& addr) override { m_pChild->UnregisterBackoffAddress(addr); }
	// ~IDatagramSocket

	enum EProfile
	{
		PROFILE_PERFECT       = -1,
		PROFILE_FATAL         = -2,
		PROFILE_RANDOM        = -3,

		DEFAULT_PROFILE_GREAT = 0,
		DEFAULT_PROFILE_GOOD,
		DEFAULT_PROFILE_TYPICAL,
		DEFAULT_PROFILE_IFFY,
		DEFAULT_PROFILE_BAD,
		DEFAULT_PROFILE_AWFUL,

		DEFAULT_PROFILE_MAX
	};

	static void LoadXMLProfiles(const char* pFileName);
	static void SetProfile(EProfile profile);

private:

	enum EPayloadType
	{
		ePT_Data,
		ePT_Voice
	};

	struct SPendingSend
	{
		EPayloadType                         eType;
		size_t                               nLength;
		uint8*                               pBuffer;
		TNetAddress                          to;
		_smart_ptr<CInternetSimulatorSocket> pThis;
	};

	struct SProfileEntry
	{
		float fLossMin;
		float fLossMax;
		float fLagMin;
		float fLagMax;
	};

	IDatagramSocketPtr    m_pChild;

	static SProfileEntry* sm_pProfiles;
	static uint32         sm_nProfiles;

	static SProfileEntry  sm_DefaultProfiles[DEFAULT_PROFILE_MAX];

	static void           SimulatorUpdate(NetTimerId, void*, CTimeValue);
};

#endif
