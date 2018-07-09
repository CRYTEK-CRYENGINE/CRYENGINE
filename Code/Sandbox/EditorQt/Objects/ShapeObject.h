// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAISystem/INavigationSystem.h>
#include <CrySystem/XML/IXml.h>
#include "EntityObject.h"
#include "SafeObjectsArray.h"
#include "Gizmos\AxisHelper.h"

#include <CryGame/IGameFramework.h>
#include <CryGame/IGameVolumes.h>

class CAIWaveObject;
class CSelectionGroup;
/*!
 *	CShapeObject is an object that represent named 3d position in world.
 *
 */
class SANDBOX_API CShapeObject : public CEntityObject
{
public:
	DECLARE_DYNCREATE(CShapeObject)

	//////////////////////////////////////////////////////////////////////////
	// Overrides from CBaseObject.
	//////////////////////////////////////////////////////////////////////////
	bool Init(CBaseObject* prev, const string& file);
	void InitVariables();
	void Done();
	bool HasMeasurementAxis() const { return true;  }
	void Display(CObjectRenderHelper& objRenderHelper);
	
	//////////////////////////////////////////////////////////////////////////
	void SetName(const string& name);

	//! Called when object is being created.
	int          MouseCreateCallback(IDisplayViewport* view, EMouseEvent event, CPoint& point, int flags);

	void         GetBoundBox(AABB& box);
	void         GetLocalBounds(AABB& box);

	bool         HitTest(HitContext& hc);

	void         OnEvent(ObjectEvent event);
	virtual void PostLoad(CObjectArchive& ar);

	virtual void SpawnEntity();
	void         Serialize(CObjectArchive& ar);
	XmlNodeRef   Export(const string& levelPath, XmlNodeRef& xmlNode);
	//////////////////////////////////////////////////////////////////////////

	int  GetAreaId();

	void SetClosed(bool bClosed);
	bool IsClosed() { return mv_closed; };

	//! Insert new point to shape at given index.
	//! @return index of newly inserted point.
	virtual int  InsertPoint(int index, const Vec3& point, bool const bModifying);
	//! Split shape on two shapes based on index. if snap is on, split precisely on the point, else use point to split the shape
	void         SplitAtPoint(int index, bool bSnap, Vec3 point);
	//! Remove point from shape at given index.
	virtual void RemovePoint(int index);
	//! Set edge index for merge.
	void         SetMergeIndex(int index);
	//! Merge shape to this.
	void         Merge(CShapeObject* shape);
	//! Remove all points.
	void         ClearPoints();

	//! Get number of points in shape.
	size_t       GetPointCount() const              { return m_points.size(); }
	//! Get point at index.
	Vec3 const&  GetPoint(size_t const index) const { return m_points[index]; }
	//! Set point position at specified index.
	virtual void SetPoint(int index, const Vec3& pos);

	//! Reverses the points of the path
	void ReverseShape();

	//! Reset z-value of points
	void ResetShape();

	void SelectPoint(int index);
	int  GetSelectedPoint() const { return m_selectedPoint; };

	//! Get shape height.
	float GetHeight() const { return mv_height; }

	//! Find shape point nearest to given point.
	int GetNearestPoint(const Vec3& raySrc, const Vec3& rayDir, float& distance);

	//! Find shape edge nearest to given point.
	void GetNearestEdge(const Vec3& pos, int& p1, int& p2, float& distance, Vec3& intersectPoint);

	//! Find shape edge nearest to given ray.
	void GetNearestEdge(const Vec3& raySrc, const Vec3& rayDir, int& p1, int& p2, float& distance, Vec3& intersectPoint);
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	virtual void SetSelected(bool bSelect);

	//////////////////////////////////////////////////////////////////////////
	// Binded entities.
	//////////////////////////////////////////////////////////////////////////
	//! Bind entity to this shape.
	void          AddEntity(CBaseObject* const pBaseObject);
	void          RemoveEntity(size_t const index);
	CBaseObject*  GetEntity(size_t const index);
	size_t        GetEntityCount() const { return m_entities.GetCount(); }

	virtual void  OnEntityAdded(IEntity const* const pIEntity);
	virtual void  OnEntityRemoved(IEntity const* const pIEntity);

	virtual void  OnContextMenu(CPopupMenuItem* menu);

	virtual float GetShapeZOffset() const { return m_zOffset; }
	virtual bool  SupportsZAxis() const { return true; }

