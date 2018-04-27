// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAISystem/MovementUpdateContext.h>
#include <CryAISystem/MovementBlock.h>


namespace Movement
{

	/// Plan monitors can be installed in an IPlan to keep track of the "health status" of specific movement blocks.
	/// The underlying planner will periodically consult these monitors and re-plan if they say so.
	struct IPlanMonitor
	{
		virtual bool        CheckForReplanning(const MovementUpdateContext& context) = 0;

	protected:
		~IPlanMonitor() {} // not intended to get deleted via base-class pointers
	};

	struct IPlan
	{
		virtual void        InstallPlanMonitor(IPlanMonitor* pMonitorToInstall) = 0;
		virtual void        UninstallPlanMonitor(IPlanMonitor* pMonitorToUninstall) = 0;

	protected:
		~IPlan() {}         // not intended to get deleted via base-class pointers
	};

}
