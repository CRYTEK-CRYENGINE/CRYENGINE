// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <CryLobby/ICryLobby.h>
#if NETWORK_HOST_MIGRATION
	#include "../../../CryExtensions/CryLobby/Module/LobbyCVars.h"
#endif

struct ICryLobbyPrivate : public ICryLobby
{
#if NETWORK_HOST_MIGRATION
	virtual CLobbyCVars& GetCVars() = 0;
#endif
};
