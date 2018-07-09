// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "BehaviorTreeNodes_Game.h"
#include <CryAISystem/BehaviorTree/IBehaviorTree.h>
#include <CryAISystem/BehaviorTree/Action.h>
#include <CryAISystem/BehaviorTree/Decorator.h>
#include "../Agent.h"
#include "../AISquadManager.h"
#include "../GameAISystem.h"
#include "GameRules.h"
#include "ActorImpulseHandler.h"
#include "Actor.h"
#include "IAnimatedCharacter.h"
#include <CrySystem/XML/XMLAttrReader.h>
#include "PlayerAnimation.h"
#include "GamePhysicsSettings.h"

#ifdef _DEBUG
# define INCLUDE_KEEP_TARGET_AT_A_DISTANCE_DEBUG_CODE
# define INCLUDE_MELEE_DEBUG_CODE
#endif

namespace
{
	IEntity* GetEntity(const BehaviorTree::UpdateContext& updateContext)
	{
		return gEnv->pEntitySystem->GetEntity(updateContext.entityId);
	}

	IAIActor* GetIAIActor(const BehaviorTree::UpdateContext& updateContext)
	{
		if (IEntity* entity = GetEntity(updateContext))
		{
			if (IAIObject* ai = entity->GetAI())
				return ai->CastToIAIActor();
		}

		return NULL;
	}
}

namespace BehaviorTree
{
	class HoldFormation : public Decorator
	{
		typedef Decorator BaseClass;

	public:
		struct RuntimeData
		{
			bool formationSet;

			RuntimeData() : formationSet(false) {}
		};
		
		HoldFormation() 
			: m_formationNameCRC32(0)
		{

		}

		virtual void OnInitialize(const UpdateContext& context)
		{
			BaseClass::OnInitialize(context);

			RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

			runtimeData.formationSet = false;
			if (SetFormation(context))
			{
				runtimeData.formationSet = true;
			}
		}

		virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context)
		{
			const char* formationName = xml->getAttr("name");
			if (!formationName)
			{
				gEnv->pLog->LogError("Missing 'name' attribute for Formation behavior tree node, at line %d.", xml->getLine());
				return LoadFailure;
			}

			m_formationNameCRC32 = CCrc32::ComputeLowercase(formationName);

			return LoadChildFromXml(xml, context);
		}

		virtual Status Update(const UpdateContext& context)
		{
			RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);
			
			if (runtimeData.formationSet)
			{
				Status childStatus = m_child->Tick(context);
				return (childStatus != Failure) ? Running : Failure;
			}
			else
			{
				return Failure;
			}
		}

		virtual void OnTerminate(const UpdateContext& context)
		{
			BaseClass::OnTerminate(context);

			RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);
			
			if (runtimeData.formationSet)
			{
				ReleaseFormation(context);
				runtimeData.formationSet = false;
			}
		}

	private:

		bool SetFormation(const UpdateContext& context)
		{
			IEntity* pEntity = gEnv->pEntitySystem->GetEntity(context.entityId);
			if (!pEntity)
				return false;

			IAIObject* pAIObject = pEntity->GetAI();
			if (!pAIObject)
				return false;

			return pAIObject->CreateFormation(m_formationNameCRC32,Vec3(ZERO));
		}

		void ReleaseFormation(const UpdateContext& context)
		{
			IEntity* pEntity = gEnv->pEntitySystem->GetEntity(context.entityId);
			if (!pEntity)
				return;

			IAIObject* pIAIObject = pEntity->GetAI();
			if (!pIAIObject)
				return;

			pIAIObject->ReleaseFormation();
		}

		unsigned int m_formationNameCRC32;
	};

	//////////////////////////////////////////////////////////////////////////

	class JoinFormation : public Action
	{
	public:
		struct RuntimeData
		{
			bool isInFormation;

			RuntimeData() : isInFormation(false) {}
		};
		
		virtual void OnInitialize(const UpdateContext& context)
		{
			RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);
			
			runtimeData.isInFormation = false;
			if (JoinFormationInSquad(context))
			{
				runtimeData.isInFormation = true;
			}
		}

		virtual Status Update(const UpdateContext& context)
		{
			RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);
			
			if (runtimeData.isInFormation)
			{
				return Running;
			}
			else
			{
				return Failure;
			}
		}

		virtual void OnTerminate(const UpdateContext& context)
		{
			Agent agent(context.entityId);
			if( agent.IsValid() )
			{
				IPipeUser* pipeUser = CastToIPipeUserSafe(agent.GetAIObject());
				pipeUser->SelectPipe(AIGOALPIPE_LOOP, "_first_");
			}
		}

	private:

		bool JoinFormationInSquad(const UpdateContext& context)
		{
			IEntity* pEntity = gEnv->pEntitySystem->GetEntity(context.entityId);
			if (!pEntity)
				return false;

			IAIObject* pAIObject = pEntity->GetAI();
			if (!pAIObject)
				return false;

			IEntity* pEntityToFollow = g_pGame->GetGameAISystem()->GetAISquadManager().GetFormationLeaderForMySquad(context.entityId);
			IAIActor* pIAIActor = CastToIAIActorSafe(pAIObject);
			if (pIAIActor)
			{
				SendSignal(pIAIActor, "ACT_JOIN_FORMATION",pEntityToFollow);
				return true;
			}

			return false;
		}

		void SendSignal(IAIActor* pIAIActor, const char* signalName, IEntity* pSender)
		{
			assert(pIAIActor);
			IAISignalExtraData*	pData = gEnv->pAISystem->CreateSignalExtraData();
			const int goalPipeId = gEnv->pAISystem->AllocGoalPipeId();
			pData->iValue = goalPipeId;
			pIAIActor->SetSignal( 10, signalName, pSender, pData );
		}
	};

	//////////////////////////////////////////////////////////////////////////

