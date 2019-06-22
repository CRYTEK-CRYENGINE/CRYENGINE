// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "SharedData.h"

#include <CrySerialization/IArchive.h>
#include <CrySandbox/CrySignal.h>

namespace ACE
{
struct IConnection
{
	//! \cond INTERNAL
	virtual ~IConnection() = default;
	//! \endcond

	//! Returns id of the connection, which is the same id as its middleware control.
	virtual ControlId GetID() const = 0;

	//! Returns a bool if the connection has properties or not.
	virtual bool HasProperties() const = 0;

	//! Serialize connection proeprties.
	virtual void Serialize(Serialization::IArchive& ar) = 0;

	//! Signal when connection properties have changed.
	CCrySignal<void()> SignalConnectionChanged;
};
} // namespace ACE
