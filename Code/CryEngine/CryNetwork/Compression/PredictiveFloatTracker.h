// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

class CPredictiveFloatTracker
{
public:
	struct SequenceEntry
	{
		int32 sequenceId;
		uint32 lastValue;
		uint32 lastDelta;
		uint32 lastTime;
		int32 quantized;
		int32 predicted;
		uint32 mementoAge;
		uint32 timeFraction32;
		float size;
		uint32 symSize;
		uint32 totalSize;
	};

	struct Sequence
	{
		CTimeValue m_lastUseTime;
		std::vector<SequenceEntry> mEntries;

		void Use();
		float GetSecondsFromLastUse() const;
	};

public:
	CPredictiveFloatTracker(const string& dir, int32 errorThreshold, float quantizationScale);
	virtual ~CPredictiveFloatTracker();

	void LogSequence(Sequence* pSequence, const string& file);
	void Manage(const char* szPolicyName, int channel);
	void Track(int32 mementoId, int32 mementoSequence, uint32 lastValue, uint32 lastDelta, uint32 lastTime, int32 quantized, int32 predicted, uint32 mementoAge, uint32 timeFraction32, float size, uint32 symSize, uint32 totalSize);

private:
	Sequence* GetSequence(int32 mementoId);

private:
	float m_quantizationScale;
	int32 m_errorThreshold;
	string m_directory;
	std::map<int32, Sequence*> m_sequences;

	CryMutex m_mutex;
};
