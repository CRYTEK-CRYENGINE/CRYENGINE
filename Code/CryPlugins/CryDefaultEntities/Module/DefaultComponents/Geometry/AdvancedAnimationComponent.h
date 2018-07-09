#pragma once

#include "BaseMeshComponent.h"

#include <bitset>

#include <ICryMannequin.h>
#include <CrySchematyc/Utils/SharedString.h>
#include <CryCore/Containers/CryArray.h>
#include <CryGame/IGameFramework.h>

#include <Animation/PoseAligner/PoseAligner.h>

class CPlugin_CryDefaultEntities;

static void ReflectType(Schematyc::CTypeDesc<EMotionParamID>& desc)
{
	desc.SetGUID("{310D2CFB-8564-4C8E-A73B-BF4920D14136}"_cry_guid);
	desc.SetLabel("Animation Motion Parameter");
	desc.SetDescription("Modifier for animation to affect blend spaces");
	desc.SetDefaultValue(EMotionParamID::eMotionParamID_TravelSpeed);
	desc.AddConstant(EMotionParamID::eMotionParamID_TravelSpeed, "TravelSpeed", "Travel Speed");
	desc.AddConstant(EMotionParamID::eMotionParamID_TurnSpeed, "TurnSpeed", "Turn Speed");
	desc.AddConstant(EMotionParamID::eMotionParamID_TravelAngle, "TravelAngle", "Travel Angle");
	desc.AddConstant(EMotionParamID::eMotionParamID_TravelSlope, "TravelSlope", "Travel Slope");
	desc.AddConstant(EMotionParamID::eMotionParamID_TurnAngle, "TurnAngle", "Turn Angle");
	desc.AddConstant(EMotionParamID::eMotionParamID_TravelDist, "TravelDist", "Travel Distance");
	desc.AddConstant(EMotionParamID::eMotionParamID_StopLeg, "StopLeg", "Move2Idle Stop Leg");

	desc.AddConstant(EMotionParamID::eMotionParamID_BlendWeight, "BlendWeight", "Custom 1");
	desc.AddConstant(EMotionParamID::eMotionParamID_BlendWeight2, "BlendWeight2", "Custom 2");
	desc.AddConstant(EMotionParamID::eMotionParamID_BlendWeight3, "BlendWeight3", "Custom 3");
	desc.AddConstant(EMotionParamID::eMotionParamID_BlendWeight4, "BlendWeight4", "Custom 4");
	desc.AddConstant(EMotionParamID::eMotionParamID_BlendWeight5, "BlendWeight5", "Custom 5");
	desc.AddConstant(EMotionParamID::eMotionParamID_BlendWeight6, "BlendWeight6", "Custom 6");
	desc.AddConstant(EMotionParamID::eMotionParamID_BlendWeight7, "BlendWeight7", "Custom 7");
}

