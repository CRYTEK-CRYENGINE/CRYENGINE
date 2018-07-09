// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:  simple arithmetic-style encoding and decoding
               (implemented currently as a range-coder)
   -------------------------------------------------------------------------
   History:
   - 26/07/2004   10:34 : Created by Craig Tiller
*************************************************************************/
#ifndef __ARITHSTREAM_H__
#define __ARITHSTREAM_H__

#pragma once

#include <CryMemory/CrySizer.h>
#include "Config.h"
#include "DebugKit/DebugKit.h"
#include <CryCore/BitFiddling.h>
#include "Utils.h"

#pragma warning(disable: 4390)

#if ENABLE_DEBUG_KIT
	#define IS_LOGGING m_bLog
#else
	#define IS_LOGGING false
#endif

#if CRC8_ENCODING
class CEncCRC
{
public:
	uint8 GetCRC() const { return m_crc.Result(); }

protected:
	void AddEncToCRC(uint64 tot, uint32 low, uint32 sym)
	{
		m_crc.Add64(tot);
		m_crc.Add32(low);
		m_crc.Add32(sym);
	}

	void ResetCRC() { m_crc = CCRC8(); }

private:
	CCRC8 m_crc;
};
#else
class CEncCRC
{
protected:
	ILINE void AddEncToCRC(uint64 tot, uint32 low, uint32 sym) {}
	ILINE void ResetCRC()                                      {}
};
#endif

/*
 * CArithOutputStream: encoding
 */

class CCommOutputStreamBackup;

class CCommOutputStream : public CEncCRC
{
	friend class CCommOutputStreamBackup;

public:
	typedef uint64 InternalStateType;
	typedef uint32 ProbabilityType;

public:
	static const InternalStateType One = 1;
	static const InternalStateType MaxProbabilityValue = One << (8 * sizeof(ProbabilityType));

protected:
	static const InternalStateType BottomValue = 0x0080000000000000ull;
	static const InternalStateType TopValue = 0x8000000000000000ull;
	static const int               ShiftBits = 55;
	static const int               ExtraBits = 7;
	static const InternalStateType ByteAllOnes = 0xff;

public:
	typedef CCommOutputStreamBackup Backup;

	// output to a fixed size buffer
	// the algorithm needs a byte queued to output before it begins; this byte
	// is called the "bonus" byte, and can be used to store any data that fits
	// into a byte that we need - often there is such a piece of information
	// (the networking lib uses this byte to write out the packet type and some
	// sequence numbering information, for instance)
	CCommOutputStream(uint8* vOutput, size_t nMaxOutputSize, uint8 bonus = 0);
	// output to a dynamically allocated buffer
	CCommOutputStream(IStreamAllocator*, size_t nInitialSize, uint8 bonus = 0);
	~CCommOutputStream()
	{
		// we clean up our buffer if we allocated it
		if (m_pSA)
			m_pSA->Free(m_vOutput);
	}
	// clear the buffer and prepare to start encoding again
	void Reset(uint8 bonus = 0);

	void GetMemoryStatistics(ICrySizer* pSizer)
	{
		SIZER_COMPONENT_NAME(pSizer, "CBasicArithOutputStream");

		pSizer->Add(*this);
		if (m_pSA)
			pSizer->Add(m_vOutput, m_nAllocSize);
	}

#if ENABLE_DEBUG_KIT
	void EnableLog(bool flag = true) { m_bLog = flag; }
#else
	void EnableLog(bool flag = true) {}
#endif

	// encode a probability:
	//   tot - the interval of all the probabilities available
	//   low - the lower bound of the probability of the current symbol
	//   sym - the interval of the probability of this symbol
	// NOTE: THESE ARE 16-BIT QUANTITIES
	//       this is so that the internal state can be represented as
	//       32-bit integers, saving us some very expensive 64-bit multiplies
	//       and divides when running on a 32-bit machine
	// Example:
	//   Encoding a binary alphabet {0,1}, with a 50% chance of each symbol
	//   occuring:
	//   Encoding "0":
	//     +--  tot = 2
	//     |
	//     +--          -+
	//     |             | sym = 1
	//     +--  low = 0 -+
	//   Encoding "1":
	//     +--  tot = 2 -+
	//     |             | sym = 1
	//     +--  low = 1 -+
	//     |
	//     +--
	void Encode(InternalStateType tot, ProbabilityType low, ProbabilityType sym);
	// special case of Encode for tot = 1 << bits (ie, we know that our
	// total probability is a power of two)
	// - makes for a more efficient encoding step
	void EncodeShift(int bits, ProbabilityType low, ProbabilityType sym);

