// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// Writes LastSession.json. Session data is information needed to launch Sandbox from code with the same
// parameters as the last time the user launched it manually.  In particular, this includes the project.
void WriteSessionData();
