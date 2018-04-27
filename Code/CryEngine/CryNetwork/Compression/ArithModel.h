// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:  declaration of CryNetwork IArithModel
   -------------------------------------------------------------------------
   History:
   - 26/07/2004   10:34 : Created by Craig Tiller
*************************************************************************/
#ifndef ARITHMODEL_H
#define ARITHMODEL_H

#include "Config.h"

#include <CryNetwork/INetwork.h>

#if USE_ARITHSTREAM
	#include "ArithAlphabet.h"
	#include "Streams/CommStream.h"
	#include "Utils.h"
	#include "StringTable.h"
#endif

enum ETimeStream
{
	eTS_Network = 0,
	eTS_NetworkPing,
	eTS_NetworkPong,
	eTS_PongElapsed,
	eTS_Physics,
	eTS_RemoteTime,
	// must be last
	NUM_TIME_STREAMS
};

class CCheckForNullEntityEncoding
{
public:
	CCheckForNullEntityEncoding() { m_check++; }
	~CCheckForNullEntityEncoding() { m_check--; }

	static void BreakpointHereToCheck()
	{
	}

	static void EncodedNull()
	{
		if (m_check)
			BreakpointHereToCheck();
	}

private:
	static int m_check;
};

class CNoNullEntityIDSendable : public INetSendable
{
public:
	CNoNullEntityIDSendable(INetSendablePtr pSendable) : INetSendable(pSendable->GetFlags(), pSendable->GetReliability()), m_pBase(pSendable)
	{
		++g_objcnt.noNullEntID;
	}

	~CNoNullEntityIDSendable()
	{
		--g_objcnt.noNullEntID;
	}

#if ENABLE_PACKET_PREDICTION
	virtual SMessageTag GetMessageTag(INetSender* pSender, IMessageMapper* mapper)
	{
		return m_pBase->GetMessageTag(pSender, mapper);
	}
#endif

	virtual size_t             GetSize() { return sizeof(*this) + m_pBase->GetSize(); }
	virtual EMessageSendResult Send(INetSender* pSender)
	{
		CCheckForNullEntityEncoding check;
		return m_pBase->Send(pSender);
	}
	virtual void UpdateState(uint32 nFromSeq, ENetSendableStateUpdate update)
	{
		m_pBase->UpdateState(nFromSeq, update);
	}
	virtual const char* GetDescription()
	{
		if (m_name.empty())
			m_name = string("No-null-entid ") + m_pBase->GetDescription();
		return m_name.c_str();
	}
	virtual void GetPositionInfo(SMessagePositionInfo& pos)
	{
		m_pBase->GetPositionInfo(pos);
	}

private:
	INetSendablePtr m_pBase;
	string          m_name;
};

inline INetSendablePtr CheckNullEntityWrites(INetSendablePtr pSendable)
{
	return new CNoNullEntityIDSendable(pSendable);
}

#if USE_ARITHSTREAM

class CNetContextState;

// implementation of IArithModel for the network engine
class CArithModel
{
private:
	struct STimeAdaption
	{
		STimeAdaption()
		{
			timeFraction32 = 0;
		}

		CTimeValue startTime;
		CTimeValue lastTime;

		struct STimeDelta
		{
			STimeDelta() { Reset(); }
			STimeDelta(const STimeDelta& other) : rate(other.rate), error(other.error) {}
			STimeDelta& operator=(const STimeDelta& other) { rate = other.rate; error = other.error; return *this; }
			uint32 rate;
			uint32 error;

			uint16 Left() const
			{
				if (rate > error)
					return rate - error;
				else
					return 0;
			}
			uint16 Right() const
			{
				uint32 sum = uint32(error) + rate;
				if (sum > 65535)
					return 65535;
				else
					return uint16(sum);
			}
			void Update(uint16 encodedDelta, bool hit);
			void Reset();
		};
		STimeDelta startDelta;
		STimeDelta frameDelta;

		uint32     timeFraction32;
		bool       isInFrame;
	};

public:
	CArithModel();
	~CArithModel();
	CArithModel(const CArithModel&);
	CArithModel&      operator=(const CArithModel&);

	size_t            GetSize();

	void              SetTimeFraction(uint32 timeFraction32);
	uint32            GetTimeFraction() const;

	void              SetNetContextState(CNetContextState* pContext) { m_pNetContext = pContext; }
	CNetContextState* GetNetContextState()                           { return m_pNetContext; }

	void              WriteString(CCommOutputStream&, const string& szString);
	void              WriteChar(CCommOutputStream&, char c);
	bool              ReadString(CCommInputStream&, string& szString);
	char              ReadChar(CCommInputStream&);
	float             EstimateSizeOfString(const string& szString) const;
	float             EstimateSizeOfChar(char c) const;

	ILINE void        TableWriteString(CCommOutputStream& out, const string& szString)
	{
		m_stringTable.WriteString(out, szString, this);
	}
	ILINE void TableReadString(CCommInputStream& in, string& szString)
	{
		m_stringTable.ReadString(in, szString, this);
	}

	bool         IsTimeInFrame(ETimeStream time) const;
	int64        WriteGetTimeDelta(ETimeStream time, const CTimeValue& value, STimeAdaption::STimeDelta** pOutDeltaInfo = 0);
	CTimeValue   ReadTimeWithDelta(CCommInputStream& stm, ETimeStream time, int64 delta);

	void         WriteTime(CCommOutputStream&, ETimeStream stream, CTimeValue timeValue);
	CTimeValue   ReadTime(CCommInputStream&, ETimeStream stream);
	float        EstimateSizeOfTime(ETimeStream stream, CTimeValue timeValue) const;

	void         WriteNetId(CCommOutputStream&, SNetObjectID id);
	SNetObjectID ReadNetId(CCommInputStream&);

	void         GetMemoryStatistics(ICrySizer* pSizer, bool countingThis = false);
	void         RecalculateProbabilities();

	void         SimplifyMemoryUsing(const CArithModel& mdl)
	{
		m_alphabet = mdl.m_alphabet;
		if (m_entityIDAlphabet.GetNumSymbols() == mdl.m_entityIDAlphabet.GetNumSymbols())
			m_entityIDAlphabet = mdl.m_entityIDAlphabet;
	}

	#if DEBUG_ENDPOINT_LOGIC
	void DumpCountsToFile(FILE* f) const { m_alphabet.DumpCountsToFile(f); }
	#endif
private:
	CNetContextState* m_pNetContext;

	typedef CArithLargeAlphabetOrder0<> TAlphabet;
	TAlphabet                   m_alphabet;
	CArithLargeAlphabetOrder0<> m_entityIDAlphabet;

	CStringTable                m_stringTable;

	// stores parameters for the time stream adaption

	uint32        m_timeFraction32;
	STimeAdaption m_vTimeAdaption[NUM_TIME_STREAMS];
};

#endif

#endif
