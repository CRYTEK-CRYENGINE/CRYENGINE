// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements vehicle functionality

   -------------------------------------------------------------------------
   History:
   - 05:10:2004: Created by Mathieu Pinard

*************************************************************************/
#include "StdAfx.h"

#include <IViewSystem.h>
#include <IItemSystem.h>
#include <IVehicleSystem.h>
#include <CryPhysics/IPhysics.h>
#include <CryAnimation/ICryAnimation.h>
#include <IActorSystem.h>
#include <CryNetwork/ISerialize.h>
#include <CryAISystem/IAgent.h>

#include "IGameRulesSystem.h"

#include "VehicleSystem.h"
#include "ScriptBind_Vehicle.h"
#include "ScriptBind_VehicleSeat.h"
#include "VehicleCVars.h"
#include "VehicleDamagesGroup.h"
#include "Vehicle.h"
#include "VehicleAnimation.h"
#include "VehicleComponent.h"
#include "VehicleHelper.h"
#include "VehicleSeat.h"
#include "VehicleSeatGroup.h"
#include "VehicleSeatActionWeapons.h"
#include "VehicleSeatActionRotateTurret.h"

#include "VehiclePartAnimated.h"
#include "VehiclePartAnimatedJoint.h"
#include "VehiclePartSubPartWheel.h"
#include "VehiclePartTread.h"
#include "VehicleUtils.h"

#include "CryAction.h"
#include "Serialization/XMLScriptLoader.h"
#include "PersistantDebug.h"
#include <CryAISystem/IAIObject.h>

#include "CryActionCVars.h"

#define ENGINESLOT_MINSPEED 0.05f

// Default distance ratio for vehicles.
#define DEFAULT_VEHICLE_VIEW_DIST_RATIO (50)

int CVehicle::s_repairHitTypeId = 0;
int CVehicle::s_disableCollisionsHitTypeId = 0;
int CVehicle::s_collisionHitTypeId = 0;
int CVehicle::s_normalHitTypeId = 0;
int CVehicle::s_fireHitTypeId = 0;
int CVehicle::s_punishHitTypeId = 0;
int CVehicle::s_vehicleDestructionTypeId = 0;

const static string s_vehicleImplXmlDir = "Scripts/Entities/Vehicles/Implementations/Xml/";

namespace Veh
{
void RegisterEvents(IGameObjectExtension& goExt, IGameObject& gameObject)
{
	const int eventToRegister[] =
	{
		eGFE_OnPostStep,
		eGFE_OnCollision,
		eGFE_OnStateChange,
		eGFE_OnBecomeVisible,
		eGFE_PreShatter,
		eGFE_StoodOnChange
	};

	gameObject.UnRegisterExtForEvents(&goExt, NULL, 0);
	gameObject.RegisterExtForEvents(&goExt, eventToRegister, sizeof(eventToRegister) / sizeof(int));
}
}

//------------------------------------------------------------------------
CVehicle::CVehicle() :
	m_mass(0.0f),
	m_wheelCount(0),
	m_pMovement(NULL),
	m_ownerId(0),
	m_customEngineSlot(false),
	m_bRetainGravity(false),
	m_usesVehicleActionToEnter(false),
	m_isDestroyed(false),
	m_initialposition(0.0f),
	m_lastWeaponId(0),
	m_pIEntityAudioComponent(),
	//m_abandonedSoundId(INVALID_SOUNDID),
	m_bNeedsUpdate(true),
	m_lastFrameId(0),
	m_pInventory(NULL),
	m_collisionDisabledTime(0.0f),
	m_indestructible(false),
	m_pVehicleSystem(NULL),
	m_damageMax(0.f),
	m_pPaintMaterial(NULL),
	m_pDestroyedMaterial(NULL),
	m_eventListeners(3),
	m_unmannedflippedThresholdDot(0.1f),
	m_ParentId(0),
	m_bCanBeAbandoned(true),
	m_isDestroyable(true),
	m_hasAuthority(false),
	m_smoothedPing(0.0f),
	m_clientSmoothedPosition(IDENTITY),
	m_clientPositionError(IDENTITY)
{
	m_gravity.zero();
	m_physUpdateTime = 0.f;

#if ENABLE_VEHICLE_DEBUG
	m_debugIndex = 0;
#endif
}

//------------------------------------------------------------------------
unsigned int CVehicle::GetPartChildrenCount(IVehiclePart* pParentPart)
{
	TVehiclePartVector::iterator partIte = m_parts.begin();
	TVehiclePartVector::iterator partEnd = m_parts.end();

	unsigned int counter = 0;

	for (; partIte != partEnd; ++partIte)
	{
		IVehiclePart* pPart = partIte->second;

		if (pPart && pPart->GetParent() == pParentPart)
			counter++;
	}

	return counter;
}

//------------------------------------------------------------------------
CVehicle::~CVehicle()
{
	CVehicleSystem* pVehicleSystem = (CVehicleSystem*)m_pVehicleSystem;
	if (pVehicleSystem->GetCurrentClientVehicle() == this)
	{
		// If another thread is currently doing things with the vehicle wait until it has finished
		CryAutoCriticalSection lock(pVehicleSystem->GetCurrentVehicleLock());
		pVehicleSystem->ClearCurrentClientVehicle();
	}

	for (TVehicleSeatVector::iterator it = m_seats.begin(), end = m_seats.end(); it != end; ++it)
	{
		if (it->second->GetPassenger())
			it->second->Exit(false, true);
	}

	SVehicleEventParams pa;
	BroadcastVehicleEvent(eVE_PreVehicleDeletion, pa);

	KillTimers();

	for (TVehicleSoundEventId id = 0; id < m_soundEvents.size(); ++id)
		StopSound(id);

	ReleaseDamages();

	SVehicleEventParams eventParams;
	BroadcastVehicleEvent(eVE_VehicleDeleted, eventParams);

	if (SpawnAndDeleteEntities(true))
	{
		// delete AI anchors
		IEntityClass* pAnchorClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("AIAnchor");

		int nChildren = GetEntity()->GetChildCount();
		for (int i = nChildren - 1; i >= 0; --i)
		{
			IEntity* pEnt = GetEntity()->GetChild(i);

			if (pEnt && pEnt->GetClass() == pAnchorClass)
			{
				gEnv->pEntitySystem->RemoveEntity(pEnt->GetId(), true);
			}
		}
	}

	TVehicleActionVector::iterator actionIte = m_actions.begin();
	TVehicleActionVector::iterator actionEnd = m_actions.end();
	for (; actionIte != actionEnd; ++actionIte)
	{
		SVehicleActionInfo& actionInfo = *actionIte;
		actionInfo.pAction->Release();
	}

	for (TVehicleSeatVector::iterator ite = m_seats.begin(); ite != m_seats.end(); ++ite)
	{
		ite->second->Release();
	}

	for (TVehicleHelperMap::iterator it = m_helpers.begin(); it != m_helpers.end(); ++it)
	{
		SAFE_RELEASE(it->second);
	}
	stl::free_container(m_helpers);

	std::for_each(m_components.begin(), m_components.end(), stl::container_object_deleter());

	TVehiclePartVector::iterator partIte;
	TVehiclePartVector::iterator partEnd;

	bool isEmpty;

	do
	{
		partIte = m_parts.begin();
		partEnd = m_parts.end();

		isEmpty = true;

		for (; partIte != partEnd; ++partIte)
		{
			IVehiclePart* pPart = partIte->second;
			if (pPart)
			{
				if (GetPartChildrenCount(pPart) == 0)
				{
					pPart->Release();
					partIte->second = NULL;
				}
				else
				{
					isEmpty = false;
				}
			}
		}
	}
	while (m_parts.size() > 0 && isEmpty != true);

	m_parts.clear();

	if (m_pMovement)
	{
		GetGameObject()->EnablePhysicsEvent(false, eEPE_OnPostStepImmediate | eEPE_OnPostStepLogged);
		SAFE_RELEASE(m_pMovement);
	}

	if (m_pInventory)
	{
		if (gEnv->bServer)
			m_pInventory->Destroy();

		GetGameObject()->ReleaseExtension("Inventory");
	}

	CCryAction::GetCryAction()->GetIVehicleSystem()->RemoveVehicle(GetEntityId());

	GetGameObject()->EnablePhysicsEvent(false, eEPE_OnCollisionLogged | eEPE_OnStateChangeLogged);
	//GetGameObject()->DisablePostUpdates(this);

	for (TVehicleAnimationsVector::iterator it = m_animations.begin(); it != m_animations.end(); ++it)
	{
		it->second->Release();
	}
}

//------------------------------------------------------------------------
bool CVehicle::SpawnAndDeleteEntities(bool clientBasedOnly)
{
	if (!clientBasedOnly)
		return !(GetISystem()->IsSerializingFile() || IsDemoPlayback());
	return !(GetISystem()->IsSerializingFile());
}

//------------------------------------------------------------------------
void CVehicle::LoadParts(const CVehicleParams& table, IVehiclePart* pParent, SPartInitInfo& initInfo)
{
	int c = table.getChildCount();
	int i = 0;
	m_parts.reserve(m_parts.capacity() + c);

	for (; i < c; i++)
	{
		CVehicleParams partTable = table.getChild(i);
		if (IVehiclePart* pPart = AddPart(partTable, pParent, initInfo))
		{
			if (CVehicleParams childPartsTable = partTable.findChild("Parts"))
				LoadParts(childPartsTable, pPart, initInfo);
		}
	}
}

namespace
{
//------------------------------------------------------------------------
XmlNodeRef LoadXmlForClass(const char* className, const string& xmlDir)
{
	// try to load file with same filename as class name
	string xmlFile = xmlDir + className + ".xml";

	IGameRules* pGameRules = CCryAction::GetCryAction()->GetIGameRulesSystem()->GetCurrentGameRules();

	XmlNodeRef xmlData = (pGameRules ? pGameRules->FindPrecachedXmlFile(xmlFile.c_str()) : (XmlNodeRef) 0);
	if (0 == xmlData)
	{
		// xml file didn't exist in precache (or precache doesn't exist!), so just load it as normal
		xmlData = gEnv->pSystem->LoadXmlFromFile(xmlFile.c_str());
	}

	if (0 == xmlData)
	{
		// if not found, search xml dir for files with name="<class>" attribute
		_finddata_t fd;
		intptr_t handle;
		int res;

		if ((handle = gEnv->pCryPak->FindFirst(xmlDir + "*.xml", &fd)) != -1)
		{
			do
			{
				if (XmlNodeRef root = GetISystem()->LoadXmlFromFile(xmlDir + string(fd.name)))
				{
					const char* name = root->getAttr("name");
					if (0 == strcmp(name, className))
					{
						xmlData = root;
						break;
					}
				}
				res = gEnv->pCryPak->FindNext(handle, &fd);
			}
			while (res >= 0);

			gEnv->pCryPak->FindClose(handle);
		}
	}

	return xmlData;
}
}

//------------------------------------------------------------------------
void CVehicle::SerializeSpawnInfo(TSerialize ser)
{
	CRY_ASSERT(ser.IsReading());
	ser.Value("modifications", m_modifications);
	ser.Value("paint", m_paintName);
	ser.Value("parentId", m_ParentId, 'eid');
}

namespace CVehicleGetSpawnInfo
{
struct SInfo : public ISerializableInfo
{
	string   modifications;
	string   paint;
	EntityId parentId;
	void SerializeWith(TSerialize ser)
	{
		ser.Value("modifications", modifications);
		ser.Value("paint", paint);
		ser.Value("parentId", parentId, 'eid');
	}
};
}
//------------------------------------------------------------------------
ISerializableInfoPtr CVehicle::GetSpawnInfo()
{
	CVehicleGetSpawnInfo::SInfo* p = new CVehicleGetSpawnInfo::SInfo;
	p->modifications = m_modifications;
	p->paint = m_paintName;
	p->parentId = m_ParentId;
	return p;
}

//------------------------------------------------------------------------
bool CVehicle::Init(IGameObject* pGameObject)
{
	CryLog("Init vehicle: %s", pGameObject->GetEntity()->GetEntityTextDescription().c_str());
	INDENT_LOG_DURING_SCOPE();

	SetGameObject(pGameObject);

	IEntity* pEntity = GetEntity();
	CRY_ASSERT(pEntity == pGameObject->GetEntity());

	if (s_disableCollisionsHitTypeId == 0 || s_repairHitTypeId == 0 || s_collisionHitTypeId == 0
	    || s_normalHitTypeId == 0 || s_fireHitTypeId == 0 || s_punishHitTypeId == 0)
	{
		IGameRules* pGR = CCryAction::GetCryAction()->GetIGameRulesSystem()->GetCurrentGameRules();
		if (pGR)
		{
			s_repairHitTypeId = pGR->GetHitTypeId("repair");
			s_disableCollisionsHitTypeId = pGR->GetHitTypeId("disableCollisions");
			s_collisionHitTypeId = pGR->GetHitTypeId("collision");
			s_normalHitTypeId = pGR->GetHitTypeId("normal");
			s_fireHitTypeId = pGR->GetHitTypeId("fire");
			s_punishHitTypeId = pGR->GetHitTypeId("punish");
			s_vehicleDestructionTypeId = pGR->GetHitTypeId("vehicleDestruction");

			assert(s_repairHitTypeId && s_disableCollisionsHitTypeId && s_collisionHitTypeId
			       && s_normalHitTypeId && s_fireHitTypeId && s_punishHitTypeId);
		}
		CRY_ASSERT_MESSAGE(pGR, "No valid game rules set!");
	}

	if (gEnv->bMultiplayer && (CCryActionCVars::Get().g_multiplayerEnableVehicles == 0))
	{
		static IEntityClass* pMPGunshipClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("MP_PerkAlienGunship");

		if (pMPGunshipClass != pEntity->GetClass())
		{
			GameWarning("!Vehicles disabled in multiplayer, not spawning entity '%s'", pEntity->GetName());
			return false;
		}
	}

	m_pIEntityAudioComponent = pEntity->GetOrCreateComponent<IEntityAudioComponent>();
	CRY_ASSERT(m_pIEntityAudioComponent && "no sound proxy on entity");

	m_pVehicleSystem = CCryAction::GetCryAction()->GetIVehicleSystem();
	m_engineSlotBySpeed = true;

	m_parts.clear();
	m_seats.clear();

	CCryAction::GetCryAction()->GetIVehicleSystem()->AddVehicle(GetEntityId(), this);
	CCryAction::GetCryAction()->GetVehicleScriptBind()->AttachTo(this);

	m_pInventory = static_cast<IInventory*>(GetGameObject()->AcquireExtension("Inventory"));

	InitRespawn();

	const char* className = pEntity->GetClass()->GetName();

	// load the xml data into the vehicle lua table
	XmlNodeRef vehicleXmlData = LoadXmlForClass(className, s_vehicleImplXmlDir);

	if (!vehicleXmlData)
	{
		GameWarning("<%s>: failed loading xml file (directory %s), aborting initialization", pEntity->GetName(), s_vehicleImplXmlDir.c_str());
		return false;
	}

	// if modification is used, merge its elements with main tree
	if (m_modifications.empty())
	{
		IScriptTable* pScriptTable = pEntity->GetScriptTable();
		if (pScriptTable)
		{
			SmartScriptTable pProps;
			if (pScriptTable->GetValue("Properties", pProps))
			{
				const char* modification = 0;
				if (pProps->GetValue("Modification", modification))
				{
					//Trim out all whitespace (as the modification name is used for sending over the network, shared params and more)
					stack_string mods(modification);
					mods = mods.replace(" ", "");
					m_modifications = mods.c_str();
				}
			}
		}
	}

	if (m_paintName.empty())
	{
		IScriptTable* pScriptTable = pEntity->GetScriptTable();
		if (pScriptTable)
		{
			SmartScriptTable pProps;
			if (pScriptTable->GetValue("Properties", pProps))
			{
				const char* paint = 0;
				if (pProps->GetValue("Paint", paint))
					m_paintName = paint;
			}
		}
	}

	// this merges modification data and must be done before further processing
	//InitModification(xmlData, m_modification.c_str());
	CVehicleModificationParams vehicleModificationParams(vehicleXmlData, m_modifications.c_str());
	CVehicleParams vehicleParams(vehicleXmlData, vehicleModificationParams);

	vehicleParams.getAttr("isDestroyable", m_isDestroyable);

	// read physics properties

	if (CVehicleParams physicsTable = vehicleParams.findChild("Physics"))
	{
		if (CVehicleParams buoyancyTable = physicsTable.findChild("Buoyancy"))
		{
			buoyancyTable.getAttr("waterDamping", m_buoyancyParams.waterDamping);
			buoyancyTable.getAttr("waterDensity", m_buoyancyParams.waterDensity);
			buoyancyTable.getAttr("waterResistance", m_buoyancyParams.waterResistance);
		}

		physicsTable.getAttr("damping", m_simParams.damping);
		physicsTable.getAttr("dampingFreefall", m_simParams.dampingFreefall);
		physicsTable.getAttr("gravityFreefall", m_simParams.gravityFreefall);
		physicsTable.getAttr("retainGravity", m_bRetainGravity);

		if (CVehicleParams simulationTable = physicsTable.findChild("Simulation"))
		{
			simulationTable.getAttr("maxTimeStep", m_simParams.maxTimeStep);
			simulationTable.getAttr("minEnergy", m_simParams.minEnergy);
			simulationTable.getAttr("maxLoggedCollisions", m_simParams.maxLoggedCollisions);

			if (VehicleCVars().v_vehicle_quality == 1)
				m_simParams.maxTimeStep = max(m_simParams.maxTimeStep, 0.04f);
		}

		bool pushable = false;
		physicsTable.getAttr("pushable", pushable);
		if (!pushable)
			m_paramsFlags.flagsAND = ~pef_pushable_by_players;
	}

	// load parts
	SPartInitInfo partInitInfo;

	if (CVehicleParams partsTable = vehicleParams.findChild("Parts"))
		LoadParts(partsTable, NULL, partInitInfo);
	else
		CryLog("%s: No Parts table found!", pEntity->GetName());

	for (TVehiclePartVector::iterator ite = m_parts.begin(); ite != m_parts.end(); ++ite)
	{
		IVehiclePart* pPart = ite->second;
		pPart->ChangeState(IVehiclePart::eVGS_Default);
	}

	InitHelpers(vehicleParams);

	// Init the mannequin data
	CVehicleParams mannequinTable = vehicleParams.findChild("Mannequin");
	if (mannequinTable && CVehicleCVars::Get().v_enableMannequin)
	{
		m_vehicleAnimation.Initialise(*this, mannequinTable);
	}

	// load the components (after the parts)
	if (CVehicleParams componentsTable = vehicleParams.findChild("Components"))
	{
		int componentCount = componentsTable.getChildCount();
		m_components.reserve(componentCount);

		int i = 0;
		for (; i < componentCount; i++)
		{
			if (CVehicleParams componentTable = componentsTable.getChild(i))
			{
				AddComponent(componentTable);
			}
		}

		// register parts at their components
		for (std::vector<SPartComponentPair>::const_iterator it = partInitInfo.partComponentMap.begin(), end = partInitInfo.partComponentMap.end(); it != end; ++it)
		{
			CVehicleComponent* pVehicleComponent = static_cast<CVehicleComponent*>(GetComponent(it->component.c_str()));
			if (pVehicleComponent)
				pVehicleComponent->AddPart(it->pPart);
		}
	}

	InitParticles(vehicleParams); // must be after part init because it needs to find the helpers!

	// Init Animations
	if (CVehicleParams animationsTable = vehicleParams.findChild("Animations"))
	{
		int i = 0;
		int c = animationsTable.getChildCount();
		m_animations.reserve(c);

		for (; i < c; i++)
		{
			if (CVehicleParams animTable = animationsTable.getChild(i))
			{
				IVehicleAnimation* pVehicleAnimation = new CVehicleAnimation;

				if (animTable.haveAttr("name") && pVehicleAnimation->Init(this, animTable))
					m_animations.push_back(TVehicleStringAnimationPair(animTable.getAttr("name"), pVehicleAnimation));
				else
					delete pVehicleAnimation;
			}
		}
	}

	// Init the movement
	if (CVehicleParams movementsTable = vehicleParams.findChild("MovementParams"))
	{
		int i = 0;
		int c = movementsTable.getChildCount();

		for (; i < c; i++)
		{
			if (CVehicleParams moveTable = movementsTable.getChild(i))
			{
				if (!SetMovement(moveTable.getTag(), moveTable))
					return false;
			}
		}
	}

	// load the seats, if any
	if (CVehicleParams seatsTable = vehicleParams.findChild("Seats"))
	{
		int c = seatsTable.getChildCount();
		m_seats.reserve(c);

		int i = 0;
		for (; i < c; i++)
		{
			if (CVehicleParams seatRef = seatsTable.getChild(i))
			{
				CVehicleSeat* pVehicleSeat = new CVehicleSeat;
				CRY_ASSERT(pVehicleSeat != NULL);

				if (seatRef.haveAttr("name") && pVehicleSeat)
				{
					if (pVehicleSeat->Init(this, m_seats.size() + 1, seatRef))
					{
						m_seats.push_back(TVehicleSeatPair(seatRef.getAttr("name"), pVehicleSeat));
					}
					else
					{
						SAFE_DELETE(pVehicleSeat);
					}
				}
			}
		}

		m_transitionInfo.resize(c);

		i = 0;
		for (; i < c; i++)
		{
			if (CVehicleParams seatRef = seatsTable.getChild(i))
			{
				if (IVehicleSeat* pVehicleSeat = GetSeatById(i + 1))
				{
					STransitionInfo& info = m_transitionInfo[i];

					if (CVehicleParams transitionTable = seatRef.findChild("Transitions"))
					{
						if (CVehicleParams waitTable = transitionTable.findChild("WaitFor"))
						{
							unsigned int waitCount = waitTable.getChildCount();
							info.waitFor.reserve(waitCount);

							// if "Transitions" table is specified, read the seats from there.
							for (int w = 1, nWait = waitCount; w <= nWait; ++w)
							{
								CVehicleParams waitRef = waitTable.getChild(w);

								TVehicleSeatId waitForId = GetSeatId(waitRef.getAttr("value"));
								if (waitForId != InvalidVehicleSeatId)
									info.waitFor.push_back(waitForId);
							}
						}
					}
					else
					{
						// else add all seats that use the same exit helper as the current one
						for (TVehicleSeatVector::const_iterator it = m_seats.begin(), end = m_seats.end(); it != end; ++it)
						{
							if (it->second == pVehicleSeat)
								continue;

							if (IVehicleHelper* pHelper = it->second->GetExitHelper())
							{
								if (pHelper == static_cast<CVehicleSeat*>(pVehicleSeat)->GetExitHelper())
									info.waitFor.push_back(it->second->GetSeatId());
							}
						}
					}
				}
			}
		}
	}

	if (CVehicleParams seatGroupsTable = vehicleParams.findChild("SeatGroups"))
	{
		int c = seatGroupsTable.getChildCount();
		int i = 0;

		m_seatGroups.reserve(c);

		for (; i < c; i++)
		{
			if (CVehicleParams seatGroupTable = vehicleParams.getChild(i))
			{
				CVehicleSeatGroup* pVehicleSeatGroup = new CVehicleSeatGroup;

				if (pVehicleSeatGroup->Init(this, seatGroupTable))
					m_seatGroups.push_back(pVehicleSeatGroup);
				else
					delete pVehicleSeatGroup;
			}
		}
	}

	if (CVehicleParams inventoryTable = vehicleParams.findChild("Inventory"))
	{
		if (CVehicleParams ammosTable = inventoryTable.findChild("AmmoTypes"))
		{
			int i = 0;
			int c = ammosTable.getChildCount();

			for (; i < c; i++)
			{
				if (CVehicleParams ammoTable = ammosTable.getChild(i))
				{
					string type;
					int capacity = 0;

					if ((type = ammoTable.getAttr("type")) && ammoTable.getAttr("capacity", capacity))
					{
						IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(type);

						if (pClass)
							SetAmmoCapacity(pClass, capacity);
						else
							GameWarning("Could not find entity class for ammo type '%s' when initializing vehicle '%s' of class '%s'", type.c_str(), pEntity->GetName(), className);
					}
				}
			}
		}
	}

	InitPaint(vehicleParams);
	InitActions(vehicleParams);

	m_actionMap = vehicleParams.getAttr("actionMap");

	pEntity->AddFlags(ENTITY_FLAG_CASTSHADOW | ENTITY_FLAG_CUSTOM_VIEWDIST_RATIO | ENTITY_FLAG_TRIGGER_AREAS);
	//////////////////////////////////////////////////////////////////////////
	{
		int viewDistRatio = DEFAULT_VEHICLE_VIEW_DIST_RATIO;
		vehicleParams.getAttr("viewDistRatio", viewDistRatio);
		pEntity->SetViewDistRatio(viewDistRatio);
	}
	//////////////////////////////////////////////////////////////////////////

	InitDamages(this, vehicleParams);

#if ENABLE_VEHICLE_DEBUG
	if (VehicleCVars().v_debugdraw == eVDB_Parts)
	{
		for (TVehiclePartVector::iterator it = m_parts.begin(); it != m_parts.end(); ++it)
		{
			if (CVehiclePartAnimated* pPart = CAST_VEHICLEOBJECT(CVehiclePartAnimated, it->second))
				pPart->Dump();
		}
	}
#endif

	// calculate total damage
	m_damageMax = 0.0f;
	m_majorComponentDamageMax = 0.0f;
	for (TVehicleComponentVector::iterator ite = m_components.begin(); ite != m_components.end(); ++ite)
	{
		m_damageMax += (*ite)->GetMaxDamage();
		if ((*ite)->IsMajorComponent())
			m_majorComponentDamageMax += (*ite)->GetMaxDamage();
	}

	// each component should also record the percentage it contributes to the total vehicle health
	if (m_majorComponentDamageMax > 0)
	{
		for (TVehicleComponentVector::iterator ite = m_components.begin(); ite != m_components.end(); ++ite)
		{
			if ((*ite)->IsMajorComponent())
			{
				float proportion = (*ite)->GetMaxDamage() / m_majorComponentDamageMax;
				(*ite)->SetProportionOfVehicleHealth(proportion);
			}
		}
	}

	// attach seat scriptbinds
	// this is done during OnSpawn when spawning initially, or here in case only the extension is created
	for (TVehicleSeatVector::iterator it = m_seats.begin(); it != m_seats.end(); ++it)
		CCryAction::GetCryAction()->GetVehicleSeatScriptBind()->AttachTo(this, GetSeatId(it->second));

	if (0 == (pEntity->GetFlags() & (ENTITY_FLAG_CLIENT_ONLY | ENTITY_FLAG_SERVER_ONLY)))
	{
		if (!GetGameObject()->BindToNetwork())
		{
			GameWarning("<%s> BindToNetwork failed", pEntity->GetName());
			return false;
		}
	}

	if (VehicleCVars().v_serverControlled)
	{
		pGameObject->EnableDelegatableAspect(eEA_Physics, false);
	}

	return true;
}

