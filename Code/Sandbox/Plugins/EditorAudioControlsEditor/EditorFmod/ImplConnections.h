// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <ImplConnection.h>

namespace ACE
{
namespace Fmod
{
enum class EEventType
{
	Start = 0,
	Stop,
};

class CEventConnection final : public CImplConnection
{
public:

	explicit CEventConnection(CID const nID)
		: CImplConnection(nID)
	{}

	// CImplConnection
	virtual bool HasProperties() const override { return true; }
	virtual void Serialize(Serialization::IArchive& ar) override;
	// ~CImplConnection

	EEventType m_type = EEventType::Start;
};

class CParamConnection final : public CImplConnection
{
public:

	explicit CParamConnection(CID const nID)
		: CImplConnection(nID)
	{}

	// CImplConnection
	virtual bool HasProperties() const override { return true; }
	virtual void Serialize(Serialization::IArchive& ar) override;
	// ~CImplConnection

	float m_mult = 1.0f;
	float m_shift = 0.0f;
};

class CParamToStateConnection final : public CImplConnection
{
public:

	explicit CParamToStateConnection(CID const id)
		: CImplConnection(id)
	{}

	// CImplConnection
	virtual bool HasProperties() const override { return true; }
	virtual void Serialize(Serialization::IArchive& ar) override;
	// ~CImplConnection

	float m_value = 1.0f;
};
} // namespace Fmod
} // namespace ACE
