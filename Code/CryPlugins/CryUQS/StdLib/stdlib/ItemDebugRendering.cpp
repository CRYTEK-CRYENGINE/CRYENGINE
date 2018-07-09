// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryEntitySystem/IEntitySystem.h>

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace StdLib
	{

		void EntityId_AddToDebugRenderWorld(const EntityIdWrapper& item, Core::IDebugRenderWorldPersistent& debugRW)
		{
			if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(item.value))
			{
				const Matrix34& worldTM = pEntity->GetWorldTM();
				AABB entityBounds(AABB::RESET);
				pEntity->GetLocalBounds(entityBounds);

				// ensure a halfway proper AABB
				if (entityBounds.IsReset() || entityBounds.IsEmpty())
				{
					static const Vec3 defaultSize(0.2f, 0.2f, 0.2f);
					entityBounds.min = -defaultSize;
					entityBounds.max = defaultSize;
				}

				entityBounds.Move(worldTM.GetTranslation());
				OBB obb;
				obb.SetOBBfromAABB(Matrix33(worldTM), entityBounds);

				debugRW.AddOBB(obb, Col_Blue);

				// 0.5 meter above the entity's AABB
				const Vec3 top((entityBounds.min.x + entityBounds.max.x) * 0.5f, (entityBounds.min.y + entityBounds.max.y) * 0.5f, entityBounds.max.z + 0.5f);

				static_assert(std::is_same<unsigned int, EntityId>::value, "Update printf format for entity id");
				debugRW.AddText(top, 1.5f, Col_BlueViolet, "%u: %s", item.value, pEntity->GetName());
			}
		}

		void Pos3_AddToDebugRenderWorld(const Pos3& item, Core::IDebugRenderWorldPersistent& debugRW)
		{
			debugRW.AddSphere(item.value, 0.2f, Col_White);
		}

	}
}
