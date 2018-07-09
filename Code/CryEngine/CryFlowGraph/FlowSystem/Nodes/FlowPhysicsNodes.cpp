// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <CryEntitySystem/IEntitySystem.h>
#include <CryAISystem/IAISystem.h>
#include <CryAISystem/IAgent.h>
#include <CryAISystem/IAIObject.h>
#include <CryFlowGraph/IFlowBaseNode.h>

class CFlowNode_Dynamics : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_Dynamics(SActivationInfo* pActInfo)
	{
	}

	enum EInPorts
	{
		EIN_Enabled = 0,
		EIN_Get,
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<bool>("active", true, _HELP("Enable / disable continuous output"), _HELP("Enabled")),
			InputPortConfig_Void("Get", _HELP("Trigger to get the values when continuous output is not enabled")),
			{ 0 }
		};

		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<Vec3>("v",  _HELP("Velocity of entity")),
			OutputPortConfig<Vec3>("a",  _HELP("Acceleration of entity")),
			OutputPortConfig<Vec3>("w",  _HELP("Angular velocity of entity")),
			OutputPortConfig<Vec3>("wa", _HELP("Angular acceleration of entity")),
			OutputPortConfig<float>("m", _HELP("Mass of entity")),
			{ 0 }
		};
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.sDescription = _HELP("Outputs the dynamic physical state of an entity");
		config.SetCategory(EFLN_APPROVED); // POLICY CHANGE: Maybe an Enable/Disable Port
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			if (pActInfo->pGraph)
			{
				pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, GetPortBool(pActInfo, EIN_Enabled));
			}
			break;
		case eFE_Activate:
			{
				if (IsPortActive(pActInfo, EIN_Get))
				{
					OutputDynamicsComponents(pActInfo);
				}
				if (IsPortActive(pActInfo, EIN_Enabled))
				{
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, GetPortBool(pActInfo, EIN_Enabled));
				}
			}
			break;
		case eFE_Update:
			{
				OutputDynamicsComponents(pActInfo);
			}
			break;
		}
	}

	void OutputDynamicsComponents(SActivationInfo* pActInfo)
	{
		IEntity * pEntity = pActInfo->pEntity;
		if (pEntity)
		{
			IPhysicalEntity * pPhysEntity = pEntity->GetPhysics();
			if (pPhysEntity)
			{
				pe_status_dynamics dyn;
				pPhysEntity->GetStatus(&dyn);
				ActivateOutput(pActInfo, 0, dyn.v);
				ActivateOutput(pActInfo, 1, dyn.a);
				ActivateOutput(pActInfo, 2, dyn.w);
				ActivateOutput(pActInfo, 3, dyn.wa);
				ActivateOutput(pActInfo, 4, dyn.mass);
			}
		}
	}
};

class CFlowNode_PhysicsSleepQuery : public CFlowBaseNode<eNCT_Instanced>
{
	enum EInPorts
	{
		IN_CONDITION = 0,
		IN_RESET
	};
	enum EOutPorts
	{
		OUT_SLEEPING = 0,
		OUT_ONSLEEP,
		OUT_ONAWAKE
	};

	bool m_Activated;

public:
	CFlowNode_PhysicsSleepQuery(SActivationInfo* pActInfo) : m_Activated(false) {}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_PhysicsSleepQuery(pActInfo);
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<bool>("condition", _HELP("Setting this input to TRUE sends the sleeping condition of the entity to the [sleeping] port, and triggers [sleep] and [awake]")),
			InputPortConfig<bool>("reset",     _HELP("Triggering this input to TRUE resets the node")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<bool>("sleeping",         _HELP("Set to TRUE when the physics of the entity are sleeping (passes the value through)")),
			OutputPortConfig<SFlowSystemVoid>("sleep", _HELP("Triggers to TRUE when the physics of the entity switch to sleep")),
			OutputPortConfig<SFlowSystemVoid>("awake", _HELP("Triggers to TRUE when the physics of the entity switch to awake")),
			{ 0 }
		};
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.sDescription = _HELP("Node that returns the sleeping state of the physics of a given entity.");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Initialize:
			pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
			m_Activated = false;
			break;

		case eFE_Update:
			{
				bool bReset = GetPortBool(pActInfo, IN_RESET);
				bool bCondition = GetPortBool(pActInfo, IN_CONDITION);

				if (bReset)
				{
					ActivateOutput(pActInfo, OUT_SLEEPING, !bCondition);
				}
				else
				{
					if (bCondition != m_Activated)
					{
						IEntity* pEntity = pActInfo->pEntity;

						if (pEntity)
						{
							IPhysicalEntity* pPhysEntity = pEntity->GetPhysics();

							if (pPhysEntity)
							{
								pe_status_awake psa;

								bool isSleeping = pPhysEntity->GetStatus(&psa) ? false : true;

								ActivateOutput(pActInfo, OUT_SLEEPING, isSleeping);

								if (isSleeping)
									ActivateOutput(pActInfo, OUT_ONSLEEP, true);
								else
									ActivateOutput(pActInfo, OUT_ONAWAKE, true);
							}
						}

						m_Activated = bCondition;
					}
				}
			}
		}
	}
};

class CFlowNode_ActionImpulse : public CFlowBaseNode<eNCT_Singleton>
{
	enum ECoordSys
	{
		CS_PARENT = 0,
		CS_WORLD,
		CS_LOCAL
	};

	enum EInPorts
	{
		IN_ACTIVATE = 0,
		IN_IMPULSE,
		IN_ANGIMPULSE,
		IN_POINT,
		IN_PARTINDEX,
		IN_COORDSYS,
	};

public:
	CFlowNode_ActionImpulse(SActivationInfo* pActInfo) {}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("activate",    _HELP("Trigger the impulse")),
			InputPortConfig<Vec3>("impulse",    Vec3(0,                       0,                                                                                             0),                _HELP("Impulse vector")),
			InputPortConfig<Vec3>("angImpulse", Vec3(0,                       0,                                                                                             0),                _HELP("The angular impulse")),
			InputPortConfig<Vec3>("Point",      Vec3(0,                       0,                                                                                             0),                _HELP("Point of application (optional)")),
			InputPortConfig<int>("partIndex",   0,                            _HELP("Index of the part that will receive the impulse (optional, 1-based, 0=unspecified)")),
			InputPortConfig<int>("CoordSys",    1,                            _HELP("Defines which coordinate system is used for the inputs values"),                        _HELP("CoordSys"), _UICONFIG("enum_int:Parent=0,World=1,Local=2")),
			{ 0 }
		};
		config.nFlags |= EFLN_TARGET_ENTITY | EFLN_AISEQUENCE_SUPPORTED;
		config.pInputPorts = in_config;
		config.pOutputPorts = 0;
		config.sDescription = _HELP("Applies an impulse on an entity");
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (!pActInfo->pEntity)
			return;

		if (event == eFE_Activate && IsPortActive(pActInfo, IN_ACTIVATE))
		{
			pe_action_impulse action;

			int ipart = GetPortInt(pActInfo, IN_PARTINDEX);
			if (ipart > 0)
				action.ipart = ipart - 1;

			IEntity* pEntity = pActInfo->pEntity;
			ECoordSys coordSys = (ECoordSys)GetPortInt(pActInfo, IN_COORDSYS);

			if (coordSys == CS_PARENT && !pEntity->GetParent())
				coordSys = CS_WORLD;

			// When a "zero point" is set in the node, the value is left undefined and physics assume it is the CM of the object.
			// but when the entity has a parent (is linked), then we have to use a real world coordinate for the point, because we have to apply the impulse to the highest entity
			// on the hierarchy and physics will use the position of that entity instead of the position of the entity assigned to the node
			bool bHaveToUseTransformedZeroPoint = false;
			Vec3 transformedZeroPoint;
			Matrix34 transMat;

			switch (coordSys)
			{
			case CS_WORLD:
			default:
				{
					transMat.SetIdentity();
					bHaveToUseTransformedZeroPoint = pEntity->GetParent() != NULL;
					transformedZeroPoint = pEntity->GetWorldPos();
					break;
				}

			case CS_PARENT:
				{
					transMat = pEntity->GetParent()->GetWorldTM();
					bHaveToUseTransformedZeroPoint = pEntity->GetParent()->GetParent() != NULL;
					transformedZeroPoint = pEntity->GetParent()->GetWorldPos();
					break;
				}

			case CS_LOCAL:
				{
					transMat = pEntity->GetWorldTM();
					bHaveToUseTransformedZeroPoint = pEntity->GetParent() != NULL;
					transformedZeroPoint = pEntity->GetWorldPos();
					break;
				}
			}

			action.impulse = GetPortVec3(pActInfo, IN_IMPULSE);
			action.impulse = transMat.TransformVector(action.impulse);

			Vec3 angImpulse = GetPortVec3(pActInfo, IN_ANGIMPULSE);
			if (!angImpulse.IsZero())
				action.angImpulse = transMat.TransformVector(angImpulse);

			Vec3 pointApplication = GetPortVec3(pActInfo, IN_POINT);
			if (!pointApplication.IsZero())
				action.point = transMat.TransformPoint(pointApplication);
			else
			{
				if (bHaveToUseTransformedZeroPoint)
					action.point = transformedZeroPoint;
			}

			// the impulse has to be applied to the highest entity in the hierarchy. This comes from how physics manage linked entities.
			IEntity* pEntityImpulse = pEntity;
			while (pEntityImpulse->GetParent())
			{
				pEntityImpulse = pEntityImpulse->GetParent();
			}

			IPhysicalEntity* pPhysEntity = pEntityImpulse->GetPhysics();
			if (pPhysEntity)
				pPhysEntity->Action(&action);
		}
	}
};

