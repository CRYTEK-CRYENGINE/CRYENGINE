#include "StdAfx.h"
#include "TagPoint.h"

#include <CryAISystem/IAgent.h>

class CTagPointRegistrator
	: public IEntityRegistrator
{
	virtual void Register() override
	{
		if (gEnv->pEntitySystem->GetClassRegistry()->FindClass("TagPoint") != nullptr)
		{
			// Skip registration of default engine class if the game has overridden it
			CryLog("Skipping registration of default engine entity class TagPoint, overridden by game");
			return;
		}

		RegisterEntityWithDefaultComponent<CTagPoint>("TagPoint", "AI");
	}
};

CTagPointRegistrator g_tagPointRegistrator;

void CTagPoint::OnResetState()
{
	AIObjectParams params(AIOBJECT_WAYPOINT, 0, GetEntityId());

	// Register in AI to get a new AI object, deregistering the old one in the process
	GetEntity()->RegisterInAISystem(params);
}