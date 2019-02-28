// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifndef MovementSystemCreator_h
	#define MovementSystemCreator_h

// The creator is used as a middle step, so that the code that wants
// to create the movement system doesn't necessarily have to include
// the movement system header and be recompiled every time it changes.
class MovementSystemCreator
{
public:
	struct IMovementSystem* CreateMovementSystem() const;
};

#endif // MovementSystemCreator_h