class CFlowNode_PartIdConversion : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_PartIdConversion( SActivationInfo * pActInfo ) {}

	enum EInPorts
	{
		eIP_GetJointId = 0,
		eIP_GetJointName,
		eIP_PartId,
	};
	enum EOutPorts
	{
		eOP_Failed = 0,
		eOP_JointId,
		eOP_JointName,
	};

	virtual void GetMemoryUsage(ICrySizer * s) const override
	{
		s->Add(*this);
	}

	virtual void GetConfiguration( SFlowNodeConfig& config ) override
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void( "GetJointId", _HELP("Get the Id of the joint to which the specified part Id is attached on the selected entity.") ),
			InputPortConfig_Void( "GetJointName", _HELP("Get the name of the joint to which the specified part Id is attached on the selected entity.") ),
			InputPortConfig<int>( "PartId", -1, _HELP("Part ID to get joint info for.") ),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_Void( "Failed", _HELP("Will trigger if the entity is invalid or doesn't have that part ID to find its joint name.") ),
			OutputPortConfig<int>( "JointID", _HELP("Id of the joint to which the specified part Id is attached on the selected entity.") ),
			OutputPortConfig<string>( "JointName", _HELP("Name of the joint to which the specified part Id is attached on the selected entity.") ),
			{0}
		};
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo ) override
	{
		if (event == eFE_Activate)
		{
			const bool bGetJointId   = IsPortActive(pActInfo, eIP_GetJointId);
			const bool bGetJointName = IsPortActive(pActInfo, eIP_GetJointName);
			if (bGetJointId || bGetJointName)
			{
				const int partId = GetPortInt(pActInfo, eIP_PartId);
				if (bGetJointId)
				{
					ActivateOutput(pActInfo, eOP_JointId, partId);
				}

				if (bGetJointName)
				{
					bool bSucceeded = false;
					if (IEntity * pEntity = pActInfo->pEntity)
					{
						if (const ICharacterInstance* pCharacterInstance = pEntity->GetCharacter(0))
						{
							const IDefaultSkeleton& defaultSkeleton = pCharacterInstance->GetIDefaultSkeleton();
							const char* szJointName = defaultSkeleton.GetJointNameByID(partId);
							if (szJointName && *szJointName)
							{
								ActivateOutput(pActInfo, eOP_JointName, string(szJointName));
								bSucceeded = true;
							}
						}
					}

					if (!bSucceeded)
					{
						ActivateOutput(pActInfo, eOP_Failed, true);
					}
				}
			}
		}
	}
};

struct SPhysicsRaycastFlowNodeHelper : CFlowBaseNode<eNCT_Singleton>
{
	static int GetObjTypes(SActivationInfo* pActInfo, int objTypeInputIndex)
	{
		int result = GetPortInt(pActInfo, objTypeInputIndex);
		if ((result&ent_all) == 0)
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Raycast node got invalid flag sum %d for Object Types input", result);
			result = ent_all;
		}
		if ((result&ent_all) != result)
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Raycast node got invalid flag sum %d for Object Types input", result);
			result = ent_all;
		}
		return result;
	}

	static const char* GetObjTypesDescription()
	{
		static stack_string s_uiDesc;
		if (s_uiDesc.empty())
		{
			s_uiDesc.append( "Sum of flags :" );
			s_uiDesc.append( stack_string().Format("\n%d = ent_all"           , ent_all           ).c_str() );
			s_uiDesc.append( stack_string().Format("\n%d = ent_static"        , ent_static        ).c_str() );
			s_uiDesc.append( stack_string().Format("\n%d = ent_sleeping_rigid", ent_sleeping_rigid).c_str() );
			s_uiDesc.append( stack_string().Format("\n%d = ent_rigid"         , ent_rigid         ).c_str() );
			s_uiDesc.append( stack_string().Format("\n%d = ent_living"        , ent_living        ).c_str() );
			s_uiDesc.append( stack_string().Format("\n%d = ent_independent"   , ent_independent   ).c_str() );
			s_uiDesc.append( stack_string().Format("\n%d = ent_terrain"       , ent_terrain       ).c_str() );
		}
		return s_uiDesc.c_str();
	}

	static const char* GetObjTypesUiConfig()
	{
		static stack_string s_uiConfig;
		if (s_uiConfig.empty())
		{
			s_uiConfig.append( "enum_int:" );
			s_uiConfig.append( stack_string().Format("ent_all=%d"            , ent_all           ).c_str() );
			s_uiConfig.append( stack_string().Format(",ent_static=%d"        , ent_static        ).c_str() );
			s_uiConfig.append( stack_string().Format(",ent_sleeping_rigid=%d", ent_sleeping_rigid).c_str() );
			s_uiConfig.append( stack_string().Format(",ent_rigid=%d"         , ent_rigid         ).c_str() );
			s_uiConfig.append( stack_string().Format(",ent_living=%d"        , ent_living        ).c_str() );
			s_uiConfig.append( stack_string().Format(",ent_independent=%d"   , ent_independent   ).c_str() );
			s_uiConfig.append( stack_string().Format(",ent_terrain=%d"       , ent_terrain       ).c_str() );
		}
		return s_uiConfig.c_str();
	}
};

class CFlowNode_ObjTypeSelection : public SPhysicsRaycastFlowNodeHelper
{
public:
	CFlowNode_ObjTypeSelection( SActivationInfo * pActInfo ) {}

	enum EInPorts
	{
		eIP_ObjType = 0,
	};
	enum EOutPorts
	{
		eOP_ObjType = 0,
	};

	virtual void GetMemoryUsage(ICrySizer * s) const override
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config) override
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<int>("ObjType", ent_all, _HELP(GetObjTypesDescription()),nullptr, _UICONFIG(GetObjTypesUiConfig())),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<int>("ObjType", _HELP("The selected value as an integer.")),
			{0}
		};
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo *pActInfo) override
	{
		if ((event == eFE_Initialize)
			||((event == eFE_Activate) && IsPortActive(pActInfo, eIP_ObjType)))
		{
			const int val = GetPortInt(pActInfo, eIP_ObjType);
			ActivateOutput(pActInfo, eOP_ObjType, val);
		}
	}
};

class CFlowNode_Raycast : public SPhysicsRaycastFlowNodeHelper
{
public:
	CFlowNode_Raycast(SActivationInfo* pActInfo) {}

	enum EInPorts
	{
		GO = 0,
		DIR,
		MAXLENGTH,
		POS,
		TRANSFORM_DIRECTION,
		OBJ_TYPES,
		IGNORED_ENTITY,
	};
	enum EOutPorts
	{
		NOHIT = 0,
		HIT,
		DIROUT,
		DISTANCE,
		HITPOINT,
		NORMAL,
		SURFTYPE,
		PARTID,
		HIT_ENTITY,
		HIT_ENTITY_PHID,
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<SFlowSystemVoid>("go", SFlowSystemVoid(), _HELP("Perform Raycast")),
			InputPortConfig<Vec3>("direction",     Vec3(0,            1,                                                        0),  _HELP("Direction of Raycast")),
			InputPortConfig<float>("maxLength",    10.0f,             _HELP("Maximum length of Raycast")),
			InputPortConfig<Vec3>("position",      Vec3(0,            0,                                                        0),  _HELP("Ray start position, relative to entity")),
			InputPortConfig<bool>("transformDir",  true,              _HELP("Direction is transformed by entity orientation.")),
			InputPortConfig<int>("ObjTypes", ent_all, _HELP(GetObjTypesDescription())),
			InputPortConfig<EntityId>("IgnoredEntity", INVALID_ENTITYID, _HELP("Entity to ignore in the raycast.")),
			{ 0 }
		};

		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<SFlowSystemVoid>("nohit", _HELP("Triggered if NO object was hit by raycast")),
			OutputPortConfig<SFlowSystemVoid>("hit",   _HELP("Triggered if an object was hit by raycast")),
			OutputPortConfig<Vec3>("direction",        _HELP("Actual direction of the cast ray (transformed by entity rotation")),
			OutputPortConfig<float>("distance",        _HELP("Distance to object hit")),
			OutputPortConfig<Vec3>("hitpoint",         _HELP("Position the ray hit")),
			OutputPortConfig<Vec3>("normal",           _HELP("Normal of the surface at the hitpoint")),
			OutputPortConfig<int>("surfacetype",       _HELP("Surface type index at the hit point")),
			OutputPortConfig<int>("partid", _HELP("Hit part id")),
			OutputPortConfig<EntityId>("entity",       _HELP("Entity which was hit")),
			OutputPortConfig<EntityId>("entityPhysId", _HELP("Id of the physical entity that was hit")),
			{ 0 }
		};
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (event == eFE_Activate && IsPortActive(pActInfo, GO))
		{
			IEntity* pEntity = pActInfo->pEntity;
			if (pEntity)
			{
				int objTypes = GetObjTypes(pActInfo, OBJ_TYPES);
				ray_hit hit;
				IPhysicalEntity* pSkip = pEntity->GetPhysics();
				Vec3 direction = GetPortVec3(pActInfo, DIR).GetNormalized();
				if (GetPortBool(pActInfo, TRANSFORM_DIRECTION))
					direction = pEntity->GetWorldTM().TransformVector(GetPortVec3(pActInfo, DIR).GetNormalized());
				IPhysicalWorld* pWorld = gEnv->pPhysicalWorld;
				int numHits = pWorld->RayWorldIntersection(
				  pEntity->GetWorldPos() + GetPortVec3(pActInfo, POS),
				  direction * GetPortFloat(pActInfo, MAXLENGTH),
				  objTypes,
				  rwi_stop_at_pierceable | rwi_colltype_any,
				  &hit, 1,
				  &pSkip, 1);

				if (numHits)
				{
					pEntity = (IEntity*)hit.pCollider->GetForeignData(PHYS_FOREIGN_ID_ENTITY);
					ActivateOutput(pActInfo, HIT, (bool)true);
					ActivateOutput(pActInfo, DIROUT, direction);
					ActivateOutput(pActInfo, DISTANCE, hit.dist);
					ActivateOutput(pActInfo, HITPOINT, hit.pt);
					ActivateOutput(pActInfo, NORMAL, hit.n);
					ActivateOutput(pActInfo, SURFTYPE, (int)hit.surface_idx);
					ActivateOutput(pActInfo, PARTID, hit.partid);
					ActivateOutput(pActInfo, HIT_ENTITY, pEntity ? pEntity->GetId() : 0);
					ActivateOutput(pActInfo, HIT_ENTITY_PHID, gEnv->pPhysicalWorld->GetPhysicalEntityId(hit.pCollider));
				}
				else
					ActivateOutput(pActInfo, NOHIT, false);
			}
		}
	}
};

