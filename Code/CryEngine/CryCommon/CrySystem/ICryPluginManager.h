// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryCore/Containers/CryListenerSet.h>

class CGameEngine;

namespace Cry
{
	struct IEnginePlugin;

	//! Main source of plug-in management, from loading to querying of existing plug-ins
	struct IPluginManager
	{
		//! Determines the type of a plug-in
		enum class EType
		{
			// C++ plug-in
			Native = 0,
			// Mono / C# plug-in
			Managed
		};

		//! Alias for backwards compatibility
		using EPluginType = EType;

		struct IEventListener
		{
			enum class EEvent
			{
				Initialized,
				Unloaded,
			};

			virtual ~IEventListener() {}
			virtual void OnPluginEvent(const CryClassID& pluginClassId, EEvent event) = 0;
		};

		virtual ~IPluginManager() = default;

		//! Registers a listener that is notified when a specific plug-in is loaded and unloaded
		template<typename T>
		void RegisterEventListener(IEventListener* pListener) { RegisterEventListener(cryiidof<T>(), pListener); }
	
		//! Removes a listener registered with RegisterEventListener
		template<typename T>
		void RemoveEventListener(IEventListener* pListener) { RemoveEventListener(cryiidof<T>(), pListener); }

		//! Queries a plug-in by implementation (T has to implement Cry::IEnginePlugin)
		//! This call can only succeed if the plug-in was specified in the running project's .cryproject file
		template<typename T>
		T* QueryPlugin() const
		{
			if (ICryUnknownPtr pExtension = QueryPluginById(cryiidof<T>()))
			{
				return cryinterface_cast<T>(pExtension.get());
			}

			return nullptr;
		}

	protected:
		friend IEnginePlugin;
		virtual void OnPluginUpdateFlagsChanged(IEnginePlugin& plugin, uint8 newFlags, uint8 changedStep) = 0;

		virtual std::shared_ptr<Cry::IEnginePlugin> QueryPluginById(const CryClassID& classID) const = 0;

		virtual void RegisterEventListener(const CryClassID& pluginClassId, IEventListener* pListener) = 0;
		virtual void RemoveEventListener(const CryClassID& pluginClassId, IEventListener* pListener) = 0;


		friend CGameEngine;
		virtual void UpdateBeforeSystem() = 0;
		virtual void UpdateBeforePhysics() = 0;
		virtual void UpdateAfterSystem() = 0;
		virtual void UpdateBeforeFinalizeCamera() = 0;
		virtual void UpdateBeforeRender() = 0;
		virtual void UpdateAfterRender() = 0;
		virtual void UpdateAfterRenderSubmit() = 0;
	};
}