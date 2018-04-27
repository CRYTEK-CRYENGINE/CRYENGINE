// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "RecordingSystemPackets.h"
#include "RecordingSystemCompressor.h"
#include "RecordingBuffer.h"
#include "AdaptiveCompressor.h"

class CRecordingSystemCompressor::CompressionSerializer
{
public:
	static ILINE bool IsWrite() { return true; }
	template <class T>
	static ILINE void Serialise(CAdaptiveCompressor& compressor, void * __restrict pInput, uint32 stride, uint32 numInputs)
	{
		compressor.Compress<T>(pInput, stride, numInputs);
	}
	template <class T>
	static ILINE T SerialiseValue(CAdaptiveCompressor& compressor, T in)
	{
		compressor.m_stream.WriteVariableLengthValue((int)in);
		return in;
	}
	static ILINE void SerialiseBool(CAdaptiveCompressor& compressor, bool &in)
	{
		compressor.m_stream.WriteBit(in);
	}
};

class CRecordingSystemCompressor::DecompressionSerializer
{
public:
	static ILINE bool IsWrite() { return false; }
	template <class T, class S>
	static ILINE void Serialise(CAdaptiveCompressor& decompressor, S * __restrict pInput, uint32 stride, uint32 numInputs)
	{
		decompressor.Decompress<T>(pInput, stride, numInputs);
	}
	template <class T>
	static ILINE T SerialiseValue(CAdaptiveCompressor& decompressor, T in)
	{
		return (T)decompressor.m_stream.ReadVariableLengthValue();
	}
	static ILINE void SerialiseBool(CAdaptiveCompressor& decompressor, bool &in)
	{
		in=decompressor.m_stream.ReadBit()?true:false;
	}
};

