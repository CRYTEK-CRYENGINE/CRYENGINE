// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "EntityComponentsVector.h"
#include "Entity.h"

SEntityComponentRecord::~SEntityComponentRecord()
{
	Shutdown();
}

void SEntityComponentRecord::Shutdown()
{
	if (pComponent != nullptr)
	{
		static_cast<CEntity*>(pComponent->GetEntity())->ShutDownComponent(*this);
	}
}