// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "NavigationSystemSchematyc.h"

#include "Components/Navigation/NavigationComponent.h"
#include "Components/Navigation/NavigationMarkupShapeComponent.h"

namespace NavigationComponentHelpers
{
	void SAgentTypesMask::Serialize(Serialization::IArchive& archive)
	{
		if (!archive.isEdit())
		{
			archive(mask, "mask");
			return;
		}

		NavigationSystem* pNavigationSystem = gAIEnv.pNavigationSystem;

		if (archive.isOutput())
		{
			const size_t agentCount = pNavigationSystem->GetAgentTypeCount();
			archive(agentCount, "AgentsCount", "");

			for (size_t i = 0; i < agentCount; ++i)
			{
				NavigationAgentTypeID agentID = pNavigationSystem->GetAgentTypeID(i);
				const char* szName = pNavigationSystem->GetAgentTypeName(agentID);

				bool bEnabled = (mask & BIT(i)) != 0;
				archive(bEnabled, szName, szName);
			}
		}
		else
		{
			size_t agentCount = 0;
			archive(agentCount, "AgentsCount", "");

			mask = 0;
			for (size_t i = 0; i < agentCount; ++i)
			{
				NavigationAgentTypeID agentID = pNavigationSystem->GetAgentTypeID(i);
				const char* szName = pNavigationSystem->GetAgentTypeName(agentID);

				bool bEnabled = false;
				archive(bEnabled, szName, szName);

				if (bEnabled)
				{
					mask |= BIT(i);
				}
			}
		}
	}

	bool Serialize(Serialization::IArchive& archive, SAnnotationFlagsMask& value, const char* szName, const char* szLabel)
	{
		return archive(NavigationSerialization::NavigationAreaFlagsMask(value.mask), szName, szLabel);
	}
}

namespace NavigationSystemSchematyc
{
	bool NearestNavmeshPositionSchematyc(NavigationAgentTypeID agentTypeID, const Vec3& location, float vrange, float hrange, Vec3& meshLocation)
	{
		//TODO: GetEnclosingMeshID(agentID, location); doesn't take into account vrange and hrange and can return false when point isn't inside mesh boundary
		return gAIEnv.pNavigationSystem->GetClosestPointInNavigationMesh(agentTypeID, location, vrange, hrange, &meshLocation);
	}

	bool TestRaycastHitOnNavmeshSchematyc(NavigationAgentTypeID agentTypeID, const Vec3& startPos, const Vec3& endPos, Vec3& hitPos)
	{
		//TODO: add query filter as a parameter
		MNM::SRayHitOutput hit;
		bool bResult = gAIEnv.pNavigationSystem->NavMeshTestRaycastHit(agentTypeID, startPos, endPos, nullptr, &hit);
		if (bResult)
		{
			hitPos = hit.position;
		}
		return bResult;
	}
	
	void Register(Schematyc::IEnvRegistrar& registrar, Schematyc::CEnvRegistrationScope& parentScope)
	{
		//Register Components
		CEntityAINavigationComponent::Register(registrar);
		CAINavigationMarkupShapeComponent::Register(registrar);

		const CryGUID NavigationSystemGUID = "ad6ac254-13b8-4a79-827c-cd6a5a8e89da"_cry_guid;

		parentScope.Register(SCHEMATYC_MAKE_ENV_MODULE(NavigationSystemGUID, "Navigation"));
		Schematyc::CEnvRegistrationScope navigationScope = registrar.Scope(NavigationSystemGUID);

		//Register Types
		navigationScope.Register(SCHEMATYC_MAKE_ENV_DATA_TYPE(NavigationAgentTypeID));
		navigationScope.Register(SCHEMATYC_MAKE_ENV_DATA_TYPE(NavigationAreaFlagID));
		
		navigationScope.Register(SCHEMATYC_MAKE_ENV_DATA_TYPE(CEntityAINavigationComponent::SMovementProperties));
		navigationScope.Register(SCHEMATYC_MAKE_ENV_DATA_TYPE(NavigationComponentHelpers::SAnnotationFlagsMask));

		//NearestNavmeshPositionSchematyc
		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&NearestNavmeshPositionSchematyc, "85938949-a02d-4596-9fe8-42d779eed458"_cry_guid, "NearestNavmeshPosition");
			pFunction->SetDescription("Returns the nearest position on navmesh within specified range.");
			pFunction->BindInput(1, 'agt', "AgentTypeId");
			pFunction->BindInput(2, 'loc', "Location");
			pFunction->BindInput(3, 'vr', "VerticalRange");
			pFunction->BindInput(4, 'hr', "HorizontalRange");
			pFunction->BindOutput(0, 'ret', "Found");
			pFunction->BindOutput(5, 'np', "NavmeshPosition");
			navigationScope.Register(pFunction);
		}

		//RaycastOnNavmeshSchematyc
		{
			auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&TestRaycastHitOnNavmeshSchematyc, "d68f9528-ebb9-4ced-8c6e-5c9b1614ab6f"_cry_guid, "TestRaycastHitOnNavmesh");
			pFunction->SetDescription("Performs raycast hit test on navmesh and returns true if the ray hits navmesh boundaries.");
			pFunction->BindInput(1, 'agt', "AgentTypeId");
			pFunction->BindInput(2, 'sp', "StartPosition");
			pFunction->BindInput(3, 'tp', "ToPosition");
			pFunction->BindOutput(0, 'ret', "IsHit");
			pFunction->BindOutput(4, 'hp', "HitPosition");
			navigationScope.Register(pFunction);
		}
	}
}
