// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TerrainMoveTool.h"
#include "Viewport.h"
#include "Vegetation/VegetationMap.h"
#include "Terrain/TerrainManager.h"
#include "CryEditDoc.h"
#include "Serialization/Decorators/EditorActionButton.h"
#include "Gizmos/IGizmoManager.h"
#include "IUndoManager.h"

namespace
{
enum ETargetRotation
{
	eTargetRotation_0,
	eTargetRotation_90,
	eTargetRotation_180,
	eTargetRotation_270,
};

SERIALIZATION_ENUM_BEGIN(ETargetRotation, "Target Rotation")
SERIALIZATION_ENUM(eTargetRotation_0, "0", "0");
SERIALIZATION_ENUM(eTargetRotation_90, "90", "90");
SERIALIZATION_ENUM(eTargetRotation_180, "180", "180");
SERIALIZATION_ENUM(eTargetRotation_270, "270", "270");
SERIALIZATION_ENUM_END()
};

CTerrainMoveTool::TMoveListener CTerrainMoveTool::ms_listeners(1);

//////////////////////////////////////////////////////////////////////////
//class CUndoTerrainMoveTool

class CUndoTerrainMoveTool : public IUndoObject
{
public:
	CUndoTerrainMoveTool(CTerrainMoveTool* pMoveTool)
	{
		m_posSourceUndo = pMoveTool->m_source.pos;
		m_posTargetUndo = pMoveTool->m_target.pos;
	}
protected:
	virtual const char* GetDescription() { return ""; };

	virtual void        Undo(bool bUndo)
	{
		CEditTool* pTool = GetIEditorImpl()->GetEditTool();
		if (!pTool || !pTool->IsKindOf(RUNTIME_CLASS(CTerrainMoveTool)))
			return;
		CTerrainMoveTool* pMoveTool = (CTerrainMoveTool*)pTool;
		if (bUndo)
		{
			m_posSourceRedo = pMoveTool->m_source.pos;
			m_posTargetRedo = pMoveTool->m_target.pos;
		}
		pMoveTool->m_source.pos = m_posSourceUndo;
		pMoveTool->m_target.pos = m_posTargetUndo;
		if (pMoveTool->m_source.isSelected)
			pMoveTool->Select(1);
		if (pMoveTool->m_target.isSelected)
			pMoveTool->Select(2);
	}
	virtual void Redo()
	{
		CEditTool* pTool = GetIEditorImpl()->GetEditTool();
		if (!pTool || !pTool->IsKindOf(RUNTIME_CLASS(CTerrainMoveTool)))
			return;
		CTerrainMoveTool* pMoveTool = (CTerrainMoveTool*)pTool;
		pMoveTool->m_source.pos = m_posSourceRedo;
		pMoveTool->m_target.pos = m_posTargetRedo;
		if (pMoveTool->m_source.isSelected)
			pMoveTool->Select(1);
		if (pMoveTool->m_target.isSelected)
			pMoveTool->Select(2);
	}
private:
	Vec3 m_posSourceRedo;
	Vec3 m_posTargetRedo;
	Vec3 m_posSourceUndo;
	Vec3 m_posTargetUndo;
};

//////////////////////////////////////////////////////////////////////////
//class CTerrainMoveTool

//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CTerrainMoveTool, CEditTool)

Vec3 CTerrainMoveTool::m_dym(512, 512, 1024);
int CTerrainMoveTool::m_targetRot = 0.0f;
//SMTBox CTerrainMoveTool::m_source;
//SMTBox CTerrainMoveTool::m_target;

//////////////////////////////////////////////////////////////////////////
CTerrainMoveTool::CTerrainMoveTool() :
	m_archive(0),
	m_isSyncHeight(false),
	m_onlyVegetation(false),
	m_onlyTerrain(false),
	m_moveObjects(false),
	m_manipulator(nullptr)
{
	CUndo undo("Unselect All");
	GetIEditorImpl()->ClearSelection();

	if (m_source.isCreated)
		m_source.isShow = true;
	if (m_target.isCreated)
		m_target.isShow = true;

	m_manipulator = GetIEditorImpl()->GetGizmoManager()->AddManipulator(this);
	m_manipulator->signalDragging.Connect(this, &CTerrainMoveTool::OnManipulatorDrag);
}