//------------------------------------------------------------------------
void CVehicle::OnSpawnComplete()
{
	for (TVehicleSeatVector::iterator it = m_seats.begin(), end = m_seats.end(); it != end; ++it)
	{
		it->second->OnSpawnComplete();
	}
}

//------------------------------------------------------------------------
void CVehicle::InitPaint(const CVehicleParams& xmlContent)
{
	// check and apply "paint"
	if (IScriptTable* pScriptTable = GetEntity()->GetScriptTable())
	{
		if (CVehicleParams paintsTable = xmlContent.findChild("Paints"))
		{
			int i = 0;
			int c = paintsTable.getChildCount();

			for (; i < c; i++)
			{
				if (CVehicleParams paintRef = paintsTable.getChild(i))
				{
					const char* paintName = paintRef.getAttr("name");
					if (!stricmp(paintName, m_paintName.c_str()))
					{
						const char* paintMaterial = paintRef.getAttr("material");
						if (paintMaterial && paintMaterial[0])
						{
							m_pPaintMaterial = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(paintMaterial, false);
							if (m_pPaintMaterial)
								GetEntity()->SetMaterial(m_pPaintMaterial);
							else
								CryLog("[CVehicle::Paint] <%s> Material %s for Paint %s not found", GetEntity()->GetName(), paintMaterial, m_paintName.c_str());
						}

						const char* destroyedMaterialName = paintRef.getAttr("materialDestroyed");
						if (destroyedMaterialName && destroyedMaterialName[0])
						{
							m_pDestroyedMaterial = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(destroyedMaterialName, false);
						}

						break;
					}
				}
			}
		}
	}
}

//------------------------------------------------------------------------
bool CVehicle::InitActions(const CVehicleParams& vehicleTable)
{
	CVehicleParams actionsTable = vehicleTable.findChild("Actions");
	if (!actionsTable)
		return false;

	int c = actionsTable.getChildCount();
	m_actions.reserve(c);

	for (int i = 0; i < c; i++)
	{
		if (CVehicleParams actionRef = actionsTable.getChild(i))
		{
			if (actionRef.haveAttr("class"))
			{
				string className = actionRef.getAttr("class");
				if (IVehicleAction* pAction = m_pVehicleSystem->CreateVehicleAction(className))
				{
					if (pAction->Init(this, actionRef))
					{
						m_actions.resize(m_actions.size() + 1);
						SVehicleActionInfo& actionInfo = m_actions.back();

						actionInfo.pAction = pAction;

						if (className == "Enter")
							m_usesVehicleActionToEnter = true;

						actionInfo.useWhenFlipped = (className == "Flip") ? 1 : 0;

						if (CVehicleParams activationsTable = actionRef.findChild("Activations"))
						{
							int numActivations = activationsTable.getChildCount();
							actionInfo.activations.resize(numActivations);

							for (int n = 0; n < numActivations; n++)
							{
								if (CVehicleParams activationRef = activationsTable.getChild(n))
								{
									SActivationInfo& activationInfo = actionInfo.activations[n];

									string type = activationRef.getAttr("type");
									if (!type.empty())
									{
										if (type == "OnUsed")
											activationInfo.m_type = SActivationInfo::eAT_OnUsed;
										else if (type == "OnGroundCollision")
											activationInfo.m_type = SActivationInfo::eAT_OnGroundCollision;
									}

									string param1 = activationRef.getAttr("param1");
									if (!param1.empty())
									{
										if (param1 == "part")
											activationInfo.m_param1 = SActivationInfo::eAP1_Part;
										else if (param1 == "component")
											activationInfo.m_param1 = SActivationInfo::eAP1_Component;
									}

									activationInfo.m_param2 = activationRef.getAttr("param2");

									if (!activationRef.getAttr("distance", activationInfo.m_distance))
										activationInfo.m_distance = 1.5f;
								}
							}
						}
					}
					else
					{
						// Failed to initialise this action (probably bad data...) so just free it
						pAction->Release();
						CryLog("Vehicle %s has an action defined (probably an 'Enter' action) that refers to a seat that doesn't exist.", vehicleTable.getAttr("name"));
					}
				}
			}
		}
	}

	return true;
}

//------------------------------------------------------------------------
void CVehicle::PostInit(IGameObject* pGameObject)
{
	Veh::RegisterEvents(*this, *pGameObject);

	if (GetMovement())
		GetMovement()->PostInit();

	for (TVehiclePartVector::iterator ite = m_parts.begin(); ite != m_parts.end(); ++ite)
	{
		IVehiclePart* part = ite->second;
		part->PostInit();
	}

	pGameObject->EnablePhysicsEvent(true, eEPE_OnCollisionLogged | eEPE_OnStateChangeLogged);
	//pGameObject->EnablePostUpdates(this);

	// not needed anymore - was required previously to make sure vehicle is not disabled when player drives
	//  for (int i=0; i<MAX_UPDATE_SLOTS_PER_EXTENSION; ++i)
	//    pGameObject->SetUpdateSlotEnableCondition(this, i, eUEC_WithoutAI);

	if (!gEnv->bServer)
	{
		pGameObject->SetUpdateSlotEnableCondition(this, eVUS_Always, eUEC_VisibleOrInRangeIgnoreAI);
		pGameObject->SetUpdateSlotEnableCondition(this, eVUS_EnginePowered, eUEC_VisibleOrInRangeIgnoreAI);
		pGameObject->SetUpdateSlotEnableCondition(this, eVUS_PassengerIn, eUEC_VisibleOrInRangeIgnoreAI);
	}

	pGameObject->SetUpdateSlotEnableCondition(this, eVUS_Visible, eUEC_Visible);
	pGameObject->EnableUpdateSlot(this, eVUS_Visible);

	pGameObject->SetMovementController(GetMovementController());

	m_initialposition = GetEntity()->GetWorldPos();

	for (TVehicleSeatVector::iterator it = m_seats.begin(), end = m_seats.end(); it != end; ++it)
	{
		it->second->PostInit(this);
	}

	if (!CCryAction::GetCryAction()->IsEditing())
		NeedsUpdate();
#if ENABLE_VEHICLE_DEBUG
	else if (IsDebugDrawing())
		NeedsUpdate();
#endif

	//  if (gEnv->bServer)
	//    SendWeaponSetup(eRMI_ToRemoteClients);

#if ENABLE_VEHICLE_DEBUG
	DumpParts();

	if (VehicleCVars().v_debug_mem > 0)
	{
		ICrySizer* pSizer = gEnv->pSystem->CreateSizer();
		GetMemoryUsage(pSizer);
		int vehicleSize = pSizer->GetTotalSize();
		CryLog("Vehicle initialized: <%s> takes %" PRISIZE_T " bytes.", GetEntity()->GetName(), pSizer->GetTotalSize());

		pSizer->Release();
	}
#endif

}

//------------------------------------------------------------------------
void CVehicle::PostInitClient(int channelId)
{
	SendWeaponSetup(eRMI_ToClientChannel, channelId);
};

//------------------------------------------------------------------------
bool CVehicle::ReloadExtension(IGameObject* pGameObject, const SEntitySpawnParams& params)
{
	ResetGameObject();

	Veh::RegisterEvents(*this, *pGameObject);

	CRY_ASSERT_MESSAGE(false, "CVehicle::ReloadExtension not implemented");

	return false;
}

#if ENABLE_VEHICLE_DEBUG
//------------------------------------------------------------------------
void CVehicle::DumpParts() const
{
	if (VehicleCVars().v_debugdraw == eVDB_PhysParts || VehicleCVars().v_debugdraw == eVDB_PhysPartsExt)
	{
		CryLog("============ %s =============", GetEntity()->GetName());

		IPhysicalEntity* pPhysics = GetEntity()->GetPhysics();
		pe_status_nparts nparts;
		int numParts = pPhysics->GetStatus(&nparts);
		CryLog("%i physical parts", numParts);
		CryLog("------------------");

		if (VehicleCVars().v_debugdraw == eVDB_PhysParts)
		{
			for (TVehiclePartVector::const_iterator ite = m_parts.begin(); ite != m_parts.end(); ++ite)
			{
				IVehiclePart* part = ite->second;
				int status = -1;
				pe_params_part params;

				if (part->GetPhysId() != -1)
				{
					params.partid = part->GetPhysId();
					status = pPhysics->GetParams(&params);
				}

				CryLog("Part <%-25s>: slot %2i, physId %8i, status %i", part->GetName(), part->GetSlot(), part->GetPhysId(), status);
			}
		}
		else
		{
			ICharacterInstance* pCharInstance = GetEntity()->GetCharacter(0);
			ISkeletonPose* pSkeletonPose = pCharInstance ? pCharInstance->GetISkeletonPose() : 0;
			IDefaultSkeleton& rIDefaultSkeleton = pCharInstance->GetIDefaultSkeleton();

			for (int part = 0; part < numParts; ++part)
			{
				pe_status_pos status;
				Matrix34 tm;
				status.ipart = part;
				status.pMtx3x4 = &tm;
				if (pPhysics->GetStatus(&status))
				{
					Vec3& min = status.BBox[0];
					Vec3& max = status.BBox[1];
					CryLog("Part [%i]: partId %i, min: (%.2f, %.2f, %.2f), max: (%.2f, %.2f, %.2f)", part, status.partid, min.x, min.y, min.z, max.x, max.y, max.z);

					{
						// quadratic, but well, we're debugging
						uint32 numJoints = rIDefaultSkeleton.GetJointCount();
						for (uint32 i = 0; i < numJoints; ++i)
						{
							if (pSkeletonPose->GetPhysIdOnJoint(i) == status.partid)
								CryLog("partId %i matches Joint <%s>", status.partid, rIDefaultSkeleton.GetJointNameByID(i));
						}
					}

					tm = GetEntity()->GetWorldTM().GetInverted() * tm;
					VehicleUtils::LogMatrix("", tm);
				}
			}
		}
	}
}
#endif

//------------------------------------------------------------------------
void CVehicle::SetAmmoCapacity(IEntityClass* pAmmoType, int capacity)
{
	if (m_pInventory)
		m_pInventory->SetAmmoCapacity(pAmmoType, capacity);
}

//------------------------------------------------------------------------
void CVehicle::SetAmmoCount(IEntityClass* pAmmoType, int amount)
{
	if (m_pInventory)
	{
		int oldAmount = m_pInventory->GetAmmoCount(pAmmoType);
		m_pInventory->SetAmmoCount(pAmmoType, amount);

		// if we didn't have any of this ammo before, and we do now, and we have a weapon inuse which uses this ammo type,
		// then trigger a reload
		if (oldAmount == 0 && amount != 0 && gEnv->IsClient())
		{
			if (IItemSystem* pItemSystem = gEnv->pGameFramework->GetIItemSystem())
			{
				int weaponCount = GetWeaponCount();
				for (int i = 0; i < weaponCount; ++i)
				{
					IItem* pItem = pItemSystem->GetItem(GetWeaponId(i));
					if (pItem && pItem->GetOwnerId() == gEnv->pGameFramework->GetClientActorId())
					{
						if (IWeapon* pWeapon = pItem->GetIWeapon())
						{
							if (IFireMode* pfm = pWeapon->GetFireMode(pWeapon->GetCurrentFireMode()))
							{
								if (pfm->GetAmmoType() == pAmmoType)
								{
									pWeapon->Reload(false);
									float time = 0;
									float rate = pfm->GetFireRate();
									if (rate > 0.0f)
										time = 60.0f / rate;
									pfm->SetNextShotTime(time);
									break;
								}
							}
						}
					}
				}
			}
		}
	}

	if (gEnv->bServer)
		GetGameObject()->InvokeRMI(ClSetAmmo(), AmmoParams(pAmmoType->GetName(), amount), eRMI_ToRemoteClients);
}

//------------------------------------------------------------------------
int CVehicle::GetAmmoCount(IEntityClass* pAmmoType) const
{
	if (m_pInventory)
		return m_pInventory->GetAmmoCount(pAmmoType);

	return 0;
}

//------------------------------------------------------------------------
SVehicleWeapon* CVehicle::GetVehicleWeaponAllSeats(EntityId weaponId) const
{
	SVehicleWeapon* pVehicleWeapon(NULL);

	TVehicleSeatVector::const_iterator iter = m_seats.begin();
	TVehicleSeatVector::const_iterator end = m_seats.end();
	while (iter != end)
	{
		const TVehicleSeatActionVector& seatActions = iter->second->GetSeatActions();

		TVehicleSeatActionVector::const_iterator seatActionsIter = seatActions.begin();
		TVehicleSeatActionVector::const_iterator seatActionsEnd = seatActions.end();
		while (seatActionsIter != seatActionsEnd)
		{
			if (CVehicleSeatActionWeapons* weapAction = CAST_VEHICLEOBJECT(CVehicleSeatActionWeapons, seatActionsIter->pSeatAction))
			{
				if (pVehicleWeapon = weapAction->GetVehicleWeapon(weaponId))
				{
					return pVehicleWeapon;
				}
			}

			++seatActionsIter;
		}

		++iter;
	}

	return NULL;
}

//------------------------------------------------------------------------
void CVehicle::SendWeaponSetup(int where, int channelId)
{
	SetupWeaponsParams params;
	for (TVehicleSeatVector::iterator it = m_seats.begin(); it != m_seats.end(); ++it)
	{
		SetupWeaponsParams::SeatWeaponParams seatparams;
		const CVehicleSeat* pSeat = it->second;

		const TVehicleSeatActionVector& seatActions = pSeat->GetSeatActions();
		for (TVehicleSeatActionVector::const_iterator ait = seatActions.begin(), aitEnd = seatActions.end(); ait != aitEnd; ++ait)
		{
			IVehicleSeatAction* pSeatAction = ait->pSeatAction;
			if (CVehicleSeatActionWeapons* weapAction = CAST_VEHICLEOBJECT(CVehicleSeatActionWeapons, pSeatAction))
			{
				SetupWeaponsParams::SeatWeaponParams::SeatActionWeaponParams actionParams;

				int n = weapAction->GetWeaponCount();
				for (int w = 0; w < n; w++)
					actionParams.weapons.push_back(weapAction->GetWeaponEntityId(w));

				seatparams.seatactions.push_back(actionParams);
			}
		}

		params.seats.push_back(seatparams);
	}

	GetGameObject()->InvokeRMI(ClSetupWeapons(), params, where, channelId);
}

//------------------------------------------------------------------------
TVehicleSoundEventId CVehicle::AddSoundEvent(SVehicleSoundInfo& info)
{
	m_soundEvents.push_back(info);

	return TVehicleSoundEventId(m_soundEvents.size() - 1);
}

//------------------------------------------------------------------------
void CVehicle::ProcessEvent(const SEntityEvent& entityEvent)
{
	switch (entityEvent.event)
	{
	case ENTITY_EVENT_START_GAME:
		Reset(true);
		break;

	case ENTITY_EVENT_RESET:
		Reset(entityEvent.nParam[0] == 1 ? true : false);
		break;

	case ENTITY_EVENT_DONE:
		{
			// Passengers should exit now.
			for (TVehicleSeatVector::iterator it = m_seats.begin(), end = m_seats.end(); it != end; ++it)
			{
				if (it->second->GetPassenger())
					it->second->Exit(false, true);
			}

			DeleteActionController();
		}
		break;

	case ENTITY_EVENT_TIMER:
		OnTimer((int)entityEvent.nParam[0]);
		break;

	case ENTITY_EVENT_MATERIAL_LAYER:
		OnMaterialLayerChanged(entityEvent);
		break;

	case ENTITY_EVENT_HIDE:
	case ENTITY_EVENT_UNHIDE:
		{
			SVehicleEventParams eventParams;

			eventParams.iParam = entityEvent.event == ENTITY_EVENT_HIDE;

			BroadcastVehicleEvent(eVE_Hide, eventParams);
		}
		break;
	case ENTITY_EVENT_ANIM_EVENT:
		{
			if (IActionController* pActionController = GetAnimationComponent().GetActionController())
			{
				const AnimEventInstance* pAnimEvent = reinterpret_cast<const AnimEventInstance*>(entityEvent.nParam[0]);
				ICharacterInstance* pCharacter = reinterpret_cast<ICharacterInstance*>(entityEvent.nParam[1]);
				if (pAnimEvent && pCharacter)
				{
					pActionController->OnAnimationEvent(pCharacter, *pAnimEvent);
				}
			}
		}
		break;
	case ENTITY_EVENT_PREPHYSICSUPDATE:
		{
			for (TVehicleSeatVector::iterator ite = m_seats.begin(); ite != m_seats.end(); ++ite)
			{
				ite->second->PrePhysUpdate(gEnv->pTimer->GetFrameTime());
			}
		}
		break;
	case ENTITY_EVENT_SET_AUTHORITY:
		{
			m_hasAuthority = entityEvent.nParam[0] ? true : false;
			if (m_pMovement)
			{
				m_pMovement->SetAuthority(m_hasAuthority);
			}
			if (m_hasAuthority)
			{
				m_clientSmoothedPosition.t = GetEntity()->GetPos();
				m_clientSmoothedPosition.q = GetEntity()->GetRotation();
			}
		}
		break;
	}

	if (m_pMovement)
		m_pMovement->ProcessEvent(entityEvent);
}

uint64 CVehicle::GetEventMask() const
{
	return
	  ENTITY_EVENT_BIT(ENTITY_EVENT_RESET) |
	  ENTITY_EVENT_BIT(ENTITY_EVENT_DONE) |
	  ENTITY_EVENT_BIT(ENTITY_EVENT_TIMER) |
	  ENTITY_EVENT_BIT(ENTITY_EVENT_MATERIAL_LAYER) |
	  ENTITY_EVENT_BIT(ENTITY_EVENT_HIDE) |
	  ENTITY_EVENT_BIT(ENTITY_EVENT_UNHIDE) |
	  ENTITY_EVENT_BIT(ENTITY_EVENT_ANIM_EVENT) |
	  ENTITY_EVENT_BIT(ENTITY_EVENT_START_GAME) |
	  ENTITY_EVENT_BIT(ENTITY_EVENT_SET_AUTHORITY) |
	  ENTITY_EVENT_BIT(ENTITY_EVENT_PREPHYSICSUPDATE);
}

//------------------------------------------------------------------------
void CVehicle::DeleteActionController()
{
	GetAnimationComponent().DeleteActionController();
	for (TVehicleSeatVector::iterator ite = m_seats.begin(), end = m_seats.end(); ite != end; ++ite)
	{
		CVehicleSeat* seat = ite->second;
		seat->OnVehicleActionControllerDeleted();
	}
}

//------------------------------------------------------------------------
void CVehicle::OnMaterialLayerChanged(const SEntityEvent& event)
{
	// only handle frozen layer for now.
	bool frozen = event.nParam[0] & MTL_LAYER_FROZEN;
	bool prev = event.nParam[1] & MTL_LAYER_FROZEN;
	if (frozen != prev)
	{
		int n = GetEntity()->GetChildCount();
		for (int i = 0; i < n; ++i)
		{
			IEntity* pChild = GetEntity()->GetChild(i);
			IEntityRender* pIEntityRender = pChild->GetRenderInterface();

			{
				if (IActor* pActor = CCryAction::GetCryAction()->GetIActorSystem()->GetActor(pChild->GetId()))
					if (pActor->IsPlayer()) // don't freeze players inside vehicles
						continue;

				//uint8 mask = pIEntityRender->GetMaterialLayersMask();
				//pIEntityRender->SetMaterialLayersMask(frozen ? mask | MTL_LAYER_FROZEN : mask & ~MTL_LAYER_FROZEN);
			}
		}
	}
}

//------------------------------------------------------------------------
int CVehicle::SetTimer(int timerId, int ms, IVehicleObject* pObject)
{
	if (timerId == -1)
	{
		if (!m_timers.empty())
		{
			TVehicleTimerMap::const_reverse_iterator last = m_timers.rbegin();
			timerId = last->first + 1;
		}
		else
			timerId = eVT_Last;
	}
	else
		KillTimer(timerId);

	if (m_timers.insert(std::make_pair(timerId, pObject)).second)
	{
		GetEntity()->SetTimer(timerId, ms);
		//CryLog("vehicle <%s> setting timer %i, %i ms", GetEntity()->GetName(), timerId, ms);

		return timerId;
	}

	return -1;
}

//------------------------------------------------------------------------
int CVehicle::KillTimer(int timerId)
{
	if (timerId != -1)
	{
		GetEntity()->KillTimer(timerId);
		m_timers.erase(timerId);
	}

	return -1;
}

//------------------------------------------------------------------------
void CVehicle::KillTimers()
{
	KillAbandonedTimer();

	GetEntity()->KillTimer(IEntity::KILL_ALL_TIMER);
	m_timers.clear();
}

//------------------------------------------------------------------------
void CVehicle::OnTimer(int timerId)
{
	TVehicleTimerMap::iterator it = m_timers.find(timerId);
	if (it != m_timers.end())
	{
		IVehicleObject* pObject = it->second;
		m_timers.erase(it);

		if (pObject)
		{
			SVehicleEventParams params;
			params.iParam = timerId;
			pObject->OnVehicleEvent(eVE_Timer, params);
		}
	}

	switch (timerId)
	{
	case eVT_Abandoned:
		Destroy();
		break;
	case eVT_AbandonedSound:
		EnableAbandonedWarnSound(true);
		break;
	case eVT_Flipped:
		ProcessFlipped();
		break;
	}
}

void CVehicle::Reset(bool enterGame)
{
	KillTimers();

	m_vehicleAnimation.Reset();

	m_ownerId = 0;

	// disable engine slot in case it was enabled by speed threshold
	if (m_customEngineSlot)
	{
		GetGameObject()->DisableUpdateSlot(this, eVUS_EnginePowered);
		m_customEngineSlot = false;
	}

	ResetDamages();

	for (TVehiclePartVector::iterator ite = m_parts.begin(); ite != m_parts.end(); ++ite)
	{
		IVehiclePart* part = ite->second;
		part->Reset();
	}

	for (TVehicleSeatVector::iterator ite = m_seats.begin(); ite != m_seats.end(); ++ite)
	{
		CVehicleSeat* pSeat = ite->second;
		pSeat->Reset();
	}

	for (TVehicleComponentVector::iterator ite = m_components.begin(); ite != m_components.end(); ++ite)
	{
		CVehicleComponent* pComponent = *ite;
		pComponent->Reset();
	}

	for (TVehicleAnimationsVector::iterator ite = m_animations.begin(); ite != m_animations.end(); ++ite)
	{
		IVehicleAnimation* pAnimation = ite->second;
		pAnimation->Reset();
	}

	for (TVehicleActionVector::iterator ite = m_actions.begin(); ite != m_actions.end(); ++ite)
	{
		SVehicleActionInfo& actionInfo = *ite;
		actionInfo.pAction->Reset();
	}

	for (TVehicleSoundEventId id = 0; id < m_soundEvents.size(); ++id)
		StopSound(id);

	m_isDestroyed = false;
	m_status.Reset();

	m_initialposition = GetEntity()->GetWorldPos();

	DoRequestedPhysicalization();

	if (enterGame)
	{
		// activate when going to game, to adjust vehicle to terrain etc.
		// will be deactivated some frames later
		NeedsUpdate(eVUF_AwakePhysics);

		// Temp Code, testing only
		CryAudio::ControlId const triggerId = CryAudio::StringToId("ENGINE_ON");
		m_pIEntityAudioComponent->ExecuteTrigger(triggerId);
	}
	else
	{
		m_bNeedsUpdate = false;

		for (int i = 0; i < eVUS_Last; ++i)
		{
			while (GetGameObject()->GetUpdateSlotEnables(this, i) > 0)
				GetGameObject()->DisableUpdateSlot(this, i);
		}

		// Temp Code, testing only
		CryAudio::ControlId const triggerId = CryAudio::StringToId("ENGINE_OFF");
		m_pIEntityAudioComponent->ExecuteTrigger(triggerId);
	}

	m_collisionDisabledTime = 0.0f;
	m_physUpdateTime = 0.f;
	m_indestructible = false;

	m_predictionHistory.clear();
#if ENABLE_VEHICLE_DEBUG
	m_debugClientPredictData.clear();
#endif
}

