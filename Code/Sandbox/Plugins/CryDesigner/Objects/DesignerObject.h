// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "DesignerBaseObject.h"
#include "IObjectEnumerator.h"

namespace Designer
{
class ModelCompiler;
class IDesignerPanel;
class IDesignerSubPanel;
class DesignerObject;

struct DesignerObjectFlags
{
	bool            outdoor;
	bool            castShadows;
	bool            giMode;
	bool            supportSecVisArea;
	bool            rainOccluder;
	bool            hideable;
	int             ratioViewDist;
	bool            excludeFromTriangulation;
	bool            noDynWater;
	bool            noStaticDecals;
	bool            excludeCollision;
	bool            occluder;

	DesignerObject* m_pObj;

	DesignerObjectFlags();
	void Set();
	void Update();
	void Serialize(Serialization::IArchive& ar);
};

class DesignerObject : public DesignerBaseObject<::CBaseObject>
{
public:
	DECLARE_DYNCREATE(DesignerObject)

	DesignerObject();
	virtual ~DesignerObject(){}

	bool                     Init(::CBaseObject* prev, const string& file) override;
	void                     Display(CObjectRenderHelper& objRenderHelper) override;

	void                     GetBoundBox(AABB& box) override;
	void                     GetLocalBounds(AABB& box) override;

	bool                     HitTest(HitContext& hc) override;
	virtual IPhysicalEntity* GetCollisionEntity() const override;

	virtual void             OnContextMenu(CPopupMenuItem* menu);

	void CreateInspectorWidgets(CInspectorWidgetCreator& creator) override;

	void                 Serialize(CObjectArchive& ar) override;

	void                 SetMaterial(IEditorMaterial* mtl) override;
	void                 SetMaterial(const string& materialName) override;
	
	string              GetMaterialName() const override;

	void                 SetMaterialLayersMask(uint32 nLayersMask);
	void                 SetMinSpec(uint32 nSpec, bool bSetChildren = true);

	bool                 IsSimilarObject(CBaseObject* pObject) const;
	void                 OnEvent(ObjectEvent event);

	XmlNodeRef           Export(const string& levelPath, XmlNodeRef& xmlNode) { return 0; }

	void                 GenerateGameFilename(string& generatedFileName) const;

	IRenderNode*         GetEngineNode() const;

	void                 UpdateEngineFlags() { m_EngineFlags.Update(); }

	DesignerObjectFlags& GetEngineFlags()    { return m_EngineFlags; }

	virtual float GetCreationOffsetFromTerrain() const override { return 0.0f; }

	std::vector<EDesignerTool> GetIncompatibleSubtools() override;

protected:
	void WorldToLocalRay(Vec3& raySrc, Vec3& rayDir) const;

	void DeleteThis() { delete this; };
	void InvalidateTM(int nWhyFlags);

	void DrawDimensions(DisplayContext& dc, AABB* pMergedBoundBox);
	void DrawOpenPolygons(DisplayContext& dc);

private:

	Matrix34            m_invertTM;
	uint32              m_nBrushUniqFileId;

	DesignerObjectFlags m_EngineFlags;

	string m_materialNameOverride; // Used to store the material name in case that the material could not be loaded
};

class DesignerObjectClassDesc : public CObjectClassDesc
{
public:
	ObjectType     GetObjectType()     { return OBJTYPE_SOLID; };
	const char*    ClassName()         { return "Designer"; };
	const char*    Category()          { return "Designer"; };
	CRuntimeClass* GetRuntimeClass()   { return RUNTIME_CLASS(DesignerObject); };
	const char*    GetFileSpec()       { return ""; };

	virtual bool IsCreatedByListEnumeration() override { return false; }
	virtual void EnumerateObjects(IObjectEnumerator* pEnumerator)
	{
		pEnumerator->AddEntry("Designer Box", "DesignerBox");
		pEnumerator->AddEntry("Designer Sphere", "DesignerSphere");
		pEnumerator->AddEntry("Designer Cone", "DesignerCone");
		pEnumerator->AddEntry("Designer Cylinder", "DesignerCylinder");
	}
};

////////// Primitive designer object types : WARNING - LEGACY CODE, DO NOT USE! /////////////

class BoxObjectClassDesc : public CObjectClassDesc
{
public:
	ObjectType     GetObjectType()                  { return OBJTYPE_SOLID; };
	const char*    ClassName()                      { return "Designer Box"; };
	const char*    Category()                       { return "Designer"; };
	CRuntimeClass* GetRuntimeClass()                { return RUNTIME_CLASS(DesignerObject); };
	const char*    GetFileSpec()                    { return ""; };
	bool           IsCreatable() const              { return false; }
	const char*    GetSuccessorClassName() override { return "Designer"; }
};

class SphereObjectClassDesc : public CObjectClassDesc
{
public:
	ObjectType     GetObjectType()     { return OBJTYPE_SOLID; };
	const char*    ClassName()         { return "Designer Sphere"; };
	const char*    Category()          { return "Designer"; };
	CRuntimeClass* GetRuntimeClass()   { return RUNTIME_CLASS(DesignerObject); };
	const char*    GetFileSpec()       { return ""; };

	bool        IsCreatable() const              { return false; }
	const char* GetSuccessorClassName() override { return "Designer"; }
};

class ConeObjectClassDesc : public CObjectClassDesc
{
public:
	ObjectType     GetObjectType()                  { return OBJTYPE_SOLID; };
	const char*    ClassName()                      { return "Designer Cone"; };
	const char*    Category()                       { return "Designer"; };
	CRuntimeClass* GetRuntimeClass()                { return RUNTIME_CLASS(DesignerObject); };
	const char*    GetFileSpec()                    { return ""; };
	bool           IsCreatable() const              { return false; }
	const char*    GetSuccessorClassName() override { return "Designer"; }
};

class CylinderObjectClassDesc : public CObjectClassDesc
{
public:
	ObjectType     GetObjectType()                  { return OBJTYPE_SOLID; };
	const char*    ClassName()                      { return "Designer Cylinder"; };
	const char*    Category()                       { return "Designer"; };
	CRuntimeClass* GetRuntimeClass()                { return RUNTIME_CLASS(DesignerObject); };
	const char*    GetFileSpec()                    { return ""; };
	bool           IsCreatable() const              { return false; }
	const char*    GetSuccessorClassName() override { return "Designer"; }
};

class SolidObjectClassDesc : public CObjectClassDesc
{
public:
	ObjectType     GetObjectType()     { return OBJTYPE_SOLID; };
	const char*    ClassName()         { return "Solid"; };
	const char*    Category()          { return ""; };
	CRuntimeClass* GetRuntimeClass()   { return RUNTIME_CLASS(DesignerObject); };
	const char*    GetFileSpec() { return ""; };
};
}

