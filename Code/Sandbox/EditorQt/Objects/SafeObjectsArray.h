// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Objects/BaseObject.h"

/** This class used as a safe collection of references to CBaseObject instances.
    Target objects in this collection can be safely removed or added,
    This object makes references to added objects and receive back event when those objects are deleted.
 */
class CSafeObjectsArray
{
public:
	CSafeObjectsArray() {};
	~CSafeObjectsArray();

	void         Add(CBaseObject* obj);
	void         Remove(CBaseObject* obj);

	bool         IsEmpty() const  { return m_objects.empty(); }
	size_t       GetCount() const { return m_objects.size(); }
	CBaseObject* Get(size_t const index) const;

	// Clear array.
	void Clear();

	void RegisterEventCallback(CBaseObject::EventCallback const& cb);
	void UnregisterEventCallback(CBaseObject::EventCallback const& cb);

private:
	void OnTargetEvent(CBaseObject* target, int event);

	//////////////////////////////////////////////////////////////////////////
	std::vector<CBaseObjectPtr>             m_objects;
	std::vector<CBaseObject::EventCallback> m_eventListeners;
};