template <class T>
SRecording_Packet* CRecordingSystemCompressor::SerialisePackets(CAdaptiveCompressor &serializer, SRecording_Packet *cur, char *outputBufferEnd)
{
	const float k_QuaternionQuantisation=800.0f;		// Gives approximately 0.16 degree accuracy
	const float k_PositionQuantisation=1.0f/0.025f;	// 1.25cm accuracy on positions
	const float k_FrameTimeQuantisation=120.0f;			// Approximately 4ms accuracy
	const float k_AngleQuantisation=180.0f/gf_PI;		// Half a degree accuracy
	const float k_UnityQuantisation=1.0f;						// Round to nearest integer
	const float k_KillPositionQuantisation=500.0f;  // 1mm accuracy
	while (true)
	{
		int packetType=T::SerialiseValue(serializer, cur->type);
		int packetCount=T::SerialiseValue(serializer, cur->size);
		if (T::IsWrite()) // drop packet when reading
		{
			cur++;
		}
		if (packetCount==0)
		{
			break;
		}
		assert(packetType&0x80);
		if (!T::IsWrite() && outputBufferEnd)
		{
			// Check that we're within the limits of our output buffer
			// Can happen even if no single compress overflowed because multiple compressions are concatenated together when received over the network
			int packetSize=0;
			switch (packetType&0x7F)
			{
			case eFPP_FPChar: packetSize=sizeof(SRecording_FPChar); break;
			case eFPP_Flashed: packetSize=sizeof(SRecording_Flashed); break;
			case eFPP_VictimPosition: packetSize=sizeof(SRecording_VictimPosition); break;
			case eFPP_KillHitPosition: packetSize=sizeof(SRecording_KillHitPosition); break;
			case eFPP_BattleChatter: packetSize=sizeof(SRecording_BattleChatter); break;
			case eFPP_RenderNearest: packetSize=sizeof(SRecording_RenderNearest); break;
			case eFPP_PlayerHealthEffect: packetSize=sizeof(SRecording_PlayerHealthEffect); break;
			case eFPP_PlaybackTimeOffset: packetSize=sizeof(SRecording_PlaybackTimeOffset); break;
			default:
				CryFatalError("Can't compress this type of packet %d %d\n", cur->type, packetType&0x7F);
				break;
			}
			if ((char*)cur+packetSize*packetCount>outputBufferEnd)
			{
#ifdef _RELEASE
				CryLogAlways("WARNING: Output buffer isn't big enough! Abandoning decompress so we don't overrun our buffer!");
#else
				CryFatalError("WARNING: Output buffer isn't big enough! Abandoning decompress so we don't overrun our buffer!");
#endif
				serializer.m_stream.m_out=serializer.m_stream.m_end; // So Decompress won't think there's another block still to decompress
				break;
			}
		}
		switch (packetType&0x7F)
		{
		case eFPP_FPChar:
			{
				SRecording_FPChar *fp=(SRecording_FPChar*)cur;
				if (!T::IsWrite()) new(fp) SRecording_FPChar[packetCount];
				assert(fp->type==eFPP_FPChar);
				serializer.ResetDictionary(k_QuaternionQuantisation);
				T::template Serialise<CAdaptiveCompressor::QuantisedFloatDelta>(serializer, &fp->relativePosition.q.v.x, sizeof(*fp), packetCount);
				T::template Serialise<CAdaptiveCompressor::QuantisedFloatDelta>(serializer, &fp->relativePosition.q.v.y, sizeof(*fp), packetCount);
				T::template Serialise<CAdaptiveCompressor::QuantisedFloatDelta>(serializer, &fp->relativePosition.q.v.z, sizeof(*fp), packetCount);
				T::template Serialise<CAdaptiveCompressor::QuantisedFloatDelta>(serializer, &fp->relativePosition.q.w, sizeof(*fp), packetCount);
				T::template Serialise<CAdaptiveCompressor::QuantisedFloatDelta>(serializer, &fp->camlocation.q.v.x, sizeof(*fp), packetCount);
				T::template Serialise<CAdaptiveCompressor::QuantisedFloatDelta>(serializer, &fp->camlocation.q.v.y, sizeof(*fp), packetCount);
				T::template Serialise<CAdaptiveCompressor::QuantisedFloatDelta>(serializer, &fp->camlocation.q.v.z, sizeof(*fp), packetCount);
				T::template Serialise<CAdaptiveCompressor::QuantisedFloatDelta>(serializer, &fp->camlocation.q.w, sizeof(*fp), packetCount);
				if (!T::IsWrite())
				{
					for (int i=0; i<packetCount; i++)
					{
						fp[i].relativePosition.q.Normalize();
						fp[i].camlocation.q.Normalize();
					}
				}
				serializer.ResetDictionary(k_PositionQuantisation);
				T::template Serialise<CAdaptiveCompressor::QuantisedFloatDelta>(serializer, &fp->relativePosition.t.x, sizeof(*fp), packetCount);
				T::template Serialise<CAdaptiveCompressor::QuantisedFloatDelta>(serializer, &fp->relativePosition.t.y, sizeof(*fp), packetCount);
				T::template Serialise<CAdaptiveCompressor::QuantisedFloatDelta>(serializer, &fp->relativePosition.t.z, sizeof(*fp), packetCount);
				T::template Serialise<CAdaptiveCompressor::QuantisedFloatDelta>(serializer, &fp->camlocation.t.x, sizeof(*fp), packetCount);
				T::template Serialise<CAdaptiveCompressor::QuantisedFloatDelta>(serializer, &fp->camlocation.t.y, sizeof(*fp), packetCount);
				T::template Serialise<CAdaptiveCompressor::QuantisedFloatDelta>(serializer, &fp->camlocation.t.z, sizeof(*fp), packetCount);
				serializer.ResetDictionary(k_AngleQuantisation);
				T::template Serialise<CAdaptiveCompressor::QuantisedFloatDelta>(serializer, &fp->fov, sizeof(*fp), packetCount);
				serializer.ResetDictionary(k_FrameTimeQuantisation);
				T::template Serialise<CAdaptiveCompressor::QuantisedFloatDelta>(serializer, &fp->frametime, sizeof(*fp), packetCount);
				serializer.ResetDictionary(k_UnityQuantisation);
				T::template Serialise<CAdaptiveCompressor::ByteDelta>(serializer, &fp->playerFlags, sizeof(*fp), packetCount);
				break;
			}
		case eFPP_Flashed:
			{
				SRecording_Flashed *fl=(SRecording_Flashed*)cur;
				if (!T::IsWrite()) new(fl) SRecording_Flashed[packetCount];
				serializer.ResetDictionary(k_FrameTimeQuantisation);
				T::template Serialise<CAdaptiveCompressor::QuantisedFloatDelta>(serializer, &fl->duration, sizeof(*fl), packetCount);
				T::template Serialise<CAdaptiveCompressor::QuantisedFloatDelta>(serializer, &fl->blindAmount, sizeof(*fl), packetCount);
				T::template Serialise<CAdaptiveCompressor::QuantisedFloatDelta>(serializer, &fl->frametime, sizeof(*fl), packetCount);
				break;
			}
		case eFPP_VictimPosition:
			{
				SRecording_VictimPosition *vp=(SRecording_VictimPosition*)cur;
				if (!T::IsWrite()) new(vp) SRecording_VictimPosition[packetCount];
				serializer.ResetDictionary(k_PositionQuantisation);
				T::template Serialise<CAdaptiveCompressor::QuantisedFloatDelta>(serializer, &vp->victimPosition.x, sizeof(*vp), packetCount);
				T::template Serialise<CAdaptiveCompressor::QuantisedFloatDelta>(serializer, &vp->victimPosition.y, sizeof(*vp), packetCount);
				T::template Serialise<CAdaptiveCompressor::QuantisedFloatDelta>(serializer, &vp->victimPosition.z, sizeof(*vp), packetCount);
				serializer.ResetDictionary(k_FrameTimeQuantisation);
				T::template Serialise<CAdaptiveCompressor::QuantisedFloatDelta>(serializer, &vp->frametime, sizeof(*vp), packetCount);
				break;
			}
		case eFPP_KillHitPosition:
			{
				SRecording_KillHitPosition *kp=(SRecording_KillHitPosition*)cur;
				if (!T::IsWrite()) new(kp) SRecording_KillHitPosition[packetCount];
				serializer.ResetDictionary(k_KillPositionQuantisation);
				T::template Serialise<CAdaptiveCompressor::QuantisedFloatDelta>(serializer, &kp->hitRelativePos.x, sizeof(*kp), packetCount);
				T::template Serialise<CAdaptiveCompressor::QuantisedFloatDelta>(serializer, &kp->hitRelativePos.y, sizeof(*kp), packetCount);
				T::template Serialise<CAdaptiveCompressor::QuantisedFloatDelta>(serializer, &kp->hitRelativePos.z, sizeof(*kp), packetCount);
				serializer.ResetDictionary(k_FrameTimeQuantisation);
				T::template Serialise<CAdaptiveCompressor::QuantisedFloatDelta>(serializer, &kp->fRemoteKillTime, sizeof(*kp), packetCount);
				break;
			}
		case eFPP_BattleChatter:
			{
				SRecording_BattleChatter *bc=(SRecording_BattleChatter*)cur;
				if (!T::IsWrite()) new(bc) SRecording_BattleChatter[packetCount];
				serializer.ResetDictionary(k_FrameTimeQuantisation);
				T::template Serialise<CAdaptiveCompressor::QuantisedFloatDelta>(serializer, &bc->frametime, sizeof(*bc), packetCount);
				serializer.ResetDictionary(k_UnityQuantisation);
				T::template Serialise<CAdaptiveCompressor::UInt16Delta>(serializer, &bc->entityNetId, sizeof(*bc), packetCount);
				serializer.ResetDictionary(k_UnityQuantisation);
				T::template Serialise<CAdaptiveCompressor::ByteDelta>(serializer, &bc->chatterType, sizeof(*bc), packetCount);
				serializer.ResetDictionary(k_UnityQuantisation);
				T::template Serialise<CAdaptiveCompressor::ByteDelta>(serializer, &bc->chatterVariation, sizeof(*bc), packetCount);
				break;
			}
		case eFPP_RenderNearest:
			{
				SRecording_RenderNearest *rn=(SRecording_RenderNearest*)cur;
				if (!T::IsWrite()) new(rn) SRecording_RenderNearest[packetCount];
				serializer.ResetDictionary(k_FrameTimeQuantisation);
				T::template Serialise<CAdaptiveCompressor::QuantisedFloatDelta>(serializer, &rn->frametime, sizeof(*rn), packetCount);
				for (int i=0; i<packetCount; i++)
					T::SerialiseBool(serializer, rn[i].renderNearest);
				break;
			}
		case eFPP_PlayerHealthEffect:
			{
				SRecording_PlayerHealthEffect *phe=(SRecording_PlayerHealthEffect*)cur;
				if (!T::IsWrite()) new(phe) SRecording_PlayerHealthEffect[packetCount];
				serializer.ResetDictionary(k_FrameTimeQuantisation); // Not all frametimes but good enough level for each component
				T::template Serialise<CAdaptiveCompressor::QuantisedFloatDelta>(serializer, &phe->hitDirection.x, sizeof(*phe), packetCount);
				T::template Serialise<CAdaptiveCompressor::QuantisedFloatDelta>(serializer, &phe->hitDirection.y, sizeof(*phe), packetCount);
				T::template Serialise<CAdaptiveCompressor::QuantisedFloatDelta>(serializer, &phe->hitDirection.z, sizeof(*phe), packetCount);
				T::template Serialise<CAdaptiveCompressor::QuantisedFloatDelta>(serializer, &phe->hitStrength, sizeof(*phe), packetCount);
				T::template Serialise<CAdaptiveCompressor::QuantisedFloatDelta>(serializer, &phe->hitSpeed, sizeof(*phe), packetCount);
				T::template Serialise<CAdaptiveCompressor::QuantisedFloatDelta>(serializer, &phe->frametime, sizeof(*phe), packetCount);
				if (!T::IsWrite())
				{
					for (int i=0; i<packetCount; i++)
					{
						phe[i].hitDirection.Normalize();
					}
				}
				break;
			}
		case eFPP_PlaybackTimeOffset:
			{
				SRecording_PlaybackTimeOffset *tof=(SRecording_PlaybackTimeOffset*)cur;
				if (!T::IsWrite()) new(tof) SRecording_PlaybackTimeOffset[packetCount];
				serializer.ResetDictionary(k_FrameTimeQuantisation);
				T::template Serialise<CAdaptiveCompressor::QuantisedFloatDelta>(serializer, &tof->timeOffset, sizeof(*tof), packetCount);
				break;
			}
		default:
			CryFatalError("Can't compress this type of packet %d %d\n", cur->type, packetType&0x7F);
			break;
		}
		cur=(SRecording_Packet*)((char*)cur+cur->size*packetCount);
	}

	return cur;
}

