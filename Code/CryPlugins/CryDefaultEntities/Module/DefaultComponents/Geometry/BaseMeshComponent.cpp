// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "BaseMeshComponent.h"
#include <CrySerialization/Enum.h>

namespace Cry
{
namespace DefaultComponents
{
	YASLI_ENUM_BEGIN_NESTED(SPhysicsParameters, EWeightType, "WeightType")
	YASLI_ENUM_VALUE_NESTED(SPhysicsParameters, EWeightType::Mass, "Mass")
	YASLI_ENUM_VALUE_NESTED(SPhysicsParameters, EWeightType::Density, "Density")
	YASLI_ENUM_VALUE_NESTED(SPhysicsParameters, EWeightType::Immovable, "Immovable")
	YASLI_ENUM_END()
}
}
