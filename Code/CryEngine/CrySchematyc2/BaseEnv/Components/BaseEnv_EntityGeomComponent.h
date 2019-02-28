// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "BaseEnv/BaseEnv_Prerequisites.h"

#include "BaseEnv/Components/BaseEnv_EntityTransformComponentBase.h"

namespace SchematycBaseEnv
{
	class CEntityGeomComponent : public CEntityTransformComponentBase
	{
	private:

		struct SProperties
		{
			void Serialize(Serialization::IArchive& archive);

			Schematyc2::SEnvFunctionResult SetFileName(const Schematyc2::SEnvFunctionContext& context, const Schematyc2::SGeometryFileName& _fileName);
			Schematyc2::SEnvFunctionResult SetTransform(const Schematyc2::SEnvFunctionContext& context, const STransform& _transform);

			Schematyc2::SGeometryFileName fileName;
			STransform                   transform;
		};

	public:

		CEntityGeomComponent();

		// Schematyc2::IComponent
		virtual bool Init(const Schematyc2::SComponentParams& params) override;
		virtual void Run(Schematyc2::ESimulationMode simulationMode) override;
		virtual Schematyc2::IComponentPreviewPropertiesPtr GetPreviewProperties() const override;
		virtual void Preview(const SRendParams& params, const SRenderingPassInfo& passInfo, const Schematyc2::IComponentPreviewProperties& previewProperties) const override;
		virtual void Shutdown() override;
		virtual Schematyc2::INetworkSpawnParamsPtr GetNetworkSpawnParams() const override;
		// ~Schematyc2::IComponent

		Schematyc2::SEnvFunctionResult SetVisible(const Schematyc2::SEnvFunctionContext& context, bool bVisible);
		Schematyc2::SEnvFunctionResult IsVisible(const Schematyc2::SEnvFunctionContext& context, bool &bVisible) const;

		static void Register();

	public:

		static const Schematyc2::SGUID s_componentGUID;

	private:

		const SProperties* m_pProperties;
	};
}