SRecording_Packet* CRecordingSystemCompressor::SortPackets(SRecording_Packet *dstBuffer, CRecordingBuffer *srcBuffer, uint8 *pSrcAuxBuffer, uint32 srcAuxBufferSize, SRecording_VictimPosition* startVictimPosition, SRecording_VictimPosition* endVictimPosition, EntityId killHitVictimID)
{
	uint32 total[eFPP_Max]={0};
	size_t offset=0;
	
	int sortOrder[eFPP_Max]={eFPP_FPChar, eFPP_VictimPosition, eFPP_KillHitPosition};
	int sortOrderPos=0;
	sortOrder[sortOrderPos++]=eFPP_FPChar;
	sortOrder[sortOrderPos++]=eFPP_VictimPosition;
	sortOrder[sortOrderPos++]=eFPP_KillHitPosition;
	for (int i=0; i<eFPP_Max; i++)
	{
		if (i!=eFPP_FPChar && i!=eFPP_VictimPosition && i!=eFPP_KillHitPosition)
			sortOrder[sortOrderPos++]=i;
	}
	assert(sortOrderPos==eFPP_Max);

	if (srcBuffer)
	{
		while (offset < srcBuffer->size())
		{
			SRecording_Packet *pPacket = (SRecording_Packet*)srcBuffer->at(offset);
			total[pPacket->type]++;
			offset += pPacket->size;
		}
		assert(total[eFPP_VictimPosition]==0);
		total[eFPP_VictimPosition]=(uint32)(endVictimPosition-startVictimPosition);

		if (killHitVictimID!=0 && total[eFPP_KillHitPosition]>0)
			total[eFPP_KillHitPosition]=1; // Will only send one kill position
		else
			total[eFPP_KillHitPosition]=0;
	}
	else if (pSrcAuxBuffer && srcAuxBufferSize)
	{
		while (offset < srcAuxBufferSize)
		{
			SRecording_Packet *pPacket = (SRecording_Packet*)(&pSrcAuxBuffer[offset]);
			total[pPacket->type]++;
			offset += pPacket->size;
		}
	}

	for (int idx=0; idx<CRY_ARRAY_COUNT(total); idx++)
	{
		int i=sortOrder[idx];
		if (total[i])
		{
			// Add marker into stream to indicate type and number
			dstBuffer->type=i|0x80;
			dstBuffer->size=total[i];
			dstBuffer++;
			// Add packets
			switch (i)
			{
			case eFPP_VictimPosition:
				if (srcBuffer)
				{
					size_t victimPositionSize=(endVictimPosition-startVictimPosition)*sizeof(*startVictimPosition);
					memcpy(dstBuffer, startVictimPosition, victimPositionSize);
					dstBuffer=(SRecording_Packet*)((char*)dstBuffer+victimPositionSize);
					break;
				}
				// no break
			case eFPP_KillHitPosition:
				if (srcBuffer)
				{
					offset=0;
					while (offset < srcBuffer->size())
					{
						SRecording_Packet *pPacket = (SRecording_Packet*)srcBuffer->at(offset);
						if (pPacket->type==eFPP_KillHitPosition && killHitVictimID==((SRecording_KillHitPosition*)pPacket)->victimId)
						{
							memcpy(dstBuffer, pPacket, pPacket->size);
							dstBuffer=(SRecording_Packet*)((char*)dstBuffer+pPacket->size);
							break;
						}
						offset += pPacket->size;
					}
					break;
				}
				// no break
			default:
				offset=0;
				if (srcBuffer)
				{
					while (offset < srcBuffer->size())
					{
						SRecording_Packet *pPacket = (SRecording_Packet*)srcBuffer->at(offset);
						if (pPacket->type==i)
						{
							memcpy(dstBuffer, pPacket, pPacket->size);
							dstBuffer=(SRecording_Packet*)((char*)dstBuffer+pPacket->size);
						}
						offset += pPacket->size;
					}
				}
				else if (pSrcAuxBuffer)
				{
					while (offset < srcAuxBufferSize)
					{
						SRecording_Packet *pPacket = (SRecording_Packet*)(&pSrcAuxBuffer[offset]);
						if (pPacket->type==i)
						{
							memcpy(dstBuffer, pPacket, pPacket->size);
							dstBuffer=(SRecording_Packet*)((char*)dstBuffer+pPacket->size);
						}
						offset += pPacket->size;
					}
				}
				break;
			}
		}
	}
	// End marker
	dstBuffer->type=0;
	dstBuffer->size=0;
	dstBuffer++;
	return dstBuffer;
}