	// finish encoding;
	// returns the size of the stream
	size_t       Flush();
	// get a pointer to the encoded data
	const uint8* GetPtr() const { return m_vOutput; }

	// returns an estimate of how many bits a given symbol will take
	// (always underestimates!)
	static inline float EstimateArithSizeInBits(InternalStateType tot, ProbabilityType sym)
	{
		static const float one_on_log2 = 1.4426950408889634073599246810019f;
		return -log(float(sym) / float(tot)) * one_on_log2;
	}
	static inline float EstimateArithSizeInBitsShift(int bits, ProbabilityType sym)
	{
		return EstimateArithSizeInBits(One << bits, sym);
	}

	// return an estimation of how big this stream will be when
	// we flush it
	// doesn't account for help bytes after renormalization in flush
	// TODO: write Size()
	size_t GetApproximateSize() const
	{
		return m_nOutputSize + m_help + 2;
	}

	// this function is useful when we wish to get the exact size the
	// stream was when it was flushed
	size_t GetOutputSize() const
	{
		return m_nOutputSize;
	}

	size_t GetOutputCapacity() const
	{
		return m_nAllocSize;
	}

	//bodies for 32Bit write functions
	inline void WriteBits(uint32 nValue, int nBits)
	{
		EncodeShift(nBits, nValue, 1);
	}

	inline void WriteBitsLarge(uint64 nValue, int nBits)
	{
		if (nBits > 32)
		{
			nBits -= 32;
			EncodeShift(32, (ProbabilityType)(nValue >> nBits), (ProbabilityType)1);
			nValue &= (One << nBits) - 1;
		}
		EncodeShift(nBits, (ProbabilityType)nValue, (ProbabilityType)1);
	}

	inline void WriteInt(uint32 nValue, uint32 nMax)
	{
		Encode(InternalStateType(nMax) + 1, nValue, 1);
	}

	inline void PutZeros(int n)
	{
		for (int i = 0; i < n; i++)
			PutByte(0);
	}

#if ENABLE_ACCURATE_BANDWIDTH_PROFILING
	ILINE float GetBitSize() const
	{
		return m_accurateSize;
	}
#endif

private:
	CCommOutputStream(const CCommOutputStream&);
	CCommOutputStream& operator=(const CCommOutputStream&);

	// makes sure we have enough precision in m_range to store
	// another value; also performs most of the calls to PutByte
	void Renormalize();

#if ENABLE_ACCURATE_BANDWIDTH_PROFILING
	ILINE void UpdateSizeBits(int n)
	{
		m_accurateSize += n;
	}

	template<class A, class B> void UpdateSizeProb(A tot, B sym)
	{
		static const float one_on_log2 = 1.4426950408889634073599246810019f;
		//return -log_tpl( float(sym) / float(tot) ) * one_on_log2;
		m_accurateSize += -log_tpl(float(sym) / float(tot)) * one_on_log2;
	}
#endif

	// output a byte into the buffer
	inline void PutByte(uint8 b)
	{
		if (m_nOutputSize == m_nAllocSize)
			Grow();
		m_vOutput[m_nOutputSize++] = b;
	}

	// increase the size of the buffer
	NO_INLINE void Grow()
	{
		if (!m_pSA)
		{
			CryFatalError("No allocator and overflowed output");
			return; // for static analysis tools to understand this
		}
		m_nAllocSize *= 2;
		m_vOutput = (uint8*)m_pSA->Realloc(m_vOutput, m_nAllocSize);
	}

	InternalStateType m_low;
	InternalStateType m_range;
	int               m_help;
	uint8             m_buffer;
	uint8*            m_vOutput;
	size_t            m_nAllocSize;
	size_t            m_nOutputSize;

