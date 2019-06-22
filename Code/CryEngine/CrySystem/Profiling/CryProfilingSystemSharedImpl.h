// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/Profilers/ICryProfilingSystem.h>
#include <CrySystem/Profilers/SamplerStats.h>
#include <CryInput/IInput.h>

//! Extension of the base interface with functionality used only in CrySystem
class CCryProfilingSystemImpl : public ICryProfilingSystem, public IInputEventListener
{
public:
	CCryProfilingSystemImpl();
	virtual ~CCryProfilingSystemImpl() = default;

	//////////////////////////////////////////////////////////
	// ICryProfilingSystem
	//! Must be called when something quacks like the end of the frame.
	void OnSliceAndSleep() final;

	float GetProfilingTimeCost() const override { return m_profilingOverhead.Latest(); }
	float GetAverageProfilingTimeCost() const override { return m_profilingOverhead.Average(); }

	const char* GetModuleName(EProfiledSubsystem) const final;
	const char* GetModuleName(const SProfilingSection*) const final;
	//////////////////////////////////////////////////////////

	virtual void RegisterCVars() = 0;

	// IInputEventListener
	bool OnInputEvent(const SInputEvent& event) override;
	// ~IInputEventListener

	// use these before construction/ after destruction of the profiling system
	static bool StartSectionStaticDummy(SProfilingSection*) { return false; }
	static void EndSectionStaticDummy(SProfilingSection*) {}
	static void RecordMarkerStaticDummy(SProfilingMarker*) {}

protected:
	uint32 GenerateColorBasedOnName(const char* name);

	// per frame
	CSamplerStats<int64> m_profilingOverhead;
	CSamplerStats<TProfilingCount> m_pageFaultsPerFrame;
};