struct SPacketSortObject
{
	float GetFrameTime(SRecording_Packet *p)
	{
		switch (p->type)
		{
		case eFPP_KillHitPosition:
			return 0.0f;
		case eFPP_PlaybackTimeOffset:
			return 0.001f; // Seperate out from kill hit position although there should only be one of each
		case eRBPT_FrameData:
			return ((SRecording_FrameData*)p)->frametime;
		case eFPP_Flashed:
			return ((SRecording_Flashed*)p)->frametime;
		case eFPP_BattleChatter:
			return ((SRecording_BattleChatter*)p)->frametime;
		case eFPP_RenderNearest:
			return ((SRecording_RenderNearest*)p)->frametime;
		case eFPP_PlayerHealthEffect:
			return ((SRecording_PlayerHealthEffect*)p)->frametime;
		default:
			CryFatalError("Don't know how to reorder packet type %d\n", p->type);
		}
		return 0.0f;
	}
	bool operator() (SRecording_Packet* a, SRecording_Packet *b)
	{
		float ta=GetFrameTime(a);
		float tb=GetFrameTime(b);
		// Sort on type is here for safety so don't get random bugs if there is a dependency on packet type order
		return ta<tb || (ta==tb && a->type<b->type);
	}
};

SRecording_Packet* CRecordingSystemCompressor::MovePacketsToBuffer(EFirstPersonPacket type, SRecording_Packet *dstBuffer, SRecording_Packet *srcBuffer, SRecording_Packet *endOfSrc)
{
	SRecording_Packet *pStart=NULL;
	while (srcBuffer<endOfSrc)
	{
		if (!pStart && srcBuffer->type==type)
		{
			pStart=srcBuffer;
		}
		else if (pStart && srcBuffer->type!=type)
		{
			size_t size=(char*)srcBuffer-(char*)pStart;
			memcpy(dstBuffer, pStart, size);
			dstBuffer=(SRecording_Packet*)((char*)dstBuffer+size);
			pStart=NULL;
		}
		srcBuffer=(SRecording_Packet*)((char*)srcBuffer+srcBuffer->size);
	}
	if (pStart)
	{
		size_t size=(char*)endOfSrc-(char*)pStart;
		memcpy(dstBuffer, pStart, size);
		dstBuffer=(SRecording_Packet*)((char*)dstBuffer+size);
	}
	return dstBuffer;
}

