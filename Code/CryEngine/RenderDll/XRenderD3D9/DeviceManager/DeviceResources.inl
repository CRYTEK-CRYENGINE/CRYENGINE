// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

////////////////////////////////////////////////////////////////////////////////////////

template<const int TrackLatency = DEVRES_TRACK_LATENCY>
class SUsageTrackedItem
{
public:
	SUsageTrackedItem() { MarkUsed(); }
	SUsageTrackedItem(uint32 lastUseFrame) : m_lastUseFrame(lastUseFrame) {}

	void MarkUsed()
	{
		m_lastUseFrame = GetDeviceObjectFactory().GetCurrentFrameCounter() + TrackLatency;
	}

	bool IsInUse() const
	{
		return m_lastUseFrame > GetDeviceObjectFactory().GetCompletedFrameCounter();
	}

protected:
	uint32 m_lastUseFrame;
};
