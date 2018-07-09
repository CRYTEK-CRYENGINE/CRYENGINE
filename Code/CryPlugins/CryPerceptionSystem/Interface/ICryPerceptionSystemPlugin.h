// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/ICryPlugin.h>

struct IPerceptionManager;

struct ICryPerceptionSystemPlugin : public Cry::IEnginePlugin
{
	CRYINTERFACE_DECLARE_GUID(ICryPerceptionSystemPlugin, "30c9bf81-e5fd-42df-ac3c-bc5fc1e6f0b4"_cry_guid);

public:
	virtual IPerceptionManager& GetPerceptionManager() const = 0;
};