	virtual void  CalcBBox();

	void          SerializeShapeTargets(Serialization::IArchive& ar);

	void CreateInspectorWidgets(CInspectorWidgetCreator& creator) override;
	void  SerializeProperties(Serialization::IArchive& ar, bool bMultiEdit);

	virtual void  SetInEditMode(bool editing) {}

	static void   StoreUndoShapeTargets(const std::vector<CBaseObject*>& objects);

	void          SaveShapeTargets(XmlNodeRef xmlNode);
	void          LoadShapeTargets(XmlNodeRef xmlNode);

protected:
	CShapeObject();
	~CShapeObject();

	virtual void PostClone(CBaseObject* pFromObject, CObjectCloneContext& ctx);

	bool         RayToLineDistance(const Vec3& rayLineP1, const Vec3& rayLineP2, const Vec3& pi, const Vec3& pj, float& distance, Vec3& intPnt);
	virtual int  GetMaxPoints() const   { return 100000; };
	virtual int  GetMinPoints() const   { return 3; };
	virtual void ReadEntityProperties() {};

	// Get obstruction colors to be used based on settings
	void GetSoundObstructionColors(ColorB& obstruction, ColorB& noObstruction);
	// Helper function to draw sound roof and floor
	void DisplaySoundRoofAndFloor(DisplayContext& dc);
	////! Calculate distance between
	//float DistanceToShape( const Vec3 &pos );
	void DrawTerrainLine(DisplayContext& dc, const Vec3& p1, const Vec3& p2);
	void AlignToGrid();

	// Ignore default draw highlight.
	void         DrawHighlight(DisplayContext& dc) {};

	virtual void EndCreation();

	//! Update game area.
	virtual void UpdateGameArea();

	virtual void EditShape();

	//overridden from CBaseObject.
	void         InvalidateTM(int nWhyFlags);
	virtual bool ConvertFromObject(CBaseObject* pObject) override;

	//! Called when shape variable changes.
	virtual void OnShapeChange(IVariable* pVariable);
	virtual void OnSizeChange(IVariable* pVariable);
	void         OnFormChange(IVariable* pVariable);
	void         OnSoundParamsChange(IVariable* var);

	//! Called when shape point sound obstruction variable changes.
	void OnPointChange(IVariable* var);

	void DeleteThis() { delete this; };

	void DisplayNormal(DisplayContext& dc);
	void DisplaySoundInfo(DisplayContext& dc);

private:

	void                  UpdateSoundPanelParams();
	void                  OnObjectEvent(CBaseObject* const pBaseObject, int const event);
	void                  UpdateAttachedEntities();
	void                  SetAreaProxy();
	IEntityAreaComponent* GetAreaProxy() const;

protected:

	AABB m_bbox;

	typedef std::vector<Vec3> AreaPoints;
	AreaPoints m_points;

	// Entities.
	CSafeObjectsArray m_entities;

	//////////////////////////////////////////////////////////////////////////
	// Shape parameters.
	//////////////////////////////////////////////////////////////////////////
	CVariable<float> mv_width;
	CVariable<float> mv_height;
	CVariable<int>   mv_areaId;
	CVariable<int>   mv_groupId;
	CVariable<int>   mv_priority;

	CVariable<bool>  mv_closed;
	//! Display triangles filling closed shape.
	CVariable<bool>  mv_displayFilled;
	CVariable<bool>  mv_displaySoundInfo;
	CVariable<float> m_innerFadeDistance;

	// For Mesh navigation.
	CVariable<bool>    m_vEnabled;
	CVariable<bool>    m_vExclusion;
	CVariable<float>   mv_agentWidth;
	CVariable<float>   mv_agentHeight;
	CVariable<float>   mv_VoxelOffsetX;
	CVariable<float>   mv_VoxelOffsetY;
	CVariable<bool>    mv_renderVoxelGrid;

	int                m_selectedPoint;
	float              m_lowestHeight;
	float              m_zOffset;
	float              m_shapePointMinDistance;

	uint32             m_bAreaModified              : 1;
	//! Forces shape to be always 2D. (all vertices lie on XY plane).
	uint32             m_bForce2D                   : 1;
	uint32             m_bDisplayFilledWhenSelected : 1;
	uint32             m_bIgnoreGameUpdate          : 1;
	uint32             m_bPerVertexHeight           : 1;
	uint32             m_bNoCulling                 : 1;

	static const float m_defaultZOffset;