//------------------------------------------------------------------------
void CVehicle::RequestPhysicalization(IVehiclePart* pPart, bool request)
{
	if (request)
		m_physicalizeParts.insert(pPart);
	else
		stl::find_and_erase(m_physicalizeParts, pPart);
}

//------------------------------------------------------------------------
void CVehicle::DoRequestedPhysicalization()
{
	if (!m_physicalizeParts.empty())
	{
		TVehiclePartSet parts(m_physicalizeParts);
		for (TVehiclePartSet::iterator it = parts.begin(), end = parts.end(); it != end; ++it)
		{
			(*it)->Physicalize();
		}

		m_physicalizeParts.clear();
	}
}

//------------------------------------------------------------------------
void CVehicle::Update(SEntityUpdateContext& ctx, int slot)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

#if ENABLE_VEHICLE_DEBUG
	gEnv->pAuxGeomRenderer->SetRenderFlags(e_Def3DPublicRenderflags);
#endif

	const float frameTime = ctx.fFrameTime;
	if (ctx.frameID != m_lastFrameId)
	{
		m_bNeedsUpdate = false;
		m_lastFrameId = ctx.frameID;
	}

	switch (slot)
	{
	case eVUS_Always:
		{
#if ENABLE_VEHICLE_DEBUG
			m_debugIndex = 0;
#endif

			if (!GetEntity()->GetPhysics())
				return;

			UpdateStatus(frameTime);
			UpdateDamages(frameTime);

			for (TVehicleObjectUpdateInfoList::iterator ite = m_objectsToUpdate.begin(), end = m_objectsToUpdate.end(); ite != end; ++ite)
			{
				SObjectUpdateInfo& updateInfo = *ite;
				if (updateInfo.updatePolicy == eVOU_AlwaysUpdate)
					updateInfo.pObject->Update(frameTime);
			}

			if (m_collisionDisabledTime > 0.0f)
				m_collisionDisabledTime -= frameTime;

			m_vehicleAnimation.Update(frameTime);

			if (m_hasAuthority && !gEnv->bServer && VehicleCVars().v_serverControlled && VehicleCVars().v_clientPredict)
			{
				const INetChannel* pNetChannel = gEnv->pGameFramework->GetClientChannel();
				if (pNetChannel)
				{
					m_smoothedPing = pNetChannel->GetPing(true);
				}
			}
		}
		break;

	case eVUS_EnginePowered:
		{
			if (m_pMovement)
			{
				NeedsUpdate();
				m_pMovement->Update(frameTime);
			}

			break;
		}
	case eVUS_AIControlled:
		{
			NeedsUpdate();

			for (TVehicleSeatVector::iterator ite = m_seats.begin(), end = m_seats.end(); ite != end; ++ite)
			{
				CVehicleSeat* seat = ite->second;

				if (CVehicleSeatActionWeapons* weapons = seat->GetSeatActionWeapons())
				{
					seat->Update(frameTime);
				}
			}
		}
		break;

	case eVUS_PassengerIn:
		{
			NeedsUpdate();

			for (TVehicleSeatVector::iterator ite = m_seats.begin(), end = m_seats.end(); ite != end; ++ite)
			{
				CVehicleSeat* seat = ite->second;
				seat->Update(frameTime);
			}

			for (TVehicleObjectUpdateInfoList::iterator ite = m_objectsToUpdate.begin(), end = m_objectsToUpdate.end(); ite != end; ++ite)
			{
				SObjectUpdateInfo& updateInfo = *ite;
				if (updateInfo.updatePolicy == eVOU_PassengerUpdate)
					updateInfo.pObject->Update(frameTime);
			}

			break;
		}

	case eVUS_Visible:
		{
			for (TVehicleObjectUpdateInfoList::iterator ite = m_objectsToUpdate.begin(), end = m_objectsToUpdate.end(); ite != end; ++ite)
			{
				SObjectUpdateInfo& updateInfo = *ite;
				if (updateInfo.updatePolicy == eVOU_Visible)
					updateInfo.pObject->Update(frameTime);
			}

#if ENABLE_VEHICLE_DEBUG
			DebugDraw(frameTime);
#endif
		}
	}

	CheckDisableUpdate(slot);

	m_physUpdateTime = 0.f;
}

void CVehicle::UpdatePassenger(float frameTime, EntityId playerId)
{
	for (uint32 updatePolicy = 0; updatePolicy <= eVUS_Last; ++updatePolicy)
	{
		for (TVehicleObjectUpdateInfoList::iterator ite = m_objectsToUpdate.begin(), end = m_objectsToUpdate.end(); ite != end; ++ite)
		{
			SObjectUpdateInfo& updateInfo = *ite;
			if (updateInfo.updatePolicy == updatePolicy)
			{
				updateInfo.pObject->UpdateFromPassenger(frameTime, playerId);
			}
		}
	}
}

//------------------------------------------------------------------------
void CVehicle::PostUpdate(float frameTime)
{
	//DebugDraw(frameTime);

	// Force update of passenger transform matrices.

	IEntitySystem* pEntitySystem = gEnv->pEntitySystem;

	for (TVehicleSeatVector::const_iterator iSeat = m_seats.begin(), iEndSeat = m_seats.end(); iSeat != iEndSeat; ++iSeat)
	{
		const CVehicleSeat* pVehicleSeat = iSeat->second;
		if (!pVehicleSeat->IsRemoteControlled())
		{
			if (IEntity* pPassengerEntity = pEntitySystem->GetEntity(pVehicleSeat->GetPassenger()))
			{
				pPassengerEntity->InvalidateTM(ENTITY_XFORM_FROM_PARENT);
			}
		}
	}
}

//------------------------------------------------------------------------
void CVehicle::NeedsUpdate(int flags, bool bThreadSafe /*=false*/)
{
	m_bNeedsUpdate = true;

	if (0 == GetGameObject()->GetUpdateSlotEnables(this, eVUS_Always))
	{
		// enable, if necessary
		GetGameObject()->EnableUpdateSlot(this, eVUS_Always);
	}

	if (flags & eVUF_AwakePhysics)
	{
		if (IPhysicalEntity* pPE = GetEntity()->GetPhysics())
		{
			pe_action_awake awakeParams;
			awakeParams.bAwake = 1;
			// keep vehicle awake for a bit to keep it from sleeping when it is going from positive to negative acceleration
			awakeParams.minAwakeTime = 2.0f;
			pPE->Action(&awakeParams, (int)bThreadSafe);
		}
	}
}

//------------------------------------------------------------------------
void CVehicle::CheckDisableUpdate(int slot)
{
	if (m_bNeedsUpdate)
		return;

	if (0 == CVehicleCVars::Get().v_autoDisable)
		return;

#if ENABLE_VEHICLE_DEBUG
	if (IsDebugDrawing())
		return;
#endif

	// check after last updated slot if we can be disabled
	for (int i = eVUS_Visible - 1; i >= 0; --i)
	{
		if (GetGameObject()->GetUpdateSlotEnables(this, i))
		{
			if (slot >= i)
			{
				if (IPhysicalEntity* pPhysics = GetEntity()->GetPhysics())
				{
					pe_status_pos status;
					if (pPhysics->GetStatus(&status) && status.iSimClass > 1)
						break;
				}

				GetGameObject()->DisableUpdateSlot(this, eVUS_Always);
			}
			break;
		}
	}
}

//------------------------------------------------------------------------
void CVehicle::OnPhysStateChange(EventPhysStateChange* pEvent)
{
	if (pEvent->iSimClass[0] < 2 && pEvent->iSimClass[1] > 1)
	{
		// awakened
		NeedsUpdate();

		SVehicleEventParams params;
		BroadcastVehicleEvent(eVE_Awake, params);
	}
	else if (pEvent->iSimClass[0] > 1 && pEvent->iSimClass[1] < 2)
	{
		// falling asleep

		// do one update pass on visibility-based objects
		for (TVehicleObjectUpdateInfoList::iterator ite = m_objectsToUpdate.begin(); ite != m_objectsToUpdate.end(); ++ite)
		{
			SObjectUpdateInfo& updateInfo = *ite;
			if (updateInfo.updatePolicy == eVOU_Visible)
				updateInfo.pObject->Update(0.f);
		}

		SVehicleEventParams params;
		BroadcastVehicleEvent(eVE_Sleep, params);
	}
}

#if ENABLE_VEHICLE_DEBUG
//------------------------------------------------------------------------
void CVehicle::DebugDraw(const float frameTime)
{
	if (IsDebugDrawing())
		NeedsUpdate();

	if (GetEntity()->GetWorldPos().GetSquaredDistance(gEnv->pSystem->GetViewCamera().GetPosition()) > sqr(100.f))
		return;

	CVehicleCVars& cvars = CVehicleCVars::Get();

	if (cvars.v_draw_components)
	{
		for (TVehicleComponentVector::const_iterator ite = m_components.begin(); ite != m_components.end(); ++ite)
		{
			CVehicleComponent* pComponent = *ite;
			pComponent->DebugDraw();
		}
	}

	if (cvars.v_debugdraw == eVDB_Parts)
	{
		static float drawColor[4] = { 1, 1, 1, 1 };

		for (TVehiclePartVector::iterator ite = m_parts.begin(); ite != m_parts.end(); ++ite)
		{
			IRenderAuxText::DrawLabelExF(ite->second->GetWorldTM().GetTranslation(), 1.0f, drawColor, true, true, "<%s>", ite->first.c_str());

			/*IRenderAuxGeom* pGeom = gEnv->pAuxGeomRenderer;
			   AABB bounds = AABB::CreateTransformedAABB(ite->second->GetWorldTM(), ite->second->GetLocalBounds());
			   ColorB col(0,255,0,255);
			   pGeom->DrawAABB(bounds, false, col, eBBD_Extremes_Color_Encoded);*/
		}
	}
	else if (cvars.v_debugdraw == eVDB_PhysPartsExt)
	{
		for (TVehiclePartVector::iterator iPart = m_parts.begin(), iEnd = m_parts.end(); iPart != iEnd; ++iPart)
		{
			if (IVehiclePart* pPart = iPart->second)
			{
				const Matrix34& partWorldTM = pPart->GetWorldTM();

				{
					const float color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

					IRenderAuxText::DrawLabelExF(partWorldTM.GetTranslation(), 1.0f, color, true, true, "<%s>", iPart->first.c_str());
				}

				if (IRenderAuxGeom* pRenderAuxGeom = gEnv->pAuxGeomRenderer)
				{
					pRenderAuxGeom->DrawAABB(pPart->GetLocalBounds(), GetEntity()->GetWorldTM(), false, ColorB(255, 255, 255, 255), eBBD_Extremes_Color_Encoded);

					const Vec3 partWorldTranslation = partWorldTM.GetTranslation();

					pRenderAuxGeom->DrawLine(partWorldTranslation, ColorB(255, 0, 0, 255), partWorldTranslation + partWorldTM.GetColumn0(), ColorB(255, 0, 0, 255), 1.0f);
					pRenderAuxGeom->DrawLine(partWorldTranslation, ColorB(0, 255, 0, 255), partWorldTranslation + partWorldTM.GetColumn1(), ColorB(0, 255, 0, 255), 1.0f);
					pRenderAuxGeom->DrawLine(partWorldTranslation, ColorB(0, 0, 255, 255), partWorldTranslation + partWorldTM.GetColumn2(), ColorB(0, 0, 255, 255), 1.0f);
				}
			}
		}
	}
	else if (cvars.v_debugdraw == eVDB_Damage)
	{
		const float color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		float damageRatio = GetDamageRatio();
		string damageStr;
		damageStr.Format("Health: %.2f/%.2f Damage Ratio: %.2f", m_damageMax * (1.f - damageRatio), m_damageMax, damageRatio);
		IRenderAuxText::DrawLabelEx(GetEntity()->GetWorldPos() + Vec3(0.f, 0.f, 1.f), 1.5f, color, true, true, damageStr.c_str());
	}
	else if (cvars.v_debugdraw == eVDB_ClientPredict)
	{
		DebugDrawClientPredict();
	}

	if (cvars.v_draw_helpers)
	{
		static float drawColor[4] = { 1, 0, 0, 1 };

		CPersistantDebug* pDB = CCryAction::GetCryAction()->GetPersistantDebug();
		CryFixedStringT<32> dbgName;
		dbgName.Format("VehicleHelpers_%s", GetEntity()->GetName());
		pDB->Begin(dbgName.c_str(), true);
		for (TVehicleHelperMap::const_iterator ite = m_helpers.begin(); ite != m_helpers.end(); ++ite)
		{
			Matrix34 tm;
			ite->second->GetWorldTM(tm);
			IRenderAuxText::DrawLabelExF(tm.GetTranslation(), 1.0f, drawColor, true, true, "<%s>", ite->first.c_str());
			pDB->AddDirection(tm.GetTranslation(), 0.25f, tm.GetColumn(1), ColorF(1, 1, 0, 1), 0.05f);
		}
	}

	if (cvars.v_draw_seats)
	{
		float seatColor[] = { 0.5, 1, 1, 1 };
		//    float enterColor[] = {0.5,1,1,0.5};
		for (TVehicleSeatVector::const_iterator it = m_seats.begin(), end = m_seats.end(); it != end; ++it)
		{
			Vec3 pos(0);
			if (IVehicleHelper* pHelper = it->second->GetSitHelper())
				pos = pHelper->GetVehicleSpaceTranslation();

			IRenderAuxText::DrawLabelExF(GetEntity()->GetWorldTM() * pos, 1.1f, seatColor, true, true, "[%s]", it->second->GetName().c_str());

			if (IVehicleHelper* pHelper = it->second->GetEnterHelper())
				IRenderAuxText::DrawLabelExF(pHelper->GetWorldSpaceTranslation(), 1.0f, seatColor, true, true, "[%s enter]", it->second->GetName().c_str());
		}
	}

	if (cvars.v_debugdraw == eVDB_Sounds && IsPlayerPassenger())
	{
		static float color[] = { 1, 1, 1, 1 };

		for (TVehicleSoundEventId i = 0; i < m_soundEvents.size(); ++i)
		{
			if (SVehicleSoundInfo* info = GetSoundInfo(i))
			{
				REINST("update speed RTPC");
				/*if (ISound* pSound = m_pIEntityAudioComponent->GetSound(info->soundId))
				   {
				   float speed = 0.f;
				   if (pSound->GetParam("speed", &speed, false))
				   {
				    IRenderAuxText::Draw2dLabel(50.f, (float)(100+15*GetDebugIndex()), 1.25f, color, false, "%s: speed %.2f", info->name.c_str(), speed);
				   }
				   }*/
			}
		}
	}
}

//------------------------------------------------------------------------
void CVehicle::DebugDrawClientPredict()
{
	if (IRenderAuxGeom* pRenderAuxGeom = gEnv->pAuxGeomRenderer)
	{
		CryAutoCriticalSection lk(m_debugDrawLock);

		Vec3 scale(1.0f, 1.0f, 1.0f);
		AABB bbox;
		GetEntity()->GetLocalBounds(bbox);
		bbox.max.z -= 1.5f;
		AABB bbox2 = bbox;
		bbox2.min.x += 0.2f;
		bbox2.max.x -= 0.2f;
		TDebugClientPredictData::iterator itInterp;
		float y = 50.0f;
		for (itInterp = m_debugClientPredictData.begin(); itInterp != m_debugClientPredictData.end(); ++itInterp)
		{
			Matrix34 matrix1 = Matrix34::Create(scale, itInterp->recordedPos.q, itInterp->recordedPos.t);
			Matrix34 matrix2 = Matrix34::Create(scale, itInterp->correctedPos.q, itInterp->correctedPos.t);
			pRenderAuxGeom->DrawAABB(bbox, matrix1, false, Col_Green, eBBD_Faceted);
			pRenderAuxGeom->DrawAABB(bbox2, matrix2, false, Col_Red, eBBD_Faceted);
			Vec3 pos1 = itInterp->recordedPos.t;
			Vec3 pos2 = itInterp->correctedPos.t;
			pos1.z += 3.0f;
			pos2.z += 3.0f;
			pRenderAuxGeom->DrawLine(pos1, Col_Green, pos1 + itInterp->recordedVel * 0.2f, Col_Green, 2.0f);
			pRenderAuxGeom->DrawLine(pos2, Col_Red, pos2 + itInterp->correctedVel * 0.2f, Col_Red, 2.0f);
		}
	}
}

//------------------------------------------------------------------------
bool CVehicle::IsDebugDrawing()
{
	CVehicleCVars& cvars = CVehicleCVars::Get();

	return (cvars.v_draw_components || cvars.v_draw_helpers || cvars.v_debugdraw);
}
#endif

//------------------------------------------------------------------------
void CVehicle::UpdateView(SViewParams& viewParams, EntityId playerId)
{
#if ENABLE_VEHICLE_DEBUG
	const CVehicleCVars& vars = VehicleCVars();
	if (vars.v_debugViewDetach == 1) return;

	if (vars.v_debugViewDetach == 2)
	{
		IEntity* pEntity = GetEntity();
		Vec3 entityPos = pEntity->GetPos();
		Vec3 dir = (entityPos - viewParams.position);
		float radius = dir.GetLength();
		if (radius > 0.01f)
		{
			dir = dir * (1.f / radius);
			viewParams.rotation = Quat::CreateRotationVDir(dir);
		}
		return;
	}
	else if (vars.v_debugViewAbove)
	{
		IEntity* pEntity = GetEntity();
		viewParams.position = pEntity->GetPos();
		viewParams.position.z += vars.v_debugViewAboveH;
		viewParams.rotation = Quat(0.7071067811865475f, -0.7071067811865475f, 0.f, 0.f);
		return;
	}
#endif

	for (TVehicleSeatVector::iterator ite = m_seats.begin(), end = m_seats.end(); ite != end; ++ite)
	{
		CVehicleSeat* seat = ite->second;
		if (seat->GetPassenger() == playerId && seat->GetCurrentTransition() != CVehicleSeat::eVT_RemoteUsage)
		{
			seat->UpdateView(viewParams);
			if (m_pMovement)
			{
				m_pMovement->PostUpdateView(viewParams, playerId);
			}
			return;
		}
	}
}

//------------------------------------------------------------------------
void CVehicle::HandleEvent(const SGameObjectEvent& event)
{
	if (event.event == eGFE_OnPostStep)
	{
		OnPhysPostStep((EventPhys*)event.ptr, (event.flags & eGOEF_LoggedPhysicsEvent) != 0);
	}
	else if (event.event == eGFE_OnCollision)
	{
		EventPhysCollision* pCollision = static_cast<EventPhysCollision*>(event.ptr);
		OnCollision(pCollision);
	}
	else if (event.event == eGFE_OnStateChange)
	{
		EventPhysStateChange* pStateChange = static_cast<EventPhysStateChange*>(event.ptr);
		OnPhysStateChange(pStateChange);
	}
	else if (event.event == eGFE_OnBecomeVisible)
	{
		if (m_pMovement)
		{
			SVehicleMovementEventParams params;
			m_pMovement->OnEvent(IVehicleMovement::eVME_BecomeVisible, params);
		}
	}
	else if (event.event == eGFE_PreShatter)
	{
		// evacuate all the passengers
		for (TVehicleSeatVector::iterator it = m_seats.begin(), end = m_seats.end(); it != end; ++it)
		{
			if (it->second->GetPassenger())
				it->second->Exit(false, true);
		}

		OnDestroyed();
		SetDestroyedStatus(true);
	}
	else if (event.event == eGFE_StoodOnChange)
	{
		if (m_pMovement)
		{
			SVehicleMovementEventParams params;
			params.sParam = (const char*)event.ptr;
			params.bValue = event.paramAsBool;
			m_pMovement->OnEvent(IVehicleMovement::eVME_StoodOnChange, params);
		}
	}
}

//------------------------------------------------------------------------
void CVehicle::UpdateStatus(const float deltaTime)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	int frameId = gEnv->nMainFrameID;
	if (!(frameId > m_status.frameId))
		return;

	m_status.frameId = frameId;
	IPhysicalEntity* pPhysics = GetEntity()->GetPhysics();

	m_status.altitude = 0.0f;

	m_status.passengerCount = 0;

	if (!IsDestroyed())
	{
		for (TVehicleSeatVector::iterator ite = m_seats.begin(), end = m_seats.end(); ite != end; ++ite)
		{
			CVehicleSeat* seat = ite->second;
			if (seat && seat->GetPassenger() && seat->GetCurrentTransition() != CVehicleSeat::eVT_RemoteUsage)
			{
				++m_status.passengerCount;
			}
		}
	}
	else
	{
		m_status.health = 0.f;
	}

	if (m_bRetainGravity)
	{
		pe_simulation_params simParams;
		simParams.gravity = m_gravity;
		GetEntity()->GetPhysics()->SetParams(&simParams);
	}

	pe_status_dynamics dyn;
	if (!IsDestroyed() && pPhysics->GetStatus(&dyn))
	{
		float speed = dyn.v.len();

		if (m_pMovement && TriggerEngineSlotBySpeed())
		{
			// we enable/disable EnginePowered slot when speed crosses
			// this threshold, so the engine/movement update can also be
			// executed when engine itself is off

			float minSpeed = ENGINESLOT_MINSPEED;

			// todo: add a small timer here
			if (speed > minSpeed && !m_customEngineSlot)
			{
				GetGameObject()->EnableUpdateSlot(this, IVehicle::eVUS_EnginePowered);
				m_customEngineSlot = true;
			}
			else if (speed <= minSpeed && m_customEngineSlot)
			{
				GetGameObject()->DisableUpdateSlot(this, IVehicle::eVUS_EnginePowered);
				m_customEngineSlot = false;

				SVehicleMovementEventParams params;
				params.bValue = false;
				m_pMovement->OnEvent(IVehicleMovement::eVME_ToggleEngineUpdate, params);
			}
		}

		float preSubmerged = m_status.submergedRatio;

		m_status.speed = speed;
		m_status.vel = dyn.v;
		m_status.submergedRatio = dyn.submergedFraction;

		pe_status_collisions collisions;
		collisions.pHistory = 0;
		collisions.len = 0;
		collisions.age = 1.f;

		m_status.contacts = pPhysics->GetStatus(&collisions);

		const SVehicleDamageParams& damageParams = m_pVehicle->GetDamageParams();
		float max = damageParams.submergedRatioMax - 0.001f;
		if (m_status.submergedRatio >= max && preSubmerged < max)
			OnSubmerged(m_status.submergedRatio);
	}

	// check flipped over status
	CheckFlippedStatus(deltaTime);
}

#if ENABLE_VEHICLE_DEBUG
//------------------------------------------------------------------------
void CVehicle::TestClientPrediction(const float deltaTime)
{
	// This function is used to test rewinding of physics and replaying inputs, the code is similar to that of
	// the client prediction and can be used to help debugging problems with it.
	int predictSamples = VehicleCVars().v_testClientPredict;
	if (predictSamples > 0 && m_hasAuthority && gEnv->bServer)
	{
		IPhysicalEntity* pPhysics = GetEntity()->GetPhysics();
		if (pPhysics)
		{
			if (m_pMovement)
			{
				pe_status_pos currentPos;
				pe_status_dynamics currentDynamics;
				pPhysics->GetStatus(&currentPos);
				pPhysics->GetStatus(&currentDynamics);

				// Record inputs
				SVehiclePredictionHistory sample;
				sample.m_actions = m_pMovement->GetMovementActions();
				sample.m_state = m_pMovement->GetVehicleNetState();
				sample.m_pos = currentPos.pos;
				sample.m_rot = currentPos.q;
				sample.m_velocity = currentDynamics.v;
				sample.m_angVelocity = currentDynamics.w;
				sample.m_deltaTime = deltaTime;

				if (m_predictionHistory.size() >= predictSamples)
				{
					pe_params_flags savedFlags;
					pPhysics->GetParams(&savedFlags);
					pe_params_flags setFlags;
					setFlags.flagsAND = ~(pef_log_state_changes | pef_log_collisions | pef_log_env_changes | pef_log_poststep);
					pPhysics->SetParams(&setFlags, 1);

					// Set the vehicle back "predictSamples" number of frames
					pe_params_pos newPosParams;
					newPosParams.bRecalcBounds = 16 | 32 | 64;
					newPosParams.pos = m_predictionHistory[0].m_pos;
					newPosParams.q = m_predictionHistory[0].m_rot;
					pPhysics->SetParams(&newPosParams, 1);

					pe_action_set_velocity velocity;
					velocity.v = m_predictionHistory[0].m_velocity;
					velocity.w = m_predictionHistory[0].m_angVelocity;
					pPhysics->Action(&velocity, 1);

					// Fast-forward the physics applying all the inputs we recorded
					CryAutoCriticalSection debuglk(m_debugDrawLock);
					ReplayPredictionHistory(0.0f);

					// The final position should now be the same as what we started with as long as everything
					// worked as expected

					// Restore inputs back to what they were before we messed with them
					m_pMovement->RequestActions(sample.m_actions);
					m_predictionHistory.clear();

					pPhysics->SetParams(&savedFlags, 1);
				}
				else
				{
					m_predictionHistory.push_back(sample);
				}
			}
		}
	}
}
#endif

