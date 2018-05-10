// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : Rename IEnvClass/IEnvBaseClass?
// #SchematycTODO : Move IFoundationPreviewExtension to a separate header?

#pragma once

#include "CrySchematyc2/Prerequisites.h"

#include "CrySchematyc2/GUID.h"
#include "CrySchematyc2/IProperties.h"
#include "CrySchematyc2/Env/EnvTypeId.h"

struct SRendParams;
struct SRenderingPassInfo;

namespace Schematyc2
{
	struct IObject;

	struct IFoundationExtension
	{
		virtual ~IFoundationExtension() {}
	};

	DECLARE_SHARED_POINTERS(IFoundationExtension)

	struct IFoundationPreviewExtension : public IFoundationExtension
	{
		virtual ~IFoundationPreviewExtension() {}

		virtual IObject* BeginPreview(const SGUID& schemaGUID, EntityId selectedEntityId) = 0;
		virtual void RenderPreview(const SRendParams& params, const SRenderingPassInfo& passInfo) = 0;
		virtual void EndPreview() = 0;
	};

	DECLARE_SHARED_POINTERS(IFoundationPreviewExtension)

	struct IFoundation
	{
		virtual ~IFoundation() {}

		virtual SGUID GetGUID() const = 0;
		virtual const char* GetName() const = 0;
		virtual void SetDescription(const char* szDescription) = 0;
		virtual const char* GetDescription() const = 0;
		virtual void SetProperties(const IPropertiesPtr& pProperties) = 0;
		virtual IPropertiesConstPtr GetProperties() const = 0;
		virtual void UseNamespace(const char* szNamespace) = 0;
		virtual size_t GetNamespaceCount() const = 0;
		virtual const char* GetNamespace(size_t namespaceIdx) const = 0;
		virtual void AddAbstractInterface(const SGUID& guid) = 0;
		virtual size_t GetAbstractInterfaceCount() const = 0;
		virtual SGUID GetAbstractInterfaceGUID(size_t abstractInterfaceIdx) const = 0;
		virtual void AddComponent(const SGUID& guid) = 0;
		virtual size_t GetComponentCount() const = 0;
		virtual SGUID GetComponentGUID(size_t componentIdx) const = 0;

		template <typename INTERFACE> inline bool AddExtension(const std::shared_ptr<INTERFACE>& pExtension)
		{
			return AddExtension_Protected(GetEnvTypeId<INTERFACE>(), pExtension);
		}

		template <typename INTERFACE> inline std::shared_ptr<INTERFACE> QueryExtension() const
		{
			return std::static_pointer_cast<INTERFACE>(QueryExtension_Protected(GetEnvTypeId<INTERFACE>()));
		}

	protected:

		virtual bool AddExtension_Protected(const EnvTypeId& typeId, const IFoundationExtensionPtr& pExtension) = 0;
		virtual IFoundationExtensionPtr QueryExtension_Protected(const EnvTypeId& typeId) const = 0;
	};

	DECLARE_SHARED_POINTERS(IFoundation)

	namespace FoundationUtils
	{
		//////////////////////////////////////////////////////////////////////////
		inline size_t FindFoundationNamespace(const IFoundation& foundation, const char* szNamespace)
		{
			CRY_ASSERT(szNamespace);
			if(szNamespace)
			{
				for(size_t namespaceIdx = 0, namespaceCount = foundation.GetNamespaceCount(); namespaceIdx < namespaceCount; ++ namespaceIdx)
				{
					if(strcmp(foundation.GetNamespace(namespaceIdx), szNamespace) == 0)
					{
						return namespaceIdx;
					}
				}
			}
			return INVALID_INDEX;
		}

		//////////////////////////////////////////////////////////////////////////
		inline size_t FindFoundationComponent(const IFoundation& foundation, const SGUID& componentGUID)
		{
			for(size_t componentIdx = 0, componentCount = foundation.GetComponentCount(); componentIdx < componentCount; ++ componentIdx)
			{
				if(foundation.GetComponentGUID(componentIdx) == componentGUID)
				{
					return componentIdx;
				}
			}
			return INVALID_INDEX;
		}
	}
}