	int                m_mergeIndex;

	bool               m_updateSucceed;

	bool               m_bBeingManuallyCreated;

	// Sound specific members
	std::vector<bool> m_abObstructSound;
	// use to store strings for inspector presentation of m_abObstructSound
	CVarBlockPtr      m_pOwnSoundVarBlock;

	bool              m_bskipInversionTools;
};

/*!
 * Class Description of ShapeObject.
 */
class CShapeObjectClassDesc : public CObjectClassDesc
{
public:
	ObjectType     GetObjectType()     { return OBJTYPE_SHAPE; };
	const char*    ClassName()         { return "Shape"; };
	const char*    Category()          { return "Area"; };
	CRuntimeClass* GetRuntimeClass()   { return RUNTIME_CLASS(CShapeObject); };
};

//////////////////////////////////////////////////////////////////////////
// Base class for all AI Shapes
//////////////////////////////////////////////////////////////////////////
class CAIShapeObjectBase : public CShapeObject
{
public:
	CAIShapeObjectBase() { m_entityClass = ""; }

	virtual string GetTypeDescription() const
	{
		// GetTypeDescription is overridden in CEntityObject to return m_entityClass
		// so we need to use default implementation from CBaseObject to get valid type description.
		return CBaseObject::GetTypeDescription();
	}

	virtual bool HasMeasurementAxis() const                           { return false; }
	XmlNodeRef   Export(const string& levelPath, XmlNodeRef& xmlNode) { return 0; };

protected:
	string m_lastGameArea;
};
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// Special object for AI Walkabale paths.
//////////////////////////////////////////////////////////////////////////
class CAIPathObject : public CAIShapeObjectBase
{
	DECLARE_DYNCREATE(CAIPathObject)
public:

	CAIPathObject();

	virtual void       Display(CObjectRenderHelper& objRenderHelper) override;
	virtual XmlNodeRef Export(const string& levelPath, XmlNodeRef& xmlNode) override { return 0; };

	virtual void       RemovePoint(int index) override;
	virtual int        InsertPoint(int index, const Vec3& point, bool const bModifying) override;
	virtual void       SetPoint(int index, const Vec3& pos) override;

	void CreateInspectorWidgets(CInspectorWidgetCreator& creator) override;

protected:

	virtual void Done() override;
	virtual void UpdateGameArea() override;
	virtual int  GetMinPoints() const override { return 2; }

private:

	bool IsSegmentValid(const Vec3& p0, const Vec3& p1, float rad);
	void DrawSphere(DisplayContext& dc, const Vec3& p0, float rad, int n);
	void DrawCylinder(DisplayContext& dc, const Vec3& p0, const Vec3& p1, float rad, int n);

	CVariable<bool>    m_bRoad;
	CVariableEnum<int> m_navType;
	CVariable<string>  m_anchorType;
	CVariable<bool>    m_bValidatePath;
};

//////////////////////////////////////////////////////////////////////////
// Generic AI shape
//////////////////////////////////////////////////////////////////////////
class CAIShapeObject : public CAIShapeObjectBase
{
	DECLARE_DYNCREATE(CAIShapeObject)
public:
	CAIShapeObject();

	XmlNodeRef Export(const string& levelPath, XmlNodeRef& xmlNode) { return 0; };

	void CreateInspectorWidgets(CInspectorWidgetCreator& creator) override;

protected:

	virtual void Done() override;
	virtual void UpdateGameArea() override;

private:

	CVariable<string>  m_anchorType;
	CVariableEnum<int> m_lightLevel;
};

//////////////////////////////////////////////////////////////////////////
// Special object for AI Walkabale paths.
//////////////////////////////////////////////////////////////////////////
class CAIOcclusionPlaneObject : public CAIShapeObjectBase
{
	DECLARE_DYNCREATE(CAIOcclusionPlaneObject)
public:
	CAIOcclusionPlaneObject();

	XmlNodeRef Export(const string& levelPath, XmlNodeRef& xmlNode) { return 0; };

protected:

	virtual void Done() override;
	virtual void UpdateGameArea() override;
};

//////////////////////////////////////////////////////////////////////////
// Object for general AI perception modification
//////////////////////////////////////////////////////////////////////////
class CAIPerceptionModifierObject : public CAIShapeObjectBase
{
	DECLARE_DYNCREATE(CAIPerceptionModifierObject)
public:
	CAIPerceptionModifierObject();
	// Override init variables to add our own, and not put irrelevant ones on the rollup bar
	void       InitVariables();
	XmlNodeRef Export(const string& levelPath, XmlNodeRef& xmlNode) { return 0; };