class CFlowNode_RaycastCamera : public SPhysicsRaycastFlowNodeHelper
{
public:
	CFlowNode_RaycastCamera(SActivationInfo* pActInfo) {}

	enum EInPorts
	{
		GO = 0,
		POS,
		MAXLENGTH,
		OBJ_TYPES,
		IGNORED_ENTITY,
	};
	enum EOutPorts
	{
		NOHIT = 0,
		HIT,
		DIROUT,
		DISTANCE,
		HITPOINT,
		NORMAL,
		SURFTYPE,
		PARTID,
		HIT_ENTITY,
		HIT_ENTITY_PHID,
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<SFlowSystemVoid>("go", SFlowSystemVoid(), _HELP("Perform Raycast")),
			InputPortConfig<Vec3>("offset",        Vec3(0,            0,                                  0),_HELP("Ray start position, relative to camera")),
			InputPortConfig<float>("maxLength",    10.0f,             _HELP("Maximum length of Raycast")),
			InputPortConfig<int>("ObjTypes", ent_all, _HELP(GetObjTypesDescription())),
			InputPortConfig<EntityId>("IgnoredEntity", INVALID_ENTITYID, _HELP("Entity to ignore in the raycast.")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<SFlowSystemVoid>("nohit", _HELP("Triggered if NO object was hit by raycast")),
			OutputPortConfig<SFlowSystemVoid>("hit",   _HELP("Triggered if an object was hit by raycast")),
			OutputPortConfig<Vec3>("direction",        _HELP("Actual direction of the cast ray (transformed by entity rotation")),
			OutputPortConfig<float>("distance",        _HELP("Distance to object hit")),
			OutputPortConfig<Vec3>("hitpoint",         _HELP("Position the ray hit")),
			OutputPortConfig<Vec3>("normal",           _HELP("Normal of the surface at the hitpoint")),
			OutputPortConfig<int>("surfacetype",       _HELP("Surface type index at the hit point")),
			OutputPortConfig<int>("partid",            _HELP("Hit part id")),
			OutputPortConfig<EntityId>("entity",       _HELP("Entity which was hit")),
			OutputPortConfig<EntityId>("entityPhysId", _HELP("Id of the physical entity that was hit")),
			{ 0 }
		};
		config.sDescription = _HELP("Perform a raycast relative to the camera");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (event == eFE_Activate && IsPortActive(pActInfo, GO))
		{
			EntityId skipEntityId = GetPortEntityId(pActInfo, IGNORED_ENTITY);
			ray_hit hit;
			const CCamera& cam = GetISystem()->GetViewCamera();
			Vec3 pos = cam.GetPosition();
			Vec3 direction = cam.GetViewdir();
			IPhysicalWorld * pWorld = gEnv->pPhysicalWorld;

			// build list of physical entities to ignore, including the one from the port IGNORED_ENTITY and the local player plus associated physentities
			IActor* pLocalPlayer = gEnv->pGameFramework->GetClientActor();
			EntityId localPlayerId = INVALID_ENTITYID;

			const size_t maxSkipEntities = 16;
			IPhysicalEntity* pSkipEntities[maxSkipEntities];
			int numSkipEntities = 0;

			if (pLocalPlayer)
			{
				localPlayerId = pLocalPlayer->GetEntityId();
				numSkipEntities = pLocalPlayer->GetPhysicalSkipEntities(&pSkipEntities[0], maxSkipEntities);
			}

			if (skipEntityId != INVALID_ENTITYID && skipEntityId != localPlayerId)
			{
				if (numSkipEntities < maxSkipEntities)
				{
					IActor* pSkippedActor = gEnv->pGameFramework->GetIActorSystem()->GetActor(skipEntityId);
					if (pSkippedActor)
					{
						numSkipEntities += pSkippedActor->GetPhysicalSkipEntities(&pSkipEntities[numSkipEntities], maxSkipEntities - numSkipEntities);
					}
					else
					{
						IEntity* pAdditionalSkipEntity = gEnv->pEntitySystem->GetEntity(skipEntityId);
						if (pAdditionalSkipEntity && pAdditionalSkipEntity->GetPhysics())
						{
							pSkipEntities[numSkipEntities++] = pAdditionalSkipEntity->GetPhysics();
						}
					}
				}
				else
				{
					CryLog("FG Node 'Physics:RayCastCamera': Maximum skipped entities reached: the IgnoredEntity is not being ignored!");
				}
			}

			int numHits = pWorld->RayWorldIntersection(
				pos + GetPortVec3(pActInfo, POS),
				direction * GetPortFloat(pActInfo, MAXLENGTH),
				GetObjTypes(pActInfo, OBJ_TYPES),
				rwi_stop_at_pierceable|rwi_colltype_any,
				&hit, 1,
				&pSkipEntities[0], numSkipEntities
			);

			if (numHits)
			{
				IEntity* pEntity = (IEntity*)hit.pCollider->GetForeignData(PHYS_FOREIGN_ID_ENTITY);
				ActivateOutput( pActInfo, HIT, (bool)true );
				ActivateOutput( pActInfo, DIROUT, direction );
				ActivateOutput( pActInfo, DISTANCE, hit.dist );
				ActivateOutput( pActInfo, HITPOINT, hit.pt );
				ActivateOutput( pActInfo, NORMAL, hit.n );
				ActivateOutput( pActInfo, SURFTYPE, (int)hit.surface_idx );
				ActivateOutput( pActInfo, PARTID, hit.partid );
				ActivateOutput( pActInfo, HIT_ENTITY, pEntity ? pEntity->GetId() : 0);
				ActivateOutput( pActInfo, HIT_ENTITY_PHID, gEnv->pPhysicalWorld->GetPhysicalEntityId(hit.pCollider));
			}
			else
			{
				ActivateOutput( pActInfo, NOHIT, false );
			}
		}
	}
};

//////////////////////////////////////////////////////////////////////////
// Enable/Disable AI for an entity/
//////////////////////////////////////////////////////////////////////////
class CFlowNode_PhysicsEnable : public CFlowBaseNode<eNCT_Singleton>
{
public:
	enum EInputs
	{
		IN_ENABLE,
		IN_DISABLE,
		IN_ENABLE_AI,
		IN_DISABLE_AI,
	};
	CFlowNode_PhysicsEnable(SActivationInfo* pActInfo) {};

