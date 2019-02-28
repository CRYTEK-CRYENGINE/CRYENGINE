// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Streams/ArithStream.h"
#include "CrySerialization/Forward.h"

enum EDistributionType
{
	eDistributionType_Write,
	eDistributionType_Read,
	eDistributionType_Count,
	eDistributionType_NotSet
};

class CErrorDistribution
{
	//for calculation of CPU performance
	static CTimeValue m_lastOpEnd;

	enum EOpType
	{
		eOpType_Write = 0,
		eOpType_Read,
		eOpType_Idle,
		eOpType_Num
	};

	static int64 m_totalTime[eOpType_Num];

	void OnOpEnd(EOpType type) const;

public:
	CErrorDistribution();

	void Init(const char* szName, uint32 channel, uint32 symbolBits, uint32 bitBucketSize, const char* szDir, EDistributionType type);

	void GetDebug(uint32& symSize, uint32& totalSize) const;

	void WriteMagic(CCommOutputStream* pStream) const;
	bool ReadMagic(const char* szTag, CCommInputStream* pStream) const;

	void AccumulateInit(CErrorDistribution& dist, const char* szPath) const;
	void Accumulate(CErrorDistribution& dist);

	bool LoadJson(const char* szPath);
	void SaveJson(const char* szPath) const;

	static void LogPerformance();
	void Serialize(Serialization::IArchive& ar);

	void Clear();
	void CountError(int32 error);
	void CountBits(uint32 index);
	uint32 GetNumBits(uint32 index) const;

	bool IsTracked(int32 error) const;

	void InitProbability() const;
	
	string GetFileName(const char* szDirectory) const;

	int32 GenerateRandom(bool bLastZero) const;
	
	void GetAfterZeroProb(uint32& prob, uint32& total) const;

	bool ReadAfterZero(bool& bZero, CCommInputStream* pStream) const;
	bool WriteAfterZero(bool bZero, CCommOutputStream* pStream) const;

	bool WriteBitOutOfRange(CCommOutputStream* pStream) const;
	bool WriteOutOfRange(CCommOutputStream* pStream) const;

	bool WriteValue(int32 value, CCommOutputStream* pStream, bool bLastTimeZero) const;
	bool ReadValue(int32& value, CCommInputStream* pStream, bool bLastTimeZero) const;

	bool WriteBits(int32 value, CCommOutputStream* pStream) const;
	bool ReadBits(int32& value, CCommInputStream* pStream) const;

	bool WriteBitBucket(uint32 bucket, CCommOutputStream* pStream) const;
	bool ReadBitBucket(uint32& bucket, CCommInputStream* pStream) const;

	bool ReadSymbol(uint32 symbol, CCommInputStream* pStream) const;

	bool FindSymbol(uint32& symbol, uint32 prob) const;
	bool GetSymbolDesc(uint32 symbol, uint32& symbolSize, uint32& symbolBase, uint32& total) const;

	void NotifyAfterZero(int error);

	void SmoothDistribution();

	bool IsEmpty() const { return m_totalCount == 0 && m_afterZero == 0; }
	unsigned GetTotalCount() const { return m_totalCount; }

	void PrepareForUse();
	void TrimToFirstZero();
	void PrepareBitBuckets();

	void AvoidEdgeProbabilities();

private:
	void TestMapping(int32 error) const;

	void CountIndex(uint32 value);

	uint32 ErrorToIndex(int32 error) const;
	int32 IndexToError(uint32 index) const;

private:
	EDistributionType m_type;
	uint32 m_channel;
	uint32 m_maxSymbols;

	uint32 m_symbolBits;
	uint32 m_bitBucketSize;

	uint32 m_afterZero;
	uint32 m_zeroAfterZero;

	uint32 m_bitBuckets[33];
	uint32 m_bitBucketsSum[33];
	uint32 m_bitBucketsTotal;
	uint32 m_bitBucketLast;

	std::vector<uint32> m_perBitsCount;

	mutable bool m_bProbabilityValid;
	mutable std::vector<uint32> m_cumulativeDistribution;

	uint32 m_totalCount;

	mutable uint32 m_debugSymSize;
	mutable uint32 m_debugTotalSize;

	std::string m_name;

	static const uint64 sm_maxProbability = CCommOutputStream::MaxProbabilityValue;

	std::vector<uint32> m_valueCounter;
};

class CCommOutputStream;
class CCommInputStream;

class CErrorDistributionTest
{
	CErrorDistribution m_distribution;

	std::vector<int32> m_testValues;

	CCommOutputStream* m_outStream;
	CCommInputStream* m_inStream;

	std::vector<uint8> m_outData;

public:
	CErrorDistributionTest(const string& name, const string& directory, uint32 channel, uint32 symbolNum, uint32 testSize);

	void InitValues(uint32 count);
	void TestRead();
	void Write();
};