//////////////////////////////////////////////////////////////////////////
CTerrainMoveTool::~CTerrainMoveTool()
{
	Select(0);

	if (m_archive)
		delete m_archive;

	if (m_manipulator)
	{
		m_manipulator->signalBeginDrag.DisconnectAll();
		m_manipulator->signalDragging.DisconnectAll();
		m_manipulator->signalEndDrag.DisconnectAll();
		GetIEditorImpl()->GetGizmoManager()->RemoveManipulator(m_manipulator);
		m_manipulator = nullptr;
	}
}

//////////////////////////////////////////////////////////////////////////
bool CTerrainMoveTool::MouseCallback(CViewport* pView, EMouseEvent event, CPoint& point, int flags)
{
	if (event == eMouseMove)
	{
		if (m_source.isSelected && !m_source.isCreated || m_target.isSelected && !m_target.isCreated)
		{
			Vec3 pos = pView->SnapToGrid(pView->ViewToWorld(point));
			if (pos == Vec3(0, 0, 0))
			{
				// collide with Z=0 plane
				Vec3 src, dir;
				pView->ViewToWorldRay(point, src, dir);
				if (fabs(dir.z) > 0.00001f)
					pos = pView->SnapToGrid(src - (src.z / dir.z) * dir);
			}

			if (m_source.isSelected)
			{
				m_source.pos = pos;
				Select(1);
				signalPropertiesChanged(this);
			}
			if (m_target.isSelected)
			{
				m_target.pos = pos;
				Select(2);
				if (m_isSyncHeight)
					m_target.pos.z = m_source.pos.z;
				signalPropertiesChanged(this);
			}
		}
	}
	else if (event == eMouseLDown)
	{
		// Close tool.
		if (!m_source.isSelected && !m_target.isSelected)
			GetIEditorImpl()->SetEditTool(0);
	}
	else if (event == eMouseLUp)
	{
		if (m_source.isSelected && !m_source.isCreated)
		{
			m_source.isCreated = true;
		}
		else if (m_target.isSelected && !m_target.isCreated)
		{
			m_target.isCreated = true;
		}
		else
			GetIEditorImpl()->GetIUndoManager()->Accept("Move Box");
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CTerrainMoveTool::Display(DisplayContext& dc)
{
	if (m_source.isShow)
	{
		dc.SetColor(RGB(255, 128, 128), 1);
		if (m_source.isSelected)
			dc.SetColor(dc.GetSelectedColor());
		dc.DrawWireBox(m_source.pos - m_dym / 2, m_source.pos + m_dym / 2);
	}

	if (m_target.isShow)
	{
		dc.SetColor(RGB(128, 128, 255), 1);
		if (m_target.isSelected)
			dc.SetColor(dc.GetSelectedColor());

		Matrix34 tm;
		tm.SetIdentity();
		if (m_targetRot)
		{
			float a = g_PI / 2 * m_targetRot;
			tm.SetRotationZ(a);
		}
		tm.SetTranslation(m_target.pos);
		dc.PushMatrix(tm);
		dc.DrawWireBox(-m_dym / 2, m_dym / 2);
		dc.PopMatrix();
	}

	/*
	   BBox box;
	   GetIEditorImpl()->GetSelectedRegion(box);

	   Vec3 p1 = GetIEditorImpl()->GetHeightmap()->HmapToWorld( CPoint(m_srcRect.left,m_srcRect.top) );
	   Vec3 p2 = GetIEditorImpl()->GetHeightmap()->HmapToWorld( CPoint(m_srcRect.right,m_srcRect.bottom) );

	   Vec3 ofs = m_pointerPos - p1;
	   p1 += ofs;
	   p2 += ofs;

	   dc.SetColor( RGB(0,0,255) );
	   dc.DrawTerrainRect( p1.x,p1.y,p2.x,p2.y,0.2f );
	 */
}

//////////////////////////////////////////////////////////////////////////
bool CTerrainMoveTool::OnKeyDown(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags)
{
	bool bProcessed = false;
	return bProcessed;
}

//////////////////////////////////////////////////////////////////////////
bool CTerrainMoveTool::OnKeyUp(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags)
{
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CTerrainMoveTool::Move(bool bOnlyVegetation, bool bOnlyTerrain)
{
	// Move terrain area.
	CUndo undo("Copy Area");
	CWaitCursor wait;

	bool isCopy = !m_moveObjects;
	CHeightmap* pHeightmap = GetIEditorImpl()->GetHeightmap();
	if (!pHeightmap || !pHeightmap->GetHeight() || !pHeightmap->GetWidth())
		return;

	AABB srcBox(m_source.pos - m_dym / 2, m_source.pos + m_dym / 2);
	pHeightmap->Copy(srcBox, m_targetRot, m_target.pos, m_dym, m_source.pos, bOnlyVegetation, bOnlyTerrain);
	
	if (bOnlyVegetation || (!bOnlyVegetation && !bOnlyTerrain))
	{
		GetIEditorImpl()->GetVegetationMap()->RepositionArea(srcBox, m_target.pos - m_source.pos, m_targetRot, isCopy);
	}

	if (!isCopy && (!bOnlyVegetation && !bOnlyTerrain))
	{
		GetIEditorImpl()->GetObjectManager()->MoveObjects(srcBox, m_target.pos - m_source.pos, m_targetRot, isCopy);
	}

	for (TMoveListener::Notifier notifier = ms_listeners; notifier.IsValid(); notifier.Next())
	{
		notifier->OnMove(m_target.pos, m_source.pos, isCopy);
	}

	GetIEditorImpl()->ClearSelection();
}

//////////////////////////////////////////////////////////////////////////
void CTerrainMoveTool::SetArchive(CXmlArchive* ar)
{
	if (m_archive)
		delete m_archive;
	m_archive = ar;

	int x1, y1, x2, y2;
	// Load rect size our of archive.
	m_archive->root->getAttr("X1", x1);
	m_archive->root->getAttr("Y1", y1);
	m_archive->root->getAttr("X2", x2);
	m_archive->root->getAttr("Y2", y2);

	m_srcRect.SetRect(x1, y1, x2, y2);
}

//////////////////////////////////////////////////////////////////////////
void CTerrainMoveTool::Select(int nBox)
{
	m_source.isSelected = false;
	m_target.isSelected = false;

	if (nBox == 0)
	{
		if (m_manipulator)
		{
			m_manipulator->signalBeginDrag.DisconnectAll();
			m_manipulator->signalDragging.DisconnectAll();
			m_manipulator->signalEndDrag.DisconnectAll();
			GetIEditorImpl()->GetGizmoManager()->RemoveManipulator(m_manipulator);
			m_manipulator = nullptr;
		}
		m_source.isShow = false;
		m_target.isShow = false;
	}
	else if (nBox == 1)
	{
		m_source.isSelected = true;
		m_source.isShow = true;
		
		m_manipulator->Invalidate();
	}
	else if (nBox == 2)
	{
		m_target.isSelected = true;
		m_target.isShow = true;
		
		m_manipulator->Invalidate();
	}
}

//////////////////////////////////////////////////////////////////////////
void CTerrainMoveTool::OnManipulatorDrag(IDisplayViewport* view, ITransformManipulator* pManipulator, const Vec2i& p0, const Vec3& value, int nFlags)
{
	int editMode = GetIEditorImpl()->GetEditMode();
	if (editMode == eEditModeMove)
	{
		CHeightmap* pHeightmap = GetIEditorImpl()->GetHeightmap();
		GetIEditorImpl()->GetIUndoManager()->Restore();

		Vec3 pos = m_source.pos;
		Vec3 val = value;

		Vec3 max = pHeightmap->HmapToWorld(CPoint(pHeightmap->GetWidth(), pHeightmap->GetHeight()));

		uint32 wid = max.x;
		uint32 hey = max.y;

		if (m_target.isSelected)
			pos = m_target.pos;

		if (CUndo::IsRecording())
			CUndo::Record(new CUndoTerrainMoveTool(this));

		if (m_source.isSelected)
		{
			m_source.pos += val;
			if (m_isSyncHeight)
				m_target.pos.z = m_source.pos.z;
			signalPropertiesChanged(this);
		}

		if (m_target.isSelected)
		{
			m_target.pos += val;
			if (m_isSyncHeight)
				m_source.pos.z = m_target.pos.z;
			signalPropertiesChanged(this);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CTerrainMoveTool::SetDym(Vec3 dym)
{
	m_dym = dym;
}

//////////////////////////////////////////////////////////////////////////
void CTerrainMoveTool::SetTargetRot(int targetRot)
{
	m_targetRot = targetRot;
}

//////////////////////////////////////////////////////////////////////////
void CTerrainMoveTool::SetSyncHeight(bool isSyncHeight)
{
	m_isSyncHeight = isSyncHeight;
	if (m_isSyncHeight)
	{
		m_target.pos.z = m_source.pos.z;
		if (m_target.isSelected)
			Select(2);
	}
}

void CTerrainMoveTool::AddListener(ITerrainMoveToolListener* pListener)
{
	ms_listeners.Add(pListener);
}

void CTerrainMoveTool::RemoveListener(ITerrainMoveToolListener* pListener)
{
	ms_listeners.Remove(pListener);
}

void CTerrainMoveTool::Serialize(Serialization::IArchive& ar)
{
	if (ar.openBlock("moveterrain", "Move Terrain"))
	{
		if (ar.openBlock("target_source", "<Select"))
		{
			ar(Serialization::ActionButton([this] { Select(1);
			                               }), "source", "^Source");
			ar(Serialization::ActionButton([this] { Select(2);
			                               }), "target", "^Target");
			ar.closeBlock();
		}

		ar(m_dym, "dimensions", "Size");
		ar((ETargetRotation&)m_targetRot, "targetrot", "Target Rotation");
		ar(m_onlyVegetation, "onlyvegetation", "Only Vegetation");
		ar(m_onlyTerrain, "onlyterrain", "Only Terrain");
		ar(m_isSyncHeight, "syncheight", "Sync Height");
		ar(m_moveObjects, "moveobjects", "Move Objects");
		
		if (ar.openBlock("action_buttons", "<Terrain"))
		{
			ar(Serialization::ActionButton([this] { Move(m_onlyVegetation, m_onlyTerrain); }), "move", "^Move");
			ar.closeBlock();
		}
		ar.closeBlock();
	}
}

void CTerrainMoveTool::GetManipulatorPosition(Vec3& position)
{
	// normally, one of the two should be selected, else there will be no manipulator
	if (m_source.isSelected)
	{
		position = m_source.pos;
	}
	else if (m_target.isSelected)
	{
		position = m_target.pos;
	}
}

bool CTerrainMoveTool::GetManipulatorMatrix(RefCoordSys coordSys, Matrix34& tm)
{
	if (coordSys == COORDS_LOCAL && m_target.isSelected)
	{
		tm.SetIdentity();
		if (m_targetRot)
		{
			float a = g_PI / 2 * m_targetRot;
			tm.SetRotationZ(a);
		}
		return true;
	}
	return false;
}

bool CTerrainMoveTool::IsManipulatorVisible()
{
	return m_source.isSelected || m_target.isSelected;
}