#define MELEE_FRAGMENT_NAME "melee"

	struct IMeleeActionListener
	{
		virtual ~IMeleeActionListener() {}
		virtual void OnMeleeHitAnimationEventReceived() = 0;
	};

	class CAnimActionMelee : public TPlayerAction
	{
		typedef TPlayerAction TBaseAction;
	public:
		DEFINE_ACTION("Melee")

		CAnimActionMelee(FragmentID fragmentID, IMeleeActionListener& pListener)
			:TBaseAction(PP_PlayerActionUrgent, fragmentID, TAG_STATE_EMPTY)
			,m_previousColliderMode(eColliderMode_Undefined)
			,m_pListener(&pListener)
		{
		}

		virtual void OnAnimationEvent(ICharacterInstance *pCharacter, const AnimEventInstance &event) override
		{
			assert(GetStatus() == Installed);

			const static uint32 meleeHitLowercaseCRC32 = CCrc32::ComputeLowercase("MeleeHit");

			if (event.m_EventNameLowercaseCRC32 == meleeHitLowercaseCRC32)
			{
				m_pListener->OnMeleeHitAnimationEventReceived();
			}
		}

		void RemoveListener(IMeleeActionListener* pListener)
		{
			if (m_pListener == pListener)
			{
				m_pListener = NULL;
			}
		}

	protected:
		virtual void Enter() override
		{
			TBaseAction::Enter();
			IAnimatedCharacter* pAnimatedCharacter = GetRootAnimatedCharacter();
			assert(pAnimatedCharacter);
			if(pAnimatedCharacter)
			{
				SetEntityControlledRotationAndAnimationControlledTranslation(*pAnimatedCharacter);
			}
		}

		virtual void Exit() override
		{
			IAnimatedCharacter* pAnimatedCharacter = GetRootAnimatedCharacter();
			assert(pAnimatedCharacter);
			if(pAnimatedCharacter)
			{
				SetEntityControlledRotationAndTranslation(*pAnimatedCharacter);
			}
			TBaseAction::Exit();
		}

	private:
		IAnimatedCharacter* GetRootAnimatedCharacter() const
		{
			const EntityId entityId = GetRootScope().GetEntityId();
			IActor* pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(entityId);
			if (pActor)
			{
				return pActor->GetAnimatedCharacter();
			}
			return NULL;
		}

		void SetEntityControlledRotationAndAnimationControlledTranslation(IAnimatedCharacter& animatedCharacter)
		{
			SetMovementControlMethods(animatedCharacter, eMCM_Entity, eMCM_Entity);
			UseAnimationMovementForEntity(animatedCharacter, true, false, false);
		}

		void SetEntityControlledRotationAndTranslation(IAnimatedCharacter& animatedCharacter)
		{
			SetMovementControlMethods(animatedCharacter, eMCM_Entity, eMCM_Entity);
			UseAnimationMovementForEntity(animatedCharacter, false, false, false);
		}

		void SetMovementControlMethods(IAnimatedCharacter& animatedCharacter, const EMovementControlMethod horizontalMcm, const EMovementControlMethod verticalMcm)
		{
			animatedCharacter.SetMovementControlMethods(horizontalMcm, verticalMcm);
		}

		void UseAnimationMovementForEntity(IAnimatedCharacter& animatedCharacter, bool xyMove, bool zMove, bool rotation)
		{
			animatedCharacter.UseAnimationMovementForEntity(xyMove, zMove, rotation);
		}

		EColliderMode         m_previousColliderMode;
		IMeleeActionListener* m_pListener;
	};

	class Melee : public Action
	{
	public:
		struct RuntimeData : public IMeleeActionListener
		{
			typedef _smart_ptr<CAnimActionMelee> CMeleeActionPtr;
			CMeleeActionPtr meleeAction;
			Vec3 meleeDirection;
			EntityId entityID;
			float cylinderRadius;
			int hitTypeID;
			CryFixedStringT<64> materialEffectName;

			RuntimeData()
				: meleeDirection(1.0f, 0.0f, 0.0f)
				, entityID(0)
				, cylinderRadius(1.0f)
				, hitTypeID(0)
				, materialEffectName("melee")
			{
			}

			~RuntimeData()
			{
				ReleaseAction();
			}

			void ReleaseAction()
			{
				if (this->meleeAction.get())
				{
					this->meleeAction->RemoveListener(this);
					this->meleeAction->ForceFinish();
					this->meleeAction.reset();
				}
			}

			void StartHitTest(EntityId pAttackerEntityId)
			{
				assert(pAttackerEntityId);
				CActor* actorAttacker = (CActor*)gEnv->pGameFramework->GetIActorSystem()->GetActor(pAttackerEntityId);
				if (IMovementController * pMC = actorAttacker->GetMovementController())
				{
					Agent agent(pAttackerEntityId);
					const float hitRange = agent.IsValid() ? agent.GetAIActor()->GetParameters().m_fMeleeHitRange : 2.0f;
					SMovementState info;
					pMC->GetMovementState(info);

					this->meleeDirection = info.entityDirection;
					PerformCylinderTest(info.weaponPosition, this->meleeDirection, hitRange);
				}
			}

			void ApplyDamagesAndImpulse(IPhysicalEntity* pCollider, const Vec3& pos, const Vec3& normal, const int surfaceIdx, 
				const int partId, const int iPart)
			{
				float damages = 100.0f;
				float impulseScale = 600.0f;
				float knockdownChance = .0f;
				const EntityId attackerId = this->entityID;
				Agent agent(attackerId);
				if (agent.IsValid())
				{
					const AgentParameters& parameters = agent.GetAIActor()->GetParameters();
					damages = parameters.m_fMeleeDamage;
					impulseScale = parameters.m_fMeleeImpulse;
					knockdownChance = parameters.m_fMeleeKnowckdownChance;
				}
				PlayHitMaterialEffect(pos, normal, surfaceIdx);
				ApplyDamage(attackerId, pCollider, pos, normal, surfaceIdx, iPart, damages);
				ApplyImpulse(attackerId, pCollider, pos, partId, iPart, impulseScale, knockdownChance);
			}

			void PlayHitMaterialEffect(const Vec3& position, const Vec3& normal, const int surfaceIdx) const
			{
				IMaterialEffects* materialEffects = gEnv->pGameFramework->GetIMaterialEffects();
				TMFXEffectId effectId = materialEffects->GetEffectId(this->materialEffectName.c_str(), surfaceIdx);
				if (effectId != InvalidEffectId)
				{
					SMFXRunTimeEffectParams params;
					params.pos = position;
					params.normal = normal;
					params.playflags = eMFXPF_All | eMFXPF_Disable_Delay;
					//params.soundSemantic = eSoundSemantic_Player_Foley;
					materialEffects->ExecuteEffect(effectId, params);
				}
			}

			void ApplyDamage(const EntityId attackerId, IPhysicalEntity* pCollider, const Vec3& pos, const Vec3& normal, const int surfaceIdx, const int iPart, const float damages)
			{
				CGameRules *pGameRules = g_pGame->GetGameRules();
				CRY_ASSERT_MESSAGE(pGameRules, "No game rules! Melee can not apply hit damage");
				if (pGameRules)
				{
					float damageScale = 1.0f;

					IEntity* pCollidedEntity = gEnv->pEntitySystem->GetEntityFromPhysics(pCollider);
					EntityId collidedEntityId = pCollidedEntity ? pCollidedEntity->GetId() : 0;
					assert(collidedEntityId != attackerId);
					HitInfo info(attackerId , collidedEntityId, 0,
						damages * damageScale, 0.0f, surfaceIdx, iPart, this->hitTypeID, pos, this->meleeDirection, normal);
					pGameRules->ClientHit(info);
				}
			}

			void ApplyImpulse(const EntityId attackerId, IPhysicalEntity* pCollider, const Vec3& pos, const int partId, const int iPart, const float impulseScale, const float knockdownChance)
			{
				const float blend = 0.5f;
				Vec3 impulseDir = (1- blend) * this->meleeDirection + blend * Vec3(.0f, .0f, 1.0f);
				impulseDir.Normalize();
				Vec3 impulse = impulseDir * impulseScale;
				bool hasBeenKnockedDown = false;

				if (knockdownChance > .0f && (cry_random(0.0f, 1.0f) < knockdownChance))
				{
					if (IEntity* pCollidedEntity = gEnv->pEntitySystem->GetEntityFromPhysics(pCollider))
					{
						CActor* pActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pCollidedEntity->GetId()));
						if (pActor)
						{
							pActor->KnockDown(impulseScale);
							hasBeenKnockedDown = true;

							SendSignal(attackerId, "OnMeleeKnockedDownTarget");
						}
					}
				}

				if (!hasBeenKnockedDown)
				{
					if (pCollider)
					{
						IEntity* pEntity = static_cast<IEntity*>(pCollider->GetForeignData(PHYS_FOREIGN_ID_ENTITY));

						if (pEntity)
						{
							CActor* pActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pEntity->GetId()));
							if (pActor)
							{
								pActor->AddLocalHitImpulse(SHitImpulse(partId, -1, pos, impulse, 1.0f));
							}

							{
								pEntity->AddImpulse(partId, pos, impulse, true, 1.0f, 1.0f);
							}
						}
						else
						{
							pe_action_impulse ai;
							ai.partid = partId;
							ai.point = pos;
							ai.impulse = impulse;
							pCollider->Action(&ai);
						}
					}
				}
			}

			void SendSignal(EntityId entityId, const char* signalName)
			{
				Agent agent(entityId);
				if(agent.IsValid())
				{
					IAISignalExtraData*	pData = gEnv->pAISystem->CreateSignalExtraData();
					agent.GetAIActor()->SetSignal( 10, signalName, agent.GetEntity(), NULL );
				}
			}

			bool PerformCylinderTest(const Vec3 &pos, const Vec3 &dir, const float hitRange)
			{
				if (!this->meleeAction.get())
					return false;

				IEntity* pAttacker = gEnv->pEntitySystem->GetEntity(this->entityID);
				assert( pAttacker );
				if( !pAttacker )
					return false;

				IPhysicalEntity* pIgnore = pAttacker->GetPhysics();

				IAIObject* pAttackerAIObject = pAttacker->GetAI();
				assert( pAttackerAIObject	);
				if( !pAttackerAIObject )
					return false;

				primitives::cylinder cylinder;
				cylinder.r = this->cylinderRadius;
				cylinder.axis = dir;
				cylinder.hh = hitRange * 0.5f;
				cylinder.center = pos + dir.normalized()*cylinder.hh;

#ifdef INCLUDE_MELEE_DEBUG_CODE
				static const bool s_debug = false;
				if(s_debug)
				{
					IPersistantDebug* persistentDebug = g_pGame->GetIGameFramework()->GetIPersistantDebug();
					persistentDebug->Begin("Melee", false);
					persistentDebug->AddCylinder(cylinder.center, cylinder.axis,
						cylinder.r, cylinder.hh, Col_Red, 1.0f);
				}
#endif

				geom_contact *contacts;
				intersection_params params;
				params.bStopAtFirstTri = false;
				params.bNoBorder = true;
				params.bNoAreaContacts = true;
				float fIntersectionsNumber = gEnv->pPhysicalWorld->PrimitiveWorldIntersection(primitives::cylinder::type, &cylinder, Vec3(ZERO), 
					ent_living|ent_rigid, &contacts, 0, geom_colltype_player, &params, 0, 0, &pIgnore, pIgnore?1:0);

				int totalIntersections = (int)fIntersectionsNumber;

				float closestdSq = 9999.0f;
				geom_contact *closestContact = 0;
				geom_contact *currentContact = contacts;

				for (int i=0; i<totalIntersections; i++)
				{
					geom_contact *contact = currentContact;
					if (contact)
					{
						IPhysicalEntity *pCollider = gEnv->pPhysicalWorld->GetPhysicalEntityById(contact->iPrim[0]);
						if (pCollider)
						{
							IEntity *pEntity = gEnv->pEntitySystem->GetEntityFromPhysics(pCollider);
							if ( pEntity == NULL || pEntity == pAttacker || !pAttackerAIObject->IsHostile( pEntity->GetAI() ) )
							{
								++currentContact;
								continue;
							}

							const float distSq = (pos-currentContact->pt).len2();
							if (distSq < closestdSq)
							{
								closestdSq = distSq;
								closestContact = contact;
							}
						}
					}
					++currentContact;
				}

				if (closestContact)
				{
					IPhysicalEntity *pCollider = gEnv->pPhysicalWorld->GetPhysicalEntityById(closestContact->iPrim[0]);
					ApplyDamagesAndImpulse(pCollider, pos, -dir, closestContact->id[1], closestContact->iPrim[1], 0);
				}

				return closestContact != 0;
			}

			// Overriding IMeleeActionListener::OnMeleeHitAnimationEventReceived
			virtual void OnMeleeHitAnimationEventReceived() override
			{
				StartHitTest(this->entityID);
			}
		};
		
		Melee()
			: m_target(Unknown)
			, m_cylinderRadius(1.0f)
			, m_hitTypeID(0)
			, m_failIfTargetNotInNavigationMesh(false)
		{
		}
		// ------------------------------------------------------------------------------------------------------------------------

		virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context) override
		{
			s_dictionaries.target.Get(xml, "target", m_target, true);

			m_cylinderRadius = 0.3f;
			xml->getAttr("cylinderRadius", m_cylinderRadius);

			m_hitTypeID = CGameRules::EHitType::Melee;
			const stack_string hitTypeName = xml->getAttr("hitType");
			if (!hitTypeName.empty())
			{
				if (CGameRules* gameRules = g_pGame->GetGameRules())
				{
					m_hitTypeID = gameRules->GetHitTypeId(hitTypeName.c_str());
				}
			}

			xml->getAttr("failIfTargetNotInNavigationMesh", m_failIfTargetNotInNavigationMesh);

			const stack_string materialEffectName = xml->getAttr("materialEffect");
			if (!materialEffectName.empty())
			{
				m_materialEffectName = materialEffectName.c_str();
			}

			return LoadSuccess;
		}
		// ------------------------------------------------------------------------------------------------------------------------

	protected:
		virtual void OnInitialize(const UpdateContext& context) override
		{
			Agent agent(context.entityId);
			IF_UNLIKELY(!agent.IsValid())
				return;

			RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

			runtimeData.entityID = context.entityId;
			runtimeData.cylinderRadius = m_cylinderRadius;
			runtimeData.hitTypeID = m_hitTypeID;
			runtimeData.materialEffectName = m_materialEffectName;

			const AgentParameters& parameters = agent.GetAIActor()->GetParameters();
			if (CanStartMeleeAttack(context, parameters.m_fMeleeAngleCosineThreshold))
			{
				AlignToTarget(context);
				const static uint32 FRAGID_MELEE = CCrc32::ComputeLowercase( MELEE_FRAGMENT_NAME );
				QueueFragment(context.entityId, FRAGID_MELEE, runtimeData);
			}
		}
		// ------------------------------------------------------------------------------------------------------------------------

		virtual void OnTerminate(const UpdateContext& context) override
		{
			RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);
			
			ResetBodyTargetDir(context);
			runtimeData.ReleaseAction();
		}
		// ------------------------------------------------------------------------------------------------------------------------

		virtual Status Update(const UpdateContext& context) override
		{
			RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);
			
			if (!runtimeData.meleeAction.get())
				return Success;

			IEntity* pEntity = gEnv->pEntitySystem->GetEntity(context.entityId);
			IF_UNLIKELY(!pEntity)
				return Success;

			Agent agent(pEntity->GetAI());
			IF_UNLIKELY(!agent.IsValid())
				return Success;

			if(agent.GetActor()->IsFallen())
				return Success;

			IAction::EStatus currentActionStatus = runtimeData.meleeAction->GetStatus();
			if (currentActionStatus == IAction::None || currentActionStatus == IAction::Finished)
				return Success;

			AlignToTarget(context);

			return Running;
		}
		// ------------------------------------------------------------------------------------------------------------------------

	private:

		void QueueFragment(const EntityId entityId, const uint32 fragNameCrc32, RuntimeData& runtimeData)
		{
			Agent agent(entityId);
			if (agent.IsValid() && !runtimeData.meleeAction.get())
			{
				IActionController* pIActionController = agent.GetActor()->GetAnimatedCharacter()->GetActionController();
				const FragmentID fragID = pIActionController->GetFragID(fragNameCrc32);
				runtimeData.meleeAction = new CAnimActionMelee(fragID, runtimeData);
				pIActionController->Queue(*runtimeData.meleeAction);
			}
		}
		// ------------------------------------------------------------------------------------------------------------------------


		bool CanStartMeleeAttack(const UpdateContext& context, const float cosineThresholtToAllowMelee)
		{
			const Vec3 targetPosition = GetTargetPosition(context);
			const Agent agent(context.entityId);
			Vec3 dirToTarget = targetPosition - agent.GetPos();
			dirToTarget.z = .0f;
			dirToTarget.Normalize();

			// Check if target is in navigation mesh
			if (m_failIfTargetNotInNavigationMesh)
			{
				if (!gEnv->pAISystem->GetNavigationSystem()->IsLocationValidInNavigationMesh(agent.GetAIActor()->GetNavigationTypeID(), targetPosition, nullptr, 5.0f, 0.5f))
				{
					return false;
				}
			}

			Vec3 entityDir(ZERO);
			CActor* actorAttacker = (CActor*)gEnv->pGameFramework->GetIActorSystem()->GetActor(context.entityId);
			if (IMovementController * pMC = actorAttacker->GetMovementController())
			{
				SMovementState info;
				pMC->GetMovementState(info);
				entityDir = info.entityDirection;
				entityDir.Normalize();
			}

			const float cosine = dirToTarget.Dot(entityDir);
			if (cosine >= cosineThresholtToAllowMelee)
				return true;

			return false;
		}
		// ------------------------------------------------------------------------------------------------------------------------

		void AlignToTarget(const UpdateContext& context)
		{
			Agent agent(context.entityId);
			if (agent.IsValid())
			{
				IAIObject* pAttTarget = agent.GetAttentionTarget();
				if (pAttTarget)
				{
					Vec3 bodyDir = pAttTarget->GetPos() - agent.GetEntityPos();
					if (bodyDir.GetLengthSquared2D() < 0.1)
						return;

					bodyDir.z = 0.0f;
					if (bodyDir.NormalizeSafe() <= 0.0f)
					{
						return;
					}

					agent.GetAIActor()->SetBodyTargetDir(bodyDir);
				}
			}
		}
		// ------------------------------------------------------------------------------------------------------------------------

		void ResetBodyTargetDir(const UpdateContext& context)
		{
			Agent agent(context.entityId);
			if (agent.IsValid())
			{
				agent.GetAIActor()->ResetBodyTargetDir();
			}
		}
		
		// ------------------------------------------------------------------------------------------------------------------------

		const Vec3 GetTargetPosition(const UpdateContext& context)
		{
			Agent agent(context.entityId);
			switch(m_target)
			{
			case AttentionTarget:
				if (agent.IsValid())
				{
					IAIObject* pIAIObject = agent.GetAttentionTarget();
					if (pIAIObject)
						return pIAIObject->GetPos();
				}
				break;
			case RefPoint:
				if (agent.IsValid())
				{
					IPipeUser* pipeUser = CastToIPipeUserSafe(agent.GetAIObject());
					return pipeUser->GetRefPoint()->GetPos();
				}
				break;

			default:
				assert(0);
				break;
			}

			return Vec3(ZERO);
		}
		// ------------------------------------------------------------------------------------------------------------------------

		// Dictionary -------------------------------------------------------------
		enum TargetType
		{
			AttentionTarget,
			RefPoint,
			Unknown,
		};

		struct Dictionaries
		{
			CXMLAttrReader<TargetType> target;

			Dictionaries()
			{
				target.Reserve(2);
				target.Add("AttentionTarget", AttentionTarget);
				target.Add("RefPoint", RefPoint);
			}
		};
		static Dictionaries s_dictionaries;
		// ------------------------------------------------------------------------

		TargetType            m_target;
		float                 m_cylinderRadius;
		int                   m_hitTypeID;
		bool                  m_failIfTargetNotInNavigationMesh;
		CryFixedStringT<64>   m_materialEffectName;
	};

	Melee::Dictionaries Melee::s_dictionaries;

	//////////////////////////////////////////////////////////////////////////

	// Keeps the live target at a distance by physically pushing the target
	// away if it is within the defined minimum distance.
	// This is useful when there's some sort of action close to the player
	// for instance and you want to avoid clipping through the camera.
	// Prefer this instead of increasing the capsule size since that
	// will affect how the character can fit through tight passages.
	class KeepTargetAtADistance : public Action
	{
		typedef Action BaseClass;
	
	public:
		struct RuntimeData
		{
		};
		
		KeepTargetAtADistance()
			: m_distance(1.0f)
			, m_impulsePower(1.0f)
		{
		}

		virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context) override
		{
			IF_UNLIKELY (BaseClass::LoadFromXml(xml, context) == LoadFailure)
				return LoadFailure;

			if (!xml->getAttr("distance", m_distance))
			{
				gEnv->pLog->LogError("KeepTargetAtADistance: Missing attribute 'distance' on line %d.", xml->getLine());
				return LoadFailure;
			}

			if (!xml->getAttr("impulsePower", m_impulsePower))
			{
				gEnv->pLog->LogError("KeepTargetAtADistance: Missing attribute 'impulsePower' on line %d.", xml->getLine());
				return LoadFailure;
			}

			return LoadSuccess;
		}
	
	protected:	
		virtual Status Update(const UpdateContext& context) override
		{
			Agent agent(context.entityId);
			assert(agent.IsValid());
			IF_UNLIKELY (!agent.IsValid())
				return Running;

			Vec3 agentVel(ZERO);
			if (IEntity* agentEntity = agent.GetEntity())
			{
				if (IPhysicalEntity* agentPhysicalEntity = agentEntity->GetPhysics())
				{
					pe_status_dynamics dynamicsStatus;
					agentPhysicalEntity->GetStatus(&dynamicsStatus);
					agentVel = dynamicsStatus.v;
				}
			}

			IAIObject* targetAI = agent.GetLiveTarget();
			IF_UNLIKELY (!targetAI)
				return Running;

			IEntity* targetEntity = targetAI->GetEntity();
			IF_UNLIKELY (!targetEntity)
				return Running;

			const EntityId playerEntityID = g_pGame->GetIGameFramework()->GetClientActorId();

			const EntityId targetEntityID = targetEntity->GetId();
			const bool objectIsPlayer = (targetEntityID == playerEntityID);
			const bool objectIsActor = gEnv->pGameFramework->GetIActorSystem()->GetActor(targetEntityID) != NULL;

			IF_UNLIKELY (!objectIsActor || !objectIsPlayer)
				return Running;

#ifdef INCLUDE_KEEP_TARGET_AT_A_DISTANCE_DEBUG_CODE
			IPersistantDebug* persistentDebug = g_pGame->GetIGameFramework()->GetIPersistantDebug();
			static bool debug = false;
#endif


			{
				const Vec3 agentPos = agent.GetEntityPos();

				const Vec3 targetPos = targetEntity->GetWorldPos();
				Vec3 agentToTargetDirXY = targetPos - agentPos;
				agentToTargetDirXY.z = 0.0f;
				agentToTargetDirXY.Normalize();

				IPhysicalEntity* physicalEntity = targetEntity->GetPhysicalEntity();
				IF_UNLIKELY (!physicalEntity)
					return Running;

				pe_status_dynamics dynamicsStatus;
				physicalEntity->GetStatus(&dynamicsStatus);

				const Vec3& targetVel = dynamicsStatus.v;
				const Vec3 targetRelVel = targetVel - agentVel;
				const bool targetMovingAwayFromAgent = targetRelVel.GetLengthSquared() > square(2.8f) && (targetRelVel.Dot(agentToTargetDirXY) > 0.0f);
				if (targetMovingAwayFromAgent)
					return Running;

#ifdef INCLUDE_KEEP_TARGET_AT_A_DISTANCE_DEBUG_CODE
				if (debug)
					persistentDebug->Begin("KeepTargetAtADistance", false);
#endif

				if (agentPos.GetSquaredDistance(targetPos) < square(m_distance))
				{
#ifdef INCLUDE_KEEP_TARGET_AT_A_DISTANCE_DEBUG_CODE
					if (debug)
						persistentDebug->AddSphere(agentPos, m_distance, Col_Red, 3.0f);
#endif

					Vec3 impulseDir;
					if (objectIsPlayer)
						impulseDir = (agentToTargetDirXY * 0.8f) + (Vec3Constants<float>::fVec3_OneZ * 0.2f);
					else
						impulseDir = (agentToTargetDirXY * 0.5f) + (Vec3Constants<float>::fVec3_OneZ * 0.5f);
					impulseDir.Normalize();

					const float mass = dynamicsStatus.mass;
					const Vec3 impulse = impulseDir * mass * m_impulsePower;
					targetEntity->AddImpulse(-1, targetPos - agentToTargetDirXY, impulse, true, 1.0f);
				}
#ifdef INCLUDE_KEEP_TARGET_AT_A_DISTANCE_DEBUG_CODE
				else if (debug)
				{
					persistentDebug->AddSphere(agentPos, m_distance, Col_Green, 0.1f);
				}
#endif
			}

			return Running;
		}
	
	private:
		float m_distance;
		float m_impulsePower;
	};
	
	//////////////////////////////////////////////////////////////////////////

	class SuppressHitReactions : public Decorator
	{
	public:
		struct RuntimeData
		{
			EntityId entityId;
			bool enableHitReactionIfDestructed;

			RuntimeData()
				: entityId(0)
				, enableHitReactionIfDestructed(false)
			{
			}

			~RuntimeData()
			{
				if (this->enableHitReactionIfDestructed)
				{
					EnableHitReactions(this->entityId);
				}
			}

			void EnableHitReactions(const EntityId _entityId)
			{
				CActor* actor = (CActor*)gEnv->pGameFramework->GetIActorSystem()->GetActor(_entityId);
				if (actor)
					actor->EnableHitReactions();

				this->enableHitReactionIfDestructed = false;
				this->entityId = 0;
			}

			void DisableHitReactions(const EntityId _entityId)
			{
				this->enableHitReactionIfDestructed = true;
				this->entityId = _entityId;

				CActor* actor = (CActor*)gEnv->pGameFramework->GetIActorSystem()->GetActor(_entityId);
				if (actor)
					actor->DisableHitReactions();
			}
		};
		
		virtual void OnInitialize(const UpdateContext& context)
		{
			RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);
			runtimeData.DisableHitReactions(context.entityId);
			Decorator::OnInitialize(context);
		}

		virtual void OnTerminate(const UpdateContext& context)
		{
			RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);
			runtimeData.EnableHitReactions(context.entityId);
			Decorator::OnTerminate(context);
		}
	};

	//////////////////////////////////////////////////////////////////////////

	// This node utilizes a trick in the physics system to inflate the
	// capsule of the agent such that it has one radius for collisions with
	// the player, and one radius for collisions with the world. The trick
	// is entirely isolated within this node.
	//
	// Warning! This node does not properly clean up after itself, so the
	// capsule will remain inflated after it has been used.
	//
	// STEP 1
	// The idea of the trick is to set the player dimensions up with the
	// agent-vs-player collision radius.
	// Let it simmer for a while; the physics system is multi-threaded
	// so we have to wait until the player dimensions have been committed.
	//
	// STEP 2
	// Every now and then we inspect the player dimensions to see if our
	// agent-vs-player collision radius has been committed successfully.
	// It could be that the agent was in a tight spot and couldn't inflate
	// and will therefore sometimes end up unchanged.
	//
	// STEP 3
	// Once the agent-vs-player collision radius has been committed
	// successfully we then go into the geometry and set the capsule's
	// radius in place, using the agent-vs-world radius.
	// This will not update the player dimensions - which is used for
	// collision between living entities. Voila!
	class InflateAgentCollisionRadiusUsingPhysicsTrick : public Action
	{
		typedef Action BaseClass;

	public:
		struct RuntimeData
		{
			bool dimensionsHaveBeenQueued;

			RuntimeData()
				: dimensionsHaveBeenQueued(false)
			{
			}
		};

		InflateAgentCollisionRadiusUsingPhysicsTrick()
			: m_radiusForAgentVsPlayer(2.0f)
			, m_radiusForAgentVsWorld(0.5f)
		{
		}

		virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context) override
		{
			IF_UNLIKELY (BaseClass::LoadFromXml(xml, context) == LoadFailure)
				return LoadFailure;

			xml->getAttr("radiusForAgentVsPlayer", m_radiusForAgentVsPlayer);
			xml->getAttr("radiusForAgentVsWorld", m_radiusForAgentVsWorld);

			return LoadSuccess;
		}

	protected:
		virtual Status Update(const UpdateContext& context) override
		{
			// Please read the description above for an overview of what the
			// code below is trying to accomplish.

			CRY_PROFILE_FUNCTION(PROFILE_AI);

			CActor* actor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(context.entityId));
			IF_UNLIKELY (!actor)
				return Running;

			IAIActor* aiActor = GetIAIActor(context);
			IF_UNLIKELY (!aiActor)
				return Running;

			const EStance visualStance = actor->GetStance();
			const EStance requestedStance = static_cast<EStance>(aiActor->GetState().bodystate);
			if (visualStance != requestedStance)
				return Running;

			RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

			if (!runtimeData.dimensionsHaveBeenQueued)
			{
				if (IEntity* entity = GetEntity(context))
				{
					if (IPhysicalEntity* pPhysEnt = entity->GetPhysics())
					{
						pe_player_dimensions playdim;
						if (pPhysEnt->GetParams(&playdim))
						{
							playdim.sizeCollider.x = m_radiusForAgentVsPlayer;
							playdim.sizeCollider.y = m_radiusForAgentVsPlayer;

							// The player dimension update might be queued
							// so don't expect the dimensions to be set right away.
							if (pPhysEnt->SetParams(&playdim))
							{
								runtimeData.dimensionsHaveBeenQueued = true;
							}
						}
					}
				}
			}

			{
				if (IEntity* entity = GetEntity(context))
				{
					if (IPhysicalEntity* pPhysEnt = entity->GetPhysics())
					{
						// When GetParams(pe_player_dimensions) returns non-zero
						// it means the set dimensions from above have gone through
						// and we can securely update the capsule.
						pe_player_dimensions playdim;
						const bool playerDimensionsHaveBeenCommitted = pPhysEnt->GetParams(&playdim) != 0;
						if (playerDimensionsHaveBeenCommitted)
						{
							// Assure the dimensions have been correctly set.
							// It could be that the dimensions could not be set because
							// the living entity was in a tight spot.
							const bool correctDimensionsWereSet = fabsf(playdim.sizeCollider.x - m_radiusForAgentVsPlayer) < 0.01f && fabsf(playdim.sizeCollider.y - m_radiusForAgentVsPlayer) < 0.01f;
							if (correctDimensionsWereSet)
							{
								pe_params_part test;
								test.ipart = 0;
								if (pPhysEnt->GetParams(&test))
								{
									if (test.pPhysGeom && test.pPhysGeom->pGeom)
									{
										IGeometry* pGeom = test.pPhysGeom->pGeom;
										if (pGeom->GetType() == primitives::capsule::type)
										{
											primitives::capsule caps = *(primitives::capsule*)pGeom->GetData();
											caps.r = m_radiusForAgentVsWorld;
											pGeom->SetData(&caps);
										}
										else if (pGeom->GetType() == primitives::cylinder::type)
										{
											primitives::cylinder cyl = *(primitives::cylinder*)pGeom->GetData();
											cyl.r = m_radiusForAgentVsWorld;
											pGeom->SetData(&cyl);
										}
									}
								}
							}
							else
							{
								// Wrong dimensions were set, rinse and repeat.
								runtimeData.dimensionsHaveBeenQueued = false;
							}
						}
					}
				}
			}

			return Running;
		}

	private:
		float m_radiusForAgentVsPlayer;
		float m_radiusForAgentVsWorld;
	};

}



void RegisterGameBehaviorTreeNodes()
{
	using namespace BehaviorTree;

	assert( gEnv->pAISystem->GetIBehaviorTreeManager() );

	IBehaviorTreeManager& manager = *gEnv->pAISystem->GetIBehaviorTreeManager();

	REGISTER_BEHAVIOR_TREE_NODE(manager, HoldFormation);
	REGISTER_BEHAVIOR_TREE_NODE(manager, JoinFormation);
	REGISTER_BEHAVIOR_TREE_NODE(manager, Melee);
	REGISTER_BEHAVIOR_TREE_NODE(manager, KeepTargetAtADistance);
	REGISTER_BEHAVIOR_TREE_NODE(manager, SuppressHitReactions);
	REGISTER_BEHAVIOR_TREE_NODE(manager, InflateAgentCollisionRadiusUsingPhysicsTrick);
}
