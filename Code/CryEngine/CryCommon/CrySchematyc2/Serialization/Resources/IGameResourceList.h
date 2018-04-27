// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc2/Prerequisites.h"

namespace Schematyc2
{
	struct IGameResourceList
	{
		enum class EType : uint32
		{
			Invalid = 0,
			Character,
			StatObject,
			Material,
			Texture,
			ParticleFX,
			MannequinADB,
			MannequinControllerDefinition,
			EntityClass,
			EntityArchetype,
			BehaviorTree,
			LensFlare,
		};

		struct SItem
		{
			SItem(const char* _szResourceName, EType _type)
				: szResourceName(_szResourceName)
				, type(_type)
			{

			}

			const char* szResourceName;
			EType       type;
		};

		virtual ~IGameResourceList() {}

		virtual void   AddResource(const char* szResource, EType type) = 0;
		virtual size_t GetResourceCount() const = 0;
		virtual SItem  GetResourceItemAt(size_t idx) const = 0;
		virtual void   Sort() = 0;
	};

	DECLARE_SHARED_POINTERS(IGameResourceList)
}
