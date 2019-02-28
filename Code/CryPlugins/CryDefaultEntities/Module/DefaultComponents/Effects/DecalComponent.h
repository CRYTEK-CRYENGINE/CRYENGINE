#pragma once

#include <CryRenderer/IRenderer.h>
#include <CryRenderer/IShader.h>

#include <CrySchematyc/ResourceTypes.h>
#include <CrySchematyc/MathTypes.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>

class CPlugin_CryDefaultEntities;

static void ReflectType(Schematyc::CTypeDesc<SDecalProperties::EProjectionType>& desc)
{
	desc.SetGUID("{8556D9B6-F2D4-41DC-9C5C-897801C1D8CA}"_cry_guid);
	desc.SetLabel("Decal Projection Type");
	desc.SetDescription("Determines the method of projecting decals to objects");
	desc.SetDefaultValue(SDecalProperties::ePlanar);
	desc.AddConstant(SDecalProperties::ePlanar, "Planar", "Planar");
	desc.AddConstant(SDecalProperties::eProjectOnStaticObjects, "StaticObjects", "Static Objects");
	desc.AddConstant(SDecalProperties::eProjectOnTerrain, "Terrain", "Terrain");
	desc.AddConstant(SDecalProperties::eProjectOnTerrainAndStaticObjects, "TerrainAndStaticObjects", "Terrain and Static Objects");
}