	/*
	   IFlowNodePtr Clone( SActivationInfo * pActInfo )
	   {
	   return this;
	   }
	 */

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("Enable",     _HELP("Enable Physics for target entity")),
			InputPortConfig_Void("Disable",    _HELP("Disable Physics for target entity")),
			InputPortConfig_Void("AI_Enable",  _HELP("Enable AI for target entity")),
			InputPortConfig_Void("AI_Disable", _HELP("Disable AI for target entity")),
			{ 0 }
		};
		config.sDescription = _HELP("Enables/Disables Physics");
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = in_config;
		config.pOutputPorts = 0;
		config.SetCategory(EFLN_ADVANCED);
	}
	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		switch (event)
		{
		case eFE_Activate:
			{
				if (!pActInfo->pEntity)
					return;

				if (IsPortActive(pActInfo, IN_ENABLE))
				{
					pActInfo->pEntity->EnablePhysics(true);
				}
				if (IsPortActive(pActInfo, IN_DISABLE))
				{
					pActInfo->pEntity->EnablePhysics(false);
				}

				if (IsPortActive(pActInfo, IN_ENABLE_AI))
				{
					if (IAIObject* aiObject = pActInfo->pEntity->GetAI())
						aiObject->Event(AIEVENT_ENABLE, 0);
				}
				if (IsPortActive(pActInfo, IN_DISABLE_AI))
				{
					if (IAIObject* aiObject = pActInfo->pEntity->GetAI())
						aiObject->Event(AIEVENT_DISABLE, 0);
				}
				break;
			}

		case eFE_Initialize:
			break;
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

class CFlowNode_CameraProxy : public CFlowBaseNode<eNCT_Singleton>
{
public:
	enum EInputs
	{
		IN_CREATE,
		IN_ID
	};
	enum EOutputs
	{
		OUT_ID
	};
	CFlowNode_CameraProxy(SActivationInfo* pActInfo) {};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<SFlowSystemVoid>("Create", SFlowSystemVoid(), _HELP("Create the proxy if it doesnt exist yet")),
			InputPortConfig<EntityId>("EntityHost",    0,                 _HELP("Activate to sync proxy rotation with the current view camera")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<int>("EntityCamera", _HELP("")),
			{ 0 }
		};
		config.sDescription = _HELP("Retrieves or creates a physicalized camera proxy attached to EntityHost");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_ADVANCED);
	}

	IEntity* GetCameraEnt(IEntity* pHost, bool bCreate)
	{
		if (!pHost->GetPhysics())
			return bCreate ? pHost : 0;

		pe_params_articulated_body pab;
		IEntityLink* plink = pHost->GetEntityLinks();
		for (; plink && strcmp(plink->name, "CameraProxy"); plink = plink->next);
		if (plink)
		{
			IEntity* pCam = gEnv->pEntitySystem->GetEntity(plink->entityId);
			if (!pCam)
			{
				pHost->RemoveEntityLink(plink);
				if (bCreate)
					goto CreateCam;
				return 0;
			}
			pab.qHostPivot = !pHost->GetRotation() * Quat(Matrix33(GetISystem()->GetViewCamera().GetMatrix()));
			pCam->GetPhysics()->SetParams(&pab);
			return pCam;
		}
		else if (bCreate)
		{
CreateCam:
			const CCamera& cam = GetISystem()->GetViewCamera();
			Quat qcam = Quat(Matrix33(cam.GetMatrix()));
			SEntitySpawnParams esp;
			esp.sName = "CameraProxy";
			esp.nFlags = 0;
			esp.pClass = gEnv->pEntitySystem->GetClassRegistry()->GetDefaultClass();
			IEntity* pCam = gEnv->pEntitySystem->SpawnEntity(esp);
			pCam->SetPos(cam.GetPosition());
			pCam->SetRotation(qcam);
			pHost->AddEntityLink("CameraProxy", pCam->GetId(),pCam->GetGuid());
			SEntityPhysicalizeParams epp;
			epp.type = PE_ARTICULATED;
			pCam->Physicalize(epp);
			pe_geomparams gp;
			gp.flags = 0;
			gp.flagsCollider = 0;
			primitives::sphere sph;
			sph.r = 0.1f;
			sph.center.zero();
			IGeometry* psph = gEnv->pPhysicalWorld->GetGeomManager()->CreatePrimitive(primitives::sphere::type, &sph);
			phys_geometry* pGeom = gEnv->pPhysicalWorld->GetGeomManager()->RegisterGeometry(psph);
			psph->Release();
			pCam->GetPhysics()->AddGeometry(pGeom, &gp);
			pGeom->nRefCount--;
			pe_params_pos pp;
			pp.iSimClass = 2;
			pCam->GetPhysics()->SetParams(&pp);
			pab.pHost = pHost->GetPhysics();
			pab.posHostPivot = (cam.GetPosition() - pHost->GetPos()) * pHost->GetRotation();
			pab.qHostPivot = !pHost->GetRotation() * qcam;
			pCam->GetPhysics()->SetParams(&pab);
			return pCam;
		}
		return 0;
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (event == eFE_Activate)
		{
			bool bCreate = IsPortActive(pActInfo, IN_CREATE);
			if (IEntity* pHost = gEnv->pEntitySystem->GetEntity(GetPortEntityId(pActInfo, IN_ID)))
				if (IEntity* pCam = GetCameraEnt(pHost, bCreate))
					if (bCreate)
						ActivateOutput(pActInfo, OUT_ID, pCam->GetId());
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

class CFlowNode_Constraint : public CFlowBaseNode<eNCT_Instanced>
{
public:
	enum EInputs
	{
		IN_CREATE,
		IN_BREAK,
		IN_ID,
		IN_ENTITY0,
		IN_PARTID0,
		IN_ENTITY1,
		IN_PARTID1,
		IN_TYPE,
		IN_POINT,
		IN_POINTB,
		IN_LOCAL_FRAMES,
		IN_IGNORE_COLLISIONS,
		IN_BREAKABLE,
		IN_FORCE_AWAKE,
		IN_FORCE_MOVE,
		IN_MAX_FORCE,
		IN_MAX_TORQUE,
		IN_MAX_FORCE_REL,
		IN_AXIS,
		IN_MIN_ROT,
		IN_MAX_ROT,
		IN_MAX_BEND,
		IN_AREA,
	};
	enum EOutputs
	{
		OUT_ID,
		OUT_BROKEN,
	};
	CFlowNode_Constraint(SActivationInfo* pActInfo) : m_id(1000) {};
	~CFlowNode_Constraint() {}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_Constraint(pActInfo);
	}

	struct SConstraintRec
	{
		SConstraintRec() : next(0), prev(0), pent(0), pNode(0), idConstraint(-1), bBroken(0), minEnergy(0), minEnergyRagdoll(0.0f) {}
		~SConstraintRec() { Free(); }
		void Free()
		{
			if (idConstraint >= 0)
			{
				if (minEnergy > 0)
				{
					pe_simulation_params sp;
					sp.minEnergy = minEnergy;
					pe_params_articulated_body pab;
					pab.minEnergyLyingMode = minEnergyRagdoll;
					pent->SetParams(&sp);
					pent->SetParams(&pab);
				}
				if (pent)
					pent->Release();
				idConstraint = -1;
				pent = 0;
				(prev ? prev->next : g_pConstrRec0) = next;
				if (next) next->prev = prev;
			}
		}
		SConstraintRec*       next, * prev;
		IPhysicalEntity*      pent;
		CFlowNode_Constraint* pNode;
		int                   idConstraint;
		int                   bBroken;
		float                 minEnergy, minEnergyRagdoll;
	};
	static SConstraintRec* g_pConstrRec0;
	int                    m_id;

	static int OnConstraintBroken(const EventPhysJointBroken* ejb)
	{
		for (SConstraintRec* pRec = g_pConstrRec0; pRec; pRec = pRec->next)
			if (pRec->pent == ejb->pEntity[0] && pRec->idConstraint == ejb->idJoint)
				pRec->bBroken = 1;
		return 1;
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<SFlowSystemVoid>("Create", SFlowSystemVoid(),        _HELP("Creates the constraint")),
			InputPortConfig<SFlowSystemVoid>("Break",  SFlowSystemVoid(),        _HELP("Breaks a constraint Id from EntityA if EntityA is activated, or a previously created one")),
			InputPortConfig<int>("Id",                 1000,                     _HELP("Requested constraint Id; -1 to auto-generate, -2 to auto-generate and remember")),
			InputPortConfig<EntityId>("EntityA",       0,                        _HELP("Constraint owner entity (can't be an actor; should be lighter than EntityB for best results)")),
			InputPortConfig<int>("PartIdA",            -1,                       _HELP("Part id to attach to; -1 to use default")),
			InputPortConfig<EntityId>("EntityB",       0,                        _HELP("Constrained entity (can be an actor)")),
			InputPortConfig<int>("PartIdB",            -1,                       _HELP("Part id to attach to; -1 to use default")),
			InputPortConfig<int>("Type",               _HELP("Constraint type"), _HELP("Type"),                                                                                         _UICONFIG("enum_int:Point=0,RotOnly=1,Line=2,Plane=3")),
			InputPortConfig<Vec3>("Point",             Vec3(ZERO),               _HELP("Connection point in world space")),
			InputPortConfig<Vec3>("PointB",            Vec3(ZERO),               _HELP("Connection point on entity B in world space (if not set, assumed same as Point)")),
			InputPortConfig<bool>("LocalFrames",       false,                    _HELP("Means Point and PointB are specified in Entity and EntityB's local frames, respectively (instead of the world)")),
			InputPortConfig<bool>("IgnoreCollisions",  true,                     _HELP("Disables collisions between constrained entities")),
			InputPortConfig<bool>("Breakable",         false,                    _HELP("Break if force limit is reached")),
			InputPortConfig<bool>("ForceAwake",        false,                    _HELP("Make EntityB always awake; restore previous sleep params on Break")),
			InputPortConfig<bool>("MoveEnforcement",   true,                     _HELP("Teleport one enity if the other has its position updated from outside the physics")),
			InputPortConfig<float>("MaxForce",         0.0f,                     _HELP("Force limit")),
			InputPortConfig<float>("MaxTorque",        0.0f,                     _HELP("Rotational force (torque) limit")),
			InputPortConfig<bool>("MaxForceRelative",  true,                     _HELP("Make limits relative to EntityB's mass")),
			InputPortConfig<Vec3>("TwistAxis",         Vec3(0,                   0,                                                                                                     1), _HELP("Main rotation axis in world space")),
			InputPortConfig<float>("MinTwist",         0.0f,                     _HELP("Lower rotation limit around TwistAxis. Linear limit for line constraints")),
			InputPortConfig<float>("MaxTwist",         0.0f,                     _HELP("Upper rotation limit around TwistAxis. Linear limit for line constraints")),
			InputPortConfig<float>("MaxBend",          0.0f,                     _HELP("Maximum bend of the TwistAxis")),
			InputPortConfig<EntityId>("Area",          0,                        _HELP("Optional physical area entity to define the constraint surface")),

			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<int>("Id",      _HELP("Constraint Id")),
			OutputPortConfig<bool>("Broken", _HELP("Activated when the constraint breaks")),
			{ 0 }
		};
		config.sDescription = _HELP("Creates a physical constraint between EntityA and EntityB");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_ADVANCED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		SConstraintRec* pRec = 0, * pRecNext;
		if (event == eFE_Update)
		{
			pe_status_pos sp;
			for (pRec = g_pConstrRec0; pRec; pRec = pRecNext)
			{
				pRecNext = pRec->next;
				if (pRec->pNode == this && (pRec->bBroken || pRec->pent->GetStatus(&sp) && sp.iSimClass == 7))
				{
					ActivateOutput(pActInfo, OUT_BROKEN, true);
					ActivateOutput(pActInfo, OUT_ID, pRec->idConstraint);
					delete pRec;
				}
			}
			if (!g_pConstrRec0)
				pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
		}
		else if (event == eFE_Activate)
		{
			if (IsPortActive(pActInfo, IN_ID))
				m_id = GetPortInt(pActInfo, IN_ID);
			if (IsPortActive(pActInfo, IN_CREATE))
			{
				IEntity* pent[3] = {
					gEnv->pEntitySystem->GetEntity(GetPortEntityId(pActInfo, IN_ENTITY0)), gEnv->pEntitySystem->GetEntity(GetPortEntityId(pActInfo, IN_ENTITY1)),
					gEnv->pEntitySystem->GetEntity(GetPortEntityId(pActInfo, IN_AREA))
				};
				if (!pent[0] || !pent[0]->GetPhysics())
					return;
				IPhysicalEntity* pentPhys = pent[0]->GetPhysics();
				pe_action_add_constraint aac;
				int i;
				float f;
				aac.pBuddy = pent[1] && pent[1]->GetPhysics() ? pent[1]->GetPhysics() : WORLD_ENTITY;
				if (pent[2] && pent[2]->GetPhysics())
					aac.pConstraintEntity = pent[2]->GetPhysics();
				if (m_id >= 0)
					aac.id = m_id;
				for (int iop = 0; iop < 2; iop++)
					if ((i = GetPortInt(pActInfo, IN_PARTID0 + iop * 2)) >= 0)
						aac.partid[iop] = i;
				aac.pt[0] = GetPortVec3(pActInfo, IN_POINT);
				Vec3 ptB = GetPortVec3(pActInfo, IN_POINTB);
				if (ptB.len2())
					aac.pt[1] = ptB;
				aac.flags = GetPortBool(pActInfo, IN_LOCAL_FRAMES) ? local_frames : world_frames;
				aac.flags |= (GetPortBool(pActInfo, IN_IGNORE_COLLISIONS) ? constraint_ignore_buddy : 0) | (GetPortBool(pActInfo, IN_BREAKABLE) ? 0 : constraint_no_tears);
				aac.flags |= GetPortBool(pActInfo, IN_FORCE_MOVE) ? 0 : constraint_no_enforcement;
				const int cflags[] = { 0, constraint_free_position, constraint_line, constraint_plane };
				aac.flags |= cflags[i = GetPortInt(pActInfo, IN_TYPE)];
				pe_status_dynamics sd;
				sd.mass = 1.0f;
				if (GetPortBool(pActInfo, IN_MAX_FORCE_REL))
					pentPhys->GetStatus(&sd);
				if ((f = GetPortFloat(pActInfo, IN_MAX_FORCE)) > 0)
					aac.maxPullForce = f * sd.mass;
				if ((f = GetPortFloat(pActInfo, IN_MAX_TORQUE)) > 0)
					aac.maxBendTorque = f * sd.mass;
				float conv = i == 2 ? 1.0f : gf_PI / 180.0f;
				for (int iop = 0; iop < 2; iop++)
					aac.xlimits[iop] = GetPortFloat(pActInfo, IN_MIN_ROT + iop) * conv;
				aac.yzlimits[0] = 0;
				aac.yzlimits[1] = DEG2RAD(GetPortFloat(pActInfo, IN_MAX_BEND));
				if (aac.xlimits[1] <= aac.xlimits[0] && aac.yzlimits[1] <= aac.yzlimits[0])
					aac.flags |= constraint_no_rotation;
				else if (aac.xlimits[0] < gf_PI * -1.01f && aac.xlimits[1] > gf_PI * 1.01f && aac.yzlimits[1] > gf_PI * 0.51f)
					MARK_UNUSED aac.xlimits[0], aac.xlimits[1], aac.yzlimits[0], aac.yzlimits[1];
				aac.qframe[0] = aac.qframe[1] = Quat::CreateRotationV0V1(Vec3(1, 0, 0), GetPortVec3(pActInfo, IN_AXIS));
				if (GetPortBool(pActInfo, IN_FORCE_AWAKE))
				{
					pRec = new SConstraintRec;
					pe_simulation_params sp;
					sp.minEnergy = 0;
					pentPhys->GetParams(&sp);
					pRec->minEnergy = pRec->minEnergyRagdoll = sp.minEnergy;
					new(&sp)pe_simulation_params;
					sp.minEnergy = 0;
					pentPhys->SetParams(&sp);
					pe_params_articulated_body pab;
					if (pentPhys->GetParams(&pab))
					{
						pRec->minEnergyRagdoll = pab.minEnergyLyingMode;
						new(&pab)pe_params_articulated_body;
						pab.minEnergyLyingMode = 0;
						pentPhys->SetParams(&pab);
					}
					pe_action_awake aa;
					aa.minAwakeTime = 0.1f;
					pentPhys->Action(&aa);
					if (aac.pBuddy != WORLD_ENTITY)
						aac.pBuddy->Action(&aa);
				}
				pe_params_flags pf;
				int queued = aac.pBuddy == WORLD_ENTITY ? 0 : aac.pBuddy->SetParams(&pf) - 1, id = pentPhys->Action(&aac, -queued >> 31);
				if (m_id == -2)
					m_id = id;
				ActivateOutput(pActInfo, OUT_ID, id);
				if (!is_unused(aac.maxPullForce || !is_unused(aac.maxBendTorque)) && !(aac.flags & constraint_no_tears))
				{
					if (!pRec)
						pRec = new SConstraintRec;
					gEnv->pPhysicalWorld->AddEventClient(EventPhysJointBroken::id, (int (*)(const EventPhys*))OnConstraintBroken, 1);
				}
				if (pRec)
				{
					(pRec->pent = pentPhys)->AddRef();
					pRec->idConstraint = id;
					pRec->pNode = this;
					if (g_pConstrRec0)
						g_pConstrRec0->prev = pRec;
					pRec->next = g_pConstrRec0;
					g_pConstrRec0 = pRec;
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
				}
				return;
			}

			if (IsPortActive(pActInfo, IN_BREAK) || IsPortActive(pActInfo, IN_POINT))
				if (IEntity* pent = gEnv->pEntitySystem->GetEntity(GetPortEntityId(pActInfo, IN_ENTITY0)))
					if (IPhysicalEntity* pentPhys = pent->GetPhysics())
					{
						pe_action_update_constraint auc;
						auc.idConstraint = GetPortInt(pActInfo, IN_ID);
						if (IsPortActive(pActInfo, IN_BREAK))
							auc.bRemove = 1;
						if (IsPortActive(pActInfo, IN_POINT))
							auc.pt[1] = GetPortVec3(pActInfo, IN_POINT);
						auc.flags = GetPortBool(pActInfo, IN_LOCAL_FRAMES) ? local_frames : world_frames;
						pentPhys->Action(&auc);
						if (auc.bRemove)
						{
							for (pRec = g_pConstrRec0; pRec; pRec = pRecNext)
							{
								pRecNext = pRec->next;
								if (pRec->pent == pentPhys && pRec->idConstraint == auc.idConstraint)
									delete pRec;
							}
							ActivateOutput(pActInfo, OUT_BROKEN, true);
							ActivateOutput(pActInfo, OUT_ID, auc.idConstraint);
							if (!g_pConstrRec0)
								pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
						}
					}
		}
	}

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}
};