	void CreateInspectorWidgets(CInspectorWidgetCreator& creator) override;

protected:

	virtual void Done() override;
	virtual void UpdateGameArea() override;
	virtual void OnShapeChange(IVariable* pVariable) override;

private:
	CVariable<float> m_fReductionPerMetre; // Value is from 0-1, UI shows percent of 0-100
	CVariable<float> m_fReductionMax;      // Value is from 0-1, UI shows percent of 0-100
};

//////////////////////////////////////////////////////////////////////////
// Object for AI Territories
//////////////////////////////////////////////////////////////////////////
class CAITerritoryObject : public CAIShapeObjectBase
{
	DECLARE_DYNCREATE(CAITerritoryObject)
public:
	CAITerritoryObject();

	void       SetName(const string& newName);
	// Override init variables to add our own, and not put irrelevant ones on the rollup bar
	void       InitVariables();
	XmlNodeRef Export(const string& levelPath, XmlNodeRef& xmlNode) { return CEntityObject::Export(levelPath, xmlNode); }
	void       GetLinkedWaves(std::vector<CAIWaveObject*>& result);

	void CreateInspectorWidgets(CInspectorWidgetCreator& creator) override;

protected:

	virtual void Done() override;
	virtual void UpdateGameArea() override;

#ifdef USE_SIMPLIFIED_AI_TERRITORY_SHAPE
	virtual int GetMaxPoints() const { return 1; }
	virtual int GetMinPoints() const { return 1; }
#endif
};

//////////////////////////////////////////////////////////////////////////
// Object for volumes with custom game functionality
//////////////////////////////////////////////////////////////////////////
struct IGameVolumesEdit;

class SANDBOX_API CGameShapeObject : public CShapeObject
{
	DECLARE_DYNCREATE(CGameShapeObject)
public:
	CGameShapeObject();

	virtual bool       Init(CBaseObject* prev, const string& file) override;
	virtual void       InitVariables();
	virtual bool       CreateGameObject();
	virtual void       SetMaterial(IEditorMaterial* mtl);
	virtual void       SetMinSpec(uint32 nSpec, bool bSetChildren = true);
	virtual void       SetMaterialLayersMask(uint32 nLayersMask);
	virtual XmlNodeRef Export(const string& levelPath, XmlNodeRef& xmlNode);
	virtual void       DeleteEntity();
	virtual void       SpawnEntity();
	virtual void       CalcBBox();
	virtual void       CreateInspectorWidgets(CInspectorWidgetCreator& creator) override;


protected:

	virtual void      UpdateGameArea() override;
	virtual void      PostClone(CBaseObject* pFromObject, CObjectCloneContext& ctx);

	virtual int       GetMaxPoints() const;
	virtual int       GetMinPoints() const;
	virtual void      ReadEntityProperties() override;

	virtual void      OnPropertyChangeDone(IVariable* var);
	virtual void      OnShapeChange(IVariable* pVariable) override;
	virtual void      OnSizeChange(IVariable* pVariable) override;

	void              SetCustomMinAndMaxPoints();

	void              OnClassChanged(IVariable* var);
	void              OnClosedChanged(IVariable* pVariable);

	void              ReloadGameObject(const string& newClass);
	void              ResetDefaultVariables();
	void              UpdateGameShape();

	void              RefreshVariables();

	IGameVolumesEdit* GetGameVolumesEdit() const;

	template<typename T>
	bool GetParameterValue(T& outValue, const char* functionName) const;
	bool IsShapeOnly() const;
	bool GetSyncsPivotWithFirstPoint(bool& syncsPivotWithFirstPoint) const;
	bool GetForce2D(bool& bForce2D) const;
	bool GetZOffset(float& zOffset) const;
	bool GetMinPointDistance(float& minDistance) const;
	bool GetIsClosedShape(bool& isClosed) const;
	bool GetCanEditClosed(bool& canEditClosed) const;
	bool NeedExportToGame() const;

	void NotifyPropertyChange();

private:

	CVariableEnum<int> m_shapeClasses;
	bool               m_syncsPivotWithFirstPoint;
	int                m_minPoints;
	int                m_maxPoints;
	bool               m_bInitialized;
};

