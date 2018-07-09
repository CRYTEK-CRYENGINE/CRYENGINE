// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc2/Prerequisites.h"

#include "CrySchematyc2/GUID.h"
#include "CrySchematyc2/IProperties.h"

namespace Schematyc2
{
	struct IAction;

	DECLARE_SHARED_POINTERS(IAction)

	enum class EActionFlags
	{
		None                      = 0,
		Singleton                 = BIT(0), // Allow only of one instance of this action to run at a time per object.
		ServerOnly                = BIT(1), // Action should only be active on server.
		ClientOnly                = BIT(2), // Action should only be active on clients.
		NetworkReplicateServer    = BIT(3), // Action should be replicated on server.
		NetworkReplicateClients   = BIT(4)  // Action should be replicated on clients.
	};

	DECLARE_ENUM_CLASS_FLAGS(EActionFlags)

	struct IActionFactory
	{
		virtual ~IActionFactory() {}

		virtual SGUID GetActionGUID() const = 0;
		virtual SGUID GetComponentGUID() const = 0;
		virtual void SetName(const char* szName) = 0;
		virtual const char* GetName() const = 0;
		virtual void SetNamespace(const char* szNamespace) = 0;
		virtual const char* GetNamespace() const = 0;
		virtual void SetFileName(const char* szFileName, const char* szProjectDir) = 0;
		virtual const char* GetFileName() const = 0;
		virtual void SetAuthor(const char* szAuthor) = 0;
		virtual const char* GetAuthor() const = 0;
		virtual void SetDescription(const char* szDescription) = 0;
		virtual const char* GetDescription() const = 0;
		virtual void SetWikiLink(const char* szWikiLink) = 0;
		virtual const char* GetWikiLink() const = 0;
		virtual void SetFlags(EActionFlags flags) = 0;
		virtual EActionFlags GetFlags() const = 0;
		virtual IActionPtr CreateAction() const = 0;
		virtual IPropertiesPtr CreateProperties() const = 0;
	};

	DECLARE_SHARED_POINTERS(IActionFactory)
}
