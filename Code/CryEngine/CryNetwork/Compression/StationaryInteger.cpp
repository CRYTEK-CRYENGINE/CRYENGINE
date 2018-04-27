// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "StationaryInteger.h"
#include <limits>
#include "Protocol/Serialize.h"

CStationaryInteger::CStationaryInteger(int32 nMin, int32 nMax)
	: m_nMin(nMin)
	, m_nMax(nMax)
#if USE_MEMENTO_PREDICTORS
	, m_haveMemento(false)
#endif
{
	PreComputeBits();
	NET_ASSERT(m_nMax > m_nMin);
}

CStationaryInteger::CStationaryInteger()
	: m_nMin(-1000)
	, m_nMax(1000)
#if USE_MEMENTO_PREDICTORS
	, m_haveMemento(false)
#endif
{
	PreComputeBits();
}

bool CStationaryInteger::Load(XmlNodeRef node, const string& filename, const string& child)
{
	bool ok = true;
	if (XmlNodeRef params = node->findChild(child))
	{
		int64 mn, mx;
		ok &= params->getAttr("min", mn);
		ok &= params->getAttr("max", mx);
		ok &= mx > mn;
		if ((mx > std::numeric_limits<int32>::max() || mn < std::numeric_limits<int32>::min()) &&
		    (mx > std::numeric_limits<uint32>::max() || mn < std::numeric_limits<uint32>::min()))
			ok = false;
		if (ok)
		{
			m_nMin = mn;
			m_nMax = mx;
			PreComputeBits();
		}
	}
	else
	{
		NetWarning("StationaryInteger couldn't find parameters named %s at %s:%d", child.c_str(), filename.c_str(), node->getLine());
		ok = false;
	}
	return ok;
}

#if USE_MEMENTO_PREDICTORS
bool CStationaryInteger::WriteMemento(CByteOutputStream& stm) const
{
	stm.PutTyped<uint32>() = m_oldValue;
	stm.PutTyped<uint8>() = m_probabilitySame;
	m_haveMemento = true;
	return true;
}

bool CStationaryInteger::ReadMemento(CByteInputStream& stm) const
{
	m_oldValue = stm.GetTyped<uint32>();
	m_probabilitySame = stm.GetTyped<uint8>();
	m_haveMemento = true;
	return true;
}

void CStationaryInteger::NoMemento() const
{
	m_oldValue = Quantize(int32((m_nMin + m_nMax) / 2));
	m_probabilitySame = 8;
	m_haveMemento = false;
}
#endif

uint32 CStationaryInteger::Quantize(int32 x) const
{
	int64 y = x;
	if (y < m_nMin)
		return 0;
	else if (y >= m_nMax)
		return uint32(m_nMax - m_nMin);
	else
		return (uint32)(y - m_nMin);
}

int32 CStationaryInteger::Dequantize(uint32 x) const
{
	return int32(x + m_nMin);
}

#if USE_ARITHSTREAM
void CStationaryInteger::WriteValue(CCommOutputStream& stm, int32 value) const
{
	uint32 quantized = Quantize(value);

	if (m_haveMemento)
	{
		if (quantized == m_oldValue)
		{
			stm.EncodeShift(4, 0, m_probabilitySame);
			m_probabilitySame = m_probabilitySame + (m_probabilitySame < 15);
		}
		else
		{
			stm.EncodeShift(4, m_probabilitySame, 16 - m_probabilitySame);
			stm.WriteInt(quantized, uint32(m_nMax - m_nMin));
			m_probabilitySame = 1;
		}
	}
	else
	{
		stm.WriteInt(quantized, uint32(m_nMax - m_nMin));
		m_probabilitySame = 1;
	}

	m_oldValue = quantized;
}

int32 CStationaryInteger::ReadValue(CCommInputStream& stm) const
{
	uint32 quantized;

	if (m_haveMemento)
	{
		uint16 probSame = stm.DecodeShift(4);
		if (probSame < m_probabilitySame)
		{
			stm.UpdateShift(4, 0, m_probabilitySame);
			quantized = m_oldValue;
			m_probabilitySame = m_probabilitySame + (m_probabilitySame < 15);
		}
		else
		{
			stm.UpdateShift(4, m_probabilitySame, 16 - m_probabilitySame);
			quantized = stm.ReadInt(uint32(m_nMax - m_nMin));
			m_probabilitySame = 1;
		}
	}
	else
	{
		quantized = stm.ReadInt(uint32(m_nMax - m_nMin));
		m_probabilitySame = 1;
	}

	m_oldValue = quantized;
	int32 ret = Dequantize(quantized);
	NetLogPacketDebug("CStationaryInteger::ReadValue %d Min %" PRIi64 " Max %" PRIi64 " NumBits %d (%f)", ret, m_nMin, m_nMax, m_numBits, stm.GetBitSize());
	return ret;
}
#else
void CStationaryInteger::WriteValue(CNetOutputSerializeImpl* stm, int32 value) const
{
	uint32 quantized = Quantize(value);

	stm->WriteBits(quantized, m_numBits);
}

int32 CStationaryInteger::ReadValue(CNetInputSerializeImpl* stm) const
{
	uint32 quantized = stm->ReadBits(m_numBits);
	int32 ret = Dequantize(quantized);
	NetLogPacketDebug("CStationaryInteger::ReadValue %d Min %" PRIi64 " Max %" PRIi64 " NumBits %d (%f)", ret, m_nMin, m_nMax, m_numBits, stm->GetBitSize());
	return ret;
}
#endif

#if NET_PROFILE_ENABLE
int CStationaryInteger::GetBitCount()
{
	return m_numBits;
}
#endif