//------------------------------------------------------------------------
// NOTE: This function must be thread-safe.
void CVehicle::OnPrePhysicsTimeStep(float deltaTime)
{
	UpdateNetwork(deltaTime);
#if ENABLE_VEHICLE_DEBUG
	TestClientPrediction(deltaTime);
#endif
}

//------------------------------------------------------------------------
// NOTE: This function must be thread-safe.
void CVehicle::UpdateNetwork(const float deltaTime)
{
	// Client sends vehicle control inputs to the server via the IMovementController interface.
	// The server responds to those inputs and drives the vehicle as if it was a local player controlling it.
	// The server then sends the positions of the vehicle to the client which processes them trying to maintain
	// smooth movement while also keeping in sync with the server position. This function deals will processing
	// the positions from the server.
	if (m_hasAuthority && !gEnv->bServer && VehicleCVars().v_serverControlled)
	{
		IPhysicalEntity* pPhysics = GetEntity()->GetPhysics();
		if (pPhysics && m_pMovement)
		{
			CryAutoCriticalSection netlk(*m_pMovement->GetNetworkLock());
#if ENABLE_VEHICLE_DEBUG
			CryAutoCriticalSection debuglk(m_debugDrawLock);
#endif

			pe_status_pos statusPos;
			pe_status_dynamics statusDynamics;
			pPhysics->GetStatus(&statusPos);
			pPhysics->GetStatus(&statusDynamics);

			// Record inputs
			SVehiclePredictionHistory sample;
			sample.m_actions = m_pMovement->GetMovementActions();
			sample.m_state = m_pMovement->GetVehicleNetState();
			sample.m_pos = statusPos.pos;
			sample.m_rot = statusPos.q;
			sample.m_velocity = statusDynamics.v;
			sample.m_angVelocity = statusDynamics.w;
			sample.m_deltaTime = deltaTime;

			// Set the vehicle back in time to where the server says it is
			pe_status_netpos netPos;
			if (pPhysics->GetStatus(&netPos))
			{
				float remainderTime = 0.0f;
				bool needsCorrection = true;
				if (VehicleCVars().v_clientPredict)
				{
					// Determine the difference between what the client predicted and what the server says
					float predictTime = min(m_smoothedPing + netPos.timeOffset + VehicleCVars().v_clientPredictAdditionalTime, VehicleCVars().v_clientPredictMaxTime);
					float accumulatedTime = 0.0f;
					for (int i = m_predictionHistory.size() - 1; i >= 0; --i)
					{
						accumulatedTime += m_predictionHistory[i].m_deltaTime;
						if (accumulatedTime >= predictTime)
						{
							if (i > 0)
							{
								m_predictionHistory.erase(m_predictionHistory.begin(), m_predictionHistory.begin() + i);
							}
							remainderTime = accumulatedTime - predictTime;
							break;
						}
					}

					if (accumulatedTime >= predictTime && m_predictionHistory.size() > 0)
					{
						Vec3 predPos;
						Quat predRot;
						//Vec3 predVel;
						//Vec3 predAngVel;
						if (m_predictionHistory.size() == 1)
						{
							predPos = m_predictionHistory[0].m_pos;
							predRot = m_predictionHistory[0].m_rot;
							//predVel = m_predictionHistory[0].m_velocity;
							//predAngVel = m_predictionHistory[0].m_angVelocity;
						}
						else
						{
							float t = (remainderTime / m_predictionHistory[0].m_deltaTime);
							predPos.SetLerp(m_predictionHistory[0].m_pos, m_predictionHistory[1].m_pos, t);
							predRot.SetSlerp(m_predictionHistory[0].m_rot, m_predictionHistory[1].m_rot, t);
							//predVel.SetLerp(m_predictionHistory[0].m_velocity, m_predictionHistory[1].m_velocity, t);
							//predAngVel.SetLerp(m_predictionHistory[0].m_angVelocity, m_predictionHistory[1].m_angVelocity, t);
						}

						float distErrorSq = predPos.GetSquaredDistance(netPos.pos);
						float rotErrorDotProd = predRot | netPos.rot;
						//float velErrorSq = predVel.GetSquaredDistance(netPos.vel);
						//float angVelErrorSq = predAngVel.GetSquaredDistance(netPos.angvel);

						if (distErrorSq < sqr(0.01f) && rotErrorDotProd > 0.9999f)
						{
							needsCorrection = false;
						}
					}
					else
					{
						needsCorrection = false;
					}
				}

				if (needsCorrection)
				{
					pe_params_flags savedFlags;
					pPhysics->GetParams(&savedFlags);
					pe_params_flags setFlags;
					setFlags.flagsAND = ~(pef_log_state_changes | pef_log_collisions | pef_log_env_changes | pef_log_poststep);
					pPhysics->SetParams(&setFlags, 1);

					pe_params_pos newPosParams;
					newPosParams.bRecalcBounds = 16 | 32 | 64;
					newPosParams.pos = netPos.pos;
					newPosParams.q = netPos.rot;
					pPhysics->SetParams(&newPosParams, 1);

					pe_action_set_velocity velocity;
					velocity.v = netPos.vel;
					velocity.w = netPos.angvel;
					pPhysics->Action(&velocity, 1);

					if (VehicleCVars().v_clientPredict && m_predictionHistory.size() > 0)
					{
						// Fast-forward the physics applying all the inputs we recorded
						ReplayPredictionHistory(remainderTime);

						pe_status_pos statusNewPos;
						pe_status_dynamics statusNewDynamics;
						pPhysics->GetStatus(&statusNewPos);
						pPhysics->GetStatus(&statusNewDynamics);

						sample.m_pos = statusNewPos.pos;
						sample.m_rot = statusNewPos.q;
						sample.m_velocity = statusNewDynamics.v;
						sample.m_angVelocity = statusNewDynamics.w;

						// Restore inputs back to what they were before we messed with them
						m_pMovement->RequestActions(sample.m_actions);
					}

					pPhysics->SetParams(&savedFlags, 1);
				}
				else
				{
#if ENABLE_VEHICLE_DEBUG
					m_debugClientPredictData.clear();
#endif
				}

				if (VehicleCVars().v_clientPredict)
				{
					m_predictionHistory.push_back(sample);

					if (VehicleCVars().v_clientPredictSmoothing)
					{
						pe_status_pos statusNewPos;
						pPhysics->GetStatus(&statusNewPos);

						if (needsCorrection)
						{
							// Error calculated this frame
							Vec3 posError = statusPos.pos - statusNewPos.pos;
							Quat rotError = statusPos.q * !statusNewPos.q;

							// Add it on to the error from the previous frames
							m_clientPositionError.t += posError;
							m_clientPositionError.q = rotError * m_clientPositionError.q;
						}

						// Smooth out the error to 0 over time
						const float smoothing = approxOneExp(deltaTime * VehicleCVars().v_clientPredictSmoothingConst);
						m_clientPositionError.t.SetLerp(m_clientPositionError.t, ZERO, smoothing);
						m_clientPositionError.q.SetSlerp(m_clientPositionError.q, IDENTITY, smoothing);

						m_clientSmoothedPosition.t.SetLerp(m_clientSmoothedPosition.t, statusNewPos.pos, smoothing);
						m_clientSmoothedPosition.q.SetSlerp(m_clientSmoothedPosition.q, statusNewPos.q, smoothing);
					}
				}
			}
		}
	}
}

//------------------------------------------------------------------------
// NOTE: This function must be thread-safe.
void CVehicle::ReplayPredictionHistory(const float remainderTime)
{
	if (m_predictionHistory.size() > 1 && remainderTime * 2.0f > m_predictionHistory[0].m_deltaTime)
	{
		m_pMovement->SetVehicleNetState(m_predictionHistory[1].m_state);
	}
	else
	{
		m_pMovement->SetVehicleNetState(m_predictionHistory[0].m_state);
	}

#if ENABLE_VEHICLE_DEBUG
	m_debugClientPredictData.clear();
#endif
	pe_status_pos statusPos;
	pe_status_dynamics statusDynamics;
	IPhysicalEntity* pPhysics = GetEntity()->GetPhysics();

	pe_params_flags set_update;
	set_update.flagsOR = pef_update;
	pPhysics->SetParams(&set_update, 1);
	m_status.doingNetPrediction = true;
	int physicsTime = gEnv->pPhysicalWorld->GetiPhysicsTime();  // Save the physics time

	TVehiclePredictionHistory::iterator itHistory;
	for (itHistory = m_predictionHistory.begin(); itHistory != m_predictionHistory.end(); ++itHistory)
	{
		float timeDiff = itHistory->m_deltaTime;
		if (itHistory == m_predictionHistory.begin())
		{
			timeDiff -= remainderTime;
			itHistory->m_deltaTime = timeDiff;
		}

		pPhysics->GetStatus(&statusPos);
		pPhysics->GetStatus(&statusDynamics);

#if ENABLE_VEHICLE_DEBUG
		SDebugClientPredictData data;
		data.recordedPos.t = itHistory->m_pos;
		data.recordedPos.q = itHistory->m_rot;
		data.recordedVel = itHistory->m_velocity;
		data.correctedPos.t = statusPos.pos;
		data.correctedPos.q = statusPos.q;
		data.correctedVel = statusDynamics.v;
		m_debugClientPredictData.push_back(data);
#endif

		itHistory->m_pos = statusPos.pos;
		itHistory->m_rot = statusPos.q;
		itHistory->m_velocity = statusDynamics.v;
		itHistory->m_angVelocity = statusDynamics.w;

		// Replay inputs
		m_pMovement->RequestActions(itHistory->m_actions);

		gEnv->pPhysicalWorld->TimeStep(timeDiff, ent_rigid | ent_flagged_only);
	}

	gEnv->pPhysicalWorld->SetiPhysicsTime(physicsTime); // Restore the physics time
	m_status.doingNetPrediction = false;
	pe_params_flags clear_update;
	clear_update.flagsAND = ~pef_update;
	pPhysics->SetParams(&clear_update, 1);
}

//------------------------------------------------------------------------
const SVehicleStatus& CVehicle::GetStatus() const
{
	return m_status;
}

//------------------------------------------------------------------------
// AI related

//------------------------------------------------------------------------
IMovementController* CVehicle::GetMovementController()
{
	if (m_seats.empty())
		return NULL;

	return m_seats[0].second;
}

//------------------------------------------------------------------------
IMovementController* CVehicle::GetPassengerMovementController(EntityId passengerId)
{
	CVehicleSeat* pSeat = (CVehicleSeat*)GetSeatForPassenger(passengerId);

	if (pSeat)
		return pSeat->GetMovementController();

	return NULL;
}

//------------------------------------------------------------------------
IFireController* CVehicle::GetFireController(uint32 controllerNum)
{
	if (!(controllerNum < m_seats.size()))
		return NULL;

	return m_seats[controllerNum].second;
}

//------------------------------------------------------------------------
bool CVehicle::NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags)
{
	if (m_pMovement)
		m_pMovement->Serialize(ser, aspect);

	// components
	float prevDmgRatio = (aspect == ASPECT_COMPONENT_DAMAGE) ? GetDamageRatio() : 0.f;

	for (TVehicleComponentVector::iterator itc = m_components.begin(); itc != m_components.end(); ++itc)
		(*itc)->Serialize(ser, aspect);

	if (aspect == ASPECT_COMPONENT_DAMAGE && ser.IsReading())
	{
		float deltaRatio = GetDamageRatio() - prevDmgRatio;
		if (abs(deltaRatio) > 0.001f)
		{
			EVehicleEvent event = (deltaRatio > 0.f) ? eVE_Damaged : eVE_Repair;
			SVehicleEventParams eventParams;
			eventParams.fParam = deltaRatio * m_damageMax;
			BroadcastVehicleEvent(event, eventParams);
		}
	}

	for (TVehicleSeatVector::iterator its = m_seats.begin(); its != m_seats.end(); ++its)
		(its->second)->Serialize(ser, aspect);

	NET_PROFILE_BEGIN("Parts", ser.IsReading());
	for (TVehiclePartVector::iterator itp = m_parts.begin(); itp != m_parts.end(); ++itp)
	{
		itp->second->Serialize(ser, aspect);
	}
	NET_PROFILE_END();

	if (aspect == eEA_Physics)
	{
		if (profile == eVPhys_DemoRecording)
		{
			uint8 currentProfile = 255;
			if (ser.IsWriting())
				currentProfile = GetGameObject()->GetAspectProfile(eEA_Physics);
			ser.Value("PhysicalizationProfile", currentProfile);
			return NetSerialize(ser, aspect, currentProfile, flags);
		}

		pe_type type = PE_NONE;

		switch (profile)
		{
		case eVPhys_Physicalized:
			{
				type = PE_RIGID;
				if (m_pMovement)
					type = m_pMovement->GetPhysicalizationType();
			}
			break;
		case eVPhys_NotPhysicalized:
			type = PE_NONE;
			break;
		}

		if (type == PE_NONE)
			return true;

		NET_PROFILE_SCOPE("Physics", ser.IsReading());

		GetEntity()->PhysicsNetSerializeTyped(ser, type, flags);
	}

	return true;
}

//------------------------------------------------------------------------
void CVehicle::FullSerialize(TSerialize ser)
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Vehicle serialization");
	ser.Value("indestructible", m_indestructible);
	bool isDestroyed = IsDestroyed();
	ser.Value("isDestroyed", isDestroyed, 'bool');

	if (ser.IsReading() && isDestroyed != IsDestroyed())
	{
		SetDestroyedStatus(isDestroyed);
	}

	ser.BeginGroup("Status");
	m_status.Serialize(ser, eEA_All);
	ser.EndGroup();

	ser.BeginGroup("Components");
	for (TVehicleComponentVector::iterator ite = m_components.begin(); ite != m_components.end(); ++ite)
	{
		CVehicleComponent* pComponent = *ite;
		ser.BeginGroup(pComponent->GetName().c_str());
		pComponent->Serialize(ser, eEA_All);
		ser.EndGroup();
	}
	ser.EndGroup();

	for (TVehicleDamagesGroupVector::iterator ite = m_damagesGroups.begin(); ite != m_damagesGroups.end(); ++ite)
	{
		CVehicleDamagesGroup* pDamageGroup = *ite;
		ser.BeginGroup("DamageGroup");
		pDamageGroup->Serialize(ser, eEA_All);
		ser.EndGroup();
	}

	for (TVehiclePartVector::iterator it = m_parts.begin(); it != m_parts.end(); ++it)
	{
		ser.BeginGroup("Part");
		it->second->Serialize(ser, eEA_All);
		ser.EndGroup();
	}

	if (m_pMovement)
	{
		ser.BeginGroup("Movement");
		m_pMovement->Serialize(ser, eEA_All);
		ser.EndGroup();
	}

	ser.BeginGroup("Seats");
	for (TVehicleSeatVector::iterator it = m_seats.begin(); it != m_seats.end(); ++it)
	{
		ser.BeginGroup(it->first);
		(it->second)->Serialize(ser, eEA_All);
		ser.EndGroup();
	}
	ser.EndGroup();

	ser.BeginGroup("Actions");
	TVehicleActionVector::iterator actionIte = m_actions.begin();
	TVehicleActionVector::iterator actionEnd = m_actions.end();
	for (; actionIte != actionEnd; ++actionIte)
	{
		SVehicleActionInfo& actionInfo = *actionIte;
		ser.BeginGroup("Actions");
		actionInfo.pAction->Serialize(ser, eEA_All);
		ser.EndGroup();
	}
	ser.EndGroup();

	if (ser.IsReading())
		DoRequestedPhysicalization();
}

//------------------------------------------------------------------------
void CVehicle::PostSerialize()
{
	for (TVehiclePartVector::iterator it = m_parts.begin(), end = m_parts.end(); it != end; ++it)
		it->second->PostSerialize();

	for (TVehicleSeatVector::iterator it = m_seats.begin(), end = m_seats.end(); it != end; ++it)
		it->second->PostSerialize();

	if (m_pMovement)
		m_pMovement->PostSerialize();

	CreateAIHidespots();
}

//------------------------------------------------------------------------
void CVehicle::OnAction(const TVehicleActionId actionId, int activationMode, float value, EntityId callerId)
{
	if (CVehicleSeat* pCurrentSeat = (CVehicleSeat*)GetSeatForPassenger(callerId))
	{
		pCurrentSeat->OnAction(actionId, activationMode, value);

		bool performAction = pCurrentSeat->GetCurrentTransition() == CVehicleSeat::eVT_None;
		if (!performAction)
		{
			IActorSystem* pActorSystem = CCryAction::GetCryAction()->GetIActorSystem();
			IActor* pActor = pActorSystem->GetActor(callerId);
			if (pActor && pActor->IsClient())
				performAction = (pCurrentSeat->GetCurrentTransition() == CVehicleSeat::eVT_Entering && actionId != eVAI_Exit);
		}
		if (performAction && activationMode == eAAM_OnPress)
		{
			if (actionId == eVAI_Exit)
			{
				IActorSystem* pActorSystem = CCryAction::GetCryAction()->GetIActorSystem();
				IActor* pActor = pActorSystem->GetActor(callerId);
				CRY_ASSERT(pActor);

				Matrix34 worldTM = pCurrentSeat->GetExitTM(pActor, false);
				Vec3 pos = worldTM.GetTranslation();
				EntityId blockerId = 0;
				bool canExit = pCurrentSeat->TestExitPosition(pActor, pos, &blockerId);
				if (!canExit)
					canExit = m_pVehicle->GetExitPositionForActor(pActor, pos);

				ExitVehicleAtPosition(callerId, pos);
			}
			else if (actionId >= eVAI_ChangeSeat && actionId <= eVAI_ChangeSeat5)
			{
				int seatIndex = actionId - eVAI_ChangeSeat;

				if (gEnv->bServer)
				{
					ChangeSeat(callerId, seatIndex);
				}
				else
				{
					if (IActor* pUser = CCryAction::GetCryAction()->GetIActorSystem()->GetActor(callerId))
					{
						if (!pUser->IsStillWaitingOnServerUseResponse())
						{
							if (pCurrentSeat->GetSeatGroupIndex() != -1)
							{
								pUser->SetStillWaitingOnServerUseResponse(true);
								GetGameObject()->InvokeRMI(SvRequestChangeSeat(), RequestChangeSeatParams(callerId, seatIndex), eRMI_ToServer);
							}
						}
					}
				}
			}
		}
	}
}

//------------------------------------------------------------------------
void CVehicle::OnHit(const HitInfo& hitInfo, IVehicleComponent* pHitComponent)
{
	if (IsDestroyed() || hitInfo.damage == 0.0f)
		return;

	float damage = hitInfo.damage;

	// repair event
	if (hitInfo.type == s_repairHitTypeId)
	{
		damage = -damage;
	}

	if (hitInfo.type == s_disableCollisionsHitTypeId)
	{
		m_collisionDisabledTime = damage;
		return;
	}

	if (damage > 0.f)
	{
		if (m_indestructible || VehicleCVars().v_goliathMode == 1)
			return;

		if (gEnv->bMultiplayer)
		{
			if (GetEntityId() != hitInfo.shooterId)
			{
				IEntity* pShooter = gEnv->pEntitySystem->GetEntity(hitInfo.shooterId);

				if (pShooter && HasFriendlyPassenger(pShooter))
					return;
			}
		}
		else if (IActor* pActor = CCryAction::GetCryAction()->GetClientActor())
		{
			if (IsPlayerDriving())
			{
				if (pActor->IsGod() > 1) // return in Team God
					return;
			}
			else if ((hitInfo.shooterId == pActor->GetEntityId()) && HasFriendlyPassenger(pActor->GetEntity())) //don't deal damage to friendly tanks
				return;
		}

		SVehicleEventParams eventParams;
		eventParams.fParam = damage;
		eventParams.iParam = hitInfo.type;
		BroadcastVehicleEvent(eVE_Hit, eventParams);
	}

	float radius = hitInfo.radius;
	if (hitInfo.type == s_collisionHitTypeId)
	{
		if (m_collisionDisabledTime > 0.0f)
			return;

		if (damage < 1.f)
			return;

		// collisions don't pass radius, therefore we determine one based on the damage
		// weighted against collision resistance. this is pretty approximate, but should
		// serve our purpose of damaging components near the hit pos.
		if (hitInfo.radius == 0.f)
			radius = max(0.25f, min(1.f, 0.5f * (damage + m_damageParams.collisionDamageThreshold) / max(1.f, m_damageParams.collisionDamageThreshold)));
	}

	Vec3 localPos = GetEntity()->GetWorldTM().GetInverted() * hitInfo.pos;

#if ENABLE_VEHICLE_DEBUG
	if (VehicleCVars().v_debugdraw == eVDB_Damage)
	{
		IGameRules* pGR = CCryAction::GetCryAction()->GetIGameRulesSystem()->GetCurrentGameRules();
		const char* pHitClass = pGR->GetHitType(hitInfo.type);
		CryLog("============================================================");
		CryLog("<%s> OnHit ('%s'): localPos %.2f %.2f %.2f, radius %.2f", GetEntity()->GetName(), pHitClass, localPos.x, localPos.y, localPos.z, radius);
	}
#endif

	float prevDmgRatio = GetDamageRatio();

	// first gather all affected components, then call OnHit and pass them the list
	TVehicleComponentVector components;

	if (pHitComponent) // Has hit component been specified?
	{
		if (pHitComponent->GetMaxDamage() != 0.f)
		{
			components.push_back(static_cast<CVehicleComponent*>(pHitComponent));
		}
	}
	else
	{
		for (TVehicleComponentVector::iterator ite = m_components.begin(), end = m_components.end(); ite != end; ++ite)
		{
			CVehicleComponent* pComponent = *ite;

			if (pComponent->GetMaxDamage() == 0.f)
				continue;

			const AABB& localBounds = pComponent->GetBounds();
			if (localBounds.IsReset() || localBounds.IsEmpty())
				continue;

			bool hit = true;
			if (damage > 0.f)
			{
				if (radius > 0.f)
				{
					if (!localBounds.IsOverlapSphereBounds(localPos, radius))
						hit = false;
				}
				else
				{
					if (!localBounds.IsContainPoint(localPos))
						hit = false;
				}
			}

			if (hit)
				components.push_back(pComponent);
		}
	}

	for (TVehicleComponentVector::iterator ite = components.begin(), end = components.end(); ite != end; ++ite)
	{
		CVehicleComponent* pComponent = *ite;
		pComponent->OnHit(hitInfo, &components);

		if (pComponent->IsHull())
			m_status.health = 1.f - pComponent->GetDamageRatio();
	}

	// direct explosion hits (eg with LAW) kill gunners in singleplayer, but only if the weapon was fired by the player.
	if (!gEnv->bMultiplayer && hitInfo.explosion && (hitInfo.targetId == GetEntityId()) && damage > 260)
	{
		IActor* pShooter = CCryAction::GetCryAction()->GetIActorSystem()->GetActor(hitInfo.shooterId);
		if (pShooter && pShooter->IsPlayer())
		{
			KillPassengersInExposedSeats(false);
		}
	}

	float damageRatio = GetDamageRatio();
	float deltaRatio = damageRatio - prevDmgRatio;
	if (deltaRatio != 0.f)
	{
		EVehicleEvent event = (deltaRatio > 0.f) ? eVE_Damaged : eVE_Repair;
		SVehicleEventParams eventParams;
		eventParams.fParam = deltaRatio * m_damageMax;
		eventParams.fParam2 = damageRatio;
		eventParams.entityId = hitInfo.shooterId;
		BroadcastVehicleEvent(event, eventParams);
	}

	if (gEnv->bMultiplayer && m_bCanBeAbandoned && gEnv->bServer && damageRatio > 0.05f && IsEmpty())
		StartAbandonedTimer();
}

//------------------------------------------------------------------------
bool CVehicle::IsCrewHostile(EntityId userId)
{
	if (GetStatus().passengerCount == 0)
		return false;

	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(userId);
	IAIObject* pUserAIObject = pEntity ? pEntity->GetAI() : 0;

	if (!pUserAIObject)
		return false;

	for (TVehicleSeatVector::const_iterator it = m_seats.begin(), end = m_seats.end(); it != end; ++it)
	{
		IActor* pPassengerActor = it->second->GetPassengerActor();

		if (pPassengerActor != NULL)
		{
			if (pPassengerActor->IsDead())
				continue;

			IAIObject* pPassengerAIObject = pPassengerActor->GetEntity()->GetAI();

			if (pPassengerAIObject)
				return pPassengerAIObject->IsHostile(pUserAIObject, false);
		}
	}

	return false;
}

