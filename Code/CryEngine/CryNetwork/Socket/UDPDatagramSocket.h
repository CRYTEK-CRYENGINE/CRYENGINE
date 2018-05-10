// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include "IDatagramSocket.h"

bool MakeSocketNonBlocking(CRYSOCKET sock);

#if SHOW_FRAGMENTATION_USAGE || ENABLE_UDP_PACKET_FRAGMENTATION
	#define FRAG_MAX_MTU_SIZE (1000)
#endif//SHOW_FRAGMENTATION_USAGE || ENABLE_UDP_PACKET_FRAGMENTATION

#if ENABLE_UDP_PACKET_FRAGMENTATION

	#define FRAG_NUM_PACKET_BUFFERS (16)
	#define FRAG_MAX_FRAGMENTS      16                    /* Must be power of 2 (at least 8 and can't be in brackets because of how TFragSeqStorage is created below) */

	#if FRAG_MAX_FRAGMENTS <= 16
		#define FRAG_SEQ_BIT_SIZE            (4)
		#define FRAG_SEQ_TRANSPORT_TYPE_SIZE 8              /* Can't be in brackets because of how TFragSeqTransport is created below */
	#elif FRAG_MAX_FRAGMENTS <= 64                        // Strictly speaking, could have up to 256 fragments here, but there's no 'uint256' intrinsic to use as a reconstitution mask
		#define FRAG_SEQ_BIT_SIZE            (8)
		#define FRAG_SEQ_TRANSPORT_TYPE_SIZE 16 /* Can't be in brackets because of how TFragSeqTransport is created below */
	#else                                     // #elif FRAG_MAX_FRAGMENTS <= 64
		#error FRAG_MAX_FRAGMENTS is invalid (must be 64 or less)
	#endif // #if FRAG_MAX_FRAGMENTS <= 16

	#define FRAG_SEQ_TRANSPORT_SIZE CRYNETWORK_CONCAT(uint, FRAG_SEQ_TRANSPORT_TYPE_SIZE)
typedef FRAG_SEQ_TRANSPORT_SIZE TFragSeqTransport;      // Used for sending/receiving fragment sequence info (as indices)
	#define FRAG_SEQ_STORAGE_SIZE   CRYNETWORK_CONCAT(uint, FRAG_MAX_FRAGMENTS)
typedef FRAG_SEQ_STORAGE_SIZE   TFragSeqStorage;        // Used for storing fragment sequence info (as bitmasks)
typedef uint16                  TFragPacketId;

struct SFragInfoTransport
{
	TFragSeqTransport m_FragmentIndex: FRAG_SEQ_BIT_SIZE;                // bit mask
	TFragSeqTransport m_TotalFragments: FRAG_SEQ_BIT_SIZE;
};

enum EFragHeaderOffsets
{
	fho_HeaderID       = 0,
	fho_PacketID,
	fho_FragInfo       = fho_PacketID + sizeof(TFragPacketId),
	#if UDP_PACKET_FRAGMENTATION_CRC_CHECK
	fho_Checksum       = fho_FragInfo + sizeof(SFragInfoTransport),
	fho_FragHeaderSize = fho_Checksum + sizeof(uint)
	#else
	fho_FragHeaderSize = fho_FragInfo + sizeof(SFragInfoTransport)
	#endif // UDP_PACKET_FRAGMENTATION_CRC_CHECK
};

struct SFragmentedPacket
{
	TNetAddress     m_from;
	CTimeValue      m_lastUpdate;
	uint            m_sequence_num;
	int16           m_Length;
	TFragSeqStorage m_ReconstitutionMask;
	TFragPacketId   m_Id;
	bool            m_inUse;
	uint8           m_FragPackets[MAX_UDP_PACKET_SIZE];
};

#endif//ENABLE_UDP_PACKET_FRAGMENTATION

class CUDPDatagramSocket : public CDatagramSocket, private IRecvFromTarget, ISendToTarget
{
public:
	CUDPDatagramSocket();
	~CUDPDatagramSocket();

	bool Init(SIPv4Addr addr, uint32 flags);

	// IDatagramSocket
	virtual void         GetSocketAddresses(TNetAddressVec& addrs) override;
	virtual ESocketError Send(const uint8* pBuffer, size_t nLength, const TNetAddress& to) override;
	virtual ESocketError SendVoice(const uint8* pBuffer, size_t nLength, const TNetAddress& to) override;
	virtual void         Die() override;
	virtual bool         IsDead() override;
	virtual CRYSOCKET    GetSysSocket() override;
	virtual void         RegisterBackoffAddress(const TNetAddress& addr) override;
	virtual void         UnregisterBackoffAddress(const TNetAddress& addr) override;
	// ~IDatagramSocket

private:
	CRYSOCKET         m_socket;
	int32             m_protocol;
	SSocketID         m_sockid;
	bool              m_bIsIP4;
	TNetAddress       m_lastError;
	ISocketIOManager* m_pSockIO;

#if SHOW_FRAGMENTATION_USAGE_VERBOSE
	int m_fragged;
	int m_unfragged;
#endif//SHOW_FRAGMENTATION_USAGE_VERBOSE
#if ENABLE_UDP_PACKET_FRAGMENTATION
	uint8*             m_pUDPFragBuffer;
	TFragPacketId      m_RollingIndex;
	SFragmentedPacket* m_pFragmentedPackets;
	float              m_fragmentLossRateAccumulator;
	int                m_packet_sequence_num;
#endif//ENABLE_UDP_PACKET_FRAGMENTATION

	bool Init(int af, uint32 flags, void* pSockAddr, size_t sizeSockAddr);
	bool InitWinError();
	void LogWinError(int err);
	void LogWinError();

	void Cleanup();

	void CloseSocket();

	// IRecvFromTarget
	virtual void OnRecvFromComplete(const TNetAddress& from, const uint8* pData, uint32 len) override;
	virtual void OnRecvFromException(const TNetAddress& from, ESocketError err) override;
	// ~IRecvFromTarget

	// ISendToTarget
	virtual void OnSendToException(const TNetAddress& from, ESocketError err) override;
	// ~ISendToTarget

	void         SocketSend(const uint8* pBuffer, size_t nLength, const TNetAddress& to);
#if ENABLE_UDP_PACKET_FRAGMENTATION
	void         ClearFragmentationEntry(uint32 index, TFragPacketId id, const TNetAddress& from);
	void         DiscardStaleEntries();
	void         SendFragmented(const uint8* pBuffer, size_t nLength, const TNetAddress& to);
	const uint8* ReceiveFragmented(const TNetAddress& from, const uint8* pData, uint32& len);
	uint8        FindBufferIndex(const TNetAddress& from, TFragPacketId id);
#endif//ENABLE_UDP_PACKET_FRAGMENTATION
};
