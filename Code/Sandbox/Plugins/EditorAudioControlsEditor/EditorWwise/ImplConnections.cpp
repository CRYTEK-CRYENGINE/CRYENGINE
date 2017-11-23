// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ImplConnections.h"

namespace ACE
{
namespace Wwise
{
//////////////////////////////////////////////////////////////////////////
void CParameterConnection::Serialize(Serialization::IArchive& ar)
{
	ar(m_mult, "mult", "Multiply");
	ar(m_shift, "shift", "Shift");

	if (ar.isInput())
	{
		signalConnectionChanged();
	}
}

//////////////////////////////////////////////////////////////////////////
void CStateToParameterConnection::Serialize(Serialization::IArchive& ar)
{
	ar(m_value, "value", "Value");

	if (ar.isInput())
	{
		signalConnectionChanged();
	}
}
} // namespace Wwise
} //namespace ACE
