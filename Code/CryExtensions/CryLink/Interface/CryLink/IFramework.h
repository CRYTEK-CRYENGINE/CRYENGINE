// Copyright 2001-2015 Crytek GmbH. All rights reserved.

#pragma once

#include <CryExtension/ICryUnknown.h>
#include <CryGame/IGameFramework.h>
#include <CryNetwork/ISimpleHttpServer.h>

namespace CryLinkService
{
	struct ICryLinkService
	{
		enum class EServiceState : uint16
		{ 
			NotStarted = 0,
			Running,
			Paused
		};

		virtual ~ICryLinkService() {}

		virtual bool Start(ISimpleHttpServer& httpServer) = 0;
		virtual void Stop() = 0;
		virtual void Pause(bool bPause) = 0;

		virtual EServiceState GetServiceState() const = 0;

		virtual void Update() = 0;
	};

	struct IFramework : public ICryUnknown
	{
		CRYINTERFACE_DECLARE_GUID(IFramework, "bddef21f-92c2-4cff-b5b5-0d8f89bd2350"_cry_guid)

		virtual ICryLinkService& GetService() = 0;
	};

	IFramework* CreateFramework( IGameFramework* pGameFramework);
}