CFlowNode_Constraint::SConstraintRec* CFlowNode_Constraint::g_pConstrRec0 = 0;

class CFlowNode_Collision : public CFlowBaseNode<eNCT_Instanced>
{
public:
	enum EInputs
	{
		IN_ADD_ID,
		IN_MAX_COLLISIONS,
		IN_IGNORE_SIBLINGS,
		IN_ONLY_SIBLINGS,
		IN_REMOVE_ID,
		IN_REMOVE_ALL,
	};
	enum EOutputs
	{
		OUT_ID_A,
		OUT_PART_ID_A,
		OUT_ID_B,
		OUT_PART_ID_B,
		OUT_POINT,
		OUT_NORMAL,
		OUT_SURFACETYPE_A,
		OUT_SURFACETYPE_B,
		OUT_IMPULSE,
	};

	struct SCollListener
	{
		EntityId             id;
		CFlowNode_Collision* pNode;
	};
	static std::map<EntityId, SCollListener*> g_listeners;
	SActivationInfo                           m_actInfo;

	CFlowNode_Collision(SActivationInfo* pActInfo) {}
	virtual ~CFlowNode_Collision()
	{
		for (auto iter = g_listeners.begin(); iter != g_listeners.end(); )
			if (iter->second->pNode == this)
			{
				delete iter->second;
				g_listeners.erase(iter++);
			}
			else iter++;
	}

