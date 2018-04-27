// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __DEBUGARITH_H__
#define __DEBUGARITH_H__

#include "Config.h"

#if ENABLE_DEBUG_KIT

	#include <vector>
	#include <list>
	#include <memory>
	#include "Context/SessionID.h"
	#include <CryNetwork/INetwork.h>
	#include "DebugOutput.h"

class CSessionID;

// not endian safe
class CEncoding
{
public:
	CEncoding(CDebugOutput*);
	~CEncoding();

	void BeginPacket(TNetChannelID channel, bool isEncoding, uint32 uniqueId);
	void Annotation(const string& annotation);
	void Coding(uint64 nTot, uint32 nLow, uint32 nSym);
	void EndPacket(bool bSent);
	bool IsInUse() const { return m_isInUse; }

	void SetEnabled(bool enable);

	void SetSessionID(const CSessionID& session);

private:
	struct SCoding
	{
		uint64 nTot;
		uint32 nLow;
		uint32 nSym;
	};

	struct SAnnotation
	{
		size_t index;
		string annotation;
	};

	CDebugOutput*            m_pOutput;
	uint32                   m_uniqueId;
	bool                     m_isEncoding;
	bool                     m_isInUse;
	bool                     m_enabled;
	bool                     m_shouldEnable;
	TNetChannelID            m_channel;
	std::vector<SCoding>     m_coding;
	std::vector<SAnnotation> m_annotation;
	CSessionID               m_sessionID;
};

#endif

#endif
