// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "../Common/IConnection.h"

#include <PoolObject.h>

namespace ACE
{
namespace Impl
{
namespace Adx2
{
class CGenericConnection final : public IConnection, public CryAudio::CPoolObject<CGenericConnection, stl::PSyncNone>
{
public:

	CGenericConnection() = delete;
	CGenericConnection(CGenericConnection const&) = delete;
	CGenericConnection(CGenericConnection&&) = delete;
	CGenericConnection& operator=(CGenericConnection const&) = delete;
	CGenericConnection& operator=(CGenericConnection&&) = delete;

	explicit CGenericConnection(ControlId const id)
		: m_id(id)
	{}

	virtual ~CGenericConnection() override = default;

	// IConnection
	virtual ControlId GetID() const override final                    { return m_id; }
	virtual bool      HasProperties() const override                  { return false; }
	virtual void      Serialize(Serialization::IArchive& ar) override {}
	// ~IConnection

private:

	ControlId const m_id;
};
} // namespace Adx2
} // namespace Impl
} // namespace ACE
