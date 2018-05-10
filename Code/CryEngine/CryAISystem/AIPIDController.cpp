// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AIPIDController.h"
void CAIPIDController::Serialize(TSerialize ser)
{
	ser.BeginGroup("AIPIDController");
	ser.Value("CP", CP);
	ser.Value("CI", CI);
	ser.Value("CD", CD);
	ser.Value("runningIntegral", runningIntegral);
	ser.Value("lastError", lastError);
	ser.Value("proportionalPower", proportionalPower);
	ser.Value("integralTimescale", integralTimescale);
	ser.EndGroup();
}