	IStreamAllocator* m_pSA;
#if ENABLE_DEBUG_KIT
	bool              m_bLog;
#endif
#if ENABLE_ACCURATE_BANDWIDTH_PROFILING
	float m_accurateSize;
#endif
};

// useful for saving the state of compression and rewinding later
class CCommOutputStreamBackup
{
	typedef uint64 InternalStateType;
	typedef uint32 ProbabilityType;
	static const InternalStateType BottomValue = 0x0080000000000000ull;
	static const InternalStateType TopValue = 0x8000000000000000ull;
	static const int               ShiftBits = 55;
	static const int               ExtraBits = 7;
	static const InternalStateType ByteAllOnes = 0xff;
	static const InternalStateType One = 1;
	static const InternalStateType MaxProbabilityValue = One << (8 * sizeof(ProbabilityType));

public:
	CCommOutputStreamBackup(CCommOutputStream& stm)
	{
		m_pStream = &stm;
		m_low = m_pStream->m_low;
		m_range = m_pStream->m_range;
		m_help = m_pStream->m_help;
		m_buffer = m_pStream->m_buffer;
		m_nOutputSize = m_pStream->m_nOutputSize;
		//		m_size = m_pStream->m_accurateSize;
	}

	void Restore()
	{
		m_pStream->m_low = m_low;
		m_pStream->m_range = m_range;
		m_pStream->m_help = m_help;
		m_pStream->m_buffer = m_buffer;
		m_pStream->m_nOutputSize = m_nOutputSize;
		//		m_pStream->m_accurateSize = m_size;
	}

private:
	CCommOutputStream* m_pStream;
	InternalStateType  m_low;
	InternalStateType  m_range;
	int                m_help;
	uint8              m_buffer;
	size_t             m_nOutputSize;
	//	float m_size;
};

inline CCommOutputStream::CCommOutputStream(uint8* vOutput, size_t nMaxOutputSize, uint8 bonus)
{
#if ENABLE_DEBUG_KIT
	m_bLog = false;
#endif
	m_vOutput = vOutput;
	m_nAllocSize = nMaxOutputSize;
	m_pSA = NULL;
	Reset(bonus);
}

inline CCommOutputStream::CCommOutputStream(IStreamAllocator* pSA, size_t nInitialSize, uint8 bonus)
{
#if ENABLE_DEBUG_KIT
	m_bLog = false;
#endif
	m_vOutput = (uint8*)pSA->Alloc(nInitialSize);
	m_nAllocSize = nInitialSize;
	m_pSA = pSA;
	Reset(bonus);
}

inline void CCommOutputStream::Reset(uint8 bonus)
{
	m_low = 0;
	m_range = TopValue;
	m_buffer = bonus;
	m_help = 0;

	m_nOutputSize = 0;
	ResetCRC();
#if ENABLE_ACCURATE_BANDWIDTH_PROFILING
	m_accurateSize = 0;
#endif
}

inline void CCommOutputStream::Renormalize()
{
	InternalStateType range = m_range;
	InternalStateType low = m_low;
	int help = m_help;
	uint8 buffer = m_buffer;

	while (range <= BottomValue)
	{
		NET_ASSERT(help >= 0);
		if (low < (ByteAllOnes << ShiftBits))
		{
			PutByte(buffer);
			for (; help != 0; --help)
				PutByte(0xff);
			buffer = uint8(low >> ShiftBits);
		}
		else if (low & TopValue)
		{
			PutByte(buffer + 1);
			for (; help != 0; --help)
				PutByte(0);
			buffer = uint8(low >> ShiftBits);
		}
		else
		{
			help++;
		}
		range <<= 8;
		NET_ASSERT(range);
		low = (low << 8) & (TopValue - 1);
	}

	m_range = range;
	m_low = low;
	m_help = help;
	m_buffer = buffer;
}

inline void CCommOutputStream::Encode(InternalStateType tot, ProbabilityType low, ProbabilityType sym)
{
	if (IS_LOGGING)
		DEBUGKIT_CODING(tot, low, sym);
#if ENABLE_ACCURATE_BANDWIDTH_PROFILING
	UpdateSizeProb(tot, sym);
#endif
	assert(tot <= MaxProbabilityValue);
	assert(sym);
	assert(tot);
	Renormalize();
	InternalStateType r = m_range / tot;
	InternalStateType tmp = r * low;
	assert(InternalStateType(low) + sym <= tot);
	assert(m_range);
	m_range = r * sym;
	assert(m_range);
	m_low += tmp;

	AddEncToCRC(tot, low, sym);
}