namespace Cry
{
	namespace DefaultComponents
	{
		class CDecalComponent
			: public IEntityComponent
#ifndef RELEASE
			, public IEntityComponentPreviewer
#endif
		{
		protected:
			friend CPlugin_CryDefaultEntities;
			static void Register(Schematyc::CEnvRegistrationScope& componentScope);

			// IEntityComponent
			virtual void Initialize() final;

			virtual void ProcessEvent(const SEntityEvent& event) final;
			virtual uint64 GetEventMask() const final;
			// ~IEntityComponent

#ifndef RELEASE
			// IEntityComponentPreviewer
			virtual IEntityComponentPreviewer* GetPreviewer() final { return this; }
			virtual void SerializeProperties(Serialization::IArchive& archive) final {}
			virtual void Render(const IEntity& entity, const IEntityComponent& component, SEntityPreviewContext &context) const final;
			// ~IEntityComponentPreviewer
#endif

		public:
			virtual ~CDecalComponent() {}

			static void ReflectType(Schematyc::CTypeDesc<CDecalComponent>& desc)
			{
				desc.SetGUID("{26C5856F-34BC-43FE-BE36-2D276D082C96}"_cry_guid);
				desc.SetEditorCategory("Effects");
				desc.SetLabel("Decal");
				desc.SetDescription("Exposes support for spawning decals");
				desc.SetComponentFlags({ IEntityComponent::EFlags::Transform, IEntityComponent::EFlags::Socket, IEntityComponent::EFlags::Attach, IEntityComponent::EFlags::ClientOnly });

				desc.AddMember(&CDecalComponent::m_bAutoSpawn, 'auto', "AutoSpawn", "Default Spawn", "Whether or not to automatically spawn the decal", true);
				desc.AddMember(&CDecalComponent::m_materialFileName, 'matf', "Material", "Material", "The material we want to use on decals", "");
				desc.AddMember(&CDecalComponent::m_bFollowEntityAfterSpawn, 'folo', "FollowEntity", "Follow Entity", "Whether or not the decal follows the entity", true);
				desc.AddMember(&CDecalComponent::m_projectionType, 'proj', "ProjectionType", "Projection Type", "The method of projection we want to use", SDecalProperties::ePlanar);
				desc.AddMember(&CDecalComponent::m_depth, 'dept', "Depth", "Projection Depth", "The depth at which we should project the decal", 1.f);
				desc.AddMember(&CDecalComponent::m_sortPriority, 'sort', "SortPriority", "Sort Priority", "Sorting priority, used to resolve depth issues with other decals", 16);
			}

			virtual void Spawn()
			{
				if (m_materialFileName.value.size() > 0 && gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(m_materialFileName.value) != nullptr)
				{
					IDecalRenderNode* pRenderNode = static_cast<IDecalRenderNode*>(m_pEntity->GetSlotRenderNode(GetEntitySlotId()));
					if (pRenderNode == nullptr)
					{
						pRenderNode = static_cast<IDecalRenderNode*>(gEnv->p3DEngine->CreateRenderNode(eERType_Decal));
					}

					bool bSelected, bHighlighted;
					m_pEntity->GetEditorObjectInfo(bSelected, bHighlighted);

					uint64 renderFlags = ERF_PROCEDURAL;

					if (bSelected)
					{
						renderFlags |= ERF_SELECTED;
					}

					if (m_pEntity->IsHidden())
					{
						renderFlags |= ERF_HIDDEN;
					}

					pRenderNode->SetRndFlags(renderFlags);
					
					SDecalProperties decalProperties;
					decalProperties.m_projectionType = m_projectionType;

					const Matrix34& slotTransform = m_pEntity->GetSlotWorldTM(GetEntitySlotId());

					// Get normalized rotation (remove scaling)
					Matrix33 rotation(slotTransform);
					if (m_projectionType != SDecalProperties::ePlanar)
					{
						rotation.SetRow(0, rotation.GetRow(0).GetNormalized());
						rotation.SetRow(1, rotation.GetRow(1).GetNormalized());
						rotation.SetRow(2, rotation.GetRow(2).GetNormalized());
					}

					decalProperties.m_pos = slotTransform.TransformPoint(Vec3(0, 0, 0));
					decalProperties.m_normal = slotTransform.TransformVector(Vec3(0, 0, 1));

					PathUtil::RemoveExtension(m_materialFileName.value);
					decalProperties.m_pMaterialName = m_materialFileName.value.c_str();	

					decalProperties.m_radius = m_projectionType != SDecalProperties::ePlanar ? decalProperties.m_normal.GetLength() : 1;
					decalProperties.m_explicitRightUpFront = rotation;
					decalProperties.m_sortPrio = m_sortPriority;
					decalProperties.m_deferred = true;
					decalProperties.m_depth = m_depth;
					pRenderNode->SetDecalProperties(decalProperties);
					pRenderNode->SetMatrix(slotTransform);
					
					m_bSpawned = true;

					m_pEntity->UpdateComponentEventMask(this);

					m_pEntity->SetSlotRenderNode(GetEntitySlotId(), pRenderNode);
				}
				else
				{
					Remove();
				}
			}

			virtual void Remove()
			{
				FreeEntitySlot();

				m_bSpawned = false;

				m_pEntity->UpdateComponentEventMask(this);
			}

			virtual void EnableAutomaticSpawn(bool bEnable) { m_bAutoSpawn = bEnable; }
			bool IsAutomaticSpawnEnabled() const { return m_bAutoSpawn; }

			virtual void EnableAutomaticMove(bool bEnable) { m_bAutoSpawn = bEnable; }
			bool IsAutomaticMoveEnabled() const { return m_bAutoSpawn; }

			virtual void SetProjectionType(SDecalProperties::EProjectionType type) { m_projectionType = type; }
			SDecalProperties::EProjectionType GetProjectionType() const { return m_projectionType; }

			virtual void SetSortPriority(uint8 priority) { m_sortPriority = (int)priority; }
			uint8 GetSortPriority() const { return m_sortPriority; }

			virtual void SetDepth(float depth) { m_depth = depth; }
			float GetDepth() const { return m_depth; }

			virtual void SetMaterialFileName(const char* szPath);
			const char* GetMaterialFileName() const { return m_materialFileName.value.c_str(); }

		protected:
			bool m_bAutoSpawn = true;
			bool m_bFollowEntityAfterSpawn = true;
			
			SDecalProperties::EProjectionType m_projectionType = SDecalProperties::ePlanar;
			Schematyc::Range<0, 255, 0, 255, int> m_sortPriority = 16;
			Schematyc::Range<0, 10> m_depth = 1.f;

			bool m_bSpawned = false;

			Schematyc::MaterialFileName m_materialFileName;
		};
	}
}