// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc2/Prerequisites.h"

#include <CrySchematyc2/TemplateUtils/TemplateUtils_TypeUtils.h>

#include "CrySchematyc2/Any.h"
#include "CrySchematyc2/IActionFactory.h"
#include "CrySchematyc2/Properties.h"
#include "CrySchematyc2/Env/IEnvRegistry.h"
#include "CrySchematyc2/Utils/StringUtils.h"

#define SCHEMATYC2_MAKE_ACTION_FACTORY_SHARED(action, properties, actionGUID)                          Schematyc2::IActionFactoryPtr(std::make_shared<Schematyc2::CActionFactory<action, properties> >(actionGUID, Schematyc2::SGUID(), TemplateUtils::GetTypeName<action>(), __FILE__, "Code"))
#define SCHEMATYC2_MAKE_COMPONENT_ACTION_FACTORY_SHARED(action, properties, actionGUID, componentGUID) Schematyc2::IActionFactoryPtr(std::make_shared<Schematyc2::CActionFactory<action, properties> >(actionGUID, componentGUID, TemplateUtils::GetTypeName<action>(), __FILE__, "Code"))

namespace Schematyc2
{
	template <typename TYPE> struct SActionPropertiesFactory
	{
		static inline IPropertiesPtr CreateProperties()
		{
			return Properties::MakeShared<TYPE>();
		}
	};

	template <> struct SActionPropertiesFactory<void>
	{
		struct SEmptyProperties
		{
			void Serialize(Serialization::IArchive& archive) {}
		};

		static inline IPropertiesPtr CreateProperties()
		{
			return Properties::MakeShared<SEmptyProperties>();
		}
	};

	template <class ACTION, class PROPERTIES> class CActionFactory : public IActionFactory
	{
	public:

		inline CActionFactory(const SGUID& actionGUID, const SGUID& componentGUID, const char* szDeclaration, const char* szFileName, const char* szProjectDir)
			: m_actionGUID(actionGUID)
			, m_componentGUID(componentGUID)
			, m_flags(EActionFlags::None)
		{
			StringUtils::SeparateTypeNameAndNamespace(szDeclaration, m_name, m_namespace);
			SetFileName(szFileName, szProjectDir);
		}

		// IActionFactory

		virtual SGUID GetActionGUID() const override
		{
			return m_actionGUID;
		}

		virtual SGUID GetComponentGUID() const override
		{
			return m_componentGUID;
		}

		virtual void SetName(const char* szName) override
		{
			m_name = szName;
		}

		virtual const char* GetName() const override
		{
			return m_name.c_str();
		}

		virtual void SetNamespace(const char* szNamespace) override
		{
			m_namespace = szNamespace;
		}

		virtual const char* GetNamespace() const override
		{
			return m_namespace.c_str();
		}

		virtual void SetFileName(const char* szFileName, const char* szProjectDir) override
		{
			StringUtils::MakeProjectRelativeFileName(szFileName, szProjectDir, m_fileName);
		}

		virtual const char* GetFileName() const override
		{
			return m_fileName.c_str();
		}

		virtual void SetAuthor(const char* szAuthor) override
		{
			m_author = szAuthor;
		}

		virtual const char* GetAuthor() const override
		{
			return m_author.c_str();
		}

		virtual void SetDescription(const char* szDescription) override
		{
			m_description = szDescription;
		}

		virtual const char* GetDescription() const override
		{
			return m_description.c_str();
		}

		virtual void SetWikiLink(const char* szWikiLink) override
		{
			m_wikiLink = szWikiLink;
		}

		virtual const char* GetWikiLink() const override
		{
			return m_wikiLink.c_str();
		}

		virtual void SetFlags(EActionFlags flags) override
		{
			m_flags = flags;
		}

		virtual EActionFlags GetFlags() const override
		{
			return m_flags;
		}

		virtual IActionPtr CreateAction() const override
		{
			return IActionPtr(std::make_shared<ACTION>());
		}

		virtual IPropertiesPtr CreateProperties() const override
		{
			return SActionPropertiesFactory<PROPERTIES>::CreateProperties();
		}

		// ~IActionFactory

	private:

		SGUID        m_actionGUID;
		SGUID        m_componentGUID;
		string       m_name;
		string       m_namespace;
		string       m_fileName;
		string       m_author;
		string       m_description;
		string       m_wikiLink;
		EActionFlags m_flags;
	};
}
