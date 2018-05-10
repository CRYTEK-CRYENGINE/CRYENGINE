// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "../Interface/ICrySensorSystemPlugin.h"

namespace Cry
{
	namespace SensorSystem
	{
		class CSensorSystem;

		class CCrySensorSystemPlugin : public ICrySensorSystemPlugin
		{
			CRYINTERFACE_BEGIN()
				CRYINTERFACE_ADD(ICrySensorSystemPlugin)
				CRYINTERFACE_ADD(Cry::IEnginePlugin)
			CRYINTERFACE_END()

			CRYGENERATE_SINGLETONCLASS_GUID(CCrySensorSystemPlugin, "Plugin_CrySensorSystem", "08a96846-8933-4211-913f-7a64c0bf9822"_cry_guid)

			virtual ~CCrySensorSystemPlugin() {}

			// Cry::IEnginePlugin
			virtual bool Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) override;
			virtual void MainUpdate(float frameTime) override;
			// ~Cry::IEnginePlugin

		public:

			// ICrySensorSystemPlugin
			virtual ISensorSystem& GetSensorSystem() const override;
			// ~ICrySensorSystemPlugin

		private:

			std::unique_ptr<CSensorSystem> m_pSensorSystem;
		};
	}
}