//------------------------------------------------------------------------
bool CVehicle::IsActionUsable(const SVehicleActionInfo& actionInfo, const SMovementState* movementState)
{
	for (TActivationInfoVector::const_iterator ite = actionInfo.activations.begin(), end = actionInfo.activations.end();
	     ite != end; ++ite)
	{
		const SActivationInfo& activationInfo = *ite;

		if (activationInfo.m_type == SActivationInfo::eAT_OnUsed)
		{
			AABB localbounds;
			AABB bounds;
			localbounds.Reset();

			if (activationInfo.m_param1 == SActivationInfo::eAP1_Component)
			{
				if (IVehicleComponent* pComponent = GetComponent(activationInfo.m_param2.c_str()))
					localbounds = pComponent->GetBounds();
			}
			else if (activationInfo.m_param1 == SActivationInfo::eAP1_Part)
			{
				if (IVehiclePart* pPart = GetPart(activationInfo.m_param2.c_str()))
					localbounds = pPart->GetLocalBounds();
			}
			bounds = AABB::CreateTransformedAABB(GetEntity()->GetWorldTM(), localbounds);

			Vec3 intersectOut;
			Lineseg lineSeg(movementState->eyePosition, movementState->eyePosition + movementState->eyeDirection * activationInfo.m_distance);

			int hit = Intersect::Lineseg_AABB(lineSeg, bounds, intersectOut);

#if ENABLE_VEHICLE_DEBUG
			if (VehicleCVars().v_debugdraw > 1)
			{
				IRenderer* pRenderer = gEnv->pRenderer;
				IRenderAuxGeom* pRenderAux = gEnv->pAuxGeomRenderer;
				const Matrix34& worldTM = m_pVehicle->GetEntity()->GetWorldTM();
				pRenderAux->DrawAABB(localbounds, worldTM, false, hit ? Col_Green : Col_Red, eBBD_Faceted);
				pRenderAux->DrawLine(lineSeg.start, Col_Green, lineSeg.end, Col_Green);
			}
#endif

			if (hit) return true;
		}
	}

	return false;
}

//------------------------------------------------------------------------
int CVehicle::IsUsable(EntityId userId)
{
	if (IsDestroyed() || IsSubmerged() || VehicleCVars().v_disableEntry != 0)
		return 0;

	if (!gEnv->bMultiplayer)
	{
		if (IsCrewHostile(userId))
			return 0;
	}

	IActor* pActor = CCryAction::GetCryAction()->GetIActorSystem()->GetActor(userId);
	if (pActor == NULL)
		return false;

	IMovementController* pController = pActor->GetMovementController();
	CRY_ASSERT(pController);
	SMovementState movementState;
	pController->GetMovementState(movementState);

	const int actionsUsable = IsFlipped() ? 0 : 1;

	if (m_usesVehicleActionToEnter)
	{
		int actionIndex = 0;

		for (TVehicleActionVector::iterator ite = m_actions.begin(), end = m_actions.end(); ite != end; ++ite)
		{
			SVehicleActionInfo& actionInfo = *ite;

			if ((actionsUsable ^ actionInfo.useWhenFlipped) && IsActionUsable(actionInfo, &movementState))
			{
				SVehicleEventParams eventParams;
				eventParams.entityId = userId;

				if (actionInfo.pAction->OnEvent(eVAE_IsUsable, eventParams) > 0)
				{
					int ret = (actionIndex << 16) + eventParams.iParam;
					return ret;
				}
			}

			actionIndex++;
		}
	}
	else
	{
		int seatIndex = 1;

		for (TVehicleSeatVector::iterator ite = m_seats.begin(), end = m_seats.end(); ite != end; ++ite)
		{
			CVehicleSeat* pSeat = ite->second;
			if (pSeat->IsFree(pActor))
				return seatIndex;

			seatIndex++;
		}
	}

	return 0;
}

//------------------------------------------------------------------------
bool CVehicle::OnUsed(EntityId userId, int index)
{
	if (index <= 0)
		return false;

	IActor* pUser = CCryAction::GetCryAction()->GetIActorSystem()->GetActor(userId);
	if (!pUser)
		return false;

	if (gEnv->bServer)
	{
		if (!CanEnter(userId))
			return false;

		const string userDesc = pUser->GetEntity()->GetEntityTextDescription();
		const string vehicleDesc = GetEntity()->GetEntityTextDescription();
		CryLog("[VEHICLE] %s is using vehicle %s...", userDesc.c_str(), vehicleDesc.c_str());
		INDENT_LOG_DURING_SCOPE();

		if (m_usesVehicleActionToEnter)
		{
			int actionIndex = (index & 0xffff0000) >> 16;
			int param = index & 0x0000ffff;

			assert(actionIndex < m_actions.size());

			SVehicleEventParams eventParams;
			eventParams.entityId = userId;
			eventParams.iParam = param;
			m_actions[actionIndex].pAction->OnEvent(eVAE_OnUsed, eventParams);
		}
		else
		{
			if (IVehicleSeat* pSeat = GetSeatById(index))
			{
				if (pSeat->Enter(userId))
					return true;
			}
		}
	}
	else
	{
		if (!pUser->IsStillWaitingOnServerUseResponse())
		{
			pUser->SetStillWaitingOnServerUseResponse(true);
			GetGameObject()->InvokeRMI(SvRequestUse(), RequestUseParams(userId, index), eRMI_ToServer);
		}
	}

	return true;
}

//------------------------------------------------------------------------
IVehiclePart* CVehicle::AddPart(const CVehicleParams& partParams, IVehiclePart* parent, SPartInitInfo& initInfo)
{
	string partClass = partParams.getAttr("class");
	string partName = partParams.getAttr("name");

	if (partClass.empty() || partName.empty())
		return NULL;

	for (TVehiclePartVector::iterator ite = m_parts.begin(); ite != m_parts.end(); ++ite)
	{
		if (ite->first == partName)
			return NULL;
	}

	if (IVehiclePart* pPart = m_pVehicleSystem->CreateVehiclePart(partClass))
	{
		initInfo.index = m_parts.size();
		// using base as a default part type here, actual version is filled in by the part class
		if (!((CVehiclePartBase*)pPart)->Init(this, partParams, parent, initInfo, IVehiclePart::eVPT_Base))
		{
			CryLog("Vehicle error: part <%s> failed to initialize on vehicle <%s>", partName.c_str(), GetEntity()->GetName());

			pPart->Release();
			return NULL;
		}

		m_parts.push_back(TVehiclePartPair(partName, pPart));
		return pPart;
	}

	return NULL;
}

//------------------------------------------------------------------------
bool CVehicle::AddSeat(const SmartScriptTable& seatTable)
{
	int seatId = 0;
	if (seatTable->GetValue("seatId", seatId))
	{
		CCryAction::GetCryAction()->GetVehicleSeatScriptBind()->AttachTo(this, seatId);
	}

	return false;
}

//------------------------------------------------------------------------
IVehiclePart* CVehicle::GetPart(const char* name)
{
	if (0 == name)
		return NULL;

	for (TVehiclePartVector::iterator ite = m_parts.begin(); ite != m_parts.end(); ++ite)
	{
		if (0 == strcmp(name, ite->first))
		{
			IVehiclePart* part = ite->second;
			return part;
		}
	}

	return NULL;
}

//------------------------------------------------------------------------
IVehicleAnimation* CVehicle::GetAnimation(const char* name)
{
	if (0 == name)
		return NULL;

	for (TVehicleAnimationsVector::iterator ite = m_animations.begin(); ite != m_animations.end(); ++ite)
	{
		if (0 == strcmp(name, ite->first))
			return ite->second;
	}

	return NULL;
}

//------------------------------------------------------------------------
bool CVehicle::AddHelper(const char* pName, Vec3 position, Vec3 direction, IVehiclePart* pPart)
{
	TVehicleHelperMap::iterator it = m_helpers.find(CONST_TEMP_STRING(pName));
	IF_UNLIKELY (it != m_helpers.end())
	{
		GameWarning("Vehicle::AddHelper: Helper '%s' already defined for vehicle '%s', skipping", pName, GetEntity()->GetClass()->GetName());
		return true;
	}

	CVehicleHelper* pHelper = new CVehicleHelper;

	if (position.IsZero() && (direction.IsZero() || direction.IsEquivalent(FORWARD_DIRECTION)))
	{
		if (CVehiclePartAnimated* pAnimatedPart = GetVehiclePartAnimated())
		{
			if (ICharacterInstance* pCharInstance =
			      pAnimatedPart->GetEntity()->GetCharacter(pAnimatedPart->GetSlot()))
			{
				IDefaultSkeleton& rIDefaultSkeleton = pCharInstance->GetIDefaultSkeleton();
				ISkeletonPose* pSkeletonPose = pCharInstance->GetISkeletonPose();
				CRY_ASSERT(pSkeletonPose);

				int16 id = rIDefaultSkeleton.GetJointIDByName(pName);
				if (id > -1)
				{
					pHelper->m_localTM = Matrix34(pSkeletonPose->GetRelJointByID(id));
					pHelper->m_pParentPart = pPart;

					m_helpers.insert(TVehicleHelperMap::value_type(pName, pHelper));
					return true;
				}
			}
		}
	}
	else
	{
		Matrix34 vehicleTM = Matrix33::CreateRotationVDir(direction.GetNormalizedSafe(FORWARD_DIRECTION));
		vehicleTM.SetTranslation(position);

		pHelper->m_localTM = pPart->GetLocalTM(false).GetInverted() * vehicleTM;
		pHelper->m_pParentPart = pPart;

		m_helpers.insert(TVehicleHelperMap::value_type(pName, pHelper));
		return true;
	}

	SAFE_RELEASE(pHelper);

	return false;
}

//------------------------------------------------------------------------
void CVehicle::InitHelpers(const CVehicleParams& table)
{
	CVehicleParams helpersTable = table.findChild("Helpers");
	if (!helpersTable)
		return;

	int i = 0;
	int c = helpersTable.getChildCount();

	for (; i < c; i++)
	{
		if (CVehicleParams helperRef = helpersTable.getChild(i))
		{
			//<Helper name="exhaust_pos" position="0.56879997,-2.7351,0.62400001" direction="0,-1,0" part="body" />

			if (!helperRef.haveAttr("name") || !helperRef.haveAttr("part"))
				continue;

			Vec3 position, direction;
			if (!helperRef.getAttr("position", position))
				position.zero();

			if (!helperRef.getAttr("direction", direction))
				direction.zero();

			if (IVehiclePart* pPart = GetPart(helperRef.getAttr("part")))
				AddHelper(helperRef.getAttr("name"), position, direction, pPart);
			else
				CryLog("Error adding helper <%s> on vehicle <%s>", helperRef.getAttr("part"), GetEntity()->GetName());
		}
	}

	// if loading a save, ai hidespots will be created in PostSerialize instead of here
	if (SpawnAndDeleteEntities(true))
		CreateAIHidespots();
}

void CVehicle::CreateAIHidespots()
{
	TVehicleHelperMap::iterator endIte = m_helpers.end();
	for (TVehicleHelperMap::iterator it = m_helpers.begin(); it != endIte; ++it)
	{
		const string& helperName = it->first;
		IVehicleHelper* pHelper = it->second;

		// if AI helper, spawn AIAnchor entity
		if (helperName.substr(0, 3) == "ai_")
		{
			IEntityClass* pEntityClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("AIAnchor");

			if (!pEntityClass)
			{
				CryLog("Entity class 'AIAnchor' not found (entity name: %s)", GetEntity()->GetName());
				continue;
			}

			string entName(GetEntity()->GetName());
			entName += "_";
			entName += helperName;

			IEntity* pEntity = 0;

			SEntitySpawnParams spawnParams;
			spawnParams.sName = entName.c_str();
			spawnParams.nFlags = ENTITY_FLAG_CLIENT_ONLY | ENTITY_FLAG_NO_SAVE;   // don't save: recreated in PostSerialize on reading
			spawnParams.pUserData = 0;
			spawnParams.pClass = pEntityClass;

			pEntity = gEnv->pEntitySystem->SpawnEntity(spawnParams, false);
			IScriptTable* pEntityTable = pEntity->GetScriptTable();

			if (pEntityTable)
			{
				SmartScriptTable entityProps;
				if (pEntityTable->GetValue("Properties", entityProps))
				{
					entityProps->SetValue("aianchor_AnchorType", "COMBAT_HIDESPOT");
				}
			}
			gEnv->pEntitySystem->InitEntity(pEntity, spawnParams);

			GetEntity()->AttachChild(pEntity);
			Matrix34 tm;
			pHelper->GetVehicleTM(tm);
			pEntity->SetLocalTM(tm);
		}
	}
}

//------------------------------------------------------------------------
bool CVehicle::HasHelper(const char* pName)
{
	TVehicleHelperMap::iterator ite = m_helpers.find(CONST_TEMP_STRING(pName));
	return (ite != m_helpers.end());
}

//------------------------------------------------------------------------
bool CVehicle::AddComponent(const CVehicleParams& componentParams)
{
	CVehicleComponent* pComponent = new CVehicleComponent;

	if (pComponent && pComponent->Init(this, componentParams))
	{
		m_components.push_back(pComponent);
		return true;
	}

	if (pComponent)
		delete pComponent;

	return false;
}

//------------------------------------------------------------------------
IVehicleComponent* CVehicle::GetComponent(const char* name)
{
	if (0 == name)
		return NULL;

	for (TVehicleComponentVector::iterator ite = m_components.begin(); ite != m_components.end(); ++ite)
	{
		if (0 == strcmpi(name, (*ite)->GetName().c_str()))
		{
			return (IVehicleComponent*) *ite;
		}
	}

	return NULL;
}

//------------------------------------------------------------------------
void CVehicle::SetUnmannedFlippedThresholdAngle(float angle)
{
	m_unmannedflippedThresholdDot = cos_tpl(angle);
}

//------------------------------------------------------------------------
// TODO: this could be updated and stored in OnHit instead of calculated each time
float CVehicle::GetDamageRatio(bool onlyMajorComponents /*=false*/) const
{
	float damageRatio = 0.0f;
	if (onlyMajorComponents)
	{
		if (m_majorComponentDamageMax == 0.0f)
			return 0.0f;

		int numComp = 0;
		for (TVehicleComponentVector::const_iterator ite = m_components.begin(); ite != m_components.end(); ++ite)
		{
			if ((*ite)->IsMajorComponent())
			{
				damageRatio += (*ite)->GetDamageRatio();
				++numComp;
			}
		}
		if (numComp)
			damageRatio /= numComp;
	}
	else
	{
		if (m_damageMax == 0.0f)
			return 0.0f;

		float invMaxDamage = 1.0f / m_damageMax;
		for (TVehicleComponentVector::const_iterator ite = m_components.begin(), end = m_components.end(); ite != end; ++ite)
		{
			damageRatio += (*ite)->GetDamageRatio() * (*ite)->GetMaxDamage() * invMaxDamage;
		}
	}

	return damageRatio;
}

//------------------------------------------------------------------------
IVehiclePart* CVehicle::GetWeaponParentPart(EntityId weaponId)
{
	for (TVehicleSeatId id = FirstVehicleSeatId; id <= GetSeatCount(); ++id)
	{
		const CVehicleSeat* pSeat = (CVehicleSeat*)GetSeatById(id);

		const TVehicleSeatActionVector& seatActions = pSeat->GetSeatActions();
		for (TVehicleSeatActionVector::const_iterator it = seatActions.begin(), itEnd = seatActions.end(); it != itEnd; ++it)
		{
			if (CVehicleSeatActionWeapons* pAction = CAST_VEHICLEOBJECT(CVehicleSeatActionWeapons, it->pSeatAction))
			{
				if (SVehicleWeapon* pWeaponInfo = pAction->GetVehicleWeapon(weaponId))
				{
					return pWeaponInfo->pPart;
				}
			}
		}
	}

	return NULL;
}

//------------------------------------------------------------------------
IVehicleSeat* CVehicle::GetWeaponParentSeat(EntityId weaponId)
{
	for (TVehicleSeatId id = FirstVehicleSeatId; id <= GetSeatCount(); ++id)
	{
		CVehicleSeat* pSeat = (CVehicleSeat*)GetSeatById(id);

		const TVehicleSeatActionVector& seatActions = pSeat->GetSeatActions();
		for (TVehicleSeatActionVector::const_iterator it = seatActions.begin(), itEnd = seatActions.end(); it != itEnd; ++it)
		{
			if (CVehicleSeatActionWeapons* pAction = CAST_VEHICLEOBJECT(CVehicleSeatActionWeapons, it->pSeatAction))
			{
				if (SVehicleWeapon* pWeaponInfo = pAction->GetVehicleWeapon(weaponId))
				{
					return pSeat;
				}
			}
		}
	}

	return NULL;
}

//------------------------------------------------------------------------
bool CVehicle::SetMovement(const string& movementName, const CVehicleParams& table)
{
	assert(!m_pMovement);

	m_pMovement = m_pVehicleSystem->CreateVehicleMovement(movementName);

	if (m_pMovement && m_pMovement->Init(this, table))
	{
		Physicalize();

		return true;
	}
	else
	{
		CryLogAlways("CVehicle::SetMovemement() failed to init movement <%s> on vehicle of class <%s>.", movementName.c_str(), GetEntity()->GetClass()->GetName());
		SAFE_RELEASE(m_pMovement);

		return false;
	}
}

//------------------------------------------------------------------------
void CVehicle::OnPhysPostStep(const EventPhys* pEvent, bool logged)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);
	const EventPhysPostStep* eventPhys = (const EventPhysPostStep*)pEvent;
	float deltaTime = eventPhys->dt;
	if (logged)
	{
		m_physUpdateTime += deltaTime;

		if (m_hasAuthority && !gEnv->bServer && VehicleCVars().v_serverControlled && VehicleCVars().v_clientPredict && VehicleCVars().v_clientPredictSmoothing)
		{
			// Calculate a render position which doesn't snap about too much
			Vec3 renderPos;
			Quat renderRot;

			if (VehicleCVars().v_clientPredictSmoothing == 1)
			{
				renderPos = eventPhys->pos + m_clientPositionError.t;
				renderRot = m_clientPositionError.q * eventPhys->q;
			}
			else
			{
				renderPos = m_clientSmoothedPosition.t;
				renderRot = m_clientSmoothedPosition.q;
			}

			IEntity* pEntity = GetEntity();
			{
				// Potential optimisation: Should stop CEntityPhysics::OnPhysicsPostStep from also calling SetPosRotScale as it gets overridden here anyway
				pEntity->SetPosRotScale(renderPos, renderRot, Vec3(1.0f, 1.0f, 1.0f), ENTITY_XFORM_IGNORE_PHYSICS);
			}
		}
	}
	else
	{
		// Immediate event - pass on to movement
		if (IVehicleMovement* pMovement = GetMovement())
		{
			// (MATT) Avoid f.p. exceptions in quickload {2009/05/25}
			if (pMovement->IsMovementProcessingEnabled() && deltaTime > 0.0f)
				pMovement->ProcessMovement(deltaTime);
		}
	}
}

//------------------------------------------------------------------------
void CVehicle::OnCollision(EventPhysCollision* pCollision)
{
	if (m_status.beingFlipped || m_collisionDisabledTime > 0.0f)
		return;

	IEntity* pE1 = gEnv->pEntitySystem->GetEntityFromPhysics(pCollision->pEntity[0]);
	IEntity* pE2 = gEnv->pEntitySystem->GetEntityFromPhysics(pCollision->pEntity[1]);

	int idx = (pE1 == GetEntity()) ? 0 : 1;
	IEntity* pCollider = idx ? pE1 : pE2;

	SVehicleEventParams eventParams;
	eventParams.entityId = pCollider ? pCollider->GetId() : 0;
	eventParams.vParam = pCollision->pt;
	eventParams.fParam = pCollision->vloc[idx].GetLength();
	eventParams.fParam2 = pCollision->normImpulse;
	BroadcastVehicleEvent(eVE_Collision, eventParams);

	if (m_pMovement)
	{
		SVehicleMovementEventParams params;

		params.fValue = pCollision->normImpulse;
		params.sParam = (const char*)pCollision;

		if (pCollider)
		{
			params.iValue = pCollision->partid[idx];

			m_pMovement->OnEvent(IVehicleMovement::eVME_Collision, params);
		}
		else
		{
			m_pMovement->OnEvent(IVehicleMovement::eVME_GroundCollision, params);
		}
	}

	if (!pCollider)
	{
		for (TVehicleActionVector::const_iterator ite = m_actions.begin(); ite != m_actions.end(); ++ite)
		{
			const SVehicleActionInfo& actionInfo = *ite;

			for (TActivationInfoVector::const_iterator iter = actionInfo.activations.begin(); iter != actionInfo.activations.end(); ++iter)
			{
				const SActivationInfo& activationInfo = *iter;

				if (activationInfo.m_type == SActivationInfo::eAT_OnGroundCollision)
				{
					IVehicleAction* pAction = actionInfo.pAction;

					SVehicleEventParams eventParams2;
					eventParams2.iParam = 0;
					eventParams2.vParam = pCollision->pt;
					EVehicleActionEvent event = eVAE_OnGroundCollision;
					pAction->OnEvent(event, eventParams2);
				}
			}
		}
	}
}

//------------------------------------------------------------------------
float CVehicle::GetSelfCollisionMult(const Vec3& velocity, const Vec3& normal, int partId, EntityId colliderId) const
{
	float mult = 1.f;
	bool wheel = false;

	if (m_wheelCount && partId >= 0)
	{
		pe_status_wheel sw;
		sw.partid = partId;
		wheel = GetEntity()->GetPhysics()->GetStatus(&sw) && sw.suspLen0 > 0.f;
	}

	float zspeed = -velocity.z * normal.z;
	if (colliderId == 0 && zspeed > m_damageParams.groundCollisionMinSpeed)
	{
#if ENABLE_VEHICLE_DEBUG
		if (VehicleCVars().v_debugCollisionDamage)
			CryLog("[coll] %s [part %i], zspeed: %.1f [min: %.1f, max: %.1f]", GetEntity()->GetName(), partId, zspeed, m_damageParams.groundCollisionMinSpeed, m_damageParams.groundCollisionMaxSpeed);
#endif

		float diffSpeed = m_damageParams.groundCollisionMaxSpeed - m_damageParams.groundCollisionMinSpeed;
		float diffMult = m_damageParams.groundCollisionMaxMult - m_damageParams.groundCollisionMinMult;

		float speedratio = (diffSpeed > 0.f) ? (zspeed - m_damageParams.groundCollisionMinSpeed) / diffSpeed : 1.f;
		float groundMult = m_damageParams.groundCollisionMinMult + speedratio * diffMult;

		bool hard = zspeed > m_damageParams.groundCollisionMaxSpeed && !wheel;
		if (hard)
			groundMult = max(groundMult, 5.f * m_damageParams.groundCollisionMaxMult);

#if ENABLE_VEHICLE_DEBUG
		if (VehicleCVars().v_debugCollisionDamage)
			CryLog("[coll] %s ratio %.2f, groundmult %.2f", GetEntity()->GetName(), speedratio, groundMult);
#endif

		mult *= groundMult;
	}

	if (wheel)
	{
		const float wheelMaxAbsorbance = 0.5f;

		// absorb collisions along suspension axis (here: always along entity z)
		float dot = normal.Dot(GetEntity()->GetWorldTM().GetColumn2());
		float wheelMult = 1.f - wheelMaxAbsorbance * abs(dot);

#if ENABLE_VEHICLE_DEBUG
		if (wheelMult < 0.98f)
		{
			if (VehicleCVars().v_debugCollisionDamage)
				CryLog("[coll] %s, wheelMult %.2f", GetEntity()->GetName(), wheelMult);
		}
#endif

		mult *= wheelMult;
	}

	// for task 29233: TASK: SYSTEMIC: VEHICLE: Upon 2 Vehicles Colliding at a certain speed - both should explode
	if (colliderId && m_damageParams.vehicleCollisionDestructionSpeed > 0 && !IsDestroyed())
	{
		if (IVehicle* pVehicle = m_pVehicleSystem->GetVehicle(colliderId))
		{
			// this will destroy us
			float thresholdSq = m_damageParams.vehicleCollisionDestructionSpeed * m_damageParams.vehicleCollisionDestructionSpeed;
			if (velocity.GetLengthSquared() > thresholdSq)
			{
				// tweak: make sure at least one of the vehicles is above the threshold. Prevents the MP 2-vehicles-at-moderate-speed explosion...
				if ((GetStatus().speed * GetStatus().speed > thresholdSq)
				    || (pVehicle->GetStatus().speed * pVehicle->GetStatus().speed > thresholdSq))
				{
					mult *= 1000.0f;  // should do it :)

#if ENABLE_VEHICLE_DEBUG
					if (VehicleCVars().v_debugCollisionDamage)
						CryLog("[coll] above destruction speed: speed %.2f, threshold %.2f, mult %.2f", velocity.GetLength(), m_damageParams.vehicleCollisionDestructionSpeed, mult);
#endif
				}
			}

			// also destroy other vehicle? If the other vehicle has a vehicleCollisionDestructionSpeed set,
			//	then it will also get this collision and kill itself. Otherwise, it'll just be damaged by
			//	our explosion (which is enough to take out a truck, destroy a tank's treads (but not the tank), etc...
		}
	}

	return mult;
}

//------------------------------------------------------------------------
bool CVehicle::IsProbablyDistant() const
{
	return GetGameObject()->IsProbablyDistant();
}

