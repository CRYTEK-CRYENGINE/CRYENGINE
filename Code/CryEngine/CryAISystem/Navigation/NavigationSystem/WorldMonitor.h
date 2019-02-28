// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __WorldMonitor_h__
#define __WorldMonitor_h__

#pragma once

namespace MNM
{
struct BoundingVolume;
}

class WorldMonitor
{
public:
	typedef Functor2<int, const AABB&> Callback;
	typedef MNM::BoundingVolume NavigationBoundingVolume;

	WorldMonitor();
	WorldMonitor(const Callback& callback);

	void Start();
	void Stop();

	// - fires all AABB changes that were queued asynchronously
	// - should be called on a regular basis (i. e. typically on each frame)
	void FlushPendingAABBChanges();

protected:
	bool IsEnabled() const;

	Callback m_callback;

	bool     m_enabled;

private:
	struct EntityAABBChange
	{
		AABB aabb;
		int entityId;
	};
	
	// physics event handlers
	static int StateChangeHandler(const EventPhys* pPhysEvent);
	static int EntityRemovedHandler(const EventPhys* pPhysEvent);
	static int EntityRemovedHandlerAsync(const EventPhys* pPhysEvent);

	// - common code for EntityRemovedHandler() and EntityRemovedHandlerAsync()
	// - inspects the type of physics entity in given event, returns true if we're interested in this kind of entity and fills given AABB
	static bool ShallEventPhysEntityDeletedBeHandled(const EventPhys* pPhysEvent, EntityAABBChange& outChange);

	CryMT::vector<EntityAABBChange> m_queuedAABBChanges;    // changes from the physical world that have been queued asynchronously; will be fired by FlushPendingAABBChanges()
};

#endif