namespace Cry
{
namespace DefaultComponents
{
class CAdvancedAnimationComponent
	: public CBaseMeshComponent
{
protected:
	friend CPlugin_CryDefaultEntities;
	static void Register(Schematyc::CEnvRegistrationScope& componentScope);

	// IEntityComponent
	virtual void   Initialize() override;

	virtual void   ProcessEvent(const SEntityEvent& event) override;
	virtual uint64 GetEventMask() const override;
	// ~IEntityComponent

public:
	struct SDefaultScopeSettings
	{
		inline bool operator==(const SDefaultScopeSettings& rhs) const { return 0 == memcmp(this, &rhs, sizeof(rhs)); }
		static void ReflectType(Schematyc::CTypeDesc<SDefaultScopeSettings>& desc)
		{
			desc.SetGUID("{1ADE6AA3-79F6-48F2-9D61-913BF88573E6}"_cry_guid);
			desc.SetLabel("Advanced Animation Component Default Scope");
			desc.SetDescription("Settings for the default Mannequin scope");
		}

		string m_controllerDefinitionPath;
		string m_contextName;
		string m_fragmentName;
	};

	CAdvancedAnimationComponent() = default;
	virtual ~CAdvancedAnimationComponent();

	static void ReflectType(Schematyc::CTypeDesc<CAdvancedAnimationComponent>& desc)
	{
		desc.SetGUID("{3CD5DDC5-EE15-437F-A997-79C2391537FE}"_cry_guid);
		desc.SetEditorCategory("Geometry");
		desc.SetLabel("Advanced Animations");
		desc.SetDescription("Exposes playback of more advanced animations going through the Mannequin systems");
		desc.SetIcon("icons:General/Mannequin.ico");
		desc.SetComponentFlags({ IEntityComponent::EFlags::Transform, IEntityComponent::EFlags::Socket, IEntityComponent::EFlags::Attach });

		desc.AddMember(&CAdvancedAnimationComponent::m_type, 'type', "Type", "Type", "Determines the behavior of the static mesh", EMeshType::RenderAndCollider);
		desc.AddMember(&CAdvancedAnimationComponent::m_characterFile, 'file', "Character", "Character", "Determines the character to load", "");
		desc.AddMember(&CAdvancedAnimationComponent::m_renderParameters, 'rend', "Render", "Rendering Settings", "Settings for the rendered representation of the component", SRenderParameters());

		desc.AddMember(&CAdvancedAnimationComponent::m_databasePath, 'dbpa', "DatabasePath", "Animation Database", "Path to the Mannequin .adb file", "");
		desc.AddMember(&CAdvancedAnimationComponent::m_defaultScopeSettings, 'defs', "DefaultScope", "Default Scope Context Name", "Default Mannequin scope settings", CAdvancedAnimationComponent::SDefaultScopeSettings());
		desc.AddMember(&CAdvancedAnimationComponent::m_bAnimationDrivenMotion, 'andr', "AnimDriven", "Animation Driven Motion", "Whether or not to use root motion in the animations", true);
		desc.AddMember(&CAdvancedAnimationComponent::m_bGroundAlignment, 'grou', "GroundAlign", "Use Ground Alignment", "Enables adjustment of leg positions to align to the ground surface", false);

		desc.AddMember(&CAdvancedAnimationComponent::m_physics, 'phys', "Physics", "Physics", "Physical properties for the object, only used if a simple physics or character controller is applied to the entity.", SPhysicsParameters());
	}

	virtual void ActivateContext(const Schematyc::CSharedString& contextName)
	{
		if (m_pCachedCharacter == nullptr)
		{
			return;
		}

		const TagID scopeContextId = m_pAnimationContext->controllerDef.m_scopeContexts.Find(contextName.c_str());
		if (scopeContextId == TAG_ID_INVALID)
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Failed to find scope context %s in controller definition.", contextName.c_str());
			return;
		}

		// Setting Scope contexts can happen at any time, and what entity or character instance we have bound to a particular scope context
		// can change during the lifetime of an action controller.
		m_pActionController->SetScopeContext(scopeContextId, *m_pEntity, m_pCachedCharacter, m_pDatabase);
	}

	// TODO: Expose resource selector for fragments
	virtual void QueueFragment(const Schematyc::CSharedString& fragmentName)
	{
		if (m_pAnimationContext == nullptr)
		{
			return;
		}

		FragmentID fragmentId = m_pAnimationContext->controllerDef.m_fragmentIDs.Find(fragmentName.c_str());
		if (fragmentId == FRAGMENT_ID_INVALID)
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Failed to find Mannequin fragment %s in controller definition %s", fragmentName.c_str(), m_pAnimationContext->controllerDef.m_filename.c_str());
			return;
		}

		return QueueFragmentWithId(fragmentId);
	}

