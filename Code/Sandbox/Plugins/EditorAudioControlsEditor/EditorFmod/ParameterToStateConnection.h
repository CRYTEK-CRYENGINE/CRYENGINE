// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Item.h"
#include "../Common/IConnection.h"

#include <PoolObject.h>
#include <CryAudioImplFmod/GlobalData.h>

namespace ACE
{
namespace Impl
{
namespace Fmod
{
class CParameterToStateConnection final : public IConnection, public CryAudio::CPoolObject<CParameterToStateConnection, stl::PSyncNone>
{
public:

	CParameterToStateConnection() = delete;
	CParameterToStateConnection(CParameterToStateConnection const&) = delete;
	CParameterToStateConnection(CParameterToStateConnection&&) = delete;
	CParameterToStateConnection& operator=(CParameterToStateConnection const&) = delete;
	CParameterToStateConnection& operator=(CParameterToStateConnection&&) = delete;

	explicit CParameterToStateConnection(ControlId const id, EItemType const itemType, float const value)
		: m_id(id)
		, m_itemType(itemType)
		, m_value(value)
	{}

	explicit CParameterToStateConnection(ControlId const id, EItemType const itemType)
		: m_id(id)
		, m_itemType(itemType)
		, m_value(CryAudio::Impl::Fmod::g_defaultStateValue)
	{}

	virtual ~CParameterToStateConnection() override = default;

	// CBaseConnection
	virtual ControlId GetID() const override final   { return m_id; }
	virtual bool      HasProperties() const override { return true; }
	virtual void      Serialize(Serialization::IArchive& ar) override;
	// ~CBaseConnection

	float GetValue() const { return m_value; }

private:

	ControlId const m_id;
	EItemType const m_itemType;
	float           m_value;
};
} // namespace Fmod
} // namespace Impl
} // namespace ACE
