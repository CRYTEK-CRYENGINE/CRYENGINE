#pragma once

#include "BaseMeshComponent.h"

class CPlugin_CryDefaultEntities;

namespace Cry
{
	namespace DefaultComponents
	{
		class CAnimatedMeshComponent
			: public CBaseMeshComponent
		{
		protected:
			friend CPlugin_CryDefaultEntities;
			static void Register(Schematyc::CEnvRegistrationScope& componentScope);

			// IEntityComponent
			virtual void   Initialize() final;

			virtual void   ProcessEvent(const SEntityEvent& event) final;
			// ~IEntityComponent

		public:
			CAnimatedMeshComponent() {}
			virtual ~CAnimatedMeshComponent() {}

			static void ReflectType(Schematyc::CTypeDesc<CAnimatedMeshComponent>& desc)
			{
				desc.SetGUID("{5F543092-53EA-46D7-9376-266E778317D7}"_cry_guid);
				desc.SetEditorCategory("Geometry");
				desc.SetLabel("Animated Mesh");
				desc.SetDescription("A component containing a simple mesh that can be animated");
				desc.SetComponentFlags({ IEntityComponent::EFlags::Transform, IEntityComponent::EFlags::Socket, IEntityComponent::EFlags::Attach });

				desc.AddMember(&CAnimatedMeshComponent::m_type, 'type', "Type", "Type", "Determines the behavior of the static mesh", EMeshType::RenderAndCollider);

				desc.AddMember(&CAnimatedMeshComponent::m_filePath, 'file', "FilePath", "File", "Determines the animated mesh to load", "");
				desc.AddMember(&CAnimatedMeshComponent::m_renderParameters, 'rend', "Render", "Rendering Settings", "Settings for the rendered representation of the component", SRenderParameters());

				desc.AddMember(&CAnimatedMeshComponent::m_defaultAnimation, 'anim', "Animation", "Default Animation", "Specifies the animation we want to play by default", "");
				desc.AddMember(&CAnimatedMeshComponent::m_bLoopDefaultAnimation, 'loop', "Loop", "Loop Default", "Whether or not to loop the default animation", false);
				desc.AddMember(&CAnimatedMeshComponent::m_defaultAnimationSpeed, 'sped', "AnimSpeed", "Default Animation Speed", "Speed at which to play the default animation", 1.0f);
				
				desc.AddMember(&CAnimatedMeshComponent::m_physics, 'phys', "Physics", "Physics", "Physical properties for the object, only used if a simple physics or character controller is applied to the entity.", SPhysicsParameters());
			}

			virtual void PlayAnimation(Schematyc::LowLevelAnimationName name, bool bLoop = false)
			{
				if (ICharacterInstance* pCharacter = m_pEntity->GetCharacter(GetEntitySlotId()))
				{
					CryCharAnimationParams animParams = m_animationParams;
					if (bLoop)
					{
						animParams.m_nFlags |= CA_LOOP_ANIMATION;
					}
					else
					{
						animParams.m_nFlags &= ~CA_LOOP_ANIMATION;
					}

					string animationName = PathUtil::GetFileName(name.value);
					pCharacter->GetISkeletonAnim()->StartAnimation(animationName, animParams);
				}
			}

			virtual void SetPlaybackSpeed(float multiplier) { m_animationParams.m_fPlaybackSpeed = multiplier; }
			virtual void SetPlaybackWeight(float weight) { m_animationParams.m_fPlaybackWeight = weight; }
			virtual void SetLayer(int layer) { m_animationParams.m_nLayerID = layer; }

			virtual void SetCharacterFile(const char* szPath);
			const char* GetCharacterFile() const { return m_filePath.value.c_str(); }

			virtual void SetDefaultAnimationName(const char* szPath);
			const char* GetDefaultAnimationName() const { return m_defaultAnimation.value.c_str(); }
			virtual void SetDefaultAnimationLooped(bool bLooped) { m_bLoopDefaultAnimation = bLooped; }
			bool IsDefaultAnimationLooped() const { return m_bLoopDefaultAnimation; }

			// Loads character and mannequin data from disk
			virtual void LoadFromDisk();
			// Applies the character to the entity
			virtual void ResetObject();

			// Helper to allow exposing derived function to Schematyc
			virtual void SetMeshType(EMeshType type) { SetType(type); }

		protected:
			CryCharAnimationParams m_animationParams;

			Schematyc::CharacterFileName m_filePath;

			Schematyc::LowLevelAnimationName m_defaultAnimation;
			bool m_bLoopDefaultAnimation = false;
			Schematyc::PositiveFloat m_defaultAnimationSpeed = 1.f;

			_smart_ptr<ICharacterInstance> m_pCachedCharacter = nullptr;
		};
	}
}