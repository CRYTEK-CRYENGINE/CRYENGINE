// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Objects/EntityObject.h"
#include "Objects/AreaBox.h"
#include "DesignerBaseObject.h"

class IDesignerEventHandler;
class CPickEntitiesPanel;
class ICrySizer;

namespace Designer
{
class AreaSolidObject final : public DesignerBaseObject<CAreaObjectBase>
{
public:
	DECLARE_DYNCREATE(AreaSolidObject)

	~AreaSolidObject();

	bool                       CreateGameObject();
	virtual void               InitVariables();
	void                       GetBoundBox(AABB& box);
	void                       GetLocalBounds(AABB& box);
	bool                       HitTest(HitContext& hc);
	void                       Display(CObjectRenderHelper& objRenderHelper);
	void                       DisplayMemoryUsage(DisplayContext& dc);

	bool                       Init(CBaseObject* prev, const string& file);

	virtual void               OnEntityAdded(IEntity const* const pIEntity) override;
	virtual void               OnEntityRemoved(IEntity const* const pIEntity) override;

	void                       CreateInspectorWidgets(CInspectorWidgetCreator& creator) override;

	void                       SetMaterial(IEditorMaterial* mtl);

	ModelCompiler*             GetCompiler() const override;

	void                       PostLoad(CObjectArchive& ar) override { UpdateGameArea(); }
	void                       InvalidateTM(int nWhyFlags) override;

	void                       Serialize(CObjectArchive& ar);
	XmlNodeRef                 Export(const string& levelPath, XmlNodeRef& xmlNode);

	void                       SetAreaId(int nAreaId);
	int                        GetAreaId() const { return m_areaId; }

	void                       UpdateGameArea();
	void                       UpdateGameResource() override { UpdateGameArea(); };

	void                       GenerateGameFilename(string& generatedFileName) const;

	void                       OnEvent(ObjectEvent event) override;
	void                       SetHidden(bool bHidden, bool bAnimated = false) override;
	bool                       IsHiddenByOption() override;

	std::vector<EDesignerTool> GetIncompatibleSubtools() override;
protected:
	//! Dtor must be protected.
	AreaSolidObject();

	void AddConvexhullToEngineArea(IEntityAreaComponent* pArea, std::vector<std::vector<Vec3>>& faces, bool bObstructrion);
	void DeleteThis() { delete this; };

	void Reload(bool bReloadScript = false) override;
	void OnAreaChange(IVariable* pVar) override;
	void OnSizeChange(IVariable* pVar) { UpdateGameArea(); }

	CVariable<float> m_innerFadeDistance;
	CVariable<int>   m_areaId;
	CVariable<float> m_edgeWidth;
	CVariable<int>   mv_groupId;
	CVariable<int>   mv_priority;
	CVariable<bool>  mv_filled;

	static int       s_nGlobalFileAreaSolidID;
	int              m_nUniqFileIdForExport;
	bool             m_bIgnoreGameUpdate;
	ICrySizer*       m_pSizer;
};

class AreaSolidClassDesc final : public CObjectClassDesc
{
public:
	virtual ObjectType     GetObjectType() override     { return OBJTYPE_VOLUMESOLID; };
	virtual const char*    ClassName() override         { return "AreaSolid"; };
	virtual const char*    UIName() override            { return "Solid"; };
	virtual const char*    Category() override          { return "Area"; };
	virtual CRuntimeClass* GetRuntimeClass() override   { return RUNTIME_CLASS(AreaSolidObject); };
	virtual const char*    GetToolClassName() override  { return "EditTool.CreateAreaSolidTool"; }
};
} // namespace Designer