inline void CCommOutputStream::EncodeShift(int bits, ProbabilityType low, ProbabilityType sym)
{
#if ENABLE_DEBUG_KIT
	if (m_bLog)
		DEBUGKIT_CODING(One << bits, low, sym);
#endif
#if ENABLE_ACCURATE_BANDWIDTH_PROFILING
	UpdateSizeProb(One << bits, sym);
#endif
	assert(sym);
	Renormalize();
	InternalStateType r = m_range >> bits;
	InternalStateType tmp = r * low;
	assert(InternalStateType(low) + InternalStateType(sym) <= (One << bits));
	assert(m_range);
	m_range = r * sym;
	assert(m_range);
	m_low += tmp;

	AddEncToCRC(One << bits, low, sym);
}

inline size_t CCommOutputStream::Flush()
{
	Renormalize();
	InternalStateType tmp = (m_low >> ShiftBits) + 1;
	if (tmp > ByteAllOnes)
	{
		PutByte(m_buffer + 1);
		for (; m_help != 0; --m_help)
			PutByte(0);
	}
	else
	{
		PutByte(m_buffer);
		for (; m_help != 0; --m_help)
			PutByte(0xff);
	}
	PutByte(uint8(tmp & 0xff));

	return m_nOutputSize;
}

/*
 * CArithInputStream: decoding
 */

class CCommInputStream : public CEncCRC
{
public:
	typedef uint64 InternalStateType;
	typedef uint32 ProbabilityType;

protected:
	static const InternalStateType BottomValue = 0x0080000000000000ull;
	static const InternalStateType TopValue = 0x8000000000000000ull;
	static const int               ShiftBits = 55;
	static const int               ExtraBits = 7;
	static const InternalStateType ByteAllOnes = 0xff;
	static const InternalStateType One = 1;
	static const InternalStateType MaxProbabilityValue = One << (8 * sizeof(ProbabilityType));

public:
	CCommInputStream(const uint8* input, size_t length);

	// reading from an arithmetic stream is a three stage affair:
	// - first we call decode with the total probability of the
	// symbol we're reading had of occuring (Decode/DecodeShift)
	// - then we look up which symbol would have produced such a value
	// (this happens in the code using CArithInputStream)
	// - finally, we call Update or UpdateShift to inform it of
	// what probability range would have been used to encode that symbol

	// first step: decode the probability given the probability interval
	// DO NOT expect to get the value of "low" here -- you probably won't...
	// instead you'll get a value in the range of [low,low+sym), where low
	// and sym were the values passed into Encode in the source stream; you'll
	// need to perform some search of your input space in order to find the
	// correct values of low and sym, and of course what the symbol actually
	// was
	ProbabilityType Decode(InternalStateType tot)
	{
		NET_ASSERT(tot <= MaxProbabilityValue);
		Renormalize();
		NET_ASSERT(tot);
		m_help = m_range / tot;
		NET_ASSERT(m_help);
		InternalStateType tmp = m_low / m_help;
		NET_ASSERT(tmp < tot);
		return ProbabilityType(tmp);
	}
	// optimization of Decode, when we know that tot = One<<bits
	ProbabilityType DecodeShift(int bits)
	{
		Renormalize();
		m_help = m_range >> bits;
		NET_ASSERT(m_help);
		InternalStateType tmp = m_low / m_help;
		NET_ASSERT(tmp < (One << bits));
		return ProbabilityType(tmp);
	}
	// third step: update the state of the stream once we know which symbol
	// was decoded, and the corresponding values of low and sym; must be called
	// before the next call to decode
	void Update(InternalStateType tot, ProbabilityType low, ProbabilityType sym);
	// just like Update, but named to correspond to DecodeShift (there is no
	// optimization we can do here!) - use it however if you can use DecodeShift,
	// to make things consistant!
	void UpdateShift(int bits, ProbabilityType low, ProbabilityType sym)
	{
		Update(One << bits, low, sym);
	}

#if ENABLE_DEBUG_KIT
	void EnableLog(bool flag = true) { m_bLog = flag; }
#else
	void EnableLog(bool flag = true) {}
#endif

