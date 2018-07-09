// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __MOVEMENTBLOCK_H__
#define __MOVEMENTBLOCK_H__

#include <CryAISystem/IPathfinder.h>

struct IMovementActor;
struct MovementUpdateContext;
class MovementStyle;

namespace Movement
{
struct Block
{
	enum Status
	{
		Running,
		Finished,
		CantBeFinished,
	};

	// Allocate and deallocate memory in the ctor and dtor only.
	// End will not be called when resetting the movement system
	// as the entities have been erased at that point and a
	// healthy movement actor cannot be passed in.
	virtual ~Block() {}
	virtual void        Begin(IMovementActor& actor)                 {}
	virtual void        End(IMovementActor& actor)                   {}
	virtual Status      Update(const MovementUpdateContext& context) { return Finished; }
	virtual bool        InterruptibleNow() const                     { return false; }
	virtual const char* GetName() const = 0;
};

typedef std::shared_ptr<Movement::Block>                                                                           BlockPtr;
typedef Functor3wRet<const INavPath&, const PathPointDescriptor::OffMeshLinkData&, const MovementStyle&, BlockPtr> CustomNavigationBlockCreatorFunction;
}

#endif // __MOVEMENTBLOCK_H__