	IFlowNodePtr Clone(SActivationInfo* pActInfo)
	{
		return new CFlowNode_Collision(pActInfo);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<EntityId>( "AddListener", 0, _HELP("Register entity id as collision listener") ),
			InputPortConfig<int>( "CollisionsPerFrame", 0, _HELP("Set the maximum amount of reported collisions per frame (0 = don't change)") ),
			InputPortConfig<bool>( "IgnoreSameNode", false, _HELP("Suppress events if both colliders are registered via the same node") ),
			InputPortConfig<bool>( "OnlySameNode", false, _HELP("Only send events if both colliders are registered via the same node") ),
			InputPortConfig<EntityId>( "RemoveListener", 0, _HELP("Unregister entity id from collision listeners") ),
			InputPortConfig<SFlowSystemVoid>("RemoveAll", SFlowSystemVoid(), _HELP("Remove listeners registered through this node")),
			{0}
		};

		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<EntityId>("IdA",        _HELP("Id of the first colliding entity (one that is registered as a listener)")),
			OutputPortConfig<int>("PartIdA",         _HELP("Part id inside the first colliding entity")),
			OutputPortConfig<EntityId>("IdB",        _HELP("Id of the second colliding entity")),
			OutputPortConfig<int>("PartIdB",         _HELP("Part id inside the second colliding entity")),
			OutputPortConfig<Vec3>("Point",          _HELP("Collision point in world space")),
			OutputPortConfig<Vec3>("Normal",         _HELP("Collision normal in world space")),
			OutputPortConfig<string>("SurfacetypeA", _HELP("Name of the surfacetype of the first colliding entity")),
			OutputPortConfig<string>("SurfacetypeB", _HELP("Name of the surfacetype of the second colliding entity")),
			OutputPortConfig<float>("HitImpulse",    _HELP("Collision impulse along the normal")),
			{ 0 }
		};
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.sDescription = _HELP("Set up collision listeners");
		config.SetCategory(EFLN_APPROVED); // POLICY CHANGE: Maybe an Enable/Disable Port
	}

	virtual void GetMemoryUsage(ICrySizer* s) const { s->Add(*this); }

	static int   OnCollision(const EventPhysCollision* pColl)
	{
		std::map<EntityId, SCollListener*>::iterator iter;
		SCollListener* coll[2] = { 0, 0 };
		for (int i = 0; i < 2; i++)
			if (pColl->iForeignData[i] == PHYS_FOREIGN_ID_ENTITY && (iter = g_listeners.find(((IEntity*)pColl->pForeignData[i])->GetId())) != g_listeners.end())
				coll[i] = iter->second;
		if (coll[0] && coll[1] && coll[0]->pNode == coll[1]->pNode && GetPortBool(&coll[0]->pNode->m_actInfo, IN_IGNORE_SIBLINGS))
			return 1;
		for (int i = 0; i < 2; i++)
			if (coll[i] && GetPortBool(&coll[i]->pNode->m_actInfo, IN_ONLY_SIBLINGS) && (!coll[i ^ 1] || coll[i]->pNode != coll[i ^ 1]->pNode))
				return 1;
		for (int i = 0; i < 2; i++)
			if (coll[i])
			{
				SActivationInfo* pActInfo = &coll[i]->pNode->m_actInfo;
				ActivateOutput(pActInfo, OUT_PART_ID_A, pColl->partid[i]);
				ActivateOutput(pActInfo, OUT_ID_B, pColl->iForeignData[i ^ 1] == PHYS_FOREIGN_ID_ENTITY ? ((IEntity*)pColl->pForeignData[i ^ 1])->GetId() : 0);
				ActivateOutput(pActInfo, OUT_PART_ID_B, pColl->partid[i ^ 1]);
				ActivateOutput(pActInfo, OUT_POINT, pColl->pt);
				ActivateOutput(pActInfo, OUT_NORMAL, pColl->n * (1.0f - i * 2));
				ActivateOutput(pActInfo, OUT_IMPULSE, pColl->normImpulse);
				if (ISurfaceType* pSurf = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager()->GetSurfaceType(pColl->idmat[i]))
					ActivateOutput(pActInfo, OUT_SURFACETYPE_A, string(pSurf->GetName()));
				if (ISurfaceType* pSurf = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager()->GetSurfaceType(pColl->idmat[i ^ 1]))
					ActivateOutput(pActInfo, OUT_SURFACETYPE_B, string(pSurf->GetName()));
				ActivateOutput(pActInfo, OUT_ID_A, coll[i]->id);
			}
		return 1;
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (event == eFE_Initialize)
			m_actInfo = *pActInfo;
		else if (event == eFE_Activate)
		{
			IEntity* pent;
			if (IsPortActive(pActInfo, IN_ADD_ID))
			{
				SCollListener cl;
				cl.id = GetPortEntityId(pActInfo, IN_ADD_ID);
				cl.pNode = this;
				if (g_listeners.find(cl.id) == g_listeners.end() && (pent = gEnv->pEntitySystem->GetEntity(cl.id)) && pent->GetPhysics())
				{
					if (g_listeners.empty())
						gEnv->pPhysicalWorld->AddEventClient(EventPhysCollision::id, (int (*)(const EventPhys*))OnCollision, 1);
					pe_simulation_params sp;
					if (sp.maxLoggedCollisions = GetPortInt(pActInfo, IN_MAX_COLLISIONS))
						pent->GetPhysics()->SetParams(&sp);
					pe_params_flags pf;
					pf.flagsOR = pef_log_collisions;
					pent->GetPhysics()->SetParams(&pf);
					g_listeners.insert(std::pair<EntityId, SCollListener*>(cl.id, new SCollListener(cl)));
				}
			}

			std::map<EntityId, SCollListener*>::iterator iter;
			if (IsPortActive(pActInfo, IN_REMOVE_ID) && (iter = g_listeners.find(GetPortEntityId(pActInfo, IN_REMOVE_ID))) != g_listeners.end())
			{
				delete iter->second;
				g_listeners.erase(iter);
				if (g_listeners.empty())
					gEnv->pPhysicalWorld->RemoveEventClient(EventPhysCollision::id, (int (*)(const EventPhys*))OnCollision, 1);
			}

			if (IsPortActive(pActInfo, IN_REMOVE_ALL))
			{
				for(auto iter=g_listeners.begin(); iter!=g_listeners.end(); )
					if (iter->second->pNode==this)
					{
						delete iter->second;
						iter = g_listeners.erase(iter);
					}
					else
						++iter;
				if (g_listeners.empty())
					gEnv->pPhysicalWorld->RemoveEventClient(EventPhysCollision::id, (int(*)(const EventPhys*))OnCollision, 1);
			}
		}
	}
};

std::map<EntityId, CFlowNode_Collision::SCollListener*> CFlowNode_Collision::g_listeners;

class CFlowNode_PhysId : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_PhysId(SActivationInfo* pActInfo) {}

	enum EInPorts
	{
		IN_ENTITY_ID,
	};
	enum EOutPorts
	{
		OUT_PHYS_ID,
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<EntityId>("EntityId", EntityId(0), _HELP("Entity id")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<int>("PhysId", _HELP("Physical entity id")),
			{ 0 }
		};
		config.sDescription = _HELP("Return id of a physical entity associated with EntityId (-1 if none)");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (event == eFE_Activate)
			if (IEntity* pent = gEnv->pEntitySystem->GetEntity(GetPortEntityId(pActInfo, IN_ENTITY_ID)))
				ActivateOutput(pActInfo, OUT_PHYS_ID, gEnv->pPhysicalWorld->GetPhysicalEntityId(pent->GetPhysics()));
	}
};

