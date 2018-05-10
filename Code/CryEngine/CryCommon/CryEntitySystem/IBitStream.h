// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef IBITSTREAM_H__
#define IBITSTREAM_H__

#if _MSC_VER > 1000
	#pragma once
#endif // _MSC_VER > 1000

class CStream;

#include <CryCore/Platform/platform.h>

enum eBitStreamHint
{
	eDoNotCompress,       //!< ...
	e8BitNormal,          //!< Vec3, low quality normalized vector.
	eWorldPos,            //!< Vec3, absolute world position.
	eASCIIText,           //!< Char *, static huffman compression.
	eEntityId,            //!< U __int32,__int32, u __int16,16bit - some entities have higher probability (e.g. player).
	eEntityClassId,       //!< __int32,u __int16, for entity creation.
	e8BitRGB,             //!< Vec3, 8bit Color.
	e4BitRGB,             //!< Vec3, 4bit Color.
	eQuaternion,          //!< Vec3, eQuaternion.
	eEulerAnglesHQ,       //!< Vec3, YAW,PITCH,ROLL cyclic in [0..360[, special compression if PITCH=0 (not float but still quite high quality).
	eSignedUnitValueLQ,   //!< Float, [-1..1] 8+1+1 bit if not zero, 1 bit of zero.
};

struct IBitStream
{
	// <interfuscator:shuffle>
	//  virtual bool ReadBitStream( bool &Value )=0;
	virtual bool ReadBitStream(CStream& stm, int8& Value, const eBitStreamHint eHint) = 0;
	virtual bool ReadBitStream(CStream& stm, int16& Value, const eBitStreamHint eHint) = 0;
	virtual bool ReadBitStream(CStream& stm, int32& Value, const eBitStreamHint eHint) = 0;
	virtual bool ReadBitStream(CStream& stm, uint8& Value, const eBitStreamHint eHint) = 0;
	virtual bool ReadBitStream(CStream& stm, uint16& Value, const eBitStreamHint eHint) = 0;
	virtual bool ReadBitStream(CStream& stm, uint32& Value, const eBitStreamHint eHint) = 0;
	virtual bool ReadBitStream(CStream& stm, float& Value, const eBitStreamHint eHint) = 0;
	virtual bool ReadBitStream(CStream& stm, Vec3& Value, const eBitStreamHint eHint) = 0;

	//! Max 256 characters.
	virtual bool ReadBitStream(CStream& stm, char* Value, const uint32 nBufferSize, const eBitStreamHint eHint) = 0;

	// ----------------------------------------------------------------------------------------------------

	//! virtual bool WriteBitStream( const bool Value )=0;.
	virtual bool WriteBitStream(CStream& stm, const int8 Value, const eBitStreamHint eHint) = 0;
	virtual bool WriteBitStream(CStream& stm, const int16 Value, const eBitStreamHint eHint) = 0;
	virtual bool WriteBitStream(CStream& stm, const int32 Value, const eBitStreamHint eHint) = 0;
	virtual bool WriteBitStream(CStream& stm, const uint8 Value, const eBitStreamHint eHint) = 0;
	virtual bool WriteBitStream(CStream& stm, const uint16 Value, const eBitStreamHint eHint) = 0;
	virtual bool WriteBitStream(CStream& stm, const uint32 Value, const eBitStreamHint eHint) = 0;
	virtual bool WriteBitStream(CStream& stm, const float Value, const eBitStreamHint eHint) = 0;
	virtual bool WriteBitStream(CStream& stm, const Vec3& Value, const eBitStreamHint eHint) = 0;

	//! Max 256 characters.
	virtual bool WriteBitStream(CStream& stm, const char* Value, const uint32 nBufferSize, const eBitStreamHint eHint) = 0;

	//----------------------------------------------------------------------------------------------------
	//! The follwoing method make use of the WriteBitStream and the ReadBitStream to simulate the error the
	//! read and write operations would have.

	//! To get the compression error.
	virtual void SimulateWriteRead(int8& Value, const eBitStreamHint eHint) = 0;

	//! To get the compression error.
	virtual void SimulateWriteRead(int16& Value, const eBitStreamHint eHint) = 0;

	//! To get the compression error.
	virtual void SimulateWriteRead(int32& Value, const eBitStreamHint eHint) = 0;

	//! To get the compression error.
	virtual void SimulateWriteRead(uint8& Value, const eBitStreamHint eHint) = 0;

	//! To get the compression error.
	virtual void SimulateWriteRead(uint16& Value, const eBitStreamHint eHint) = 0;

	//! To get the compression error.
	virtual void SimulateWriteRead(uint32& Value, const eBitStreamHint eHint) = 0;

	//! To get the compression error.
	virtual void SimulateWriteRead(float& Value, const eBitStreamHint eHint) = 0;

	//! To get the compression error.
	virtual void SimulateWriteRead(Vec3& Value, const eBitStreamHint eHint) = 0;

	//! To get the compression error.
	virtual void SimulateWriteRead(char* Value, const uint32 nBufferSize, const eBitStreamHint eHint) = 0;
	// </interfuscator:shuffle>
};

#endif //IBITSTREAM_H__
