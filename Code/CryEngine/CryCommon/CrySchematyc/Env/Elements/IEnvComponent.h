// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc/Env/IEnvElement.h"
#include <CryEntitySystem/IEntityComponent.h>

namespace Schematyc
{

struct IEnvComponent : public IEnvElementBase<EEnvElementType::Component>
{
	virtual ~IEnvComponent() {}

	virtual const CEntityComponentClassDesc&  GetDesc() const = 0;
	virtual std::shared_ptr<IEntityComponent> CreateFromPool() const = 0;
	virtual size_t                            GetSize() const = 0;
	virtual std::shared_ptr<IEntityComponent> CreateFromBuffer(void* pBuffer) const = 0;
};

} // Schematyc
