// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryString/CryString.h>
#include "ACETypes.h"
#include <CrySerialization/IArchive.h>
#include <CrySerialization/STL.h>
#include <CrySystem/ISystem.h>
#include <CrySandbox/CrySignal.h>

namespace ACE
{
class IAudioConnection
{
public:
	IAudioConnection(CID id)
		: m_id(id)
	{
		m_configurationsMask = std::numeric_limits<uint>::max();
	}

	virtual ~IAudioConnection() = default;

	CID          GetID() const                          { return m_id; }
	virtual bool HasProperties()                        { return false; }
	virtual void Serialize(Serialization::IArchive& ar) {};

	//
	void EnableForPlatform(const uint platformIndex, bool bEnable)
	{
		if (bEnable)
		{
			m_configurationsMask |= 1 << platformIndex;
		}
		else
		{
			m_configurationsMask &= ~(1 << platformIndex);
		}

		signalConnectionChanged();
	}

	bool IsPlatformEnabled(const uint platformIndex)
	{
		return (m_configurationsMask & (1 << platformIndex)) > 0;
	}

	void ClearPlatforms()
	{
		if (m_configurationsMask != 0)
		{
			m_configurationsMask = 0;
			signalConnectionChanged();
		}
	}

	CCrySignal<void()> signalConnectionChanged;

private:
	CID  m_id;
	uint m_configurationsMask;
};
} // namespace ACE
