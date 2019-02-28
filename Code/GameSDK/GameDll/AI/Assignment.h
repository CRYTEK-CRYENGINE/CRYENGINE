// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifndef Assignments_h
#define Assignments_h

struct Assignment
{
	// When changing this enum please maintain the same values for the types 
	// (so that validity of the saved games is kept) and the corresponding references to 
	// the global lua values set in ScriptBind_GameAI.cpp /Mario 08-12-2011
	enum Type
	{
		NoAssignment        = 0,
		DefendArea          = 1,
		HoldPosition        = 2,
		CombatMove          = 3,
		ScanSpot            = 4,
		ScorchSpot          = 5,
		PsychoCombatAllowed = 6,
	};

	Assignment()
		: type(NoAssignment)
	{}

	Assignment(Type type_, SmartScriptTable data_)
		: type(type_)
		, data(data_)
	{}

	Type type;
	SmartScriptTable data;
};

#endif //Assignments_h