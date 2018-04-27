// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "BaseEnv/BaseEnv_BasicTypes.h"

#include "BaseEnv/BaseEnv_AutoRegistrar.h"

namespace SchematycBaseEnv
{
	STransform::STransform()
		: pos(ZERO)
		, scale(1.0f)
		, rot(ZERO)
	{}

	void STransform::Serialize(Serialization::IArchive& archive)
	{
		static const int32 minAngleValue = 0;
		static const int32 maxAngleValue = 360;

		archive(pos, "pos", "Position");
		archive(GameSerialization::RadianAngleDecorator<minAngleValue, maxAngleValue>(rot), "rot", "Rotation");
		archive(scale, "scale", "Scale");
	}

	bool STransform::operator == (const STransform& rhs) const
	{
		return (pos == rhs.pos) && (scale == rhs.scale) && (rot == rhs.rot);
	}

	static const Schematyc2::SGUID s_transformTypeGUID = "a19ff444-7d11-49ea-80ea-5b629c44f588";

	void RegisterBasicTypes()
	{
		Schematyc2::IEnvRegistry& envRegistry = gEnv->pSchematyc2->GetEnvRegistry();

		envRegistry.RegisterTypeDesc(Schematyc2::MakeEnvTypeDescShared(s_transformTypeGUID, "Transform", "Transform", STransform(), Schematyc2::EEnvTypeFlags::None));
	}
}

SCHEMATYC2_GAME_ENV_AUTO_REGISTER(&SchematycBaseEnv::RegisterBasicTypes)
