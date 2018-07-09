// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __brushobject_h__
#define __brushobject_h__

#if _MSC_VER > 1000
	#pragma once
#endif

#include "Objects/BaseObject.h"
#include "CollisionFilteringProperties.h"
#include "Geometry/EdMesh.h"

/*!
 *	CTagPoint is an object that represent named 3d position in world.
 *
 */
class SANDBOX_API CBrushObject : public CBaseObject
{
public:
	DECLARE_DYNCREATE(CBrushObject)

	//////////////////////////////////////////////////////////////////////////
	// Ovverides from CBaseObject.
	//////////////////////////////////////////////////////////////////////////
	bool                     Init(CBaseObject* prev, const string& file);
	void                     Done();
	void                     Display(CObjectRenderHelper& objRenderHelper);
	bool                     CreateGameObject();

	void                     GetLocalBounds(AABB& box);

	bool                     HitTest(HitContext& hc);
	int                      HitTestAxis(HitContext& hc);

	virtual void             SetSelected(bool bSelect);
	virtual void             SetMinSpec(uint32 nSpec, bool bSetChildren = true);
	virtual IPhysicalEntity* GetCollisionEntity() const;

	void CreateInspectorWidgets(CInspectorWidgetCreator& creator) override;

	virtual float            GetCreationOffsetFromTerrain() const override { return 0.f; }

	void                     Serialize(CObjectArchive& ar);
	XmlNodeRef               Export(const string& levelPath, XmlNodeRef& xmlNode);

	virtual void             Validate();
	virtual string           GetAssetPath() const override;
	virtual void             GatherUsedResources(CUsedResources& resources);
	virtual bool             IsSimilarObject(CBaseObject* pObject);
	virtual bool             StartSubObjSelection(int elemType);
	virtual void             EndSubObjectSelection();
	virtual void             CalculateSubObjectSelectionReferenceFrame(SubObjectSelectionReferenceFrameCalculator* pCalculator);

	virtual CEdGeometry*     GetGeometry();
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	virtual IStatObj* GetIStatObj();
	int               GetRenderFlags() const                       { return m_renderFlags; };
	IRenderNode*      GetEngineNode() const                        { return m_pRenderNode; };
	float             GetRatioLod() const                          { return mv_ratioLOD; };
	float             GetRatioViewDist() const                     { return mv_ratioViewDist; };
	const string&     GetGeometryFile() const                      { return mv_geometryFile; }
	void              SetGeometryFile(const string& geometryFile) { mv_geometryFile = geometryFile; }

	//////////////////////////////////////////////////////////////////////////
	// Material
	//////////////////////////////////////////////////////////////////////////
	virtual void       SetMaterial(IEditorMaterial* mtl);
	virtual IEditorMaterial* GetRenderMaterial() const;
	virtual void       SetMaterialLayersMask(uint32 nLayersMask);

	virtual void InvalidateGeometryFile(const string& gamePath) override;

	bool  ApplyIndirLighting() const { return false; }

	void  SaveToCGF();
	void  ReloadGeometry();

	bool  IncludeForGI();

	void  CreateBrushFromMesh(const char* meshFilname);

	void  GetVerticesInWorld(std::vector<Vec3>& vertices) const;

protected:
	//! Dtor must be protected.
	CBrushObject();
	void         FreeGameData();

	virtual void UpdateVisibility(bool visible);

	//! Convert ray given in world coordinates to the ray in local brush coordinates.
	void WorldToLocalRay(Vec3& raySrc, Vec3& rayDir);

	bool ConvertFromObject(CBaseObject* object);
	void DeleteThis() { delete this; };
	void InvalidateTM(int nWhyFlags);
	void OnEvent(ObjectEvent event);

	void UpdateEngineNode(bool bOnlyTransform = false);

	void OnFileChange(string filename);

	//////////////////////////////////////////////////////////////////////////
	// Local callbacks.
	void OnGeometryChange(IVariable* var);
	void OnRenderVarChange(IVariable* var);
	void OnAIRadiusVarChange(IVariable* var);
	//////////////////////////////////////////////////////////////////////////

	virtual string GetMouseOverStatisticsText() const;

	virtual void UpdateHighlightPassState(bool bSelected, bool bHighlighted);

	AABB     m_bbox;
	Matrix34 m_invertTM;

	//! Engine node.
	//! Node that registered in engine and used to render brush prefab
	IRenderNode*        m_pRenderNode;
	_smart_ptr<CEdMesh> m_pGeometry;
	bool                m_bNotSharedGeom;

	//////////////////////////////////////////////////////////////////////////
	// Brush parameters.
	//////////////////////////////////////////////////////////////////////////
	CVariable<string> mv_geometryFile;

	//////////////////////////////////////////////////////////////////////////
	// Brush rendering parameters.
	//////////////////////////////////////////////////////////////////////////

	CCollisionFilteringProperties m_collisionFiltering;
	CVariable<bool>               mv_outdoor;
	CVariable<bool>    mv_castShadowMaps;
	CSmartVariable<bool> mv_giMode;
	CVariable<bool>    mv_dynamicDistanceShadows;
	CVariable<bool>    mv_rainOccluder;
	CVariable<bool>    mv_registerByBBox;
	CVariableEnum<int> mv_hideable;
	CVariable<int>   mv_ratioLOD;
	CVariable<int>   mv_ratioViewDist;
	CVariable<bool>  mv_excludeFromTriangulation;
	CVariable<bool>  mv_noDynWater;
	CVariable<float> mv_aiRadius;
	CVariable<bool>  mv_noDecals;
	CVariable<bool>  mv_recvWind;
	CVariable<bool>  mv_Occluder;
	CVariable<bool>  mv_drawLast;
	CVariable<int>   mv_shadowLodBias;

	//////////////////////////////////////////////////////////////////////////
	// Rendering flags.
	int               m_renderFlags;

	bool              m_bIgnoreNodeUpdate;
	bool              m_RePhysicalizeOnVisible;

private:
	// Brushes need to explicitly notify the NavMesh of their old AABB *before* it gets changed by transformation, because
	// scaling and rotation will re-physicalize the brush, and then the old AABB doesn't exist anymore and the physics system
	// will report the "wrong" old AABB to the NavMesh. In fact, what gets reported then is the new AABB, but in local space, and
	// that would cause the NavMesh to regenerate only a part of the affected area.
	virtual bool ShouldNotifyOfUpcomingAABBChanges() const override { return true; }

	// Initialize brush parameters from properties of the current geometry, if any.
	void ApplyStatObjProperties();
};

/*!
 * Class Description of CBrushObject.
 */
class CBrushObjectClassDesc : public CObjectClassDesc
{
public:
	ObjectType          GetObjectType()                     { return OBJTYPE_BRUSH; };
	const char*         ClassName()                         { return "Brush"; };
	const char*         Category()                          { return "Brush"; };
	CRuntimeClass*      GetRuntimeClass()                   { return RUNTIME_CLASS(CBrushObject); };
	const char*         GetFileSpec()                       { return "*.cgf"; };
	virtual const char* GetDataFilesFilterString() override { return GetFileSpec(); }
};

#endif // __brushobject_h__

