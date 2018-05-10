// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __RECORDINGSYSTEMCOMPRESSOR_H__
#define __RECORDINGSYSTEMCOMPRESSOR_H__

struct SRecording_Packet;
struct SRecording_VictimPosition;
class CAdaptiveCompressor;
class CRecordingBuffer;

class CRecordingSystemCompressor
{
public:
	// Compress: Compresses first person kill cam packets
	//  srcBuffer:           CRecordingBuffer containing the first person kill cam packets to be compressed
	//  outBuffer:           Output buffer for compressed data
	//  maxOutputSize:       Size of output buffer
	//  startVictimPosition: Pointer to first victim position packet to be incorporated into stream
	//  endVictimPosition:   Pointer to end of victim position packets
	//  killHitVictimID:		 Used to pick out what SRecording_KillHitPosition to send
	//  Returns: Size of compressed output
	static size_t Compress(CRecordingBuffer *srcBuffer, uint8* outBuffer, uint32 maxOutputSize, SRecording_VictimPosition* startVictimPosition, SRecording_VictimPosition* endVictimPosition, EntityId killHitVictimID);

	// Decompress: Extracts first person kill cam packets compressed using Compress
	//  compressInput:  Output buffer from Compress
	//  compressedSize: Returned size from Compress
	//  outputBuffer:   Decompress output buffer
	//  maxOutputSize:  Size of output buffer
	//  streamAlignment:Alignment of stream parts (by calling Compress multiple times) in bytes. Zero means no gap between streams.
	//  Returns: Size of decompressed output
	// Nb. compressedInput and outputBuffer can point to the same data
	static size_t Decompress(uint8 *compressedInput, size_t compressedSize, void *outputBuffer, size_t maxOutputSize, size_t streamAlignment=1);

	// CompressRaw: Compresses first person kill cam packets from input buffer
	//  inBuffer:            Contains data to be compressed
	//  inBufferSize:				 Size of buffer to be compressed
	//  outBuffer:           Output buffer for compressed data
	//  maxOutputSize:       Size of output buffer
	static size_t CompressRaw(uint8 *inBuffer, uint32 inBufferSize, uint8* outBuffer, uint32 maxOutputSize);

private:
	class CompressionSerializer;
	class DecompressionSerializer;
	
	// SerializePackets: Does compression/decompression and controls quantisation
	//  T: Should be either CompressionSerializer or DecompressionSerializer
	template <class T>
	static SRecording_Packet* SerialisePackets(CAdaptiveCompressor &serializer, SRecording_Packet *cur, char *outputBufferEnd);
	// SortPackets: Marshalls the data ready for compression
	static SRecording_Packet* SortPackets(SRecording_Packet *dstBuffer, CRecordingBuffer *srcBuffer, uint8 *pSrcAuxBuffer, uint32 srcAuxBufferSize, SRecording_VictimPosition* startVictimPosition, SRecording_VictimPosition* endVictimPosition, EntityId killHitVictimID);
	// MovePacketsToBuffer: Helper for InterleavePackets
	static SRecording_Packet* MovePacketsToBuffer(EFirstPersonPacket type, SRecording_Packet *dstBuffer, SRecording_Packet *srcBuffer, SRecording_Packet *endOfSrc);
	// InterleavePackets: Re-orders output after decompression
	static void InterleavePackets(SRecording_Packet *dstBuffer, SRecording_Packet *srcBuffer, SRecording_Packet *endOfSrc);
};

#endif // __RECORDINGSYSTEMCOMPRESSOR_H__