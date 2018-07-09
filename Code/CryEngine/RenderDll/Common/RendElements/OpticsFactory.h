// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryRenderer/IFlares.h>

class COpticsFactory
{

private:
	COpticsFactory(){}

public:
	static COpticsFactory* GetInstance()
	{
		static COpticsFactory instance;
		return &instance;
	}

	IOpticsElementBase* Create(EFlareType type) const;
};
