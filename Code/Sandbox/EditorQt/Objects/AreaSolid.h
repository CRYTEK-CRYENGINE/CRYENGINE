// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2011
// -------------------------------------------------------------------------
//  File name:   AreaSolid.h
//  Created:     13/9/2011 by Jaesik
//  Description: AreaSolid object definition
//
////////////////////////////////////////////////////////////////////////////
#include "EntityObject.h"
#include "AreaBox.h"
#include "Brush/BrushUtil.h"
#include "Brush/DesignerBaseObject.h"

class IDesignerEventHandler;
class CPickEntitiesPanel;
class CCreateAreaSolidPanel;
class CEditAreaSolidPanel;
class CAreaSolidPanel;

class CAreaSolid : public CDesignerBaseObject<CAreaObjectBase>
{
public:
	DECLARE_DYNCREATE(CAreaSolid)

	bool             CreateGameObject();
	virtual void     InitVariables();
	void             GetBoundBox(AABB& box);
	void             GetLocalBounds(AABB& box);
	bool             HitTest(HitContext& hc);
	void             Display(DisplayContext& dc);
	void             DisplayMemoryUsage(DisplayContext& dc);

	bool             Init(IEditor* ie, CBaseObject* prev, const CString& file);
	void             Done();

	void             BeginEditParams(IEditor* ie, int flags);
	void             EndEditParams(IEditor* ie);

	void             SetMaterial(IEditorMaterial* mtl);

	CBaseBrush*      GetBrush() const override;

	virtual void     PostLoad(CObjectArchive& ar);
	virtual void     InvalidateTM(int nWhyFlags);

	void             Serialize(CObjectArchive& ar);
	XmlNodeRef       Export(const CString& levelPath, XmlNodeRef& xmlNode);

	void             SetAreaId(int nAreaId);
	int              GetAreaId() const { return m_areaId; }

	void             UpdateGameArea() override;
	void             UpdateGameResource() override { UpdateGameArea(); };

	CString          GenerateGameFilename() const;

	CAreaSolidPanel* GetAreaSolidPanel() const
	{
		return m_pActivateEditAreaSolidPanel;
	}

	void OnEvent(ObjectEvent event) override;

protected:
	//! Dtor must be protected.
	CAreaSolid();

	static void AddConvexhullToEngineArea(IEntityAreaProxy* pArea, const BUtil::VertexList& vertexList, const BUtil::FaceList& faceList, bool bObstructrion);
	void        DeleteThis() { delete this; };

	void        OnAreaChange(IVariable* pVar) override;
	void        OnSizeChange(IVariable* pVar);

	bool                       m_bAreaModified;

	CVariable<int>             m_areaId;
	CVariable<float>           m_edgeWidth;
	CVariable<int>             mv_groupId;
	CVariable<int>             mv_priority;
	CVariable<bool>            mv_filled;

	std::unique_ptr<ICrySizer> m_pSizer;

	static int                 s_nGlobalFileAreaSolidID;
	int                        m_nUniqFileIdForExport;

	static int                 m_nRollupId;
	static CPickEntitiesPanel* m_pPanel;

	static int                 m_nActivateEditAreaSolidRollUpID;
	static CAreaSolidPanel*    m_pActivateEditAreaSolidPanel;
};

class CAreaSolidClassDesc : public CObjectClassDesc
{
public:
	REFGUID ClassID()
	{
		// {672811ea-da24-4586-befb-742a4ed96d0c}
		static const GUID guid = { 0x672811ea, 0xda24, 0x4586, { 0xbe, 0xfb, 0x74, 0x2a, 0x4e, 0xd9, 0x6d, 0x0c } };
		return guid;
	}
	ObjectType     GetObjectType()     { return OBJTYPE_SOLID; };
	const char*    ClassName()         { return "AreaSolid"; };
	const char*    Category()          { return "Area"; };
	CRuntimeClass* GetRuntimeClass()   { return RUNTIME_CLASS(CAreaSolid); };
	int            GameCreationOrder() { return 51; };
};

