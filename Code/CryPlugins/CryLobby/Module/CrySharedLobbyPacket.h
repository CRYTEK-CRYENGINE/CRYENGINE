// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CRYSHAREDLOBBYPACKET_H__
#define __CRYSHAREDLOBBYPACKET_H__

#pragma once

#include <CryLobby/CryLobbyPacket.h>
#include "CryMatchMaking.h"

const uint32 CryLobbyPacketErrorSize = CryLobbyPacketUINT16Size;
const uint32 CryLobbyPacketLobbySessionHandleSize = CryLobbyPacketUINT8Size;

class CCrySharedLobbyPacket : public CCryLobbyPacket
{
public:
	void WritePacketHeader()
	{
		CCryLobby* pLobby = (CCryLobby*)CCryLobby::GetLobby();

		WriteUINT8(pLobby->GetLobbyPacketID());

		if (pLobby->GetLobbyServiceType() == eCLS_Online)
		{
			// Online use WriteData since if there is no cross platform WriteData or WriteUINT64 can both be used but WriteData is faster.
			// Also if we want to support cross platform with games for windows live and xbox live WriteData must be used.
			WriteData(&m_packetHeader.fromUID.m_sid, CryLobbyPacketUINT64Size);
		}
		else
		{
			// LAN always needs to use WriteUINT64 to keep cross platform compatibility.
			WriteUINT64(m_packetHeader.fromUID.m_sid);
		}

		WriteUINT16(m_packetHeader.fromUID.m_uid);
		WriteBool(m_packetHeader.reliable);

		if (m_packetHeader.reliable)
		{
			WriteUINT8(m_packetHeader.counterOut);
			WriteUINT8(m_packetHeader.counterIn);
		}
	}

	void ReadPacketHeader()
	{
		ReadUINT8();

		if (CCryLobby::GetLobby()->GetLobbyServiceType() == eCLS_Online)
		{
			// Online use ReadData since if there is no cross platform ReadData or ReadUINT64 can both be used but ReadData is faster.
			// Also if we want to support cross platform with games for windows live and xbox live ReadData must be used.
			ReadData(&m_packetHeader.fromUID.m_sid, CryLobbyPacketUINT64Size);
		}
		else
		{
			// LAN always needs to use ReadUINT64 to keep cross platform compatibility.
			m_packetHeader.fromUID.m_sid = ReadUINT64();
		}

		m_packetHeader.fromUID.m_uid = ReadUINT16();
		m_packetHeader.reliable = ReadBool();

		if (m_packetHeader.reliable)
		{
			m_packetHeader.counterOut = ReadUINT8();
			m_packetHeader.counterIn = ReadUINT8();
		}
	}

	void WriteDataHeader()
	{
		WriteUINT8(m_dataHeader.lobbyPacketType);

		if (m_packetHeader.reliable)
		{
			WriteUINT16(m_dataHeader.dataSize);
		}
	}

	void ReadDataHeader()
	{
		m_dataHeader.lobbyPacketType = ReadUINT8();

		if (m_packetHeader.reliable)
		{
			m_dataHeader.dataSize = ReadUINT16();
		}
	}

	void WriteError(ECryLobbyError data)
	{
		WriteUINT16((uint16)data);
	}

	ECryLobbyError ReadError()
	{
		return (ECryLobbyError)ReadUINT16();
	}

	void WriteLobbySessionHandle(CryLobbySessionHandle h)
	{
		WriteUINT8(h.GetID());
	}

	CryLobbySessionHandle ReadLobbySessionHandle()
	{
		return CryLobbySessionHandle(ReadUINT8());
	}

	void SetFromSessionSID(uint64 sid)
	{
		m_packetHeader.fromUID.m_sid = sid;
	}

	uint64 GetFromSessionSID()
	{
		return m_packetHeader.fromUID.m_sid;
	}

	void SetFromConnectionUID(SCryMatchMakingConnectionUID uid)
	{
		m_packetHeader.fromUID = uid;
	}

	SCryMatchMakingConnectionUID GetFromConnectionUID()
	{
		return m_packetHeader.fromUID;
	}

	SCryLobbyPacketHeader*     GetLobbyPacketHeader()     { return &m_packetHeader; }
	SCryLobbyPacketDataHeader* GetLobbyPacketDataHeader() { return &m_dataHeader; }

	void                       ResetBuffer()
	{
		m_allocated = false;
		m_pWriteBuffer = NULL;
	}
};

#endif // __CRYSHAREDLOBBYPACKET_H__
