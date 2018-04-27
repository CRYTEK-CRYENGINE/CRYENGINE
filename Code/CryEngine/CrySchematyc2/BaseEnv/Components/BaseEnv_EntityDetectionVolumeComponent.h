// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "BaseEnv/BaseEnv_Prerequisites.h"

#include "BaseEnv/Components/BaseEnv_EntityTransformComponentBase.h"
#include "BaseEnv/Utils/BaseEnv_EntitySignals.h"
#include "BaseEnv/Utils/BaseEnv_EntityUpdateFilters.h"
#include "BaseEnv/Utils/BaseEnv_SpatialIndex.h"

namespace SchematycBaseEnv
{
	extern const Schematyc2::SGUID s_detectionVolumeComponentGUID;

	class CEntityDetectionVolumeComponent : public CEntityTransformComponentBase
	{
	private:

		struct SVolumeProperties
		{
			SVolumeProperties();

			void Serialize(Serialization::IArchive& archive);

			SpatialVolumeTagNames tags;
			SpatialVolumeTagNames monitorTags;
			Vec3                  size;
			float                 radius;
			ESpatialVolumeShape   shape;
		};

		struct SProperties
		{
			SProperties();

			void Serialize(Serialization::IArchive& archive);

			// PropertyGraphs
			Schematyc2::SEnvFunctionResult SetTransform(const Schematyc2::SEnvFunctionContext& context, const STransform& _transform);
			// ~PropertyGraphs

			STransform        transform;
			SVolumeProperties volume;
			bool              bEnabled;
		};

		struct SVolume
		{
			SVolume();

			Matrix33            rot;
			Vec3                pos;
			Vec3                size;
			float               radius;
			SpatialVolumeId     volumeId;
			ESpatialVolumeShape shape;
		};

		typedef std::vector<EntityId> EntityIds;

	public:

		CEntityDetectionVolumeComponent();

		// Schematyc2::IComponent
		virtual bool Init(const Schematyc2::SComponentParams& params) override;
		virtual void Run(Schematyc2::ESimulationMode simulationMode) override;
		virtual Schematyc2::IComponentPreviewPropertiesPtr GetPreviewProperties() const override;
		virtual void Preview(const SRendParams& params, const SRenderingPassInfo& passInfo, const Schematyc2::IComponentPreviewProperties& previewProperties) const override;
		virtual void Shutdown() override;
		virtual Schematyc2::INetworkSpawnParamsPtr GetNetworkSpawnParams() const override;
		// ~Schematyc2::IComponent

		bool SetEnabled(bool bEnabled);

		static void Register();

	private:

		void SetVolumeSize(float sizeX, float sizeY, float sizeZ);
		void GetVolumeSize(float& sizeX, float& sizeY, float& sizeZ) const;

		void SetVolumeRadius(float radius);
		float GetVolumeRadius() const;

		const char* GetVolumeName(SpatialVolumeId volumeId) const;

		void VolumeEventCallback(const SSpatialIndexEvent& event);
		void OnEvent(const SEntityEvent& event);

		void Update(const Schematyc2::SUpdateContext& updateContext);

		void Enable();
		void Disable();

	public:

		static const Schematyc2::SGUID s_componentGUID;
		static const Schematyc2::SGUID s_enterSignalGUID;
		static const Schematyc2::SGUID s_leaveSignalGUID;

	private:

		const SProperties*              m_pProperties;
		SVolume                         m_volume;
		EntityIds                       m_entityIds;
		bool                            m_bEnabled;
		EntityNotHiddenUpdateFilter     m_updateFilter;
		Schematyc2::CUpdateScope         m_updateScope;
		TemplateUtils::CConnectionScope m_connectionScope;
	};
}