//------------------------------------------------------------------------
bool CVehicle::InitParticles(const CVehicleParams& table)
{
	CVehicleParams particleTable = table.findChild("Particles");

	if (!table || !particleTable)
		return false;

	CEnvironmentParticles& envParams = m_particleParams.m_envParams;

	// previously this was hardcoded to use vfx_<vehicleclass> as the lookup for
	//	material effects (requiring a new row in the materialeffects.xml file for
	//	each vehicle type). Now can be shared between vehicles.
	if (particleTable.haveAttr("mfxRow"))
		envParams.mfx_rowName = particleTable.getAttr("mfxRow");
	else
		envParams.mfx_rowName.Format("vfx_%s", GetEntity()->GetClass()->GetName());

	if (CVehicleParams exhaustTable = particleTable.findChild("Exhaust"))
	{
		CExhaustParams& exhaustParams = m_particleParams.m_exhaustParams;

		if (CVehicleParams pTable = exhaustTable.findChild("Helpers"))
		{
			int i = 0;
			int c = pTable.getChildCount();

			for (; i < c; i++)
			{
				if (CVehicleParams helperRef = pTable.getChild(i))
				{
					if (IVehicleHelper* pHelper = GetHelper(helperRef.getAttr("value")))
						exhaustParams.helpers.push_back(pHelper);
				}
			}
		}

		if (exhaustParams.helpers.size() != 0)
		{
			exhaustTable.getAttr("insideWater", exhaustParams.insideWater);
			exhaustTable.getAttr("outsideWater", exhaustParams.outsideWater);

			if (CVehicleParams pTable = exhaustTable.findChild("EngineStart"))
			{
				exhaustParams.startEffect = pTable.getAttr("effect");

				if (!exhaustParams.startEffect.empty())
					m_pParticleEffects.push_back(gEnv->pParticleManager->FindEffect(exhaustParams.startEffect, "CVehicle::InitParticles"));
			}

			if (CVehicleParams pTable = exhaustTable.findChild("EngineStop"))
			{
				exhaustParams.stopEffect = pTable.getAttr("effect");

				if (!exhaustParams.stopEffect.empty())
					m_pParticleEffects.push_back(gEnv->pParticleManager->FindEffect(exhaustParams.stopEffect, "CVehicle::InitParticles"));
			}

			if (CVehicleParams pTable = exhaustTable.findChild("EngineRunning"))
			{
				exhaustParams.runEffect = pTable.getAttr("effect");
				exhaustParams.boostEffect = pTable.getAttr("boostEffect");

				if (!exhaustParams.runEffect.empty())
					m_pParticleEffects.push_back(gEnv->pParticleManager->FindEffect(exhaustParams.runEffect, "CVehicle::InitParticles"));

				if (!exhaustParams.boostEffect.empty())
					m_pParticleEffects.push_back(gEnv->pParticleManager->FindEffect(exhaustParams.boostEffect, "CVehicle::InitParticles"));

				pTable.getAttr("baseSizeScale", exhaustParams.runBaseSizeScale);
				pTable.getAttr("minSpeed", exhaustParams.runMinSpeed);
				pTable.getAttr("minSpeedSizeScale", exhaustParams.runMinSpeedSizeScale);
				pTable.getAttr("minSpeedCountScale", exhaustParams.runMinSpeedCountScale);
				pTable.getAttr("minSpeedSpeedScale", exhaustParams.runMinSpeedSpeedScale);
				pTable.getAttr("maxSpeed", exhaustParams.runMaxSpeed);
				pTable.getAttr("maxSpeedSizeScale", exhaustParams.runMaxSpeedSizeScale);
				pTable.getAttr("maxSpeedCountScale", exhaustParams.runMaxSpeedCountScale);
				pTable.getAttr("maxSpeedSpeedScale", exhaustParams.runMaxSpeedSpeedScale);
				pTable.getAttr("minPower", exhaustParams.runMinPower);
				pTable.getAttr("minPowerSizeScale", exhaustParams.runMinPowerSizeScale);
				pTable.getAttr("minPowerCountScale", exhaustParams.runMinPowerCountScale);
				pTable.getAttr("minPowerSpeedScale", exhaustParams.runMinPowerSpeedScale);
				pTable.getAttr("maxPower", exhaustParams.runMaxPower);
				pTable.getAttr("maxPowerSizeScale", exhaustParams.runMaxPowerSizeScale);
				pTable.getAttr("maxPowerCountScale", exhaustParams.runMaxPowerCountScale);
				pTable.getAttr("maxPowerSpeedScale", exhaustParams.runMaxPowerSpeedScale);
				pTable.getAttr("disableWithNegativePower", exhaustParams.disableWithNegativePower);
			}

			exhaustParams.hasExhaust = true;
		}
	}

	/*
	   <DamageEffects>
	   <DamageEffect name="EngineDamaged" effect="" helper="engineSmokeOut" />
	   </DamageEffects>
	 */
	if (CVehicleParams damageEffectsTable = particleTable.findChild("DamageEffects"))
	{
		int i = 0;
		int c = damageEffectsTable.getChildCount();

		for (; i < c; i++)
		{
			if (CVehicleParams damageEffectRef = damageEffectsTable.getChild(i))
			{
				SDamageEffect damageEffect;

				if (damageEffectRef.haveAttr("name") && damageEffectRef.haveAttr("effect"))
				{
					const char* pEffectName = damageEffectRef.getAttr("name");
					damageEffect.effectName = damageEffectRef.getAttr("effect");

					if (damageEffectRef.haveAttr("helper"))
						damageEffect.pHelper = GetHelper(damageEffectRef.getAttr("helper"));

					if (damageEffectRef.getAttr("gravityDirection", damageEffect.gravityDir))
					{
						damageEffect.isGravityDirUsed = true;
					}
					else
					{
						damageEffect.isGravityDirUsed = false;
						damageEffect.gravityDir.zero();
					}

					if (!damageEffectRef.getAttr("pulsePeriod", damageEffect.pulsePeriod))
						damageEffect.pulsePeriod = 0.0f;

#if ENABLE_VEHICLE_DEBUG
					if (VehicleCVars().v_debugdraw == eVDB_Damage)
						CryLog("Adding damage effect: name %s, effect %s", damageEffectsTable.getAttr("name"), damageEffect.effectName.c_str());
#endif

					m_particleParams.damageEffects.insert(TDamageEffectMap::value_type(pEffectName, damageEffect));

					// preload
					m_pParticleEffects.push_back(gEnv->pParticleManager->FindEffect(damageEffect.effectName.c_str(), "CVehicle::InitParticles"));
				}
			}
		}
	}

	if (CVehicleParams envTable = particleTable.findChild("EnvironmentLayers"))
	{
		int cnt = envTable.getChildCount();
		envParams.layers.reserve(cnt);

		for (int i = 1; i <= cnt; ++i)
		{
			CVehicleParams layerTable = envTable.getChild(i - 1);
			if (!layerTable)
				continue;

			CEnvironmentLayer layerParams;

			layerParams.name = layerTable.getAttr("name");

			layerTable.getAttr("active", layerParams.active);
			layerTable.getAttr("active", layerParams.active);
			layerTable.getAttr("minSpeed", layerParams.minSpeed);
			layerTable.getAttr("minSpeedSizeScale", layerParams.minSpeedSizeScale);
			layerTable.getAttr("minSpeedCountScale", layerParams.minSpeedCountScale);
			layerTable.getAttr("minSpeedSpeedScale", layerParams.minSpeedSpeedScale);
			layerTable.getAttr("maxSpeed", layerParams.maxSpeed);
			layerTable.getAttr("maxSpeedSizeScale", layerParams.maxSpeedSizeScale);
			layerTable.getAttr("maxSpeedCountScale", layerParams.maxSpeedCountScale);
			layerTable.getAttr("maxSpeedSpeedScale", layerParams.maxSpeedSpeedScale);
			layerTable.getAttr("minPowerSizeScale", layerParams.minPowerSizeScale);
			layerTable.getAttr("maxPowerSizeScale", layerParams.maxPowerSizeScale);
			layerTable.getAttr("minPowerCountScale", layerParams.minPowerCountScale);
			layerTable.getAttr("maxPowerCountScale", layerParams.maxPowerCountScale);
			layerTable.getAttr("minPowerSpeedScale", layerParams.minPowerSpeedScale);
			layerTable.getAttr("maxPowerSpeedScale", layerParams.maxPowerSpeedScale);

			SmartScriptTable wheels;

			if (CVehicleParams alignmentTable = layerTable.findChild("Alignment"))
			{
				alignmentTable.getAttr("alignGroundHeight", layerParams.alignGroundHeight);
				alignmentTable.getAttr("maxHeightSizeScale", layerParams.maxHeightSizeScale);
				alignmentTable.getAttr("maxHeightCountScale", layerParams.maxHeightCountScale);
				alignmentTable.getAttr("alignToWater", layerParams.alignToWater);
			}

			if (CVehicleParams emitterTable = layerTable.findChild("Emitters"))
			{
				int n = 0;
				int cEmitters = emitterTable.getChildCount();

				for (; n < cEmitters; n++)
				{
					if (CVehicleParams emitterRef = emitterTable.getChild(n))
					{
						if (IVehicleHelper* pHelper = GetHelper(emitterRef.getAttr("value")))
							layerParams.helpers.push_back(pHelper);
					}
				}
			}

			if (CVehicleParams wheelsTable = layerTable.findChild("Wheels"))
			{
				bool all = false;

				if (wheelsTable.getAttr("all", all) && all)
				{
					bool allActive = true;
					wheelsTable.getAttr("allActive", allActive);

					for (int n = 1; n <= GetWheelCount(); ++n)
					{
						CEnvironmentLayer::SWheelGroup group;
						group.m_wheels.push_back(n);
						group.active = allActive;
						layerParams.wheelGroups.push_back(group);
					}
				}
				else
				{
					if (CVehicleParams groupsTable = wheelsTable.findChild("WheelGroups"))
					{
						int n = 0;
						int cWheelGroups = groupsTable.getChildCount();

						for (; n < cWheelGroups; n++)
						{
							if (CVehicleParams wheelGroupRef = groupsTable.getChild(n))
							{
								CEnvironmentLayer::SWheelGroup group;

								if (CVehicleParams wheelArrayRef = wheelGroupRef.findChild("Wheels"))
								{
									int k = 0;
									int cWheels = wheelArrayRef.getChildCount();

									for (; k < cWheels; k++)
									{
										if (CVehicleParams wheelRef = wheelArrayRef.getChild(k))
										{
											int wheelIndex = 0;
											wheelRef.getAttr("value", wheelIndex);
											group.m_wheels.push_back(wheelIndex);
										}
									}
								}

								groupsTable.getAttr("active", group.active);
								layerParams.wheelGroups.push_back(group);
							}
						}
					}
				}
			}

			envParams.layers.push_back(layerParams);
		}
	}

	return true;
}

//------------------------------------------------------------------------
TVehicleSeatId CVehicle::GetSeatId(const char* pSeatName)
{
	int seatId = InvalidVehicleSeatId + 1;

	for (TVehicleSeatVector::iterator ite = m_seats.begin(); ite != m_seats.end(); ++ite)
	{
		if (ite->first == pSeatName)
			return seatId;

		seatId++;
	}

	return InvalidVehicleSeatId;
}

//------------------------------------------------------------------------
TVehicleSeatId CVehicle::GetSeatId(CVehicleSeat* pSeat)
{
	int i = 1;

	for (TVehicleSeatVector::iterator ite = m_seats.begin(); ite != m_seats.end(); ++ite)
	{
		if (ite->second == pSeat)
		{
			return i;
		}

		i++;
	}

	return 0;
}

//------------------------------------------------------------------------
TVehicleSeatId CVehicle::GetDriverSeatId() const
{
	for (TVehicleSeatVector::const_iterator ite = m_seats.begin(); ite != m_seats.end(); ++ite)
	{
		IVehicleSeat* pSeat = ite->second;
		if (pSeat->IsDriver())
			return pSeat->GetSeatId();
	}

	return InvalidVehicleSeatId;
}

//------------------------------------------------------------------------
IActor* CVehicle::GetDriver() const
{
	if (CVehicleSeat* pSeat = (CVehicleSeat*)GetSeatById(GetDriverSeatId()))
		return pSeat->GetPassengerActor();

	return NULL;
}

//------------------------------------------------------------------------
IVehicleSeat* CVehicle::GetSeatForPassenger(EntityId passengerId) const
{
	if (passengerId)
	{
		for (TVehicleSeatVector::const_iterator ite = m_seats.begin(), end = m_seats.end(); ite != end; ++ite)
		{
			CVehicleSeat* pSeat = ite->second;
			if (pSeat->GetPassenger() == passengerId && pSeat->GetCurrentTransition() != CVehicleSeat::eVT_RemoteUsage)
				return pSeat;
		}
	}

	return NULL;
}

//------------------------------------------------------------------------
IVehicleSeat* CVehicle::GetSeatById(const TVehicleSeatId seatId) const
{
	if (seatId > 0 && seatId <= m_seats.size())
	{
		return m_seats[seatId - 1].second;
	}

	return NULL;
}

//------------------------------------------------------------------------
TVehicleSeatId CVehicle::GetLastSeatId()
{
	const size_t seatCount = m_seats.size();
	return (seatCount > 0) ? (TVehicleSeatId)seatCount : InvalidVehicleSeatId;
}

//------------------------------------------------------------------------
TVehicleSeatId CVehicle::GetNextSeatId(const TVehicleSeatId currentSeatId)
{
	if (currentSeatId >= 0)
	{
		TVehicleSeatId seatId = currentSeatId + 1;
		if (seatId <= GetLastSeatId())
			return seatId;
		else
			return 1;
	}

	return InvalidVehicleSeatId;
}

//------------------------------------------------------------------------
void CVehicle::ChangeSeat(EntityId actorId, int seatChoice, TVehicleSeatId newSeatId)
{
	if (IsFlipped())
		return; // cannot change seats when vehicle is turned over

	IActorSystem* pActorSystem = CCryAction::GetCryAction()->GetIActorSystem();
	CRY_ASSERT(pActorSystem);

	IActor* pActor = pActorSystem->GetActor(actorId);
	if (!pActor)
		return;

	CVehicleSeat* pCurrentSeat = (CVehicleSeat*)GetSeatForPassenger(actorId);
	if (!pCurrentSeat || pCurrentSeat->GetSeatGroupIndex() == -1)
		return;

	CVehicleSeat* pNewSeat = pCurrentSeat;
	CVehicleSeat* pSeat = pCurrentSeat;

	if (newSeatId != InvalidVehicleSeatId)
	{
		if (CVehicleSeat* pSeatById = (CVehicleSeat*)GetSeatById(newSeatId))
		{
			if (pSeatById->IsFree(pActor))
				pNewSeat = pSeatById;
		}
	}
	else
	{
		for (int i = 0; i < GetSeatCount(); i++)
		{
			TVehicleSeatId seatId = GetNextSeatId(pSeat->GetSeatId());
			pSeat = (CVehicleSeat*)GetSeatById(seatId);

			if (pSeat->IsFree(pActor)
			    && (seatChoice == 0 || pSeat->GetSeatGroupIndex() == seatChoice))
			{
				pNewSeat = pSeat;
				break;
			}
		}
	}

	if (pNewSeat && pNewSeat != pCurrentSeat)
	{
		if (pNewSeat->GetPassenger(true)) // remote or dead
			pNewSeat->Exit(false);

		EntityId passengerId = pCurrentSeat->GetPassenger();

		pActor->GetEntity()->GetCharacter(0)->GetISkeletonAnim()->StopAnimationsAllLayers();

		if (m_vehicleAnimation.IsEnabled() && pCurrentSeat && pCurrentSeat->ShouldEnslaveActorAnimations())
		{
			m_vehicleAnimation.AttachPassengerScope(pCurrentSeat, passengerId, false);
		}

		pCurrentSeat->GivesActorSeatFeatures(false);

		// allow lua side of the seat implementation to do its business
		HSCRIPTFUNCTION scriptFunction(0);
		IScriptSystem* pIScriptSystem = gEnv->pScriptSystem;
		if (IScriptTable* pScriptTable = GetEntity()->GetScriptTable())
		{
			if (pActor->IsPlayer())
			{
				if (pScriptTable->GetValue("OnActorStandUp", scriptFunction))
				{
					CRY_ASSERT(scriptFunction);
					ScriptHandle passengerHandle(pCurrentSeat->GetPassenger(true));
					Script::Call(pIScriptSystem, scriptFunction, pScriptTable, passengerHandle, false);
					pIScriptSystem->ReleaseFunc(scriptFunction);
				}
			}
			else
			{
				if (pScriptTable->GetValue("OnActorChangeSeat", scriptFunction))
				{
					CRY_ASSERT(scriptFunction);
					ScriptHandle passengerHandle(pCurrentSeat->GetPassenger(true));
					Script::Call(pIScriptSystem, scriptFunction, pScriptTable, passengerHandle, false);
					pIScriptSystem->ReleaseFunc(scriptFunction);
				}
			}
		}

		pCurrentSeat->SetPasssenger(0);

		if (m_vehicleAnimation.IsEnabled() && pNewSeat->ShouldEnslaveActorAnimations())
		{
			m_vehicleAnimation.AttachPassengerScope(pNewSeat, passengerId, true);
		}

		pNewSeat->SetPasssenger(actorId);
		pNewSeat->SitDown();

		CHANGED_NETWORK_STATE(this, ASPECT_SEAT_PASSENGER);

		SVehicleEventParams eventParams;
		eventParams.entityId = passengerId;
		eventParams.iParam = pCurrentSeat->GetSeatId();
		BroadcastVehicleEvent(eVE_SeatFreed, eventParams);

		eventParams.entityId = actorId;
		eventParams.iParam = pNewSeat->GetSeatId();
		BroadcastVehicleEvent(eVE_PassengerChangeSeat, eventParams);
	}
}

//------------------------------------------------------------------------
void CVehicle::RegisterVehicleEventListener(IVehicleEventListener* pEventListener, const char* name)
{
	m_eventListeners.Add(pEventListener);
}

//------------------------------------------------------------------------
void CVehicle::UnregisterVehicleEventListener(IVehicleEventListener* pEventListener)
{
	m_eventListeners.Remove(pEventListener);
}

//------------------------------------------------------------------------
void CVehicle::BroadcastVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params)
{
	if (event == eVE_BeingFlipped)
	{
		m_status.beingFlipped = params.bParam;
		return;
	}

	if (event == eVE_PassengerEnter)  // need to setup listener _before_ the listener event is sent (which params contain seat id).
	{
		IScriptTable* pScriptTable = GetEntity()->GetScriptTable();
		if (pScriptTable)
			Script::CallMethod(pScriptTable, "OnPassengerEnter", ScriptHandle(params.entityId));

		m_pVehicleSystem->BroadcastVehicleUsageEvent(event, params.entityId /*playerId*/, this);
	}

	for (TVehicleEventListenerSet::Notifier notifier(m_eventListeners); notifier.IsValid(); notifier.Next())
	{
		notifier->OnVehicleEvent(event, params);
	}

	for (TVehicleSeatVector::iterator seat_it = m_seats.begin(), seat_end = m_seats.end(); seat_it != seat_end; ++seat_it)
	{
		CVehicleSeat* pSeat = seat_it->second;
		pSeat->OnVehicleEvent(event, params);
	}

	switch (event)
	{
	case eVE_Collision:
		{
			break;
		}
	case eVE_PassengerExit:
		m_pVehicleSystem->BroadcastVehicleUsageEvent(event, params.entityId /*playerId*/, this);
	// fall through intentional.
	case eVE_PassengerEnter:
		{
			if (params.iParam == 1) // driver seat
			{
				IVehiclePart::SVehiclePartEvent partEvent;
				partEvent.type = (eVE_PassengerEnter == event) ? IVehiclePart::eVPE_DriverEntered : IVehiclePart::eVPE_DriverLeft;

				for (TVehiclePartVector::iterator it = m_parts.begin(), end = m_parts.end(); it != end; ++it)
					it->second->OnEvent(partEvent);
			}
			break;
		}
	case eVE_Indestructible:
		{
			m_indestructible = params.bParam;
			break;
		}
	case eVE_PreVehicleDeletion:
		{
			SVehicleEventParams seatEventParams;
			for (TVehicleSeatVector::iterator seat_it = m_seats.begin(), seat_end = m_seats.end(); seat_it != seat_end; ++seat_it)
			{
				CVehicleSeat* pSeat = seat_it->second;
				EntityId playerId = pSeat->GetPassenger();
				if (playerId)
				{
					seatEventParams.iParam = pSeat->GetSeatId();
					seatEventParams.entityId = playerId;
					BroadcastVehicleEvent(eVE_PassengerExit, seatEventParams);
				}
			}

			for (TVehiclePartVector::iterator it = m_parts.begin(), end = m_parts.end(); it != end; ++it)
				it->second->OnVehicleEvent(event, params);
			break;
		}
	default:
		break;
	}

	if (event == eVE_PassengerEnter)
	{
		GetGameObject()->EnablePostUpdates(this);
	}
	else if (event == eVE_PassengerExit)
	{
		IScriptTable* pScriptTable = GetEntity()->GetScriptTable();
		if (pScriptTable)
			Script::CallMethod(pScriptTable, "OnPassengerExit", ScriptHandle(params.entityId));
		uint32 numberOfPassengers = 0;

		for (TVehicleSeatVector::const_iterator iSeat = m_seats.begin(), iEndSeat = m_seats.end(); iSeat != iEndSeat; ++iSeat)
		{
			if (iSeat->second->GetPassenger())
			{
				++numberOfPassengers;
			}
		}

		if (numberOfPassengers <= 1)
		{
			GetGameObject()->DisablePostUpdates(this);
		}
	}
}

//------------------------------------------------------------------------
bool CVehicle::IsDestroyed() const
{
	return m_isDestroyed;
}

bool CVehicle::IsDestroyable() const
{
	return m_isDestroyable;
}

bool CVehicle::IsSubmerged()
{
	return (m_status.submergedRatio >= (GetDamageParams().submergedRatioMax - 0.001f));
}

//------------------------------------------------------------------------
const CVehicleSeatActionWeapons* CVehicle::GetCurrentSeatActionWeapons(EntityId passengerId, bool secondary) const
{
	if (const CVehicleSeat* pSeat = static_cast<const CVehicleSeat*>(GetSeatForPassenger(passengerId)))
	{
		const int transition = pSeat->GetCurrentTransition();
		if (transition == CVehicleSeat::eVT_None || transition == CVehicleSeat::eVT_RemoteUsage)
		{
			const TVehicleSeatActionVector& seatActions = pSeat->GetSeatActions();
			for (TVehicleSeatActionVector::const_iterator it = seatActions.begin(), itEnd = seatActions.end(); it != itEnd; ++it)
			{
				IVehicleSeatAction* pSeatAction = it->pSeatAction;
				assert(pSeatAction);

				if (const CVehicleSeatActionWeapons* pWeaponAction = CAST_VEHICLEOBJECT(CVehicleSeatActionWeapons, pSeatAction))
				{
					if (secondary == pWeaponAction->IsSecondary())
					{
						return pWeaponAction;
					}
				}
			}

			if (const CVehicleSeat* pRemoteSeat = pSeat->GetSeatUsedRemotely(true))
			{
				const TVehicleSeatActionVector& remoteSeatActions = pRemoteSeat->GetSeatActions();
				for (TVehicleSeatActionVector::const_iterator it = remoteSeatActions.begin(), itEnd = remoteSeatActions.end(); it != itEnd; ++it)
				{
					if (!it->isAvailableRemotely)
						continue;

					if (const CVehicleSeatActionWeapons* pWeaponAction = CAST_VEHICLEOBJECT(CVehicleSeatActionWeapons, it->pSeatAction))
					{
						if (secondary == pWeaponAction->IsSecondary())
						{
							return pWeaponAction;
						}
					}
				}
			}
		}
	}

	return NULL;
}

//------------------------------------------------------------------------
EntityId CVehicle::GetCurrentWeaponId(EntityId passengerId, bool secondary) const
{
	const CVehicleSeatActionWeapons* pWeaponAction = GetCurrentSeatActionWeapons(passengerId, secondary);

	return (pWeaponAction ? pWeaponAction->GetCurrentWeaponEntityId() : 0);
}

//------------------------------------------------------------------------
bool CVehicle::GetCurrentWeaponInfo(SVehicleWeaponInfo& outInfo, EntityId passengerId, bool secondary) const
{
	const CVehicleSeatActionWeapons* pWeaponAction = GetCurrentSeatActionWeapons(passengerId, secondary);

	if (pWeaponAction)
	{
		outInfo.entityId = pWeaponAction->GetCurrentWeaponEntityId();
		outInfo.bCanFire = pWeaponAction->CanFireWeapon();
	}

	// - if there's also a seat-action that can rotate the turret, ensure that the turret is aligned by now
	// - if it's not aligned yet, we should not be allowed to fire
	if (const CVehicleSeat* pSeat = static_cast<CVehicleSeat*>(GetSeatForPassenger(passengerId)))
	{
		const TVehicleSeatActionVector& seatActions = pSeat->GetSeatActions();
		for (TVehicleSeatActionVector::const_iterator it = seatActions.begin(); it != seatActions.end(); ++it)
		{
			IVehicleSeatAction* pSeatAction = it->pSeatAction;

			if (CVehicleSeatActionRotateTurret* pRotateTurretAction = CAST_VEHICLEOBJECT(CVehicleSeatActionRotateTurret, pSeatAction))
			{
				float pitch, yaw;

				if (pRotateTurretAction->GetRemainingAnglesToAimGoalInDegrees(pitch, yaw))
				{
					// TODO: allow these thresholds to get passed in from the outside, but that would mean changing the interface of this class
					static const float pitchThreshold = 1.0f;
					static const float yawThreshold = 1.0f;

					//  have an aim goal, but are we still not aligned to it?
					if (fabsf(pitch) > pitchThreshold || fabsf(yaw) > yawThreshold)
					{
						// still not aligned
						outInfo.bCanFire = false;
					}
				}
				else
				{
					// have no aim goal, so should not allow to fire
					outInfo.bCanFire = false;
				}

				break;
			}
		}
	}

	return (pWeaponAction != NULL);
}

