// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace Serialization {
struct INavigationProvider;
}

namespace CharacterTool
{
struct System;
Serialization::INavigationProvider* CreateExplorerNavigationProvider(System* system);
}