void CRecordingSystemCompressor::InterleavePackets(SRecording_Packet *dstBuffer, SRecording_Packet *srcBuffer, SRecording_Packet *endOfSrc)
{
	SRecording_Packet *srcBufferBase=srcBuffer;
	
	// Move FPChar and VictimPositions to start of buffer (needed as streams can be concatenated together)
	dstBuffer=MovePacketsToBuffer(eFPP_FPChar, dstBuffer, srcBuffer, endOfSrc);
	dstBuffer=MovePacketsToBuffer(eFPP_VictimPosition, dstBuffer, srcBuffer, endOfSrc);

	// Rest of the packets needed re-ordering back into time order
	std::vector<SRecording_Packet*> packetsForOrdering;
	while (srcBuffer<endOfSrc)
	{
		if (srcBuffer->type!=eFPP_FPChar && srcBuffer->type!=eFPP_VictimPosition)
		{
			packetsForOrdering.push_back(srcBuffer);
		}
		srcBuffer=(SRecording_Packet*)((char*)srcBuffer+srcBuffer->size);
	}
	std::sort(packetsForOrdering.begin(), packetsForOrdering.end(), SPacketSortObject());
	for (std::vector<SRecording_Packet*>::iterator it=packetsForOrdering.begin(), end=packetsForOrdering.end(); it!=end; ++it)
	{
		SRecording_Packet *p=*it;
		memcpy(dstBuffer, p, p->size);
		dstBuffer=(SRecording_Packet*)((char*)dstBuffer+p->size);
	}
}

