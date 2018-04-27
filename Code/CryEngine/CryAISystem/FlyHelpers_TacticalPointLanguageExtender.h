// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __FLY_HELPERS__TACTICAL_POINT_LANGUAGE_EXTENDER__H__
#define __FLY_HELPERS__TACTICAL_POINT_LANGUAGE_EXTENDER__H__

#include <CryAISystem/ITacticalPointSystem.h>

namespace FlyHelpers
{

class CTacticalPointLanguageExtender
	: public ITacticalPointLanguageExtender
{
public:
	void         Initialize();
	void         Deinitialize();
	virtual bool GeneratePoints(TGenerateParameters& parameters, SGenerateDetails& details, TObjectType object, const Vec3& objectPos, TObjectType auxObject, const Vec3& auxObjectPos) const;

private:
	void RegisterWithTacticalPointSystem();
	void RegisterQueries();
	void UnregisterFromTacticalPointSystem();
};

}

#endif
