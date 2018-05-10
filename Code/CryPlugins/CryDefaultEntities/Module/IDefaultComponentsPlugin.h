#pragma once
#include <CrySystem/ICryPlugin.h>
#include "DefaultComponents/Cameras/ICameraManager.h"

class IPlugin_CryDefaultEntities
	: public Cry::IEnginePlugin
	, public ISystemEventListener
{
public:
	virtual ~IPlugin_CryDefaultEntities() { };

	virtual ICameraManager* GetICameraManager() = 0;
};