size_t CRecordingSystemCompressor::Compress(CRecordingBuffer *srcBuffer, uint8* outBuffer, uint32 maxOutputSize, SRecording_VictimPosition* startVictimPosition, SRecording_VictimPosition* endVictimPosition, EntityId killHitVictimID)
{
	int inputSize=srcBuffer->size()+(endVictimPosition-startVictimPosition)*sizeof(SRecording_VictimPosition);
	uint8 *sortedPacketBuffer=new uint8[inputSize+(eFPP_Max+1)*sizeof(SRecording_Packet)];
	SRecording_Packet *sortedPackets=(SRecording_Packet*)sortedPacketBuffer;

	SRecording_Packet *pEndPacket=SortPackets(sortedPackets, srcBuffer, NULL, 0, startVictimPosition, endVictimPosition, killHitVictimID);
	if ((char*)pEndPacket>(char*)sortedPackets+maxOutputSize)
		CryLogAlways("Input size (%" PRISIZE_T ") exceeds output buffer size (%d). Will be truncated when decompressed!\n", (char*)pEndPacket-(char*)sortedPackets, maxOutputSize);

	CAdaptiveCompressor compressor(outBuffer, maxOutputSize, 8192, 8192, true);
	SerialisePackets<CompressionSerializer>(compressor, sortedPackets, NULL);

	delete[] sortedPackets;

	size_t compressedSize=(uint8*)compressor.m_stream.GetEnd()-outBuffer;
#ifndef _RELEASE
	CryLogAlways("KillCam Compressed Data %" PRISIZE_T "/%d (%.1f%%)\n", compressedSize, inputSize, 100.0f*compressedSize/(float)inputSize);
#endif
	return compressedSize;
}

