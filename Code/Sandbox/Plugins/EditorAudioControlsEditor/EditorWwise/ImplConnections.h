// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <ImplConnection.h>

namespace ACE
{
namespace Wwise
{
class CParameterConnection final : public CImplConnection
{
public:

	explicit CParameterConnection(CID const id)
		: CImplConnection(id)
		, m_mult(1.0f)
		, m_shift(0.0f)
	{}

	// CImplConnection
	virtual bool HasProperties() const override { return true; }
	virtual void Serialize(Serialization::IArchive& ar) override;
	// ~CImplConnection

	float m_mult;
	float m_shift;
};

typedef std::shared_ptr<CParameterConnection> ParameterConnectionPtr;

class CStateToParameterConnection final : public CImplConnection
{
public:

	explicit CStateToParameterConnection(CID const id)
		: CImplConnection(id)
		, m_value(0.0f)
	{}

	// CImplConnection
	virtual bool HasProperties() const override { return true; }
	virtual void Serialize(Serialization::IArchive& ar) override;
	// ~CImplConnection

	float m_value;
};

typedef std::shared_ptr<CStateToParameterConnection> StateConnectionPtr;
} // namespace Wwise
} // namespace ACE
