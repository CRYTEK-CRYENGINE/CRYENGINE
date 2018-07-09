// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __edmesh_h__
#define __edmesh_h__
#pragma once

#include "EdGeometry.h"
#include "Objects\SubObjSelection.h"
#include "TriMesh.h"

// Flags that can be set on CEdMesh.
enum CEdMeshFlags
{

};

//////////////////////////////////////////////////////////////////////////
// Description:
//    CEdMesh is a Geometry kind representing simple mesh.
//    Holds IStatObj interface from the 3D Engine.
//////////////////////////////////////////////////////////////////////////
class SANDBOX_API CEdMesh : public CEdGeometry
{
	DECLARE_DYNAMIC(CEdMesh)

public:
	//////////////////////////////////////////////////////////////////////////
	// CEdGeometry implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual EEdGeometryType GetType() const { return GEOM_TYPE_MESH; };

	virtual void            GetBounds(AABB& box);
	virtual CEdGeometry*    Clone();
	virtual IIndexedMesh*   GetIndexedMesh(size_t idx = 0);
	virtual void            GetTM(Matrix34* pTM, size_t idx = 0);
	virtual bool            StartSubObjSelection(const Matrix34& nodeWorldTM, int elemType, int nFlags);
	virtual void            EndSubObjSelection();
	virtual void            Display(DisplayContext& dc);
	virtual bool            HitTest(HitContext& hit);
	bool                    GetSelectionReferenceFrame(Matrix34& refFrame);
	//////////////////////////////////////////////////////////////////////////

	~CEdMesh();

	// Return filename of mesh.
	const string& GetFilename() const { return m_filename; };
	void           SetFilename(const string& filename);

	//! Reload geometry of mesh.
	void ReloadGeometry();
	void AddUser();
	void RemoveUser();
	int  GetUserCount() const { return m_nUserCount; };

	//////////////////////////////////////////////////////////////////////////
	void SetFlags(int nFlags) { m_nFlags = nFlags; };
	int  GetFlags()           { return m_nFlags; }
	//////////////////////////////////////////////////////////////////////////

	//! Access stored IStatObj.
	IStatObj* GetIStatObj() const { return m_pStatObj; }

	//! Returns true if filename and geomname refer to the same object as this one.
	bool IsSameObject(const char* filename);

	//! RenderMesh.
	void Render(SRendParams& rp, const SRenderingPassInfo& passInfo);

	//! Make new CEdMesh, if same IStatObj loaded, and CEdMesh for this IStatObj is allocated.
	//! This instance of CEdMesh will be returned.
	static CEdMesh* LoadMesh(const char* filename);

	// Creates a new mesh not from a file.
	// Create a new StatObj and IndexedMesh.
	static CEdMesh* CreateMesh(const char* name);

	//! Reload all geometries.
	static void ReloadAllGeometries();
	static void ReleaseAll();

	//! Check if default object was loaded.
	bool IsDefaultObject();

	//////////////////////////////////////////////////////////////////////////
	// Copy EdMesh data to the specified mesh.
	void CopyToMesh(CTriMesh& toMesh, int nCopyFlags);
	// Copy EdMesh data from the specified mesh.
	void CopyFromMesh(CTriMesh& fromMesh, int nCopyFlags, bool bUndo);

	// Retrieve mesh class.
	CTriMesh* GetMesh();

	//////////////////////////////////////////////////////////////////////////
	void InvalidateMesh();
	void SetWorldTM(const Matrix34& worldTM);

	// Save mesh into the file.
	// Optionally can provide pointer to the pak file where to save files into.
	void SaveToCGF(const char* sFilename, CPakFile* pPakFile = NULL, IMaterial* pMaterial = NULL);

	// Draw debug representation of this mesh.
	void DebugDraw(const SGeometryDebugDrawInfo& info);

private:
	//////////////////////////////////////////////////////////////////////////
	CEdMesh(IStatObj* pGeom);
	CEdMesh();
	void UpdateSubObjCache();
	void UpdateIndexedMeshFromCache(bool bFast);

	//////////////////////////////////////////////////////////////////////////
	struct SSubObjHitTestEnvironment
	{
		Vec3 vWSCameraPos;
		Vec3 vWSCameraVector;
		Vec3 vOSCameraVector;

		bool bHitTestNearest;
		bool bHitTestSelected;
		bool bSelectOnHit;
		bool bAdd;
		bool bRemove;
		bool bSelectValue;
		bool bHighlightOnly;
		bool bIgnoreBackfacing;
	};
	struct SSubObjHitTestResult
	{
		CTriMesh::EStream stream;      // To What stream of the TriMesh this result apply.
		MeshElementsArray elems;       // List of hit elements.
		float             minDistance; // Minimal distance to the hit.
		SSubObjHitTestResult() { minDistance = FLT_MAX; }
	};
	bool HitTestVertex(HitContext& hit, SSubObjHitTestEnvironment& env, SSubObjHitTestResult& result);
	bool HitTestEdge(HitContext& hit, SSubObjHitTestEnvironment& env, SSubObjHitTestResult& result);
	bool HitTestFace(HitContext& hit, SSubObjHitTestEnvironment& env, SSubObjHitTestResult& result);

	// Return`s true if selection changed.
	bool SelectSubObjElements(SSubObjHitTestEnvironment& env, SSubObjHitTestResult& result);
	bool IsHitTestResultSelected(SSubObjHitTestResult& result);

	//////////////////////////////////////////////////////////////////////////
	//! CGF filename.
	string   m_filename;
	IStatObj* m_pStatObj;
	int       m_nUserCount;
	int       m_nFlags;

	typedef std::map<string, CEdMesh*, stl::less_stricmp<string>> MeshMap;
	static MeshMap m_meshMap;

	// This cache is created when sub object selection is needed.
	struct SubObjCache
	{
		// Cache of data in geometry.
		// World space mesh.
		CTriMesh* pTriMesh;
		Matrix34  worldTM;
		Matrix34  invWorldTM;
		CBitArray m_tempBitArray;
		bool      bNoDisplay;

		SubObjCache() : pTriMesh(0), bNoDisplay(false) {};
	};
	SubObjCache*               m_pSubObjCache;

	std::vector<IIndexedMesh*> m_tempIndexedMeshes;
	std::vector<Matrix34>      m_tempMatrices;
};

#endif // __edmesh_h__

