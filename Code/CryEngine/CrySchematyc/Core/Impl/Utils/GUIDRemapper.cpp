// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GUIDRemapper.h"

namespace Schematyc
{
bool CGUIDRemapper::Empty() const
{
	return m_guids.empty();
}

void CGUIDRemapper::Bind(const CryGUID& from, const CryGUID& to)
{
	m_guids.insert(GUIDs::value_type(from, to));
}

CryGUID CGUIDRemapper::Remap(const CryGUID& from) const
{
	GUIDs::const_iterator itGUID = m_guids.find(from);
	return itGUID != m_guids.end() ? itGUID->second : from;
}
} // Schematyc