	// fetch the bonus byte
	uint8  GetBonus() const { return m_bonus; }
	// fetch the size (in bytes) of this stream
	size_t GetSize() const  { return m_length; }

	//bodies for read/write functions in 32Bit stream - see non-basic stream for implementation details
	inline uint32 ReadBits(int nBits)
	{
		uint32 val = DecodeShift(nBits);
		UpdateShift(nBits, (ProbabilityType)val, (ProbabilityType)1);
		return val;
	}

	inline uint64 ReadBitsLarge(int nBits)
	{
		uint64 nValue = 0;
		if (nBits > 32)
		{
			nBits -= 32;
			nValue = DecodeShift(32);
			UpdateShift(32, (ProbabilityType)nValue, (ProbabilityType)1);
			nValue <<= nBits;
		}
		uint32 nFinal = DecodeShift(nBits);
		UpdateShift(nBits, (ProbabilityType)nFinal, (ProbabilityType)1);
		nValue |= nFinal;
		return nValue;
	}

	inline uint32 ReadInt(uint32 nMax)
	{
		uint32 val = Decode(InternalStateType(nMax) + 1);
		Update(InternalStateType(nMax) + 1, val, 1);
		return val;
	}

	void GetMemoryStatistics(ICrySizer* pSizer)
	{
		SIZER_COMPONENT_NAME(pSizer, "CBasicArithInputStream");

		pSizer->Add(*this);
		pSizer->Add(m_input, m_length);
	}

#if ENABLE_DEBUG_KIT || ENABLE_CORRUPT_PACKET_DUMP
	ILINE float GetBitSize() const
	{
		return m_accurateSize;
	}
#endif

private:
	void         Renormalize();

	inline uint8 GetByte()
	{
		if (m_input == m_end)
			return 0;
		return *(m_input++);
	}

	template<class A, class B> void UpdateSizeProb(A tot, B sym)
	{
		static const float one_on_log2 = 1.4426950408889634073599246810019f;
		//return -log_tpl( float(sym) / float(tot) ) * one_on_log2;
		m_accurateSize += -log_tpl(float(sym) / float(tot)) * one_on_log2;
	}

	const uint8*      m_input;
	const uint8*      m_end;
	// TODO: should be able to use input directly!
	uint8             m_buffer;
	uint8             m_bonus;
	InternalStateType m_low;
	InternalStateType m_range;
	InternalStateType m_help;
	size_t            m_length;
#if ENABLE_DEBUG_KIT
	bool              m_bLog;
#endif
	float             m_accurateSize;
};

inline CCommInputStream::CCommInputStream(const uint8* input, size_t length)
{
#if ENABLE_DEBUG_KIT
	m_bLog = false;
#endif
	m_accurateSize = 0;
	m_input = input;
	m_end = input + length;
	m_length = length;

	m_bonus = GetByte();
	m_buffer = GetByte();
	m_low = m_buffer >> (8 - ExtraBits);
	m_range = One << ExtraBits;
}

inline void CCommInputStream::Renormalize()
{
	while (m_range <= BottomValue)
	{
		m_low = (m_low << 8) | ((m_buffer << ExtraBits) & 0xff);
		m_buffer = GetByte();
		m_low = m_low | (m_buffer >> (8 - ExtraBits));
		m_range <<= 8;
	}
}

inline void CCommInputStream::Update(InternalStateType tot, ProbabilityType low, ProbabilityType sym)
{
#if ARITH_STREAM_LOGGING
	if (m_bLog)
		NArithHelpers::LogArithData(this, "update", tot, low, sym);
#endif
#if ENABLE_DEBUG_KIT || ENABLE_CORRUPT_PACKET_DUMP
	UpdateSizeProb(tot, sym);
#endif
	if (IS_LOGGING)
		DEBUGKIT_CODING(tot, low, sym);
	InternalStateType tmp = m_help * low;
	m_low -= tmp;
	NET_ASSERT(InternalStateType(low) + sym <= tot);
	m_range = m_help * sym;

	AddEncToCRC(tot, low, sym);
}

#endif