size_t CRecordingSystemCompressor::CompressRaw(uint8 *inBuffer, uint32 inBufferSize, uint8* outBuffer, uint32 maxOutputSize)
{
	uint8 *sortedPacketBuffer=new uint8[inBufferSize+(eFPP_Max+1)*sizeof(SRecording_Packet)];
	SRecording_Packet *sortedPackets=(SRecording_Packet*)sortedPacketBuffer;

	SRecording_Packet *pEndPacket=SortPackets(sortedPackets, NULL, inBuffer, inBufferSize, NULL, NULL, 0);
	if ((char*)pEndPacket>(char*)sortedPackets+maxOutputSize)
		CryLogAlways("Input size (%" PRISIZE_T ") exceeds output buffer size (%d). Will be truncated when decompressed!\n", (char*)pEndPacket-(char*)sortedPackets, maxOutputSize);

	CAdaptiveCompressor compressor(outBuffer, maxOutputSize, 8192, 8192, true);
	SerialisePackets<CompressionSerializer>(compressor, sortedPackets, NULL);

	delete[] sortedPackets;

	size_t compressedSize=(uint8*)compressor.m_stream.GetEnd()-outBuffer;
#ifndef _RELEASE
	CryLogAlways("KillCam Compressed Data %" PRISIZE_T "/%d (%.1f%%)\n", compressedSize, inBufferSize, 100.0f*compressedSize/(float)inBufferSize);
#endif
	return compressedSize;
}

size_t CRecordingSystemCompressor::Decompress(uint8 *compressedInput, size_t compressedSize, void *outputBuffer, size_t maxOutputSize, size_t streamAlignment)
{
	uint8* sortedPacketBuffer=new uint8[maxOutputSize];
	SRecording_Packet *sortedPackets=(SRecording_Packet*)sortedPacketBuffer;
	
	CAdaptiveCompressor decompressor(compressedInput, compressedSize, 8192, 0, false);
	SRecording_Packet *end=sortedPackets;
	while (decompressor.m_stream.m_out<decompressor.m_stream.m_end)
	{
		end=SerialisePackets<DecompressionSerializer>(decompressor, end, (char*)sortedPackets+maxOutputSize);
		if (streamAlignment!=0)
		{
			if (decompressor.m_stream.m_mask!=1)
			{
				decompressor.m_stream.m_out++;
				decompressor.m_stream.m_mask=1;
			}
			UINT_PTR unaligned=((UINT_PTR)(decompressor.m_stream.m_out-compressedInput))%streamAlignment;
			if (unaligned>0)
			{
				decompressor.m_stream.m_out+=streamAlignment-unaligned;
			}
		}
		CryLog("Decompressed chunk: %p\n", decompressor.m_stream.m_out);
	}

#ifndef _RELEASE
	if ((size_t)((char*)end-(char*)sortedPackets)>maxOutputSize)
		CryFatalError("Decompressed data size (%" PRISIZE_T ") exceeds output buffer size (%" PRISIZE_T "). This should not be possible due to truncation in SerialisePackets\n", ((char*)end-(char*)sortedPackets), maxOutputSize);
#endif

	InterleavePackets((SRecording_Packet*)outputBuffer, sortedPackets, end);
	size_t result = (char*)end-(char*)sortedPackets;
	delete[] sortedPackets;

	return result;
}