//------------------------------------------------------------------------
void CVehicle::SetObjectUpdate(IVehicleObject* pObject, EVehicleObjectUpdate updatePolicy)
{
	CRY_ASSERT(pObject);

	for (TVehicleObjectUpdateInfoList::iterator ite = m_objectsToUpdate.begin(); ite != m_objectsToUpdate.end(); ++ite)
	{
		SObjectUpdateInfo& updateInfo = *ite;
		if (updateInfo.pObject == pObject)
		{
			updateInfo.updatePolicy = updatePolicy;
			return;
		}
	}

	if (updatePolicy != eVOU_NoUpdate)
	{
		// it wasn't found, it needs to be added
		SObjectUpdateInfo newUpdateInfo;
		newUpdateInfo.pObject = pObject;
		newUpdateInfo.updatePolicy = updatePolicy;
		m_objectsToUpdate.push_back(newUpdateInfo);
	}
}

//------------------------------------------------------------------------
bool CVehicle::IsPlayerDriving(bool clientOnly)
{
	CVehicleSeat* pSeat = (CVehicleSeat*)GetSeatById(GetDriverSeatId());

	if (!pSeat)
		return false;

	IActor* pActor = pSeat->GetPassengerActor();

	if (pActor)
	{
		if (clientOnly)
			return pActor->IsClient();
		else
			return pActor->IsPlayer();
	}

	return false;
}

//------------------------------------------------------------------------
bool CVehicle::HasFriendlyPassenger(IEntity* pPlayer)
{
	if (!pPlayer || (!gEnv->bMultiplayer && !pPlayer->HasAI()))
		return false;

	EntityId playerId = pPlayer->GetId();

	for (int i = FirstVehicleSeatId, nSeatCount = GetSeatCount(); i <= nSeatCount; ++i)
	{
		CVehicleSeat* pSeat = (CVehicleSeat*)GetSeatById(i);
		if (!pSeat)
			continue;

		IActor* pActor = pSeat->GetPassengerActor();

		if (pActor)
		{
			if (pActor->GetEntityId() == playerId)
				continue; // the requester itself doesn't count as a passenger

			if (pActor->IsFriendlyEntity(playerId, false))
			{
				return true;
			}
		}
	}

	return false;
}

//------------------------------------------------------------------------
bool CVehicle::IsPlayerPassenger()
{
	IActor* pActor = CCryAction::GetCryAction()->GetClientActor();
	if (pActor == NULL)
		return false;

	return (pActor->GetLinkedVehicle() == this);
}

//------------------------------------------------------------------------
IVehiclePart* CVehicle::GetPart(unsigned int index)
{
	if (index >= m_parts.size())
		return NULL;

	return m_parts[index].second;
}

//------------------------------------------------------------------------
void CVehicle::GetParts(IVehiclePart** parts, int nMax)
{
	for (int i = 0; i < m_parts.size() && i < nMax; ++i)
		parts[i] = m_parts[i].second;
}

//------------------------------------------------------------------------
IVehiclePart* CVehicle::GetWheelPart(int idx)
{
	CRY_ASSERT(idx >= 0 && idx < GetWheelCount());

	int i = -1;
	for (TVehiclePartVector::iterator it = m_parts.begin(); it != m_parts.end(); ++it)
	{
		if (CAST_VEHICLEOBJECT(CVehiclePartSubPartWheel, it->second))
			if (++i == idx)
				return it->second;
	}

	return NULL;
}

//------------------------------------------------------------------------
IVehicleAction* CVehicle::GetAction(int index)
{
	if (index < 0 || index >= m_actions.size())
		return NULL;

	SVehicleActionInfo& actionInfo = m_actions[index];
	return actionInfo.pAction;
}

//------------------------------------------------------------------------
IVehicleHelper* CVehicle::GetHelper(const char* pName)
{
	if (!pName)
		return NULL;

	TVehicleHelperMap::iterator it = m_helpers.find(CONST_TEMP_STRING(pName));
	if (it != m_helpers.end())
		return it->second;

	for (TVehiclePartVector::iterator ite = m_parts.begin(); ite != m_parts.end(); ++ite)
	{
		IVehiclePart* pPart = ite->second;
		if (CVehiclePartAnimated* pAnimatedPart = CAST_VEHICLEOBJECT(CVehiclePartAnimated, pPart))
		{
			if (ICharacterInstance* pCharInstance = pAnimatedPart->GetEntity()->GetCharacter(pAnimatedPart->GetSlot()))
			{
				IDefaultSkeleton& rIDefaultSkeleton = pCharInstance->GetIDefaultSkeleton();
				ISkeletonPose* pSkeletonPose = pCharInstance->GetISkeletonPose();
				CRY_ASSERT(pSkeletonPose);

				int16 id = rIDefaultSkeleton.GetJointIDByName(pName);
				if (id > -1)
				{
					CVehicleHelper* pHelper = new CVehicleHelper;

					pHelper->m_localTM = Matrix34(pSkeletonPose->GetRelJointByID(id));
					pHelper->m_pParentPart = pPart;

					m_helpers.insert(TVehicleHelperMap::value_type(pName, pHelper));
					return pHelper;
				}
			}
			break;
		}
	}

	TVehicleComponentVector::iterator compIte = m_components.begin();
	TVehicleComponentVector::iterator compEnd = m_components.end();

	for (; compIte != compEnd; ++compIte)
	{
		CVehicleComponent* pComponent = *compIte;

		if (pComponent->GetName() == pName)
		{
			CVehicleHelper* pHelper = new CVehicleHelper;

			if (pComponent->GetPartCount() > 0)
			{
				pHelper->m_pParentPart = pComponent->GetPart(0);
			}
			else
			{
				TVehiclePartVector::iterator partIte = m_parts.begin();
				TVehiclePartVector::iterator partEnd = m_parts.end();

				for (; partIte != partEnd; ++partIte)
				{
					IVehiclePart* pPart = partIte->second;
					if (pPart->GetParent() == NULL)
					{
						pHelper->m_pParentPart = pPart;
						break;
					}
				}
			}

			if (pHelper->m_pParentPart)
			{
				Matrix34 vehicleTM(IDENTITY);
				vehicleTM.SetTranslation(pComponent->GetBounds().GetCenter());

				pHelper->m_localTM = pHelper->m_pParentPart->GetLocalTM(false).GetInverted() * vehicleTM;

				m_helpers.insert(TVehicleHelperMap::value_type(pName, pHelper));
				return pHelper;
			}
			else
			{
				SAFE_RELEASE(pHelper);
			}

			break;
		}
	}

	return NULL;
}

//------------------------------------------------------------------------
int CVehicle::GetComponentCount() const
{
	return (int)m_components.size();
}

//------------------------------------------------------------------------
IVehicleComponent* CVehicle::GetComponent(int index)
{
	if (index >= 0 && index < m_components.size())
	{
		return m_components[index];
	}
	return NULL;
}

//------------------------------------------------------------------------
void CVehicle::Physicalize()
{
	m_physicsParams.nSlot = -1;
	m_physicsParams.type = PE_RIGID;
	m_physicsParams.mass = m_mass;
	m_physicsParams.pCar = 0;

	//if (!(m_pMovement && m_pMovement->GetPhysicalizationType() == PE_WHEELEDVEHICLE))
	if (!(m_pMovement && (m_pMovement->GetPhysicalizationType() == PE_WHEELEDVEHICLE || m_pMovement->GetPhysicalizationType() == PE_LIVING)))
		GetEntity()->Physicalize(m_physicsParams);

	if (m_pMovement)
		m_pMovement->Physicalize();

	IPhysicalEntity* pPhysics = GetEntity()->GetPhysics();
	if (!pPhysics)
		return;

	pPhysics->SetParams(&m_buoyancyParams);
	pPhysics->SetParams(&m_simParams);
	pPhysics->SetParams(&m_paramsFlags);

	for (TVehiclePartVector::iterator ite = m_parts.begin(); ite != m_parts.end(); ++ite)
	{
		IVehiclePart* pPart = ite->second;
		pPart->Physicalize();
	}

	GetGameObject()->EnablePhysicsEvent(true, eEPE_OnPostStepImmediate | eEPE_OnPostStepLogged);

	//if (pPhysics->GetParams(&paramsFlags))
	{
		//paramsFlags.flags = paramsFlags.flags | pef_monitor_poststep | pef_log_collisions | pef_fixed_damping;
		pe_params_flags flags;
		flags.flagsOR = pef_monitor_poststep | pef_log_poststep | pef_log_collisions | pef_fixed_damping;
		pPhysics->SetParams(&flags);
	}

	pe_simulation_params simParams;
	if (pPhysics->GetParams(&simParams))
		m_gravity = simParams.gravity;

	if (m_bRetainGravity)
	{
		pe_params_flags flags;
		flags.flagsOR = pef_ignore_areas;
		pPhysics->SetParams(&flags);
	}

	if (m_pMovement)
		m_pMovement->PostPhysicalize();

	if (!CCryAction::GetCryAction()->IsEditing())
	{
		pe_action_awake action_awake;

		action_awake.bAwake = false;

		pPhysics->Action(&action_awake);

		NeedsUpdate();
	}
}

//------------------------------------------------------------------------
const char* CVehicle::GetSoundName(const char* eventName, bool isEngineSound)
{
	return "";
}

//------------------------------------------------------------------------
bool CVehicle::EventIdValid(TVehicleSoundEventId eventId)
{
	return m_pIEntityAudioComponent && eventId > -1 && eventId < m_soundEvents.size();
}

//------------------------------------------------------------------------
SVehicleSoundInfo* CVehicle::GetSoundInfo(TVehicleSoundEventId eventId)
{
	if (!EventIdValid(eventId))
		return NULL;

	return &m_soundEvents[eventId];
}

//------------------------------------------------------------------------
//ISound* CVehicle::GetSound(TVehicleSoundEventId eventId, bool start/*=true*/)
//{
//	if (!EventIdValid(eventId))
//		return NULL;
//
//	SVehicleSoundInfo& info = m_soundEvents[eventId];
//	ISound* pSound = m_pIEntityAudioComponent->GetSound(info.soundId);
//
//	if (start && (!pSound || !pSound->IsPlaying()))
//	{
//		const char* name = GetSoundName(info.name.c_str(), info.isEngineSound);
//		Matrix34 tm;
//		info.pHelper->GetVehicleTM(tm);
//
//		info.soundId = m_pIEntityAudioComponent->PlaySound(name, tm.GetTranslation(), tm.GetColumn1(), FLAG_SOUND_DEFAULT_3D, 0, eSoundSemantic_Vehicle);
//	}
//
//	return m_pIEntityAudioComponent->GetSound(info.soundId);
//}

//------------------------------------------------------------------------
void CVehicle::SetSoundParam(TVehicleSoundEventId eventId, const char* param, float value, bool start /*=true*/)
{
	REINST("could probably be done at caller level");
	/*if (ISound* pSound = GetSound(eventId, start))
	   {
	   pSound->SetParam(param, value, false);
	   }*/
}

//------------------------------------------------------------------------
void CVehicle::StopSound(TVehicleSoundEventId eventId)
{
	SVehicleSoundInfo* pInfo = GetSoundInfo(eventId);
	if (!pInfo)
		return;

	REINST("still needed?");
	/*if (ISound* pSound = m_pIEntityAudioComponent->GetSound(pInfo->soundId))
	   {
	   if (pSound->IsPlaying())
	    pSound->Stop();
	   }*/

	pInfo->soundId = CryAudio::InvalidControlId;
}

//------------------------------------------------------------------------
void CVehicle::StartAbandonedTimer(bool force, float timer)
{
	if (IsDestroyed() || gEnv->IsEditor() || !gEnv->bServer)
		return;

	SmartScriptTable props, respawnprops;
	if (!GetEntity()->GetScriptTable()->GetValue("Properties", props) || !props->GetValue("Respawn", respawnprops))
		return;

	if (!force)
	{
		int abandon = 0;
		if (!respawnprops->GetValue("bAbandon", abandon) || !abandon)
		{
			m_bCanBeAbandoned = false;
			return;
		}

		float dmg = GetDamageRatio();
		if (dmg == 0.0f)
		{
			float dp = (m_initialposition - GetEntity()->GetWorldPos()).len2();
			if (dp < 3.0f * 3.0f)
				return;
		}

		if (timer < 0.0f)
			respawnprops->GetValue("nAbandonTimer", timer);
	}

	const float mintime = 5.0f;

	IEntity* pEntity = GetEntity();

	pEntity->KillTimer(eVT_Abandoned);
	pEntity->SetTimer(eVT_Abandoned, (int)(max(mintime + 0.5f, timer) * 1000.0f));

	pEntity->KillTimer(eVT_AbandonedSound);
	pEntity->SetTimer(eVT_AbandonedSound, (int)(max(0.0f, ((timer + 0.5f) - mintime)) * 1000.0f)); // warn sound
}

//------------------------------------------------------------------------
void CVehicle::KillAbandonedTimer()
{
	if (!gEnv->bServer)
		return;

	GetEntity()->KillTimer(eVT_Abandoned);
	GetEntity()->KillTimer(eVT_AbandonedSound);

	EnableAbandonedWarnSound(false);
}

//------------------------------------------------------------------------
void CVehicle::Destroy()
{
	if (!gEnv->bServer || IsDestroyed())
		return;

	// force vehicle destruction
	for (TVehicleComponentVector::iterator it = m_components.begin(); it != m_components.end(); ++it)
	{
		CVehicleComponent* pComponent = *it;
		HitInfo hitInfo;
		hitInfo.targetId = GetEntityId();
		hitInfo.damage = 50000.f;
		hitInfo.type = s_normalHitTypeId;
		pComponent->OnHit(hitInfo);
	}
}

//------------------------------------------------------------------------
void CVehicle::EnableAbandonedWarnSound(bool enable)
{
	if (gEnv->IsClient())
	{
		REINST("trigger corresponding audio control");
		// kill old
		/*if (m_abandonedSoundId != INVALID_SOUNDID)
		   {
		   if (ISound* pSound = m_pIEntityAudioComponent->GetSound(m_abandonedSoundId))
		    m_pIEntityAudioComponent->StopSound(m_abandonedSoundId);

		   m_abandonedSoundId = INVALID_SOUNDID;
		   }

		   if (enable)
		   {
		   m_abandonedSoundId = m_pIEntityAudioComponent->PlaySound("sounds/interface:multiplayer_interface:mp_vehicle_alarm", Vec3(0), FORWARD_DIRECTION, FLAG_SOUND_DEFAULT_3D, 0, eSoundSemantic_Vehicle);
		   }*/
	}

	if (gEnv->bServer)
		GetGameObject()->InvokeRMIWithDependentObject(ClAbandonWarning(), AbandonWarningParams(enable), eRMI_ToRemoteClients, GetEntityId());
}

//------------------------------------------------------------------------
bool CVehicle::IsFlipped(float maxSpeed)
{
	const SVehicleStatus& status = GetStatus();
	const Matrix34& worldTM = GetEntity()->GetWorldTM();
	const static Vec3 up(0, 0, 1);

	float flippedDotProductThreshold = IsEmpty() ? m_unmannedflippedThresholdDot : 0.1f;
	bool flipped = !IsDestroyed() && worldTM.GetColumn2().z < flippedDotProductThreshold && (status.contacts || status.submergedRatio > 0.1f);

	return flipped;
}

//------------------------------------------------------------------------
void CVehicle::CheckFlippedStatus(const float deltaTime)
{
	// check flipped over status
	const float activationTime = VehicleCVars().v_FlippedExplosionTimeToExplode;
	static const float activationTimeAI = 5.f;

	float initialTime = activationTimeAI;
	float prev = m_status.flipped;
	bool flipped = IsFlipped();

	if (flipped)
	{
		m_status.flipped = max(0.f, m_status.flipped);
		m_status.flipped += deltaTime;

		bool ai = false;
		// go through all seats, not just driver...
		for (TVehicleSeatVector::iterator it = m_seats.begin(); it != m_seats.end(); ++it)
		{
			IVehicleSeat* pSeat = it->second;
			if (pSeat && pSeat->GetPassenger())
			{
				IActor* pActor = gEnv->pGameFramework->GetIActorSystem()->GetActor(pSeat->GetPassenger());
				if (pActor && !pActor->IsPlayer())
				{
					ai = true;
					break;
				}
			}
		}

		if (!ai)
			initialTime = activationTime;

		if (m_status.flipped <= initialTime)
			NeedsUpdate();

		if (m_status.flipped > initialTime && prev <= initialTime)
			ProcessFlipped();
	}
	else
	{
		m_status.flipped = min(0.f, m_status.flipped);
		m_status.flipped -= deltaTime;

		if (prev >= initialTime)
		{
			KillTimer(eVT_Flipped);
		}
	}

	bool flipping = m_status.flipped > 0.5f && prev <= 0.5f;
	bool unflipping = m_status.flipped < -0.25f && prev >= -0.25f;

	if (flipping || unflipping)
	{
#if ENABLE_VEHICLE_DEBUG
		if (VehicleCVars().v_debugdraw)
			CryLog("%s: %s (%.2f)", GetEntity()->GetName(), flipping ? "flipping" : "unflipping", m_status.flipped);
#endif

		IVehiclePart::SVehiclePartEvent partEvent;
		partEvent.type = IVehiclePart::eVPE_FlippedOver;
		partEvent.bparam = flipping;

		for (TVehiclePartVector::iterator it = m_parts.begin(), end = m_parts.end(); it != end; ++it)
		{
			IVehiclePart* pPart = it->second;
			pPart->OnEvent(partEvent);
		}
	}

	// if flipping, kill AI in exposed seats (eg LTV gunners)
	if (gEnv->bServer && m_status.flipped > 0.1f && prev < 0.1f)
	{
		KillPassengersInExposedSeats(true);
	}
}

//------------------------------------------------------------------------
void CVehicle::ProcessFlipped()
{
	if (!gEnv->bServer)
		return;

	if (!IsFlipped())
		return;

	if (m_indestructible)
		return;

	const Matrix34& worldTM = GetEntity()->GetWorldTM();
	bool playerClose = false;

	bool ai = false;
	// go through all seats, not just driver...
	for (TVehicleSeatVector::iterator it = m_seats.begin(); it != m_seats.end(); ++it)
	{
		IVehicleSeat* pSeat = it->second;
		if (pSeat && pSeat->GetPassenger())
		{
			IActor* pActor = gEnv->pGameFramework->GetIActorSystem()->GetActor(pSeat->GetPassenger());
			if (pActor && !pActor->IsPlayer())
			{
				ai = true;
				break;
			}
		}
	}

	if (!ai)
	{
		// if AI guys inside, we blow up in any case
		// if not, we only blow up if no players are around
		const float r = VehicleCVars().v_FlippedExplosionPlayerMinDistance;
		IActorSystem* pActorSystem = gEnv->pGameFramework->GetIActorSystem();

		if (!gEnv->bMultiplayer)
		{
			IActor* pActor = gEnv->pGameFramework->GetClientActor();
			if (pActor)
			{
				float distSq = pActor->GetEntity()->GetWorldPos().GetSquaredDistance(worldTM.GetTranslation());
				if (distSq < r * r)
					playerClose = true;
			}
		}
		else
		{
			SEntityProximityQuery query;
			query.box = AABB(worldTM.GetTranslation() - Vec3(r, r, r), worldTM.GetTranslation() + Vec3(r, r, r));

			int count = gEnv->pEntitySystem->QueryProximity(query);
			for (int i = 0; i < query.nCount; ++i)
			{
				IEntity* pEntity = query.pEntities[i];
				if (IActor* pActor = pActorSystem->GetActor(pEntity->GetId()))
				{
					if (pActor->IsPlayer())
					{
						playerClose = true;
						break;
					}
				}
			}
		}
	}

	if (playerClose)
	{
		// check again later
		SetTimer(eVT_Flipped, VehicleCVars().v_FlippedExplosionRetryTimeMS, 0);
	}
	else
	{
		if (CVehicleComponent* pComponent = (CVehicleComponent*)GetComponent("Engine"))
		{
			HitInfo hitInfo;
			hitInfo.targetId = GetEntityId();
			hitInfo.shooterId = hitInfo.targetId;
			hitInfo.damage = 10000.f;
			hitInfo.type = s_normalHitTypeId;
			pComponent->OnHit(hitInfo);
		}

		if (CVehicleComponent* pComponent = (CVehicleComponent*)GetComponent("FlippedOver"))
		{
			HitInfo hitInfo;
			hitInfo.targetId = GetEntityId();
			hitInfo.shooterId = hitInfo.targetId;
			hitInfo.damage = 10000.f;
			hitInfo.type = s_normalHitTypeId;
			pComponent->OnHit(hitInfo);
		}

		KillTimer(eVT_Flipped);
	}
}

//------------------------------------------------------------------------
bool CVehicle::IsEmpty()
{
	return (m_status.passengerCount == 0);
}

//------------------------------------------------------------------------
bool CVehicle::InitRespawn()
{
	if (gEnv->bServer)
	{
		IScriptTable* pScriptTable = GetEntity()->GetScriptTable();
		if (pScriptTable)
		{
			SmartScriptTable props, respawnprops;
			if (pScriptTable->GetValue("Properties", props) && props->GetValue("Respawn", respawnprops))
			{
				int respawn = 0;
				if (respawnprops->GetValue("bRespawn", respawn) && respawn)
				{
					if (IGameRules* pGameRules = CCryAction::GetCryAction()->GetIGameRulesSystem()->GetCurrentGameRules())
						pGameRules->CreateEntityRespawnData(GetEntityId());

					return true;
				}
			}
		}
	}

	return false;
}

//------------------------------------------------------------------------
void CVehicle::OnDestroyed()
{
	EnableAbandonedWarnSound(false);

#if ENABLE_VEHICLE_DEBUG
	DumpParts();
#endif

	IGameRules* pGameRules = CCryAction::GetCryAction()->GetIGameRulesSystem()->GetCurrentGameRules();
	if (pGameRules)
		pGameRules->OnVehicleDestroyed(GetEntityId());

	if (gEnv->bServer && !gEnv->IsEditor())
	{
		bool mp = gEnv->bMultiplayer;

		SmartScriptTable props, respawnprops;
		if (GetEntity()->GetScriptTable()->GetValue("Properties", props) && props->GetValue("Respawn", respawnprops))
		{
			int respawn = 0;
			if (respawnprops->GetValue("bRespawn", respawn) && respawn)
			{
				int timer = 0;
				int unique = 0;

				respawnprops->GetValue("nTimer", timer);
				respawnprops->GetValue("bUnique", unique);

				float rtimer = max(0.0f, timer - 5.0f);
				if (!mp)
					rtimer = timer + 20.0f;

				if (!mp)
					unique = 0; // forced to false in singleplayer to avoid unintuitive behaviour

				if (pGameRules)
				{
					pGameRules->ScheduleEntityRemoval(GetEntityId(), rtimer, !mp);
					pGameRules->ScheduleEntityRespawn(GetEntityId(), unique != 0, (float)timer);
				}

				return;
			}
		}

		// if it's not set to respawn, remove it after the set time
		if (mp && pGameRules)
			pGameRules->ScheduleEntityRemoval(GetEntityId(), 15.0f, false);
	}

	TVehicleAnimationsVector::iterator animIte = m_animations.begin();
	TVehicleAnimationsVector::iterator animIteEnd = m_animations.end();

	for (; animIte != animIteEnd; ++animIte)
	{
		IVehicleAnimation* pAnim = animIte->second;
		pAnim->StopAnimation();
	}

	m_status.health = 0.0f;
}

//------------------------------------------------------------------------
void CVehicle::OnSubmerged(float ratio)
{
	if (gEnv->bMultiplayer && gEnv->bServer && IsEmpty())
		StartAbandonedTimer();

	if (IGameRules* pGameRules = CCryAction::GetCryAction()->GetIGameRulesSystem()->GetCurrentGameRules())
		pGameRules->OnVehicleSubmerged(GetEntityId(), ratio);
}

//------------------------------------------------------------------------
int CVehicle::GetNextPhysicsSlot(bool high) const
{
	// use the last 9 partids of the current id range for parts without slot geometry
	// their partid must not mix up with other entity slots
	int idMax = (high) ? EntityPhysicsUtils::PARTID_MAX_SLOTS - 10 : -1;

	// get next physid not belonging to CGA range
	for (TVehiclePartVector::const_iterator ite = m_parts.begin(); ite != m_parts.end(); ++ite)
	{
		int physId = ite->second->GetPhysId();

		if (physId > idMax && physId < EntityPhysicsUtils::PARTID_MAX_SLOTS && physId > EntityPhysicsUtils::PARTID_MAX_SLOTS - 10)
			idMax = physId;
	}

	return idMax + 1;
}

