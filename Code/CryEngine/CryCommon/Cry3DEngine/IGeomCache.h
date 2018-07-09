// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// ------------------------------------------------------------------------
//  File name:   IGeomCache.h
//  Created:     19/7/2012 by Axel Gneiting
//  Description: Interface for CGeomCache class
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#ifndef _IGeomCache_H_
#define _IGeomCache_H_

#include <CryCore/smartptr.h>     // TYPEDEF_AUTOPTR

//! Interface to hold geom cache data.
struct IGeomCache : public IStreamable
{
	//! Notifies that the object is being used.
	//! Increase the reference count of the object.
	virtual int AddRef() = 0;

	//! Notifies that the object is no longer needed.
	//! Decrease the reference count of the object.
	//! If the reference count reaches zero, the object will be deleted from memory.
	virtual int Release() = 0;

	//! Checks if the geometry cache was successfully loaded from disk.
	//! \return true if valid, otherwise false
	virtual bool IsValid() const = 0;

	//! Set default material for the geometry.
	//! \param pMaterial Valid pointer to the material.
	virtual void SetMaterial(IMaterial* pMaterial) = 0;

	//! Returns default material of the geometry.
	//! \param nType Pass 0 to get the physic geometry or pass 1 to get the obstruct geometry.
	//! \return Pointer to a phys_geometry class.
	virtual _smart_ptr<IMaterial>       GetMaterial() = 0;
	virtual const _smart_ptr<IMaterial> GetMaterial() const = 0;

	//! Returns the filename of the object
	//! \return A null terminated string which contain the filename of the object.
	virtual const char* GetFilePath() const = 0;

	//! Returns the duration of the geom cache animation
	//! \return float value in seconds
	virtual float GetDuration() const = 0;

	//! Reloads the cache. Need to call this when cache file changed.
	virtual void Reload() = 0;

	struct SStatistics
	{
		bool  m_bPlaybackFromMemory;
		float m_averageAnimationDataRate;
		uint  m_numStaticMeshes;
		uint  m_numStaticVertices;
		uint  m_numStaticTriangles;
		uint  m_numAnimatedMeshes;
		uint  m_numAnimatedVertices;
		uint  m_numAnimatedTriangles;
		uint  m_numMaterials;
		uint  m_staticDataSize;
		uint  m_diskAnimationDataSize;
		uint  m_memoryAnimationDataSize;
	};

	virtual SStatistics GetStatistics() const = 0;

protected:

	//! Should be never called, use Release() instead.
	virtual ~IGeomCache() {};
};

#endif // _IGeomCache_H_
