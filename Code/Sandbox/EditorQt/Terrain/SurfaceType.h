// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __SurfaceType_h__
#define __SurfaceType_h__

#if _MSC_VER > 1000
	#pragma once
#endif

#define MAX_SURFACE_TYPE_ID_COUNT 127 // Was 15 in CE2

/** Defines axises of projection for detail texture.
 */
enum ESurfaceTypeProjectionAxis
{
	ESFT_X,
	ESFT_Y,
	ESFT_Z,
};

/** CSurfaceType describe parameters of terrain surface.
    Surface types are applied to the layers, total of 7 surface types are currently supported.
 */
class SANDBOX_API CSurfaceType : public _i_reference_target_t
{
public:
	CSurfaceType();
	~CSurfaceType();

	CSurfaceType(const CSurfaceType& st) { *this = st; }

	void           SetSurfaceTypeID(int nID) { m_nSurfaceTypeID = nID; };
	int            GetSurfaceTypeID() const  { return m_nSurfaceTypeID; };
	void           AssignUnusedSurfaceTypeID();

	void           SetName(const string& name)             { m_name = name; };
	const string& GetName() const                          { return m_name; }

	void           SetDetailTexture(const string& tex)     { m_detailTexture = tex; };
	const string& GetDetailTexture() const                 { return m_detailTexture; }

	void           SetBumpmap(const string& tex)           { m_bumpmap = tex; };
	const string& GetBumpmap() const                       { return m_bumpmap; }

	void           SetDetailTextureScale(const Vec3& scale) { m_detailScale[0] = scale.x; m_detailScale[1] = scale.y; }
	Vec3           GetDetailTextureScale() const            { return Vec3(m_detailScale[0], m_detailScale[1], 0); }

	void           SetMaterial(const string& mtl);
	const string& GetMaterial() const                  { return m_material; }

	void           AddDetailObject(const string& name) { m_detailObjects.push_back(name); };
	int            GetDetailObjectCount() const         { return m_detailObjects.size(); };
	void           RemoveDetailObject(int i)            { m_detailObjects.erase(m_detailObjects.begin() + i); };
	const string& GetDetailObject(int i) const         { return m_detailObjects[i]; };

	void           AddLayerReference()                  { m_nLayerReferences++; };
	void           RemoveLayerReference()               { m_nLayerReferences--; };
	int            GetLayerReferenceCount() const       { return m_nLayerReferences; };

	//! Set detail texture projection axis.
	void SetProjAxis(int axis) { m_projAxis = axis; }

	//! Get detail texture projection axis.
	int           GetProjAxis() const { return m_projAxis; }

	CSurfaceType& operator=(const CSurfaceType& st)
	{
		m_name = st.m_name;
		m_material = st.m_material;
		m_detailTexture = st.m_detailTexture;
		m_detailObjects = st.m_detailObjects;
		m_detailScale[0] = st.m_detailScale[0];
		m_detailScale[1] = st.m_detailScale[1];
		m_projAxis = st.m_projAxis;
		m_bumpmap = st.m_bumpmap;
		return *this;
	}

	void Serialize(class CXmlArchive& xmlAr);
	void Serialize(XmlNodeRef xmlRootNode, bool boLoading);
private:
	void SaveVegetationIds(XmlNodeRef& node);

	//! Name of surface type.
	string m_name;
	//! Detail texture applied to this surface type.
	string m_detailTexture;
	//! Bump map for this surface type.
	string m_bumpmap;
	//! Detail texture tiling.
	float   m_detailScale[2];
	string m_material;

	//! Array of detail objects used for this layer.
	std::vector<string> m_detailObjects;

	//! Detail texture projection axis.
	int m_projAxis;

	int m_nLayerReferences;

	// Surface type in in 3dengine, maximum 15
	int m_nSurfaceTypeID;
};

#endif // __SurfaceType_h__