//------------------------------------------------------------------------
CVehiclePartAnimated* CVehicle::GetVehiclePartAnimated()
{
	TVehiclePartVector::const_iterator end = m_parts.end();
	for (TVehiclePartVector::const_iterator it = m_parts.begin(); it != end; ++it)
	{
		if (CVehiclePartAnimated* pPartAnimated = CAST_VEHICLEOBJECT(CVehiclePartAnimated, it->second))
		{
			// since anyway there shouldn't be more than one of such class
			return pPartAnimated;
		}
	}

	return NULL;
}

//------------------------------------------------------------------------
bool CVehicle::CanEnter(EntityId userId)
{
	if (!gEnv->bMultiplayer)
		return true;

	bool result = true;
	if (IScriptTable* pScriptTable = GetEntity()->GetScriptTable())
	{
		HSCRIPTFUNCTION pfnCanEnter = 0;
		if (pScriptTable->GetValue("CanEnter", pfnCanEnter) && pfnCanEnter)
		{
			IScriptSystem* pIScriptSystem = gEnv->pScriptSystem;
			Script::CallReturn(pIScriptSystem, pfnCanEnter, pScriptTable, ScriptHandle(userId), result);
			pIScriptSystem->ReleaseFunc(pfnCanEnter);
		}
	}

	if (result)
	{
		IGameRules* pGameRules = CCryAction::GetCryAction()->GetIGameRulesSystem()->GetCurrentGameRules();
		if (pGameRules)
		{
			result = pGameRules->CanEnterVehicle(userId);
		}
	}

	return result;
}

//------------------------------------------------------------------------
float CVehicle::GetAltitude()
{
	if (iszero(m_status.altitude))
	{
		Vec3 worldPos = GetEntity()->GetWorldPos();
		ray_hit rayHits[10];

		m_status.altitude = 0.0f;

		float terrainZ = gEnv->p3DEngine->GetTerrainElevation(worldPos.x, worldPos.y);
		m_status.altitude = worldPos.z - terrainZ;

		if (int hitCount = gEnv->pPhysicalWorld->RayWorldIntersection(worldPos, Vec3(0.0f, 0.0f, -m_status.altitude),
		                                                              ent_static | ent_sleeping_rigid | ent_rigid | ent_independent | ent_terrain, rwi_colltype_any | rwi_ignore_noncolliding,
		                                                              &rayHits[0], 10, GetEntity()->GetPhysics()))
		{
			for (int i = 0; i < hitCount; i++)
				if (rayHits[i].dist >= 0.0f)
					m_status.altitude = min(rayHits[i].dist, m_status.altitude);
		}
	}

	return m_status.altitude;
}

//------------------------------------------------------------------------
int CVehicle::GetWeaponCount() const
{
	int count = 0;

	TVehicleSeatVector::const_iterator itSeatsEnd = m_seats.end();
	for (TVehicleSeatVector::const_iterator it = m_seats.begin(); it != itSeatsEnd; ++it)
	{
		const CVehicleSeat* pSeat = it->second;
		assert(pSeat);

		const TVehicleSeatActionVector& seatActions = pSeat->GetSeatActions();
		for (TVehicleSeatActionVector::const_iterator itActions = seatActions.begin(), itActionsEnd = seatActions.end(); itActions != itActionsEnd; ++itActions)
		{
			if (CVehicleSeatActionWeapons* pAction = CAST_VEHICLEOBJECT(CVehicleSeatActionWeapons, itActions->pSeatAction))
			{
				count += pAction->GetWeaponCount();
			}
		}
	}

	return count;
}

//------------------------------------------------------------------------
EntityId CVehicle::GetWeaponId(int index) const
{
	int count = 0;

	TVehicleSeatVector::const_iterator itSeatsEnd = m_seats.end();
	for (TVehicleSeatVector::const_iterator it = m_seats.begin(); it != itSeatsEnd; ++it)
	{
		const CVehicleSeat* pSeat = it->second;
		assert(pSeat);

		const TVehicleSeatActionVector& seatActions = pSeat->GetSeatActions();
		for (TVehicleSeatActionVector::const_iterator itActions = seatActions.begin(), itActionsEnd = seatActions.end(); itActions != itActionsEnd; ++itActions)
		{
			if (CVehicleSeatActionWeapons* pAction = CAST_VEHICLEOBJECT(CVehicleSeatActionWeapons, itActions->pSeatAction))
			{
				for (int i = 0; i < pAction->GetWeaponCount(); ++i)
				{
					if (count++ == index)
						return pAction->GetWeaponEntityId(i);
				}
			}
		}
	}
	return 0;
}

//------------------------------------------------------------------------
void CVehicle::GetMemoryUsage(ICrySizer* s) const
{
	s->AddObject(this, sizeof(*this));
	s->AddObject(m_pInventory);
	s->AddObject(m_transitionInfo);

	GetDamagesMemoryStatistics(s);

	if (m_pMovement)
	{
		SIZER_COMPONENT_NAME(s, "Movement");
		s->AddObject(m_pMovement);
	}

	{
		SIZER_COMPONENT_NAME(s, "Helpers");
		s->AddObject(m_helpers);
	}

	{
		SIZER_COMPONENT_NAME(s, "Seats");
		s->AddObject(m_seats);
	}

	{
		SIZER_COMPONENT_NAME(s, "SeatGroups");
		s->AddObject(m_seatGroups);
	}

	{
		SIZER_COMPONENT_NAME(s, "Components");
		s->AddObject(m_components);
	}

	{
		SIZER_COMPONENT_NAME(s, "Parts");
		s->AddObject(m_parts);
	}
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CVehicle, SvRequestUse)
{
	OnUsed(params.actorId, params.index);

	// Tell the client their use request has now been processed
	if (IActor* pUser = CCryAction::GetCryAction()->GetIActorSystem()->GetActor(params.actorId))
	{
		if (!pUser->IsClient())
		{
			GetGameObject()->InvokeRMI(CVehicle::ClRequestComplete(), CVehicle::RequestCompleteParams(params.actorId), eRMI_ToClientChannel, pUser->GetChannelId());
		}
	}
	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CVehicle, ClRequestComplete)
{
	if (IActor* pUser = CCryAction::GetCryAction()->GetIActorSystem()->GetActor(params.actorId))
	{
		CRY_ASSERT(pUser->IsStillWaitingOnServerUseResponse());
		pUser->SetStillWaitingOnServerUseResponse(false);
	}
	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CVehicle, SvRequestChangeSeat)
{
	ChangeSeat(params.actorId, params.seatChoice);

	// Tell the client their request has now been processed
	if (IActor* pUser = CCryAction::GetCryAction()->GetIActorSystem()->GetActor(params.actorId))
	{
		if (!pUser->IsClient())
		{
			GetGameObject()->InvokeRMI(CVehicle::ClRequestComplete(), CVehicle::RequestCompleteParams(params.actorId), eRMI_ToClientChannel, pUser->GetChannelId());
		}
	}
	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CVehicle, SvRequestLeave)
{
	ExitVehicleAtPosition(params.actorId, params.exitPos);
	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CVehicle, ClProcessLeave)
{
	IVehicleSeat* pCurrentSeat = GetSeatForPassenger(params.actorId);
	if (pCurrentSeat)
		pCurrentSeat->Exit(true, false, params.exitPos);

	// Set as Request Complete.
	if (IActor* pUser = CCryAction::GetCryAction()->GetIActorSystem()->GetActor(params.actorId))
	{
		if (pUser->IsStillWaitingOnServerUseResponse())
			pUser->SetStillWaitingOnServerUseResponse(false);
	}
	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CVehicle, ClSetupWeapons)
{
	int nseats = (int)params.seats.size();
	for (int s = 0; s < nseats; s++)
	{
		const CVehicleSeat* pSeat = m_seats[s].second;
		const TVehicleSeatActionVector& seatActions = pSeat->GetSeatActions();

		int iaction = 0;
		for (TVehicleSeatActionVector::const_iterator ait = seatActions.begin(), aitEnd = seatActions.end(); ait != aitEnd; ++ait)
		{
			IVehicleSeatAction* pSeatAction = ait->pSeatAction;
			if (CVehicleSeatActionWeapons* weapAction = CAST_VEHICLEOBJECT(CVehicleSeatActionWeapons, pSeatAction))
			{
				CRY_ASSERT(iaction < params.seats[s].seatactions.size());
				int nweapons = (int)params.seats[s].seatactions[iaction].weapons.size();

				for (int w = 0; w < nweapons; w++)
				{
					weapAction->ClSetupWeapon(w, params.seats[s].seatactions[iaction].weapons[w]);
				}

				++iaction;
			}
		}
	}

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CVehicle, ClRespawnWeapon)
{
	TVehicleSeatVector::iterator seatIter = m_seats.begin();
	TVehicleSeatVector::iterator seatEnd = m_seats.end();
	while (seatIter != seatEnd)
	{
		if (seatIter->second->GetSeatId() == params.seatId) //Seat actions across seats can have the same Ids so need to check for correct seat first
		{
			TVehicleSeatActionVector& seatActions = seatIter->second->GetSeatActions();

			TVehicleSeatActionVector::iterator iter = seatActions.begin();
			TVehicleSeatActionVector::iterator end = seatActions.end();

			while (iter != end)
			{
				if (iter->pSeatAction->GetId() == params.seatActionId)
				{
					if (CVehicleSeatActionWeapons* pSeatActionWeapons = CAST_VEHICLEOBJECT(CVehicleSeatActionWeapons, iter->pSeatAction))
					{
						pSeatActionWeapons->OnWeaponRespawned(params.weaponIndex, params.weaponId);
						return true;
					}
				}
				++iter;
			}
		}
		++seatIter;
	}

	CRY_ASSERT_MESSAGE(false, "CVehicle::ClRespawnWeapon - Unable to find the correct seat weapon action to place the newly spawned weapon");
	CryLog("CVehicle::ClRespawnWeapon - Unable to find the correct seat weapon action to place the newly spawned weapon");
	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CVehicle, ClSetAmmo)
{
	IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(params.ammo.c_str());
	CRY_ASSERT(pClass);
	SetAmmoCount(pClass, params.count);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CVehicle, ClAbandonWarning)
{
	EnableAbandonedWarnSound(params.enable);

	return true;
}

//------------------------------------------------------------------------
bool CVehicle::GetExitPositionForActor(IActor* pActor, Vec3& pos, bool extended /*=false*/)
{
	Vec3 outPos = ZERO;
	Matrix34 worldTM;

	if (!pActor)
		return false;

	// loop through seats in order, check each one's exit position
	for (int i = 0; i < m_pVehicle->GetSeatCount(); ++i)
	{
		CVehicleSeat* pSeat = (CVehicleSeat*)GetSeatById(i + 1);
		// skip testing the actor's current seat (this will have been tried already)
		if (pSeat && pSeat->GetPassenger() != pActor->GetEntityId())
		{
			worldTM = pSeat->GetExitTM(pActor, false);
			outPos = worldTM.GetTranslation();
			EntityId blockerId = 0;
			if (pSeat->TestExitPosition(pActor, outPos, &blockerId))
			{
				pos = outPos;
				return true;
			}

			if (extended)
			{
				Vec3 startPos = pSeat->GetSitHelper()->GetWorldSpaceTranslation();
				Vec3 endPos = outPos;
				if (DoExtendedExitTest(startPos, endPos, blockerId, outPos))
				{
					pos = outPos;
					return true;
				}
			}
		}
	}

	// if we still can't exit, try the seat helper positions reflected to the opposite side of the vehicle
	// (helps for tanks, which have two exits on the same side but not one on the other)
	for (int i = 0; i < m_pVehicle->GetSeatCount(); ++i)
	{
		CVehicleSeat* pSeat = (CVehicleSeat*)GetSeatById(i + 1);
		if (pSeat)
		{
			worldTM = pSeat->GetExitTM(pActor, true);
			outPos = worldTM.GetTranslation();
			EntityId blockerId = 0;
			if (pSeat->TestExitPosition(pActor, outPos, &blockerId))
			{
				pos = outPos;
				return true;
			}

			if (extended)
			{
				Vec3 startPos = pSeat->GetSitHelper()->GetWorldSpaceTranslation();
				Vec3 endPos = outPos;
				if (DoExtendedExitTest(startPos, endPos, blockerId, outPos))
				{
					pos = outPos;
					return true;
				}
			}
		}
	}

	// and if we *still* can't find a valid place to get out, try in front and behind the vehicle
	//	(depending on speed ;)
	Vec3 startPos = GetEntity()->GetWorldPos();

	CVehicleSeat* pSeat = (CVehicleSeat*)GetSeatById(1);

	if (pSeat)
	{
		AABB bbox;
		GetEntity()->GetLocalBounds(bbox);
		float length = bbox.GetRadius();
		Quat rot = GetEntity()->GetWorldRotation();

		Vec3 vel = GetStatus().vel;

		Vec3 fw = rot * Vec3(0, 1, 0);
		if (vel.Dot(fw) < 0.1f)
		{
			// get vehicle bounds and put the player about half a metre in front of the centre.
			outPos = startPos + (length * fw);
			EntityId blockerId = 0;
			if (pSeat->TestExitPosition(pActor, outPos, &blockerId))
			{
				pos = outPos;
				return true;
			}

			if (extended)
			{
				Vec3 startPosEx = pSeat->GetSitHelper()->GetWorldSpaceTranslation();
				Vec3 endPos = outPos;
				if (DoExtendedExitTest(startPosEx, endPos, blockerId, outPos))
				{
					pos = outPos;
					return true;
				}
			}
		}

		if (vel.Dot(fw) > -0.1f)
		{
			// try behind the vehicle.
			outPos = startPos + (-length * fw);
			EntityId blockerId = 0;
			if (pSeat->TestExitPosition(pActor, outPos, &blockerId))
			{
				pos = outPos;
				return true;
			}

			if (extended)
			{
				Vec3 startPosEx = pSeat->GetSitHelper()->GetWorldSpaceTranslation();
				Vec3 endPos = outPos;
				if (DoExtendedExitTest(startPosEx, endPos, blockerId, outPos))
				{
					pos = outPos;
					return true;
				}
			}
		}
	}

	// last chance - put the player directly on top?

	return false;
}

void CVehicle::ExitVehicleAtPosition(EntityId passengerId, const Vec3& pos)
{
	if (gEnv->bServer)
	{
		RequestLeaveParams params(passengerId, pos);
		IVehicleSeat* pCurrentSeat = GetSeatForPassenger(params.actorId);
		if (pCurrentSeat)
		{
			pCurrentSeat->Exit(true, false, params.exitPos);
			GetGameObject()->InvokeRMI(ClProcessLeave(), params, eRMI_ToRemoteClients);
		}
	}
	else
	{
		if (IActor* pUser = CCryAction::GetCryAction()->GetIActorSystem()->GetActor(passengerId))
		{
			if (!pUser->IsStillWaitingOnServerUseResponse())
			{
				pUser->SetStillWaitingOnServerUseResponse(true);
				GetGameObject()->InvokeRMI(SvRequestLeave(), RequestLeaveParams(passengerId, pos), eRMI_ToServer);
			}
		}
	}
}

void CVehicle::EvictAllPassengers()
{
	// Does this need an RMI?
	for (TVehicleSeatVector::iterator it = m_seats.begin(), end = m_seats.end(); it != end; ++it)
	{
		if (it->second->GetPassenger())
		{
			OnAction(eVAI_Exit, eAAM_OnPress, 0.f, it->second->GetPassenger());
			//it->second->Exit(false, true);
		}
	}
}

void CVehicle::ClientEvictAllPassengers()
{
	IActorSystem* pActorSystem = CCryAction::GetCryAction()->GetIActorSystem();

	EntityId passengerId;
	IActor* pActor;
	Matrix34 worldTM;
	CVehicleSeat* pSeat;
	for (TVehicleSeatVector::iterator it = m_seats.begin(), end = m_seats.end(); it != end; ++it)
	{
		pSeat = it->second;
		if (passengerId = pSeat->GetPassenger())
		{
			if (pActor = pActorSystem->GetActor(passengerId))
			{
				worldTM = pSeat->GetExitTM(pActor, false);
				pSeat->Exit(true, true, worldTM.GetTranslation());
			}
		}
	}
}

void CVehicle::ClientEvictPassenger(IActor* pActor)
{
	CVehicleSeat* pSeat;
	for (TVehicleSeatVector::iterator it = m_seats.begin(), end = m_seats.end(); it != end; ++it)
	{
		pSeat = it->second;
		if (pSeat->GetPassenger() == pActor->GetEntityId())
		{
			Matrix34 worldTM = pSeat->GetExitTM(pActor, false);
			pSeat->Exit(true, true, worldTM.GetTranslation());
			break;
		}
	}
}

void CVehicle::KillPassengersInExposedSeats(bool includePlayers)
{
	for (TVehicleSeatVector::iterator seatIter = m_seats.begin(), end = m_seats.end(); seatIter != end; ++seatIter)
	{
		IVehicleSeat* pSeat = seatIter->second;
		if (pSeat && pSeat->IsPassengerExposed() && pSeat->GetPassenger())
		{
			IActor* pActor = gEnv->pGameFramework->GetIActorSystem()->GetActor(pSeat->GetPassenger());
			if (pActor && (!pActor->IsPlayer() || includePlayers))
			{
				if (!pActor->IsDead())
				{
					IGameRules* pGameRules = CCryAction::GetCryAction()->GetIGameRulesSystem()->GetCurrentGameRules();
					if (pGameRules)
					{
						HitInfo hit;

						hit.targetId = pActor->GetEntityId();
						hit.shooterId = GetEntityId();
						hit.weaponId = GetEntityId();
						hit.damage = 1000.f;
						hit.type = pGameRules->GetHitTypeId("collision");
						hit.pos = pActor->GetEntity()->GetWorldPos();

						pGameRules->ServerHit(hit);
					}
				}
			}
		}
	}
}

int CVehicle::GetSkipEntities(IPhysicalEntity** pSkipEnts, int nMaxSkip)
{
	int nskip = 0;
	IEntity* pVehicleEntity = m_pVehicle->GetEntity();
	if (IPhysicalEntity* pVehiclePhysics = pVehicleEntity->GetPhysics())
	{
		pSkipEnts[nskip++] = pVehiclePhysics;

		if (pVehiclePhysics->GetType() == PE_LIVING && nskip < nMaxSkip)
		{
			if (ICharacterInstance* pCharacter = pVehicleEntity->GetCharacter(0))
			{
				if (IPhysicalEntity* pCharPhys = pCharacter->GetISkeletonPose()->GetCharacterPhysics())
				{
					pSkipEnts[nskip++] = pCharPhys;
				}
			}
		}
	}

	for (int i = 0, count = m_pVehicle->GetEntity()->GetChildCount(); i < count && nskip < nMaxSkip; ++i)
	{
		IEntity* pChild = m_pVehicle->GetEntity()->GetChild(i);
		if (IPhysicalEntity* pPhysics = pChild->GetPhysics())
		{
			// todo: shorter way to handle this?
			if (pPhysics->GetType() == PE_LIVING)
			{
				if (ICharacterInstance* pCharacter = pChild->GetCharacter(0))
				{
					if (IPhysicalEntity* pCharPhys = pCharacter->GetISkeletonPose()->GetCharacterPhysics())
					{
						pPhysics = pCharPhys;
					}
				}
			}

			pSkipEnts[nskip++] = pPhysics;
		}
	}

	return nskip;
}

bool CVehicle::ExitSphereTest(IPhysicalEntity** pSkipEnts, int nSkip, Vec3 startPos, Vec3 testPos, EntityId* pBlockingEntity)
{
	const float radius = 0.7f, endOffset = 0.4f, minHitDist = 0.001f;

	primitives::sphere sphere;

	sphere.center = startPos;
	sphere.r = radius;

	Vec3 endPos(testPos.x, testPos.y, testPos.z + radius + endOffset);

	geom_contact* pContact = NULL;

	float hitDist = gEnv->pPhysicalWorld->PrimitiveWorldIntersection(sphere.type, &sphere, endPos - sphere.center, ent_static | ent_terrain | ent_rigid | ent_sleeping_rigid | ent_living, &pContact, 0, (geom_colltype_player << rwi_colltype_bit) | rwi_stop_at_pierceable, 0, 0, 0, pSkipEnts, nSkip);

#if ENABLE_VEHICLE_DEBUG
	if (VehicleCVars().v_debugdraw > 0)
	{
		CPersistantDebug* pPersistantDebug = CCryAction::GetCryAction()->GetPersistantDebug();

		pPersistantDebug->Begin("VehicleSeat", false);

		ColorF color(1.0f, 1.0f, 1.0f, 1.0f);

		if (pContact && (hitDist > minHitDist))
		{
			color = ColorF(1.0f, 0.0f, 0.0f, 1.0f);
		}

		pPersistantDebug->AddLine(startPos, testPos, color, 30.0f);

		if (pContact && (hitDist > 0.0f))
		{
			pPersistantDebug->AddSphere(pContact->pt, 0.1f, color, 30.0f);

			endPos -= sphere.center;

			endPos.Normalize();

			endPos = sphere.center + (endPos * hitDist);

			color.a = 0.25f;

			pPersistantDebug->AddSphere(endPos, radius, color, 30.0f);
		}
	}
#endif //ENABLE_VEHICLE_DEBUG

	if (hitDist > minHitDist)
	{
		CRY_ASSERT(pContact);
		PREFAST_ASSUME(pContact);

		if (pBlockingEntity)
		{
			*pBlockingEntity = pContact->iPrim[0];
		}

		return false;
	}
	else if (pContact)
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool CVehicle::DoExtendedExitTest(Vec3 seatPos, Vec3 firstTestPos, EntityId blockerId, Vec3& outPos)
{
	//CryLog("Starting extended vehicle exit check");
	static int numChecks = 4;

	Vec3 startPos = seatPos;
	Vec3 endPos = firstTestPos;

	// if the test hit an actor...
	// try a further check 1.5m further in the same direction.
	static const int maxSkip = 15;
	IPhysicalEntity* pSkipEnts[maxSkip];
	int numSkip = GetSkipEntities(pSkipEnts, maxSkip - numChecks);

	// each check will proceed further from the vehicle, adding the previous contact to the skip entities.
	//	Hitting another actor will update blockerId for the next check.
	for (int check = 0; check < numChecks; ++check)
	{
		IActor* pActor = CCryAction::GetCryAction()->GetIActorSystem()->GetActor(blockerId);
		if (!pActor)
			break;

		// skip previous contact
		pSkipEnts[numSkip++] = pActor->GetEntity()->GetPhysics();

		// additional swept sphere test from the previous position to the new one...
		Vec3 prevDir = endPos - startPos;
		prevDir.z = 0.0f;
		startPos.x = endPos.x;
		startPos.y = endPos.y;
		endPos = startPos + 1.5f * prevDir.GetNormalizedSafe();
		if (ExitSphereTest(pSkipEnts, numSkip, startPos, endPos, &blockerId))
		{
			outPos = endPos;
			return true;
		}
	}

	return false;
}

const char* CVehicle::GetModification() const
{
	return m_modifications.c_str();
}

IEntityComponent::ComponentEventPriority CVehicle::GetEventPriority() const
{
	return ENTITY_PROXY_USER + EEntityEventPriority_Vehicle;
}

#if ENABLE_VEHICLE_DEBUG
void CVehicle::DebugFlipOver()
{
	IEntity* pEntity = GetEntity();
	Matrix34 tm = GetEntity()->GetWorldTM();
	const Vec3 zAxis = tm.GetColumn2();
	if (zAxis.z > 0.f)
	{
		AABB bbox;
		pEntity->GetWorldBounds(bbox);
		const Vec3 pos = tm.GetColumn3();
		const Vec3 centre = (bbox.min + bbox.max) * 0.5f;
		const Vec3 offset = centre - pos;
		const Vec3 xAxis = tm.GetColumn0();
		tm.SetColumn(0, -xAxis);
		tm.SetColumn(2, -zAxis);
		tm.SetColumn(3, pos + 2.f * offset);
		pEntity->SetWorldTM(tm);
		NeedsUpdate(eVUF_AwakePhysics);
	}
}

void CVehicle::DebugReorient()
{
	IEntity* pEntity = GetEntity();
	Matrix34 tm = GetEntity()->GetWorldTM();

	AABB bbox;
	pEntity->GetWorldBounds(bbox);
	const Vec3 pos = tm.GetColumn3();
	const Vec3 centre = (bbox.min + bbox.max) * 0.5f;
	const Vec3 offset = centre - pos;

	Matrix33 uprightRotation = Matrix33::CreateOrientation(tm.GetColumn1(), -Vec3Constants<float>::fVec3_OneZ, 0.0f);
	tm.SetRotation33(uprightRotation);
	tm.SetColumn(3, pos + 2.f * offset);

	pEntity->SetWorldTM(tm);
	NeedsUpdate(eVUF_AwakePhysics);

}

#endif

void CVehicle::OffsetPosition(const Vec3& delta)
{
	// go through all seats, not just driver...
	for (TVehicleSeatVector::iterator it = m_seats.begin(); it != m_seats.end(); ++it)
	{
		IVehicleSeat* pSeat = it->second;
		if (pSeat)
		{
			pSeat->OffsetPosition(delta);
		}
	}
}
