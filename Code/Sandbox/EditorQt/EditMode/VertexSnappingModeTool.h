// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
////////////////////////////////////////////////////////////////////////////
//  Crytek Engine Source File.
//  (c) 2001 - 2012 Crytek GmbH
// -------------------------------------------------------------------------
//  File name:   VertexSnappingMode.h
//  Created:     Sep/12/2012 by Jaesik.
////////////////////////////////////////////////////////////////////////////

#include <CrySerialization/yasli/decorators/Range.h>

// Preferences
struct SVertexSnappingPreferences : public SPreferencePage
{
	SVertexSnappingPreferences()
		: SPreferencePage("Vertex Snapping", "General/Snapping")
		, m_vertexCubeSize(0.01f)
	{
	}

	virtual bool Serialize(yasli::Archive& ar) override
	{
		ar.openBlock("Vertex Snapping", "Vertex Snapping");
		ar(yasli::Range(m_vertexCubeSize, 0.0025f, 0.25f), "vertexCubesize", "Vertex Cube Size");
		ar.closeBlock();

		return true;
	}

	ADD_PREFERENCE_PAGE_PROPERTY(float, vertexCubeSize, setVertexCubeSize)
};

extern SVertexSnappingPreferences gVertexSnappingPreferences;

class CKDTree;

class CVertexSnappingModeTool : public CEditTool
{
public:
	DECLARE_DYNCREATE(CVertexSnappingModeTool);

	CVertexSnappingModeTool();
	~CVertexSnappingModeTool();

	virtual string GetDisplayName() const override { return "Snap to Vertex"; }
	void           Display(DisplayContext& dc);
	bool           MouseCallback(CViewport* view, EMouseEvent event, CPoint& point, int flags);

protected:

	void DrawVertexCubes(DisplayContext& dc, const Matrix34& tm, IStatObj* pStatObj);
	void DeleteThis() { delete this; }
	Vec3 GetCubeSize(IDisplayViewport* pView, const Vec3& pos) const;

private:

	bool     HitTest(CViewport* view, const CPoint& point, CBaseObject* pExcludedObj, Vec3* outHitPos, CBaseObjectPtr& pHitObject, std::vector<CBaseObjectPtr>& outObjects);
	CKDTree* GetKDTree(CBaseObject* pObject);

	enum EVertexSnappingStatus
	{
		eVSS_SelectFirstVertex,
		eVSS_MoveSelectVertexToAnotherVertex
	};
	EVertexSnappingStatus m_modeStatus;

	struct SSelectionInfo
	{
		SSelectionInfo()
		{
			m_pObject = NULL;
			m_vPos = Vec3(0, 0, 0);
		}
		CBaseObjectPtr m_pObject;
		Vec3           m_vPos;
	};
	SSelectionInfo                     m_SelectionInfo;

	std::vector<CBaseObjectPtr>        m_Objects;
	Vec3                               m_vHitVertex;
	bool                               m_bHit;
	CBaseObjectPtr                     m_pHitObject;

	std::vector<AABB>                  m_DebugBoxes;
	std::map<CBaseObjectPtr, CKDTree*> m_ObjectKdTreeMap;
};

