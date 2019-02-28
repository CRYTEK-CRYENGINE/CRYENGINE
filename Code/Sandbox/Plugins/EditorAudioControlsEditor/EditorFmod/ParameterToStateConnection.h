// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "BaseConnection.h"
#include "Item.h"

#include <CryAudioImplFmod/GlobalData.h>

namespace ACE
{
namespace Impl
{
namespace Fmod
{
class CParameterToStateConnection final : public CBaseConnection
{
public:

	explicit CParameterToStateConnection(
	  ControlId const id,
	  EItemType const itemType,
	  float const value = CryAudio::Impl::Fmod::s_defaultStateValue)
		: CBaseConnection(id)
		, m_itemType(itemType)
		, m_value(value)
	{}

	CParameterToStateConnection() = delete;

	// CBaseConnection
	virtual bool HasProperties() const override { return true; }
	virtual void Serialize(Serialization::IArchive& ar) override;
	// ~CBaseConnection

	float GetValue() const { return m_value; }

private:

	EItemType const m_itemType;
	float           m_value;
};
} // namespace Fmod
} // namespace Impl
} // namespace ACE