class CFlowNode_Awake : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_Awake(SActivationInfo* pActInfo) {}

	enum EInPorts
	{
		IN_AWAKE,
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<bool>("Awake", true, _HELP("Awake or send to sleep flag")),
			{ 0 }
		};
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.sDescription = _HELP("Awakes an entity or sends it to rest");
		config.pInputPorts = in_config;
		config.pOutputPorts = 0;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (event == eFE_Activate && pActInfo->pEntity && pActInfo->pEntity->GetPhysics())
		{
			pe_action_awake aa;
			aa.bAwake = GetPortBool(pActInfo, IN_AWAKE) ? 1 : 0;
			pActInfo->pEntity->GetPhysics()->Action(&aa);
		}
	}
};

class CFlowNode_PhysParams : public CFlowBaseNode<eNCT_Singleton>
{
public:
	CFlowNode_PhysParams(SActivationInfo*) {}
	virtual void GetMemoryUsage(ICrySizer* s) const { s->Add(*this); }

	enum EInPorts
	{
		IN_SET, IN_TYPE, IN_NAME0, IN_VAL0
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("Set",  _HELP("Activate it when all values are ready")),
			InputPortConfig<int>("Type", _HELP("Type of the parameters to set"),         _HELP("Type"),_UICONFIG("enum_int:\
Particle=1,Vehicle=2,PlayerDynamics=3,PlayerSize=4,Simulation=5,Articulated=6,Joint=7,Rope=8,Buoyancy=9,Constraint=10,\
Flags=12,CarWheel=13,SoftBody=14,Velocity=15,PartFlags=16,AutoDetachment=20,CollisionClass=21")),
#define VAL(i)                                                                                        \
  InputPortConfig<string>("Name" # i "", _HELP("Will set parameter with this name to Value" # i "")), \
  InputPortConfig_AnyType("Value" # i "", _HELP("Sets the value 'Name" # i "' (must have correct type)")),
			VAL(0) VAL(1) VAL(2) VAL(3) VAL(4) VAL(5) VAL(6) VAL(7) VAL(8) VAL(9)
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<int>("Result", _HELP("Param-specific result")),
			{ 0 }
		};
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.sDescription = _HELP("Sets various physical parameters that are exposed to scripts");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (event == eFE_Activate && IsPortActive(pActInfo, IN_SET) && pActInfo->pEntity && pActInfo->pEntity->GetPhysics())
		{
			HSCRIPTFUNCTION SetParams = 0;
			IScriptTable* pEntityTable = pActInfo->pEntity->GetScriptTable();
			if (pEntityTable->GetValue("SetPhysicParams", SetParams) && SetParams)
			{
				static IScriptTable* pParams = 0;
				if (!pParams)
					(pParams = gEnv->pScriptSystem->CreateTable())->AddRef();
				pParams->Clear();
				for (int i = 0; i < 10; i++)
				{
					string name = GetPortString(pActInfo, IN_NAME0 + i * 2);
					if (name.empty())
						continue;
					switch (GetPortType(pActInfo, IN_VAL0 + i * 2))
					{
					case eFDT_Int:
						pParams->SetValue(name, GetPortInt(pActInfo, IN_VAL0 + i * 2));
						break;
					case eFDT_Float:
						pParams->SetValue(name, GetPortFloat(pActInfo, IN_VAL0 + i * 2));
						break;
					case eFDT_EntityId:
						pParams->SetValue(name, GetPortEntityId(pActInfo, IN_VAL0 + i * 2));
						break;
					case eFDT_Vec3:
						pParams->SetValue(name, GetPortVec3(pActInfo, IN_VAL0 + i * 2));
						break;
					case eFDT_Bool:
						pParams->SetValue(name, GetPortBool(pActInfo, IN_VAL0 + i * 2) ? 1 : 0);
						break;
					case eFDT_String:
						pParams->SetValue(name, GetPortString(pActInfo, IN_VAL0 + i * 2).c_str());
						break;
					}
				}
				gEnv->pScriptSystem->BeginCall(SetParams);
				gEnv->pScriptSystem->PushFuncParam(pEntityTable);
				gEnv->pScriptSystem->PushFuncParam(GetPortInt(pActInfo, IN_TYPE));
				gEnv->pScriptSystem->PushFuncParam(pParams);
				int res;
				gEnv->pScriptSystem->EndCall(res);
				gEnv->pScriptSystem->ReleaseFunc(SetParams);
				ActivateOutput(pActInfo, 0, res);
			}
		}
	}
};

class CFlowNode_CharSkel : public CFlowBaseNode<eNCT_Singleton>, public IEntitySystemSink
{
public:
	CFlowNode_CharSkel(SActivationInfo* pActInfo) {}
	~CFlowNode_CharSkel()
	{
		g_mapSkels.clear();
		if (gEnv->pEntitySystem)
			gEnv->pEntitySystem->RemoveSink(this);	
	}

	enum EInPorts
	{
		IN_GET,
		IN_ENT_SLOT,
		IN_CHAR_SLOT,
	};
	enum EOutPorts
	{
		OUT_SKEL_ENT,
	};

	virtual void GetMemoryUsage(ICrySizer* s) const
	{
		s->Add(*this);
	}

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig_Void("Get", _HELP("Trigger the output")),
			InputPortConfig<int>("EntitySlot", 0, _HELP("Entity slot that contains the character")),
			InputPortConfig<int>("PhysIndex", -1, _HELP("Index of the character physics (-1 - the main skeleton, >= 0 - auxiliary, such as ropes or cloth)")),
			{ 0 }
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig<EntityId>("SkeletonEntId", _HELP("Entity id of the skeleton")),
			{ 0 }
		};
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.sDescription = _HELP("Returns a temporary entity id that can be used to access character skeleton physics");
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (event == eFE_Activate && pActInfo->pEntity)
			if (ICharacterInstance *pChar = pActInfo->pEntity->GetCharacter(GetPortInt(pActInfo, IN_ENT_SLOT)))
				if (IPhysicalEntity *pent = pChar->GetISkeletonPose()->GetCharacterPhysics(GetPortInt(pActInfo, IN_CHAR_SLOT)))
				{
					int id = gEnv->pPhysicalWorld->GetPhysicalEntityId(pent);
					IEntity *pSkel;
					auto iter = g_mapSkels.find(id);
					if (iter != g_mapSkels.end())
						pSkel = iter->second;
					else 
					{
						pe_status_pos sp;
						pent->GetStatus(&sp);
						pe_params_flags pf;
						pent->GetParams(&pf);
						SEntitySpawnParams esp;
						esp.sName = "SkeletonTmp";
						esp.nFlags = ENTITY_FLAG_NO_SAVE | ENTITY_FLAG_CLIENT_ONLY | ENTITY_FLAG_PROCEDURAL | ENTITY_FLAG_SPAWNED;
						esp.pClass = gEnv->pEntitySystem->GetClassRegistry()->GetDefaultClass();
						esp.vPosition = sp.pos;
						esp.qRotation = sp.q;
						pSkel = gEnv->pEntitySystem->SpawnEntity(esp);
						pSkel->AssignPhysicalEntity(pent);
						pe_params_foreign_data pfd;
						pfd.pForeignData = pActInfo->pEntity;
						pent->SetParams(&pfd); // revert the changes done in AssignPhysicalEntity
						pent->SetParams(&pf);
						if (g_mapSkels.empty())
							gEnv->pEntitySystem->AddSink(this, IEntitySystem::OnRemove);
						g_mapSkels.emplace(id, pSkel);
					}
					ActivateOutput(pActInfo, OUT_SKEL_ENT, pSkel->GetId());
				}
	}

	virtual bool OnBeforeSpawn(SEntitySpawnParams& params) { return true; }
	virtual void OnSpawn(IEntity* pEntity, SEntitySpawnParams& params) {}
	virtual void OnReused(IEntity* pEntity, SEntitySpawnParams& params) {}
	virtual void OnEvent(IEntity* pEntity, const SEntityEvent& event) {}
	virtual bool OnRemove(IEntity* pEntity) 
	{
		if (pEntity->GetFlags() & ENTITY_FLAG_PROCEDURAL)
			if (IPhysicalEntity *pent = pEntity->GetPhysics())
			{
				auto iter = g_mapSkels.find(gEnv->pPhysicalWorld->GetPhysicalEntityId(pent));
				if (iter!=g_mapSkels.end())
				{
					// during entity deletion, make sure the physics is reset to 0 to save it from deletion
					pEntity->AssignPhysicalEntity(0);
					g_mapSkels.erase(iter);
					if (g_mapSkels.empty())
						gEnv->pEntitySystem->RemoveSink(this);
				}
			}
		return true;
	}

	std::map<int,IEntity*> g_mapSkels;
};


struct STableLogger : IScriptTable
{
	STableLogger() {}
	virtual IScriptSystem* GetScriptSystem() const { return gEnv->pScriptSystem; }
	virtual void AddRef() {}
	virtual void  Release() {}
	virtual void  Delegate(IScriptTable* pObj) {}
	virtual void* GetUserDataValue() { return nullptr; }
	virtual void SetValueAny(const char* sKey, const ScriptAnyValue& any, bool bChain = false) {}
	virtual bool BeginSetGetChain() { return true; }
	virtual void EndSetGetChain() {}
	virtual ScriptVarType GetValueType(const char* sKey) { return svtNull; }
	virtual ScriptVarType GetAtType(int nIdx) { return svtNull; }
	virtual void SetAtAny(int nIndex, const ScriptAnyValue& any) {}
	virtual bool GetAtAny(int nIndex, ScriptAnyValue& any) { return false; }
	virtual IScriptTable::Iterator BeginIteration(bool resolvePrototypeTableAsWell = false) { return Iterator(); }
	virtual bool MoveNext(Iterator& iter) { return false; }
	virtual void EndIteration(const Iterator& iter) {}
	virtual void Clear() {}
	virtual int Count() { return 0; }
	virtual bool Clone(IScriptTable* pSrcTable, bool bDeepCopy = false, bool bCopyByReference = false) { return false; }
	virtual void Dump(IScriptTableDumpSink* p) {}
	virtual bool AddFunction(const SUserFunctionDesc& fd)  { return false; }

