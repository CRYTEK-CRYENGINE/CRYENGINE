// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   Sampler.h
//  Version:     v1.00
//  Created:     14/3/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __Sampler_h__
#define __Sampler_h__
#pragma once

#if CRY_PLATFORM_WINDOWS

class CSamplingThread;

// Symbol database, used Microsoft DebugAPI.
class CSymbolDatabase
{
public:
	CSymbolDatabase();
	~CSymbolDatabase();

	bool Init();

	// Lookup name of the function from instruction pointer.
	bool LookupFunctionName(uint64 ip, string& funcName);
	bool LookupFunctionName(uint64 ip, string& funcName, string& fileName, int& lineNumber);

private:
	bool m_bInitialized;
};

//////////////////////////////////////////////////////////////////////////
// Sampler class is running a second thread which is at regular intervals
// eg 1ms samples main thread and stores current IP in the samples buffers.
// After sampling finishes it can resolve collected IP buffer info to
// the function names and calculated where most of the execution time spent.
//////////////////////////////////////////////////////////////////////////
class CSampler
{
public:
	struct SFunctionSample
	{
		string function;
		uint32 nSamples; // Number of samples per function.
	};

	CSampler();
	~CSampler();

	void Start();
	void Stop();
	void Update();

	// Adds a new sample to the ip buffer, return false if no more samples can be added.
	bool AddSample(uint64 ip);
	void SetMaxSamples(int nMaxSamples);

	int  GetSamplePeriod() const     { return m_samplePeriodMs; }
	void SetSamplePeriod(int millis) { m_samplePeriodMs = millis; }

private:
	void ProcessSampledData();
	void LogSampledData();

	// Buffer for IP samples.
	std::vector<uint64>          m_rawSamples;
	std::vector<SFunctionSample> m_functionSamples;
	int                          m_nMaxSamples;
	bool                         m_bSampling;
	bool                         m_bSamplingFinished;

	int                          m_samplePeriodMs;

	CSamplingThread*             m_pSamplingThread;
	CSymbolDatabase*             m_pSymDB;
};

#else // CRY_PLATFORM_WINDOWS

// Dummy sampler.
class CSampler
{
public:
	void Start()                        {}
	void Stop()                         {}
	void Update()                       {}
	void SetMaxSamples(int nMaxSamples) {}
	void SetSamplePeriod(int millis)    {}
};

#endif // CRY_PLATFORM_WINDOWS

#endif // __Sampler_h__
