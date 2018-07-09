// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc2/Prerequisites.h"

#include "CrySchematyc2/GUID.h"
#include "CrySchematyc2/INetworkSpawnParams.h"
#include "CrySchematyc2/IProperties.h"

namespace Schematyc2
{
	struct IComponent;

	DECLARE_SHARED_POINTERS(IComponent)

	enum class EComponentDependencyType
	{
		None,
		Soft, // Dependency must be initialized before component.
		Hard  // Dependency must exist and be initialized before component.
	};

	enum class EComponentSocket : uint32
	{
		Parent = 0, // Socket used to attach component to parent.
		Child,      // Socket used to attach children to component.
		Count
	};

	enum class EComponentFlags
	{
		None                = 0,
		Singleton           = BIT(0), // Allow only of one instance of this component per object.
		HideInEditor        = BIT(1), // Hide component in editor.
		CreatePropertyGraph = BIT(2)  // Create property graph for each instance of this component.
	};

	DECLARE_ENUM_CLASS_FLAGS(EComponentFlags)

	struct IComponentFactory
	{
		virtual ~IComponentFactory() {}

		virtual CTypeInfo GetTypeInfo() const = 0; // #SchematycTODO : Do we really need CTypeInfo or will just EnvTypeId suffice?
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
		virtual void AddDependency(EComponentDependencyType type, const SGUID& guid) = 0;
		virtual uint32 GetDependencyCount() const = 0;
		virtual EComponentDependencyType GetDependencyType(uint32 dependencyIdx) const = 0;
		virtual SGUID GetDependencyGUID(uint32 dependencyIdx) const = 0;
		virtual void SetAttachmentType(EComponentSocket socket, const SGUID& attachmentTypeGUID) = 0;
		virtual SGUID GetAttachmentType(EComponentSocket socket) const = 0;
		virtual void SetFlags(EComponentFlags flags) = 0;
		virtual EComponentFlags GetFlags() const = 0;
		virtual IComponentPtr CreateComponent() const = 0;
		virtual CTypeInfo GetPropertiesTypeInfo() const = 0; // #SchematycTODO : Do we really need CTypeInfo or will just EnvTypeId suffice?
		virtual IPropertiesPtr CreateProperties() const = 0;
		virtual void SetDefaultNetworkSpawnParams(const INetworkSpawnParamsPtr& pSpawnParams) = 0;
		virtual INetworkSpawnParamsPtr CreateNetworkSpawnParams() const = 0;
	};

	DECLARE_SHARED_POINTERS(IComponentFactory)

	namespace EnvComponentUtils
	{
		inline bool IsDependency(const IComponentFactory& componentFactory, const SGUID& guid)
		{
			for(uint32 dependencyIdx = 0, dependencyCount = componentFactory.GetDependencyCount(); dependencyIdx < dependencyCount; ++ dependencyIdx)
			{
				if(componentFactory.GetDependencyGUID(dependencyIdx) == guid)
				{
					return true;
				}
			}
			return false;
		}
	}
}
