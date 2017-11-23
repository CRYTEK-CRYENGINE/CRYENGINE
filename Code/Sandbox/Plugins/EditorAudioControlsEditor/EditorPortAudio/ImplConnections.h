// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <ImplConnection.h>

namespace ACE
{
namespace PortAudio
{
enum class EConnectionType
{
	Start = 0,
	Stop,
	NumTypes,
};

class CConnection final : public CImplConnection
{
public:

	explicit CConnection(CID const id)
		: CImplConnection(id)
		, m_type(EConnectionType::Start)
		, m_isPanningEnabled(true)
		, m_isAttenuationEnabled(true)
		, m_minAttenuation(0.0f)
		, m_maxAttenuation(100.0f)
		, m_volume(-14.0f)
		, m_loopCount(1)
		, m_isInfiniteLoop(false)
	{}

	// CImplConnection
	virtual bool HasProperties() const override { return true; }
	virtual void Serialize(Serialization::IArchive& ar) override;
	// ~CImplConnection

	EConnectionType m_type;
	float           m_minAttenuation;
	float           m_maxAttenuation;
	float           m_volume;
	uint            m_loopCount;
	bool            m_isPanningEnabled;
	bool            m_isAttenuationEnabled;
	bool            m_isInfiniteLoop;
};
} // namespace PortAudio
} // namespace ACE
