// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "BaseEnv/BaseEnv_Prerequisites.h"

#include <CryEntitySystem/IEntityClass.h>

struct IEntityAttributesManagerForSchematycBaseEnv;

namespace SchematycBaseEnv
{
	class CEntityClassRegistrar
	{
	public:
		struct SEntityClass
		{
			SEntityClass();
			SEntityClass(const SEntityClass& rhs);

			~SEntityClass();

			string                                 editorCategory;
			string                                 icon;
			Schematyc2::SGUID                       libClassGUID;
			IEntityClassRegistry::SEntityClassDesc desc;
		};


		void Init();
		void Refresh();

	private:

		typedef std::map<string, SEntityClass> EntityClasses;

		void RegisterEntityClass(const Schematyc2::ILibClass& libClass, SEntityClass& entityClass, bool bNewEntityClass);
		void OnClassRegistration(const Schematyc2::ILibClassConstPtr& pLibClass);

		EntityClasses                   m_entityClasses;
		TemplateUtils::CConnectionScope m_connectionScope;
	};
}