// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <CrySerialization/Color.h>
#include <CrySerialization/Math.h>

#include <CrySchematyc/CoreAPI.h>
#include "../CoreEnv.h"

namespace Schematyc
{

void RegisterMathTypes(IEnvRegistrar& registrar)
{
	CEnvRegistrationScope scope = registrar.Scope(g_mathModuleGUID);
	{
		scope.Register(SCHEMATYC_MAKE_ENV_DATA_TYPE(float));
		scope.Register(SCHEMATYC_MAKE_ENV_DATA_TYPE(Vec2));
		scope.Register(SCHEMATYC_MAKE_ENV_DATA_TYPE(Vec3));
		scope.Register(SCHEMATYC_MAKE_ENV_DATA_TYPE(ColorF));
		scope.Register(SCHEMATYC_MAKE_ENV_DATA_TYPE(ColorB));
		scope.Register(SCHEMATYC_MAKE_ENV_DATA_TYPE(CryTransform::CRotation));
		scope.Register(SCHEMATYC_MAKE_ENV_DATA_TYPE(CryTransform::CAngle));
		scope.Register(SCHEMATYC_MAKE_ENV_DATA_TYPE(CryTransform::CAngles3));
		scope.Register(SCHEMATYC_MAKE_ENV_DATA_TYPE(CryTransform::CTransform));
	}
}

} // Schematyc

CRY_STATIC_AUTO_REGISTER_FUNCTION(&Schematyc::RegisterMathTypes)
