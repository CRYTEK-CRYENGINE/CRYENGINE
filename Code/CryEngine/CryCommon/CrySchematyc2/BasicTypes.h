// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc2/Prerequisites.h"

namespace Schematyc2
{
	static const uint32 s_invalidId   = ~0;
	static const uint32 s_invalidIdx  = ~0;
	static const size_t INVALID_INDEX = ~0; // #SchematycTODO : Replace with s_invalidIdx!

	enum class EVisitFlags
	{
		None             = 0,
		RecurseHierarchy = BIT(0),
		IgnoreBlacklist  = BIT(1)
	};

	DECLARE_ENUM_CLASS_FLAGS(EVisitFlags)

	enum class EVisitStatus
	{
		Stop,
		Continue,
		End
	};

	enum class EDomain
	{
		Unknown,
		Env,
		Script,
		//Runtime
	};

	enum class EAccessor
	{
		Public,
		Protected,
		Private
	};

	enum class EOverridePolicy
	{
		Default,
		Override,
		Finalize
	};

	enum class ESimulationMode // #SchematycTODO : Move to IObject?
	{
		NotSet,
		Editing,         // Object is placed in level and we are currently editing.
		EditorAIPhysics, // Editor is running in AI/Physics mode. Not yet supported!!!
		Preview,         // Object is in preview window.
		Game             // Object is in game.
	};

	WRAP_TYPE(EntityId, ExplicitEntityId, 0) // Strongly typed entity id.
	WRAP_TYPE(uint32,   ObjectId,         0)
	WRAP_TYPE(uint32,   LibFunctionId,    0) // #SchematycTODO : Remove!!!

	typedef TemplateUtils::CArrayView<char> CharArrayView;

#ifdef GUID_DEFINED
	typedef ::GUID SysGUID;
#else
	typedef struct
	{
		unsigned long  Data1;
		unsigned short Data2;
		unsigned short Data3;
		unsigned char  Data4[8];
	} SysGUID;
#endif

	// Simple wrapper around Quat class to avoid Quat::IsValid() assert firing when value is copied following default initialization.
	// #SchematycTODO : Move to BaseEnv!!!
	struct QRotation : public Quat
	{
		inline QRotation()
			: Quat(IDENTITY)
		{}

		explicit inline QRotation(const Quat& rhs)
			: Quat(rhs)
		{}
	};

	inline bool Serialize(Serialization::IArchive& archive, QRotation& value, const char* szName, const char* szLabel)
	{
		if(archive.isEdit())
		{
			archive(Serialization::AsAng3(static_cast<Quat&>(value)), szName, szLabel);
		}
		else
		{
			archive(static_cast<Quat&>(value), szName, szLabel);
		}
		return true;
	}
}