	virtual void QueueFragmentWithId(const FragmentID& fragmentId)
	{
		if (m_pAnimationContext == nullptr)
		{
			return;
		}

		if (m_pActiveAction)
		{
			m_pActiveAction->Stop();
		}

		const int priority = 0;
		m_pActiveAction = new TAction<SAnimationContext>(priority, fragmentId);
		m_pActionController->Queue(*m_pActiveAction);
	}
	
	virtual void QueueCustomFragment(IAction& action)
	{
		m_pActionController->Queue(action);
	}

	// TODO: Expose resource selector for tags
	virtual void SetTag(const Schematyc::CSharedString& tagName, bool bSet)
	{
		SetTagWithId(GetTagId(tagName.c_str()), bSet);
	}

	virtual void SetMotionParameter(EMotionParamID motionParam, float value)
	{
		CRY_ASSERT(m_pCachedCharacter != nullptr);
		if (m_pCachedCharacter != nullptr)
		{
			m_pCachedCharacter->GetISkeletonAnim()->SetDesiredMotionParam(motionParam, value, 0.f);
			m_overriddenMotionParams.set(motionParam);
		}
	}

	TagID GetTagId(const char* szTagName) const
	{
		return m_pControllerDefinition->m_tags.Find(szTagName);
	}

	FragmentID GetFragmentId(const char* szFragmentName) const
	{
		return m_pControllerDefinition->m_fragmentIDs.Find(szFragmentName);
	}

	virtual void SetTagWithId(TagID id, bool bSet)
	{
		m_pAnimationContext->state.Set(id, bSet);
	}

	ICharacterInstance* GetCharacter() const { return m_pCachedCharacter; }

	// Loads character and mannequin data from disk
	virtual void LoadFromDisk()
	{
		if (m_characterFile.value.size() > 0)
		{
			m_pCachedCharacter = gEnv->pCharacterManager->CreateInstance(m_characterFile.value);
			if (m_pCachedCharacter == nullptr)
			{
				CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Failed to load character %s!", m_characterFile.value.c_str());
				return;
			}

			if (m_bGroundAlignment && m_pCachedCharacter != nullptr)
			{
				if (m_pPoseAligner == nullptr)
				{
					CryCreateClassInstance(CPoseAlignerC3::GetCID(), m_pPoseAligner);
				}

				m_pPoseAligner->Clear();
			}
			else
			{
				m_pPoseAligner.reset();
			}
		}
		else
		{
			m_pCachedCharacter = nullptr;
		}

		if (m_defaultScopeSettings.m_controllerDefinitionPath.size() > 0 && m_databasePath.value.size() > 0)
		{
			// Now start loading the Mannequin data
			IMannequin& mannequinInterface = gEnv->pGameFramework->GetMannequinInterface();
			IAnimationDatabaseManager& animationDatabaseManager = mannequinInterface.GetAnimationDatabaseManager();

			// Load the Mannequin controller definition.
			// This is owned by the animation database manager
			m_pControllerDefinition = animationDatabaseManager.LoadControllerDef(m_defaultScopeSettings.m_controllerDefinitionPath);
			if (m_pControllerDefinition == nullptr)
			{
				CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Failed to load controller definition %s!", m_defaultScopeSettings.m_controllerDefinitionPath.c_str());
				return;
			}

			// Load the animation database
			m_pDatabase = animationDatabaseManager.Load(m_databasePath.value);
			if (m_pDatabase == nullptr)
			{
				CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Failed to load animation database %s!", m_databasePath.value.c_str());
				return;
			}
		}
	}

