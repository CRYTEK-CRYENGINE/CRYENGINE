// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc2/IFoundation.h>
#include <CrySchematyc2/Env/IEnvRegistry.h>

#include "Signal.h"

namespace Schematyc2
{
	typedef std::vector<string>	TStringVector;
	typedef std::vector<SGUID>	GUIDVector;

	// Foundation.
	////////////////////////////////////////////////////////////////////////////////////////////////////
	class CFoundation : public IFoundation
	{
	public:

		CFoundation(const SGUID& guid, const char* name);

		// IFoundation
		virtual SGUID GetGUID() const override;
		virtual const char* GetName() const override;
		virtual void SetDescription(const char* szDescription) override;
		virtual const char* GetDescription() const override;
		virtual void SetProperties(const IPropertiesPtr& pProperties) override;
		virtual IPropertiesConstPtr GetProperties() const override;
		virtual void UseNamespace(const char* szNamespace) override;
		virtual size_t GetNamespaceCount() const override;
		virtual const char* GetNamespace(size_t namespaceIdx) const override;
		virtual void AddAbstractInterface(const SGUID& guid) override;
		virtual size_t GetAbstractInterfaceCount() const override;
		virtual SGUID GetAbstractInterfaceGUID(size_t iAbstractInterface) const override;
		virtual void AddComponent(const SGUID& guid) override;
		virtual size_t GetComponentCount() const override;
		virtual SGUID GetComponentGUID(size_t iComponent) const override;
		// ~IFoundation

	protected:

		// IFoundation
		virtual bool AddExtension_Protected(const EnvTypeId& typeId, const IFoundationExtensionPtr& pExtension) override;
		virtual IFoundationExtensionPtr QueryExtension_Protected(const EnvTypeId& typeId) const override;
		// ~IFoundation

	private:

		typedef std::map<EnvTypeId, IFoundationExtensionPtr> TExtensionMap;

		SGUID          m_guid;
		string         m_name;
		string         m_description;
		IPropertiesPtr m_pProperties;
		TStringVector  m_namespaces;
		GUIDVector     m_abstractInterfaces;
		GUIDVector     m_components;
		TExtensionMap  m_extensions;
	};
}
