// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ObjectEvent.h"

enum RefCoordSys;

// Windows API defines this
#ifdef GetObject
#undef GetObject
#endif

struct ISelectionGroup
{
public:
	virtual ~ISelectionGroup() {}

	//! Adds object into selection list.
	virtual void AddObject(CBaseObject* obj) = 0;
	//! Remove object from selection list.
	virtual void RemoveObject(CBaseObject* obj) = 0;
	//! Get object at given index.
	virtual CBaseObject* GetObject(int index) const = 0;
	//! Number of selected object.
	virtual int GetCount() const = 0;
	//! Get mass center of selected objects.
	virtual Vec3 GetCenter() const = 0;
	//! Get Bounding box of selection.
	virtual AABB GetBounds() const = 0;
	//! Send event to all objects in selection group.
	virtual void SendEvent(ObjectEvent event) const = 0;
	//! Get matrix suitable for manipulator
	virtual bool GetManipulatorMatrix(RefCoordSys coordSys, Matrix34& tm) const = 0;
};


