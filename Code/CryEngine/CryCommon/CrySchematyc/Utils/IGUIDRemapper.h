// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace Schematyc
{
// Forward declare structures.


struct IGUIDRemapper
{
	virtual ~IGUIDRemapper() {}

	virtual bool  Empty() const = 0;
	virtual void  Bind(const CryGUID& from, const CryGUID& to) = 0;
	virtual CryGUID Remap(const CryGUID& from) const = 0;
};
} // Schematyc
