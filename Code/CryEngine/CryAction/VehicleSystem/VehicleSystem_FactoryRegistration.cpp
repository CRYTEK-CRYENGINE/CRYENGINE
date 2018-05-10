// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "VehicleSystem.h"

#include "Vehicle.h"

#include "VehicleDamageBehaviorAISignal.h"
#include "VehicleDamageBehaviorDestroy.h"
#include "VehicleDamageBehaviorDetachPart.h"
#include "VehicleDamageBehaviorEffect.h"
#include "VehicleDamageBehaviorGroup.h"
#include "VehicleDamageBehaviorHitPassenger.h"
#include "VehicleDamageBehaviorImpulse.h"
#include "VehicleDamageBehaviorMovementNotification.h"
#include "VehicleDamageBehaviorSink.h"
#include "VehicleDamageBehaviorSpawnDebris.h"
#include "VehicleDamageBehaviorIndicator.h"
#include "VehicleDamageBehaviorDisableSeatAction.h"

#include "VehiclePartBase.h"
#include "VehiclePartAnimated.h"
#include "VehiclePartAnimatedJoint.h"
#include "VehiclePartLight.h"
#include "VehiclePartPulsingLight.h"
#include "VehiclePartMassBox.h"
#include "VehiclePartStatic.h"
#include "VehiclePartSubPart.h"
#include "VehiclePartSubPartWheel.h"
#include "VehiclePartSuspensionPart.h"
#include "VehiclePartTread.h"
#include "VehiclePartAttachment.h"
#include "VehiclePartEntity.h"
#include "VehiclePartEntityDelayedDetach.h"
#include "VehiclePartParticleEffect.h"
#include "VehiclePartAnimatedChar.h"
#include "VehiclePartWaterRipplesGenerator.h"
#include "VehiclePartDetachedEntity.h"

#include "VehicleSeatSerializer.h"

#include "VehicleSeatActionLights.h"
#include "VehicleSeatActionMovement.h"
#include "VehicleSeatActionPassengerIK.h"
#include "VehicleSeatActionRotateTurret.h"
#include "VehicleSeatActionSteeringWheel.h"
#include "VehicleSeatActionWeapons.h"
#include "VehicleSeatActionWeaponsBone.h"
#include "VehicleSeatActionAnimation.h"
#include "VehicleSeatActionPassiveAnimation.h"
#include "VehicleSeatActionOrientatePartToView.h"
#include "VehicleSeatActionOrientateBoneToView.h"
#include "VehicleSeatActionRotateBone.h"
#include "VehicleSeatActionShakeParts.h"

#include "VehicleUsableActionEnter.h"
#include "VehicleUsableActionFlip.h"

#include "VehicleViewActionThirdPerson.h"
#include "VehicleViewSteer.h"
#include "VehicleViewFirstPerson.h"
#include "VehicleViewThirdPerson.h"

#include "CryAction.h"

