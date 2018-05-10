// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryMath/Cry_Geo.h>

//////////////////////////////////////////////////////////////////////////
// IMeshObj:
//		Minimal base for static and animated mesh objects
//		Uses CryNameId facilities, to support cryinterface_cast<>, to query for derived classes.
//		But not based on ICryUnknown, as existing mesh classes are not compatible with ICryUnknown creation and ref-counting.

struct IMeshObj
{
	virtual ~IMeshObj() {}

	//! Standard reference-counting methods
	virtual int AddRef() = 0;
	virtual int Release() = 0;
	virtual int GetRefCount() const = 0;

	//! Get the local bounding box
	virtual AABB GetAABB() const = 0;

	//! Get the object radius or radius^2
	virtual float GetRadiusSqr() const { return GetAABB().GetRadiusSqr(); }
	virtual float GetRadius() const    { return GetAABB().GetRadius(); }

	//! Get the mesh extent in the requested dimension
	virtual float GetExtent(EGeomForm eForm) = 0;

	//! Generate a random point in the requested dimension
	virtual void GetRandomPoints(Array<PosNorm> points, CRndGen& seed, EGeomForm eForm) const = 0;

	//! Return the rendering material
	virtual struct IMaterial* GetMaterial() const = 0;

	//! Return the rendering geometry
	virtual struct IRenderMesh* GetRenderMesh() const = 0;

	//! Return the associated physical entity
	virtual struct IPhysicalEntity* GetPhysEntity() const = 0;

	//! Return the physical representation of the object.
	//! Arguments:
	//!     nType - one of PHYS_GEOM_TYPE_'s, or an explicit slot index
	virtual struct phys_geometry* GetPhysGeom(int nType = PHYS_GEOM_TYPE_DEFAULT) const = 0;

	//! Render the object
	virtual void Render(const struct SRendParams& rParams, const SRenderingPassInfo& passInfo) = 0;
};
