// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc/Utils/GUID.h"
#include "CrySchematyc/Utils/IGUIDRemapper.h"

namespace Schematyc
{
class CGUIDRemapper : public IGUIDRemapper
{
private:

	typedef std::unordered_map<CryGUID, CryGUID> GUIDs;

public:

	// IGUIDRemapper
	virtual bool  Empty() const override;
	virtual void  Bind(const CryGUID& from, const CryGUID& to) override;
	virtual CryGUID Remap(const CryGUID& from) const override;
	// ~IGUIDRemapper

private:

	GUIDs m_guids;
};
} // Schematyc