//------------------------------------------------------------------------
void CVehicleSystem::RegisterVehicles(IGameFramework* gameFramework)
{
	LOADING_TIME_PROFILE_SECTION;
	IEntityClassRegistry::SEntityClassDesc serializerClass;
	serializerClass.sName = "VehicleSeatSerializer";
	serializerClass.sScriptFile = "";
	serializerClass.flags = ECLF_INVISIBLE;

	static IGameFramework::CGameObjectExtensionCreator<CVehicleSeatSerializer> createVehicleSeatSerializer;
	CCryAction::GetCryAction()->GetIGameObjectSystem()->RegisterExtension(serializerClass.sName, &createVehicleSeatSerializer, &serializerClass);

	// register the detached part entity

	IEntityClassRegistry::SEntityClassDesc detachedPartClass;
	detachedPartClass.sName = "VehiclePartDetached";
	detachedPartClass.sScriptFile = "Scripts/Entities/Vehicles/VehiclePartDetached.lua";
	detachedPartClass.flags = ECLF_INVISIBLE;

	static IGameFramework::CGameObjectExtensionCreator<CVehiclePartDetachedEntity> createVehicleDetachedPartEntity;
	CCryAction::GetCryAction()->GetIGameObjectSystem()->RegisterExtension(detachedPartClass.sName, &createVehicleDetachedPartEntity, &detachedPartClass);

	// register all the vehicles

	ICryPak* pack = gEnv->pCryPak;
	_finddata_t fd;
	int res;
	intptr_t handle;
	std::set<string> setVehicles;

	string sExt(".xml");
	string sPath("Scripts/Entities/Vehicles/Implementations/Xml/");

	if ((handle = pack->FindFirst(sPath + string("*") + sExt, &fd)) != -1)
	{
		do
		{
			if (XmlNodeRef root = GetISystem()->LoadXmlFromFile(sPath + string(fd.name)))
			{
				const char* name = root->getAttr("name");
				if (name[0])
				{
					// Allow the name to contain relative path, but use only the name part as class name.
					string className(PathUtil::GetFile(name));

					// register only once
					std::pair<std::set<string>::iterator, bool> result = setVehicles.insert(className);
					if (result.second)
					{
						IEntityClassRegistry::SEntityClassDesc vehicleClass;
						vehicleClass.sName = className.c_str();

						char scriptName[1024];

						const char* isOld = root->getAttr("isOld");
						if (!strcmp("1", isOld))
							cry_sprintf(scriptName, "Scripts/Entities/Vehicles/Old/VehiclePool.lua");
						else
							cry_sprintf(scriptName, "Scripts/Entities/Vehicles/VehiclePool.lua");

						bool show = true;
						if (root->getAttr("show", show))
						{
							if (!show && VehicleCVars().v_show_all == 0)
								vehicleClass.flags |= ECLF_INVISIBLE;
						}

						vehicleClass.sScriptFile = scriptName;

						static IGameFramework::CGameObjectExtensionCreator<CVehicle> vehicleCreator;
						CCryAction::GetCryAction()->GetIGameObjectSystem()->RegisterExtension(name, &vehicleCreator, &vehicleClass);
						m_classes.insert(TVehicleClassMap::value_type(name, &vehicleCreator));
					}
					else
						CryLog("Vehicle <%s> already registered", name);
				}
				else
				{
					CryLog("VehicleSystem: %s is missing 'name' attribute, skipping", fd.name);
				}
			}
			res = pack->FindNext(handle, &fd);
		}
		while (res >= 0);

		pack->FindClose(handle);
	}

#define REGISTER_VEHICLEOBJECT(name, obj)                    \
  REGISTER_FACTORY((IVehicleSystem*)this, name, obj, false); \
  obj::m_objectId = this->AssignVehicleObjectId(name);

	// register other factories

	// vehicle views
	REGISTER_VEHICLEOBJECT("ActionThirdPerson", CVehicleViewActionThirdPerson);
	REGISTER_VEHICLEOBJECT("SteerThirdPerson", CVehicleViewSteer);
	REGISTER_VEHICLEOBJECT("FirstPerson", CVehicleViewFirstPerson);
	REGISTER_VEHICLEOBJECT("ThirdPerson", CVehicleViewThirdPerson);

	// vehicle parts
	REGISTER_VEHICLEOBJECT("Base", CVehiclePartBase);
	REGISTER_VEHICLEOBJECT("Animated", CVehiclePartAnimated);
	REGISTER_VEHICLEOBJECT("AnimatedJoint", CVehiclePartAnimatedJoint);
	REGISTER_VEHICLEOBJECT("SuspensionPart", CVehiclePartSuspensionPart);
	REGISTER_VEHICLEOBJECT("Light", CVehiclePartLight);
	REGISTER_VEHICLEOBJECT("PulsingLight", CVehiclePartPulsingLight);
	REGISTER_VEHICLEOBJECT("MassBox", CVehiclePartMassBox);
	REGISTER_VEHICLEOBJECT("Static", CVehiclePartStatic);
	REGISTER_VEHICLEOBJECT("SubPart", CVehiclePartSubPart);
	REGISTER_VEHICLEOBJECT("SubPartWheel", CVehiclePartSubPartWheel);
	REGISTER_VEHICLEOBJECT("Tread", CVehiclePartTread);
	REGISTER_VEHICLEOBJECT("EntityAttachment", CVehiclePartEntityAttachment);
	REGISTER_VEHICLEOBJECT("Entity", CVehiclePartEntity);
	REGISTER_VEHICLEOBJECT("EntityDelayedDetach", CVehiclePartEntityDelayedDetach);
	REGISTER_VEHICLEOBJECT("ParticleEffect", CVehiclePartParticleEffect);
	REGISTER_VEHICLEOBJECT("AnimatedChar", CVehiclePartAnimatedChar);
	REGISTER_VEHICLEOBJECT("WaterRipplesGenerator", CVehiclePartWaterRipplesGenerator);

	// vehicle damage behaviors
	REGISTER_VEHICLEOBJECT("AISignal", CVehicleDamageBehaviorAISignal);
	REGISTER_VEHICLEOBJECT("Destroy", CVehicleDamageBehaviorDestroy);
	REGISTER_VEHICLEOBJECT("DetachPart", CVehicleDamageBehaviorDetachPart);
	REGISTER_VEHICLEOBJECT("Effect", CVehicleDamageBehaviorEffect);
	REGISTER_VEHICLEOBJECT("Group", CVehicleDamageBehaviorGroup);
	REGISTER_VEHICLEOBJECT("HitPassenger", CVehicleDamageBehaviorHitPassenger);
	REGISTER_VEHICLEOBJECT("Impulse", CVehicleDamageBehaviorImpulse);
	REGISTER_VEHICLEOBJECT("Indicator", CVehicleDamageBehaviorIndicator);
	REGISTER_VEHICLEOBJECT("MovementNotification", CVehicleDamageBehaviorMovementNotification);
	REGISTER_VEHICLEOBJECT("Sink", CVehicleDamageBehaviorSink);
	REGISTER_VEHICLEOBJECT("SpawnDebris", CVehicleDamageBehaviorSpawnDebris);
	REGISTER_VEHICLEOBJECT("DisableSeatAction", CVehicleDamageBehaviorDisableSeatAction);

	// seat actions
	REGISTER_VEHICLEOBJECT("Lights", CVehicleSeatActionLights);
	REGISTER_VEHICLEOBJECT("Movement", CVehicleSeatActionMovement);
	REGISTER_VEHICLEOBJECT("PassengerIK", CVehicleSeatActionPassengerIK);
	REGISTER_VEHICLEOBJECT("RotateTurret", CVehicleSeatActionRotateTurret);
	REGISTER_VEHICLEOBJECT("SteeringWheel", CVehicleSeatActionSteeringWheel);
	REGISTER_VEHICLEOBJECT("Weapons", CVehicleSeatActionWeapons);
	REGISTER_VEHICLEOBJECT("WeaponsBone", CVehicleSeatActionWeaponsBone);
	REGISTER_VEHICLEOBJECT("Animation", CVehicleSeatActionAnimation);
	REGISTER_VEHICLEOBJECT("PassiveAnimation", CVehicleSeatActionPassiveAnimation);
	REGISTER_VEHICLEOBJECT("OrientatePartToView", CVehicleSeatActionOrientatePartToView);
	REGISTER_VEHICLEOBJECT("OrientateBoneToView", CVehicleSeatActionOrientateBoneToView);
	REGISTER_VEHICLEOBJECT("RotateBone", CVehicleSeatActionRotateBone);
	REGISTER_VEHICLEOBJECT("ShakeParts", CVehicleSeatActionShakeParts);

	// actions
	REGISTER_FACTORY((IVehicleSystem*)this, "Enter", CVehicleUsableActionEnter, false);
	REGISTER_FACTORY((IVehicleSystem*)this, "Flip", CVehicleUsableActionFlip, false);
}