	// Resets the character and Mannequin
	virtual void ResetCharacter()
	{
		m_pActiveAction = nullptr;

		// Release previous controller and context
		SAFE_RELEASE(m_pActionController);
		m_pAnimationContext.reset();

		if (m_pCachedCharacter == nullptr)
		{
			return;
		}

		m_pEntity->SetCharacter(m_pCachedCharacter, GetOrMakeEntitySlotId(), false);
		SetAnimationDrivenMotion(m_bAnimationDrivenMotion);

		if (m_pControllerDefinition != nullptr)
		{
			// Create a new animation context for the controller definition we loaded above
			m_pAnimationContext = stl::make_unique<SAnimationContext>(*m_pControllerDefinition);

			// Now create the controller that will be handling animation playback
			IMannequin& mannequinInterface = gEnv->pGameFramework->GetMannequinInterface();
			m_pActionController = mannequinInterface.CreateActionController(GetEntity(), *m_pAnimationContext);
			CRY_ASSERT(m_pActionController != nullptr);

			if (m_defaultScopeSettings.m_contextName.size() > 0)
			{
				ActivateContext(m_defaultScopeSettings.m_contextName);
			}

			if (m_defaultScopeSettings.m_fragmentName.size() > 0)
			{
				QueueFragment(m_defaultScopeSettings.m_fragmentName);
			}

			m_pEntity->UpdateComponentEventMask(this);
		}
	}

	// Enable / disable motion on entity being applied from animation on the root node
	virtual void SetAnimationDrivenMotion(bool bSet)
	{
		m_bAnimationDrivenMotion = bSet;

		if (m_pCachedCharacter == nullptr)
		{
			return;
		}

		// Disable animation driven motion, note that the function takes the inverted parameter of what you would expect.
		m_pCachedCharacter->GetISkeletonAnim()->SetAnimationDrivenMotion(m_bAnimationDrivenMotion ? 0 : 1);
	}
	bool         IsAnimationDrivenMotionEnabled() const { return m_bAnimationDrivenMotion; }

	virtual void SetCharacterFile(const char* szPath, bool applyImmediately = true);
	const char*  GetCharacterFile() const                  { return m_characterFile.value.c_str(); }
	virtual void SetMannequinAnimationDatabaseFile(const char* szPath);
	const char*  GetMannequinAnimationDatabaseFile() const { return m_databasePath.value.c_str(); }

	virtual void SetControllerDefinitionFile(const char* szPath);
	const char*  GetControllerDefinitionFile() const { return m_defaultScopeSettings.m_controllerDefinitionPath.c_str(); }

	virtual void SetDefaultScopeContextName(const char* szName);
	const char*  GetDefaultScopeContextName() const { return m_defaultScopeSettings.m_fragmentName.c_str(); }
	virtual void SetDefaultFragmentName(const char* szName);
	const char*  GetDefaultFragmentName() const     { return m_defaultScopeSettings.m_fragmentName.c_str(); }

	virtual void EnableGroundAlignment(bool bEnable) { m_bGroundAlignment = bEnable; }
	bool IsGroundAlignmentEnabled() const { return m_bGroundAlignment; }

	virtual bool IsTurning() const { return fabsf(m_turnAngle) > 0; }

	// Helper to allow exposing derived function to Schematyc
	virtual void SetMeshType(EMeshType type) { SetType(type); }

protected:
	bool                                      m_bAnimationDrivenMotion = true;

	Schematyc::CharacterFileName              m_characterFile;
	Schematyc::MannequinAnimationDatabasePath m_databasePath;

	SDefaultScopeSettings                     m_defaultScopeSettings;

	// Run-time info below
	IActionController*                 m_pActionController = nullptr;
	std::unique_ptr<SAnimationContext> m_pAnimationContext;
	const IAnimationDatabase*          m_pDatabase = nullptr;

	_smart_ptr<IAction>                m_pActiveAction;
	std::bitset<eMotionParamID_COUNT>  m_overriddenMotionParams;

	const SControllerDef*              m_pControllerDefinition = nullptr;
	_smart_ptr<ICharacterInstance>     m_pCachedCharacter = nullptr;

	IAnimationPoseAlignerPtr m_pPoseAligner;
	Vec3 m_prevForwardDir = ZERO;
	float m_turnAngle = 0.f;

	bool m_bGroundAlignment = false;
};
}
}