class CGameShapeLedgeObject : public CGameShapeObject
{
	DECLARE_DYNCREATE(CGameShapeLedgeObject)
public:
	CGameShapeLedgeObject();

	virtual void InitVariables();
	virtual void SetPoint(int index, const Vec3& pos);

protected:

	virtual void UpdateGameArea();
};

class CGameShapeLedgeStaticObject : public CGameShapeLedgeObject
{
	DECLARE_DYNCREATE(CGameShapeLedgeStaticObject)
public:
	CGameShapeLedgeStaticObject();
};

//////////////////////////////////////////////////////////////////////////
// Object for Navigation Surfaces and Volumes
//////////////////////////////////////////////////////////////////////////
class CNavigationAreaObject : public CAIShapeObjectBase, public INavigationSystem::INavigationSystemListener
{
	DECLARE_DYNCREATE(CNavigationAreaObject)

protected:
	enum ERelinkWithMeshesMode
	{
		eRelinkOnly     = 0,
		eUpdateGameArea = BIT(0),
		ePostLoad       = BIT(1)
	};

public:
	CNavigationAreaObject();
	virtual ~CNavigationAreaObject();

	virtual void       Serialize(CObjectArchive& ar);

	void CreateInspectorWidgets(CInspectorWidgetCreator& creator) override;

	virtual void       Display(CObjectRenderHelper& objRenderHelper);
	virtual void       PostLoad(CObjectArchive& ar);
	// Override init variables to add our own, and not put irrelevant ones on the rollup bar
	virtual void       InitVariables();
	virtual XmlNodeRef Export(const string& levelPath, XmlNodeRef& xmlNode) { return CEntityObject::Export(levelPath, xmlNode); }
	virtual void       SetName(const string& name);

	virtual void       RemovePoint(int index);
	virtual int        InsertPoint(int index, const Vec3& point, bool const bModifying);
	virtual void       SetPoint(int index, const Vec3& pos);
	virtual void       OnSizeChange(IVariable* pVariable) override;
	virtual void       EndCreation() override;

	virtual bool       SupportsZAxis() const final { return false; }

	virtual void       OnContextMenu(CPopupMenuItem* menu) override;

	//////////////////////////////////////////////////////////////////////////
	virtual void ChangeColor(COLORREF color);

	virtual void OnShapeTypeChange(IVariable* var);
	virtual void OnShapeAgentTypeChange(IVariable* var);

	virtual void OnNavigationEvent(const INavigationSystem::ENavigationEvent event);

	void         UpdateMeshes();
	void         ApplyExclusion(bool apply);
	void         RelinkWithMesh(const ERelinkWithMeshesMode relinkMode);
	void         CreateVolume(Vec3* points, size_t pointsSize, NavigationVolumeID requestedID = NavigationVolumeID(0));
	void         DestroyVolume();

	virtual void SetInEditMode(bool editing) override;

protected:

	void         RegisterNavigationArea(INavigationSystem& navigationSystem, const char* szAreaName);
	void         ReregisterNavigationAreaAfterReload();

	void         RegisterNavigationListener(INavigationSystem& navigationSystem, const char* szAreaName);
	void         UnregisterNavigationListener(INavigationSystem& navigationSystem);

	virtual void Done() override;
	virtual void UpdateGameArea() override;

	CVariable<bool>               mv_exclusion;
	CVariableArray                mv_agentTypes;

	std::vector<CVariable<bool>>  mv_agentTypeVars;
	std::vector<NavigationMeshID> m_meshes;
	NavigationVolumeID            m_volume;
	bool                          m_bRegistered;
	bool                          m_bListenerRegistered;
	bool                          m_bEditingPoints;
	bool                          m_updateAfterEdit;
};

/*!
 * Class Description of CAIPathObject.
 */
class CAIPathObjectClassDesc : public CObjectClassDesc
{
public:
	ObjectType     GetObjectType()     { return OBJTYPE_SHAPE; };
	const char*    ClassName()         { return "AIPath"; };
	const char*    Category()          { return "AI"; };
	CRuntimeClass* GetRuntimeClass()   { return RUNTIME_CLASS(CAIPathObject); };
};

/*!
 * Class Description of CAIShapeObject.
 */
class CAIShapeObjectClassDesc : public CObjectClassDesc
{
public:
	ObjectType     GetObjectType()     { return OBJTYPE_SHAPE; };
	const char*    ClassName()         { return "AIShape"; };
	const char*    Category()          { return "AI"; };
	CRuntimeClass* GetRuntimeClass()   { return RUNTIME_CLASS(CAIShapeObject); };
};

