// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc/Script/IScriptElement.h"
#include "CrySchematyc/Utils/EnumFlags.h"
#include "CrySchematyc/Utils/PreprocessorUtils.h"

#include <CryMath/Transform.h>

namespace Schematyc
{
// Forward declare classes.
class CClassProperties;

enum class EScriptComponentInstanceFlags
{
	None     = 0,
	EnvClass = BIT(0)   // Component is part of environment class and can't be removed.
};

typedef CEnumFlags<EScriptComponentInstanceFlags> ScriptComponentInstanceFlags;

struct IScriptComponentInstance : public IScriptElementBase<EScriptElementType::ComponentInstance>
{
	virtual ~IScriptComponentInstance() {}

	virtual CryGUID                      GetTypeGUID() const = 0;
	virtual ScriptComponentInstanceFlags GetComponentInstanceFlags() const = 0;
	virtual bool                         HasTransform() const = 0;
	
	virtual void                               SetTransform(const CryTransform::CTransformPtr& transform) = 0;
	virtual const CryTransform::CTransformPtr& GetTransform() const = 0;

	virtual const CClassProperties&      GetProperties() const = 0;
};

} // Schematyc
