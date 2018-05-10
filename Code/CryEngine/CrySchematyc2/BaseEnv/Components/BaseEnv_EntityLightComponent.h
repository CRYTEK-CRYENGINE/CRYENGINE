// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "BaseEnv/BaseEnv_Prerequisites.h"

#include "BaseEnv/Components/BaseEnv_EntityTransformComponentBase.h"

namespace SchematycBaseEnv
{
	class CEntityLightComponent : public CEntityTransformComponentBase
	{
	private:

		struct SProperties
		{
			SProperties();

			void Serialize(Serialization::IArchive& archive);

			Schematyc2::SEnvFunctionResult SetTransform(const Schematyc2::SEnvFunctionContext& context, const STransform& _transform);

			STransform transform;

			bool   bOn;
			ColorF color;
			float  diffuseMultiplier;
			float  specularMultiplier;
			float  hdrDynamicMultiplier;
			float  radius;
			float  frustumAngle;
			float  attenuationBulbSize;
		};

	public:

		CEntityLightComponent();

		// Schematyc2::IComponent
		virtual bool Init(const Schematyc2::SComponentParams& params) override;
		virtual void Run(Schematyc2::ESimulationMode simulationMode) override;
		virtual Schematyc2::IComponentPreviewPropertiesPtr GetPreviewProperties() const override;
		virtual void Preview(const SRendParams& params, const SRenderingPassInfo& passInfo, const Schematyc2::IComponentPreviewProperties& previewProperties) const override;
		virtual void Shutdown() override;
		virtual Schematyc2::INetworkSpawnParamsPtr GetNetworkSpawnParams() const override;
		// ~Schematyc2::IComponent

		bool Switch(bool bOn);

		static void Register();

	private:

		void SwitchOn();
		void SwitchOff();

	public:

		static const Schematyc2::SGUID s_componentGUID;

	private:

		const SProperties* m_pProperties;
		bool               m_bOn;
	};
}