/*!
 * Class Description of CAIOcclusionPlaneObject.
 */
class CAIOcclusionPlaneObjectClassDesc : public CObjectClassDesc
{
public:
	ObjectType     GetObjectType()     { return OBJTYPE_SHAPE; };
	const char*    ClassName()         { return "AIHorizontalOcclusionPlane"; };
	const char*    Category()          { return "AI"; };
	CRuntimeClass* GetRuntimeClass()   { return RUNTIME_CLASS(CAIOcclusionPlaneObject); };
};

/*!
 * Class Description of CAIPerceptionModifierObject.
 */
class CAIPerceptionModifierObjectClassDesc : public CObjectClassDesc
{
public:
	ObjectType     GetObjectType()     { return OBJTYPE_SHAPE; };
	const char*    ClassName()         { return "AIPerceptionModifier"; };
	const char*    Category()          { return "AI"; };
	CRuntimeClass* GetRuntimeClass()   { return RUNTIME_CLASS(CAIPerceptionModifierObject); };
};

/*!
 * Class Description of CGameShapeObject
 */
class CGameShapeObjectClassDesc : public CObjectClassDesc
{
public:
	ObjectType     GetObjectType()     { return OBJTYPE_SHAPE; }
	const char*    ClassName()         { return "GameVolume"; }
	const char*    Category()          { return "Area"; }
	CRuntimeClass* GetRuntimeClass()   { return RUNTIME_CLASS(CGameShapeObject); }
	virtual bool   IsCreatable() const override
	{
		if (gEnv->pGameFramework == nullptr)
			return false;

		IGameVolumes* pGameVolumesMgr = gEnv->pGameFramework->GetIGameVolumesManager();
		if (pGameVolumesMgr == nullptr)
			return false;

		IGameVolumesEdit* pGameVolumesEdit = pGameVolumesMgr->GetEditorInterface();
		if (pGameVolumesEdit == nullptr)
			return false;

		return pGameVolumesEdit->GetVolumeClassesCount() > 0;
	}
	virtual bool  IsCreatedByListEnumeration() override { return false; }
};

/*!
 * Class Description of CGameShapeLedgeObject
 */
class CGameShapeLedgeObjectClassDesc : public CObjectClassDesc
{
public:
	ObjectType     GetObjectType()     { return OBJTYPE_SHAPE; }
	const char*    ClassName()         { return "Ledge"; }
	const char*    Category()          { return "GameCustom"; }
	CRuntimeClass* GetRuntimeClass()   { return RUNTIME_CLASS(CGameShapeLedgeObject); }
	virtual bool   IsCreatable() const override { return gEnv->pEntitySystem->GetClassRegistry()->FindClass("LedgeObject") != nullptr; }
};

/*!
 * Class Description of CGameShapeLedgeStaticObject
 */
class CGameShapeLedgeStaticObjectClassDesc : public CObjectClassDesc
{
public:
	ObjectType     GetObjectType()     { return OBJTYPE_SHAPE; }
	const char*    ClassName()         { return "LedgeStatic"; }
	const char*    Category()          { return "GameCustom"; }
	CRuntimeClass* GetRuntimeClass()   { return RUNTIME_CLASS(CGameShapeLedgeStaticObject); }
	virtual bool   IsCreatable() const override { return gEnv->pEntitySystem->GetClassRegistry()->FindClass("LedgeObjectStatic") != nullptr; }
};
/*!
 * Class Description of CNavigationAreaObject, Surface
 */
class CNavigationAreaObjectDesc : public CObjectClassDesc
{
public:
	ObjectType     GetObjectType()     { return OBJTYPE_SHAPE; }
	const char*    ClassName()         { return "NavigationArea"; }
	const char*    Category()          { return "AI"; }
	CRuntimeClass* GetRuntimeClass()   { return RUNTIME_CLASS(CNavigationAreaObject); }
};

/*!
 * Class Description of CAITerritoryObject.
 */
class CAITerritoryObjectClassDesc : public CObjectClassDesc
{
public:
	ObjectType     GetObjectType()     { return OBJTYPE_SHAPE; };
	const char*    ClassName()         { return "Entity::AITerritory"; };
	const char*    Category()          { return ""; };
	CRuntimeClass* GetRuntimeClass()   { return RUNTIME_CLASS(CAITerritoryObject); };
};

