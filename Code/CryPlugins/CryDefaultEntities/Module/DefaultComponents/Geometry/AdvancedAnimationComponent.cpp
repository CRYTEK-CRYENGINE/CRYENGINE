#include "StdAfx.h"
#include "AdvancedAnimationComponent.h"

#include <Cry3DEngine/IRenderNode.h>

namespace Cry
{
	namespace DefaultComponents
	{
		void CAdvancedAnimationComponent::Register(Schematyc::CEnvRegistrationScope& componentScope)
		{
			// Functions
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CAdvancedAnimationComponent::ActivateContext, "{2DA18E2A-75F6-4EEE-9C7C-60ABF275555E}"_cry_guid, "ActivateContext");
				pFunction->SetDescription("Activates a Mannequin context");
				pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
				pFunction->BindInput(1, 'cont', "Context Name");
				componentScope.Register(pFunction);
			}
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CAdvancedAnimationComponent::QueueFragment, "{6F5AA73B-B8F9-4392-9651-468DEB342D91}"_cry_guid, "QueueFragment");
				pFunction->SetDescription("Queues a Mannequin fragment for playback");
				pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
				pFunction->BindInput(1, 'frag', "Fragment Name");
				componentScope.Register(pFunction);
			}
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CAdvancedAnimationComponent::SetTag, "{04768686-96DF-4F82-8CC8-109522A9F2E0}"_cry_guid, "SetTag");
				pFunction->SetDescription("Sets a Mannequin tag's state to true or false");
				pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
				pFunction->BindInput(1, 'tagn', "Tag Name");
				pFunction->BindInput(2, 'set', "Set");
				componentScope.Register(pFunction);
			}
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CAdvancedAnimationComponent::SetMotionParameter, "{67B8D89B-D56B-443E-B88B-BFEBEE52AFB7}"_cry_guid, "SetMotionParameter");
				pFunction->SetDescription("Sets a motion parameter to affect a blend space");
				pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
				pFunction->BindInput(1, 'mtnp', "Motion Parameter");
				pFunction->BindInput(2, 'val', "Value");
				componentScope.Register(pFunction);
			}
			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CAdvancedAnimationComponent::SetMeshType, "{A4D2B249-64BE-48DA-8700-E3B479BB6F65}"_cry_guid, "SetType");
				pFunction->BindInput(1, 'type', "Type");
				pFunction->SetDescription("Changes the type of the object");
				pFunction->SetFlags({ Schematyc::EEnvFunctionFlags::Member });
				componentScope.Register(pFunction);
			}
		}

		inline bool Serialize(Serialization::IArchive& archive, CAdvancedAnimationComponent::SDefaultScopeSettings& defaultSettings, const char* szName, const char* szLabel)
		{
			archive(Serialization::MannequinControllerDefinitionPath(defaultSettings.m_controllerDefinitionPath), "ControllerDefPath", "Controller Definition");
			archive.doc("Path to the Mannequin controller definition");

			std::shared_ptr<Serialization::SMannequinControllerDefResourceParams> pParams;

			// Load controller definition for the context and fragment selectors
			if (archive.isEdit())
			{
				pParams = std::make_shared<Serialization::SMannequinControllerDefResourceParams>();

				IAnimationDatabaseManager &animationDatabaseManager = gEnv->pGameFramework->GetMannequinInterface().GetAnimationDatabaseManager();
				if (defaultSettings.m_controllerDefinitionPath.size() > 0)
				{
					pParams->pControllerDef = animationDatabaseManager.LoadControllerDef(defaultSettings.m_controllerDefinitionPath);
				}
			}

			archive(Serialization::MannequinScopeContextName(defaultSettings.m_contextName, pParams), "DefaultScope", "Default Scope Context Name");
			archive.doc("The Mannequin scope context to activate by default");

			archive(Serialization::MannequinFragmentName(defaultSettings.m_fragmentName, pParams), "DefaultFragment", "Default Fragment Name");
			archive.doc("The fragment to play by default");

			return true;
		}

		CAdvancedAnimationComponent::~CAdvancedAnimationComponent()
		{
			SAFE_RELEASE(m_pActionController);
		}

		void CAdvancedAnimationComponent::Initialize()
		{
			LoadFromDisk();

			ResetCharacter();
		}

		void CAdvancedAnimationComponent::ProcessEvent(const SEntityEvent& event)
		{
			if (event.event == ENTITY_EVENT_UPDATE)
			{
				SEntityUpdateContext* pCtx = (SEntityUpdateContext*)event.nParam[0];

				if (m_pActionController != nullptr)
				{
					m_pActionController->Update(pCtx->fFrameTime);
				}

				Matrix34 characterTransform = GetWorldTransformMatrix();

				// Set turn rate as the difference between previous and new entity rotation
				m_turnAngle = Ang3::CreateRadZ(characterTransform.GetColumn1(), m_prevForwardDir) / pCtx->fFrameTime;
				m_prevForwardDir = characterTransform.GetColumn1();

				if (m_pCachedCharacter != nullptr)
				{
					if (IPhysicalEntity* pPhysicalEntity = m_pEntity->GetPhysicalEntity())
					{
						pe_status_dynamics dynStatus;
						if (pPhysicalEntity->GetStatus(&dynStatus))
						{
							float travelAngle = Ang3::CreateRadZ(characterTransform.GetColumn1(), dynStatus.v.GetNormalized());
							float travelSpeed = dynStatus.v.GetLength2D();

							// Set the travel speed based on the physics velocity magnitude
							// Keep in mind that the maximum number for motion parameters is 10.
							// If your velocity can reach a magnitude higher than this, divide by the maximum theoretical account and work with a 0 - 1 ratio.
							if (!m_overriddenMotionParams.test(eMotionParamID_TravelSpeed))
							{
								m_pCachedCharacter->GetISkeletonAnim()->SetDesiredMotionParam(eMotionParamID_TravelSpeed, travelSpeed, 0.f);
							}

							// Update the turn speed in CryAnimation, note that the maximum motion parameter (10) applies here too.
							if (!m_overriddenMotionParams.test(eMotionParamID_TurnAngle))
							{
								m_pCachedCharacter->GetISkeletonAnim()->SetDesiredMotionParam(eMotionParamID_TurnAngle, m_turnAngle, 0.f);
							}

							if (!m_overriddenMotionParams.test(eMotionParamID_TravelAngle))
							{
								m_pCachedCharacter->GetISkeletonAnim()->SetDesiredMotionParam(eMotionParamID_TravelAngle, travelAngle, 0.f);
							}
						}
					}

					if (m_pPoseAligner != nullptr && m_pPoseAligner->Initialize(*m_pEntity, m_pCachedCharacter))
					{
						m_pPoseAligner->SetBlendWeight(1.f);
						m_pPoseAligner->Update(m_pCachedCharacter, QuatT(characterTransform), pCtx->fFrameTime);
					}
				}

				m_overriddenMotionParams.reset();
			}
			else if (event.event == ENTITY_EVENT_ANIM_EVENT)
			{
				if (m_pActionController != nullptr)
				{
					const AnimEventInstance *pAnimEvent = reinterpret_cast<const AnimEventInstance*>(event.nParam[0]);
					ICharacterInstance *pCharacter = reinterpret_cast<ICharacterInstance*>(event.nParam[1]);

					m_pActionController->OnAnimationEvent(pCharacter, *pAnimEvent);
				}
			}
			else if (event.event == ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED)
			{
				LoadFromDisk();
				ResetCharacter();
			}

			CBaseMeshComponent::ProcessEvent(event);
		}

		uint64 CAdvancedAnimationComponent::GetEventMask() const
		{
			uint64 bitFlags = CBaseMeshComponent::GetEventMask() | ENTITY_EVENT_BIT(ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED);

			if (m_pPoseAligner != nullptr)
			{
				bitFlags |= ENTITY_EVENT_BIT(ENTITY_EVENT_UPDATE);
			}

			if (m_pActionController != nullptr)
			{
				bitFlags |= ENTITY_EVENT_BIT(ENTITY_EVENT_UPDATE) | ENTITY_EVENT_BIT(ENTITY_EVENT_ANIM_EVENT);
			}

			return bitFlags;
		}

		void CAdvancedAnimationComponent::SetCharacterFile(const char* szPath, bool applyImmediately)
		{
			m_characterFile = szPath;
			LoadFromDisk();

			if (applyImmediately)
			{
				ResetCharacter();
			}
		}

		void CAdvancedAnimationComponent::SetMannequinAnimationDatabaseFile(const char* szPath)
		{
			m_databasePath = szPath;
		}

		void CAdvancedAnimationComponent::SetControllerDefinitionFile(const char* szPath)
		{
			m_defaultScopeSettings.m_controllerDefinitionPath = szPath;
		}

		void CAdvancedAnimationComponent::SetDefaultScopeContextName(const char* szName)
		{
			m_defaultScopeSettings.m_contextName = szName;
		}

		void CAdvancedAnimationComponent::SetDefaultFragmentName(const char* szName)
		{
			m_defaultScopeSettings.m_fragmentName = szName;
		}
	}
}