	std::vector<string> m_fields;
	virtual bool GetValueAny(const char* sKey, ScriptAnyValue& any, bool bChain = false) { m_fields.push_back(string(sKey)); return false; }
};


#define PP(name) ("Physics:Params:" name)
static const char *s_PhysParamNames[] = 
{	// correspond to PHYSICPARAM_... constants in ScriptBind_Entity.cpp, in order
	PP("Particle"), PP("Vehicle"), PP("PlayerDynamics"), PP("PlayerDimensions"), PP("Simulation"), PP("ArticulatedBody"), "", PP("Rope"), PP("Buoyancy"), PP("Constraint"), 
	PP("ConstraintRemove"), PP("Flags"), PP("Wheel"), PP("Cloth"), PP("Velocity"), PP("PartFlags"), "", PP("GroundPlane"), "", PP("AutoDetachment"), PP("CollisionClass")
};
#undef PP

class CFlowNode_PhysParams2 : public CFlowBaseNode<eNCT_Instanced>
{
public:
	CFlowNode_PhysParams2(int type, SActivationInfo *pActInfo, SInputPortConfig* inputs, string* params) : m_type(type), m_inputs(inputs), m_params(params)
	{
		m_outputs.push_back(OutputPortConfig_Void("Done"));
		m_outputs.push_back(SOutputPortConfig({ nullptr }));
	}
	IFlowNodePtr Clone(SActivationInfo* pActInfo)	{	return new CFlowNode_PhysParams2(m_type, pActInfo, m_inputs, m_params);	}
	virtual void GetMemoryUsage(ICrySizer* s) const	{	s->Add(*this); }

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.sDescription = _HELP("Sets physical parameters");
		config.pInputPorts = &m_inputs[0];
		config.pOutputPorts = &m_outputs[0];
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
	{
		if (event == eFE_Activate)
		{
			for(int i=1; m_inputs[i].name; m_activeMask |= (uint64)IsPortActive(pActInfo,i) << (i-1), i++)
				;
			if (IsPortActive(pActInfo, 0) && pActInfo->pEntity)
			{
				static IScriptTable* params = nullptr;
				if (!params)
					(params = gEnv->pScriptSystem->CreateTable())->AddRef();
				params->Clear();
				for(int i=1; m_inputs[i].name; i++)
					if (m_activeMask & 1ull << (i-1))
						switch (GetPortType(pActInfo, i))
						{
							case eFDT_Int    : params->SetValue(m_params[i-1], GetPortInt(pActInfo, i)); break;
							case eFDT_Float  : params->SetValue(m_params[i-1], GetPortFloat(pActInfo, i)); break;
							case eFDT_Vec3   : params->SetValue(m_params[i-1], GetPortVec3(pActInfo, i)); break;
							case eFDT_Bool   : params->SetValue(m_params[i-1], GetPortBool(pActInfo, i)); break;
							case eFDT_String : params->SetValue(m_params[i-1], GetPortString(pActInfo, i).c_str()); break;
						}
				IEntityScriptComponent *pScript0 = pActInfo->pEntity->GetComponent<IEntityScriptComponent>(), *pScript = pScript0;
				if (!pScript)
					pScript = pActInfo->pEntity->CreateComponent<IEntityScriptComponent>();
				pScript->SetPhysParams(m_type+1, params);
				if (!pScript0)
					pActInfo->pEntity->RemoveComponent(pScript);

				ActivateOutput(pActInfo, 0, 0);
			}
		}
	}

	int          m_type;
	uint64       m_activeMask = 0;

	SInputPortConfig*              m_inputs;
	std::vector<SOutputPortConfig> m_outputs;
	string*                        m_params;
};

class CAutoRegParamsNode : public CAutoRegFlowNodeBase
{
public:
	static CAutoRegParamsNode s_Params[CRY_ARRAY_COUNT(s_PhysParamNames)];
	int GetType() const { return (int)(this-s_Params); }

	CAutoRegParamsNode() : CAutoRegFlowNodeBase(s_PhysParamNames[GetType()]) 
	{
		if (!*m_szClassName)
			this->~CAutoRegParamsNode();
	}
	IFlowNodePtr Create(IFlowNode::SActivationInfo* pActInfo)	
	{ 
		int type = GetType();
		if (m_inputs.empty())
		{
			// Horrible workaround for existing code relying on calling code in init that depends on an entity existing.
			static std::shared_ptr<IEntityScriptComponent> pScriptComponentDummy;
			if (pScriptComponentDummy == nullptr)
			{
				CryCreateClassInstanceForInterface(cryiidof<IEntityScriptComponent>(), pScriptComponentDummy);
			}

			pScriptComponentDummy->SetPhysParams(type+1, &m_table);

			m_inputs.push_back(InputPortConfig_Void("Set"));
			for(const auto& str : m_table.m_fields)
			{
				stack_string buf = str;
				char *dst = (char*)buf.data();
				bool caps = str[0] != 'k';
				for(const char *src=str.c_str(); *src; src++)
				{
					if (*src != '_')
					{
						*dst++ = caps ? toupper(*src) : *src;
						caps = false;
					}
					else
						caps = true;
				}
				*dst = 0;
				m_inputNames.push_back(buf.c_str());
				const char *subs;
				SInputPortConfig input = InputPortConfig<float>(m_inputNames.back());
				if (str[0]=='b' && isupper(str[1]) || !strncmp(str,"is_",3))
				{
					input.defaultData = TFlowInputData::CreateDefaultInitialized<bool>(true);
					input.name += 1 + (str[0] == 'i');
				}
				else if (strstr(str, "Class") || strstr(str, "id") || strstr(str, "mask") || strstr(str, "flags") || strstr(str, "index") || strstr(str, "type") || strstr(str, "num"))
					input.defaultData = TFlowInputData::CreateDefaultInitialized<int>(true);
				else if (type != 3 && (
					strstr(str, "heading") || strstr(str, "normal") || strstr(str, "origin") || (subs = (char*)strstr(str, "end")) && subs[-1] != 'b' || 
					strstr(str, "pivot") || *(short*)&str[0] == 'v' || *(short*)&str[0] == 'w' || !strcmp(str, "wind") ||
					(subs = strstr(str, "gravity")) && subs[7] != 'x' && subs[7] != 'y' && subs[7] != 'z'))
					input.defaultData = TFlowInputData::CreateDefaultInitialized<Vec3>(true);
				else if (strstr(str, "name"))
					input.defaultData = TFlowInputData::CreateDefaultInitialized<string>(true);
				m_inputs.push_back(input);
			}
			m_inputs.push_back(SInputPortConfig({ nullptr }));
		}

		return new CFlowNode_PhysParams2(GetType(), pActInfo, m_inputs.data(), m_table.m_fields.data()); 
	}

	STableLogger                  m_table;
	std::vector<SInputPortConfig> m_inputs;
	std::vector<string>           m_inputNames;
};

CAutoRegParamsNode CAutoRegParamsNode::s_Params[CRY_ARRAY_COUNT(s_PhysParamNames)];

REGISTER_FLOW_NODE("Physics:Dynamics", CFlowNode_Dynamics);
REGISTER_FLOW_NODE("Physics:ActionImpulse", CFlowNode_ActionImpulse);
REGISTER_FLOW_NODE("Physics:PartIdConversion", CFlowNode_PartIdConversion);
REGISTER_FLOW_NODE("Physics:ObjTypeSelection", CFlowNode_ObjTypeSelection);
REGISTER_FLOW_NODE("Physics:RayCast", CFlowNode_Raycast);
REGISTER_FLOW_NODE("Physics:RayCastCamera", CFlowNode_RaycastCamera);
REGISTER_FLOW_NODE("Physics:PhysicsEnable", CFlowNode_PhysicsEnable);
REGISTER_FLOW_NODE("Physics:PhysicsSleepQuery", CFlowNode_PhysicsSleepQuery);
REGISTER_FLOW_NODE("Physics:Constraint", CFlowNode_Constraint);
REGISTER_FLOW_NODE("Physics:CameraProxy", CFlowNode_CameraProxy);
REGISTER_FLOW_NODE("Physics:CollisionListener", CFlowNode_Collision);
REGISTER_FLOW_NODE("Physics:GetPhysId", CFlowNode_PhysId);
REGISTER_FLOW_NODE("Physics:Awake", CFlowNode_Awake);
REGISTER_FLOW_NODE("Physics:SetParams", CFlowNode_PhysParams);
REGISTER_FLOW_NODE("Physics:GetSkeleton", CFlowNode_CharSkel);
