// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryNetwork/INetwork.h>

namespace Schematyc
{
struct INetworkSpawnParams : public ISerializableInfo
{
	virtual ~INetworkSpawnParams() {}

	virtual _smart_ptr<INetworkSpawnParams> Clone() const = 0;
};

typedef _smart_ptr<INetworkSpawnParams> INetworkSpawnParamsPtr;   // #SchematycTODO : Can we replace _smart_ptr with std::share_ptr?

struct IObjectNetworkSpawnParams : public ISerializableInfo
{
	virtual ~IObjectNetworkSpawnParams() {}

	virtual void                   AddComponentSpawnParams(uint32 componentIdx, const INetworkSpawnParamsPtr& pParams) = 0;
	virtual INetworkSpawnParamsPtr GetComponentSpawnParams(uint32 componentIdx) const = 0;
};

typedef _smart_ptr<IObjectNetworkSpawnParams> IObjectNetworkSpawnParamsPtr; // #SchematycTODO : Can we replace _smart_ptr with std::share_ptr?

} // Schematyc
