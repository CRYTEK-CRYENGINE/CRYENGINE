// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc2/Prerequisites.h"

#include <CrySchematyc2/TemplateUtils/TemplateUtils_Delegate.h>
#include <CrySchematyc2/TemplateUtils/TemplateUtils_ScopedConnection.h>

namespace Schematyc2
{
	typedef TemplateUtils::CDelegate<void (TSerialize, int32, uint8, int)>        NetworkSerializeCallback;
	typedef std::pair<NetworkSerializeCallback, TemplateUtils::CScopedConnection> NetworkSerializeCallbackConnection;

	struct INetworkObject
	{
		enum class EAspectDelegation
		{
			Server, 
			Authority,
		};

		~INetworkObject() {}

		virtual bool RegisterNetworkSerializer(const NetworkSerializeCallbackConnection& callbackConnection, int32 aspects) = 0;
		virtual void MarkAspectsDirty(int32 aspects) = 0;
		virtual void SetAspectDelegation(int32 aspects, const EAspectDelegation delegation) = 0;
		virtual uint16 GetChannelId() const = 0;
		virtual bool ServerAuthority() const = 0;
		virtual bool ClientAuthority() const = 0;
		virtual bool LocalAuthority() const = 0;
	};
}
