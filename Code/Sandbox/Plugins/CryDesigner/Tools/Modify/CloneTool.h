// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Tools/BaseTool.h"
#include "Objects/DesignerObject.h"

namespace Designer
{
enum EPlacementType
{
	ePlacementType_Divide,
	ePlacementType_Multiply,
	ePlacementType_None
};

static const int kDefaultNumberOfClone = 5;

struct CloneParameter
{
	int            m_NumberOfClones;
	EPlacementType m_PlacementType;

	CloneParameter() : m_NumberOfClones(kDefaultNumberOfClone), m_PlacementType(ePlacementType_Divide)
	{
	}

	void Serialize(Serialization::IArchive& ar);
};

typedef DesignerObject Clone;

class CloneTool : public BaseTool
{
public:

	CloneTool(EDesignerTool tool);

	const char* ToolClass() const override;

	void        Enter() override;
	void        Leave() override;

	void        Display(struct DisplayContext& dc) override;
	bool        OnLButtonDown(CViewport* view, UINT nFlags, CPoint point) override;
	bool        OnMouseMove(CViewport* view, UINT nFlags, CPoint point) override;
	void        Serialize(Serialization::IArchive& ar);
	void        OnChangeParameter(bool continuous) override;

private:

	void Confirm();
	void FreezeClones();

	void SetPivotToObject(CBaseObject* pObj, const Vec3& pos);
	Vec3 GetCenterBottom(const AABB& aabb) const;
	void UpdateClonePositions();
	void UpdateCloneList();

	void UpdateClonePositionsAlongCircle();
	void UpdateClonePositionsAlongLine();

	typedef _smart_ptr<Clone> ClonePtr;

	std::vector<ClonePtr> m_Clones;
	ClonePtr              m_SelectedClone;

	BrushPlane            m_Plane;
	Vec3                  m_vStartPos;
	Vec3                  m_vPickedPos;
	float                 m_fRadius;
	bool                  m_bSuspendedUndo;

	Vec3                  m_InitObjectWorldPos;

	CloneParameter        m_CloneParameter;
};

class ArrayCloneTool : public CloneTool
{
public:
	ArrayCloneTool(EDesignerTool tool) : CloneTool(tool)
	{
	}
};
}

