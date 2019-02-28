// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifndef TACTICAL_POINT_LANGUAGE_EXTENDER_H
#define TACTICAL_POINT_LANGUAGE_EXTENDER_H

#include <CryAISystem/ITacticalPointSystem.h>

class CAIBattleFrontModule;

namespace FlankCalculator
{
	bool IsPointOnMyFlank(EntityId actorEntityID, const Vec3& actorPos, const Vec3& targetPos, const Vec3& pointPos);
}

class CTacticalPointLanguageExtender : public ITacticalPointLanguageExtender
{
public:
	CTacticalPointLanguageExtender();

	void Reset();
	void FullSerialize(TSerialize ser);

	void Initialize();
	void Deinitialize();
	virtual bool GeneratePoints(TGenerateParameters& parameters, SGenerateDetails& details, TObjectType object, const Vec3& objectPos, TObjectType auxObject, const Vec3& auxObjectPos) const;
	virtual bool GetObject(TObjectParameters& parameters) const;
	virtual bool BoolTest(TBoolParameters& params, TObjectType pObject, const Vec3& objPos, TPointType point) const;

private:
	void RegisterWithTacticalPointSystem();
	void RegisterQueries();
	void UnregisterFromTacticalPointSystem();
	void UnregisterQueries();
	IAIObject* GetBattleFrontObject() const;
	void ReleaseBattleFrontObject();

	mutable tAIObjectID m_battlefrontAIObject;
};

#endif // TACTICAL_POINT_LANGUAGE_EXTENDER_H
