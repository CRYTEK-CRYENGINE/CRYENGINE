// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __ADAPTIVEFLOAT_H__
#define __ADAPTIVEFLOAT_H__

#pragma once

#include "Quantizer.h"
#include "IntegerValuePredictor.h"
#include "Streams/CommStream.h"

class CNetInputSerializeImpl;
class CNetOutputSerializeImpl;

class CAdaptiveFloat
{
public:
	CAdaptiveFloat();

	void Init(const char* szName, uint32 channel, const char* szUseDir, const char* szAccDir) {}
	bool Manage(CCompressionManager* pManager, const char* szPolicy, int channel) { return false; }

	bool Load(XmlNodeRef node, const string& filename, const string& child);
#if USE_MEMENTO_PREDICTORS
	void ReadMemento(CByteInputStream& stm) const;
	void WriteMemento(CByteOutputStream& stm) const;
	void NoMemento() const;
#endif
#if USE_ARITHSTREAM
	bool ReadValue(CCommInputStream& stm, float& value, uint32 mementoAge, uint32 timeFraction32 = 0) const;
	void WriteValue(CCommOutputStream& stm, float value, uint32 mementoAge, uint32 timeFraction32 = 0) const;
#else
	bool ReadValue(CNetInputSerializeImpl* stm, float& value, uint32 mementoAge, uint32 timeFraction32 = 0) const;
	void WriteValue(CNetOutputSerializeImpl* stm, float value, uint32 mementoAge, uint32 timeFraction32 = 0) const;
#endif
#if NET_PROFILE_ENABLE
	int GetBitCount();
#endif

private:
	CQuantizer                     m_quantizer;
#if USE_MEMENTO_PREDICTORS
	uint32                         m_nHeight;
	uint32                         m_nQuantizedMinDifference;
	uint32                         m_nQuantizedMaxDifference;
	uint32                         m_nQuantizedStartValue;
	uint8                          m_nInRangePercentage;

	mutable CIntegerValuePredictor m_predictor;
	mutable bool                   m_haveMemento;
#endif
};

#endif
