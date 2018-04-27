// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __TerrainMoveTool_h__
#define __TerrainMoveTool_h__

#if _MSC_VER > 1000
	#pragma once
#endif

#include "EditTool.h"
#include "Gizmos/ITransformManipulator.h"
#include <CryCore/Containers/CryListenerSet.h>

struct SMTBox
{
	bool isShow;
	bool isSelected;
	bool isCreated;
	Vec3 pos;

	SMTBox()
	{
		isShow = false;
		isSelected = false;
		isCreated = false;
		pos = Vec3(0, 0, 0);
	}
};

struct ITerrainMoveToolListener
{
	virtual void OnMove(const Vec3 targetPos, Vec3 sourcePos, bool bIsCopy) = 0;
};

class CXmlArchive;

//////////////////////////////////////////////////////////////////////////
class CTerrainMoveTool : public CEditTool, public ITransformManipulatorOwner
{
	DECLARE_DYNCREATE(CTerrainMoveTool)
public:
	CTerrainMoveTool();
	virtual ~CTerrainMoveTool();

	virtual string GetDisplayName() const override { return "Move Terrain"; }

	virtual void   Display(DisplayContext& dc);

	// Ovverides from CEditTool
	bool MouseCallback(CViewport* pView, EMouseEvent event, CPoint& point, int flags);

	// Key down.
	bool OnKeyDown(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags);
	bool OnKeyUp(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags);

	void OnManipulatorDrag(IDisplayViewport* view, ITransformManipulator* pManipulator, const Vec2i& p0, const Vec3& value, int nFlags);

	// Delete itself.
	void DeleteThis() { delete this; };

	void Move(bool bOnlyVegetation = true, bool bOnlyTerrain = true);

	void SetDym(Vec3 dym);
	Vec3 GetDym() { return m_dym; }

	void SetTargetRot(int targetRot);
	int  GetTargetRot() { return m_targetRot; }

	void SetSyncHeight(bool isSyncHeight);
	bool GetSyncHeight() { return m_isSyncHeight; }

	// 0 - unselect all, 1 - select source, 2 - select target
	void         Select(int nBox);

	void         SetArchive(CXmlArchive* ar);

	bool         IsNeedMoveTool() { return true; };

	static void  AddListener(ITerrainMoveToolListener* pListener);
	static void  RemoveListener(ITerrainMoveToolListener* pListener);

	virtual void Serialize(Serialization::IArchive& ar) override;

	// ITransformManipulatorOwner interface
	virtual void GetManipulatorPosition(Vec3& position) override;
	virtual bool GetManipulatorMatrix(RefCoordSys coordSys, Matrix34& tm) override;
	virtual bool IsManipulatorVisible();

private:
	CXmlArchive* m_archive;

	// !!!WARNING
	CRect m_srcRect;

	//static SMTBox m_source;
	//static SMTBox m_target;
	SMTBox      m_source;
	SMTBox      m_target;

	bool        m_isSyncHeight;
	bool        m_onlyVegetation;
	bool        m_onlyTerrain;
	bool		m_moveObjects;

	static Vec3 m_dym;
	static int  m_targetRot;

	ITransformManipulator* m_manipulator;

	friend class CUndoTerrainMoveTool;

	typedef CListenerSet<ITerrainMoveToolListener*> TMoveListener;
	static TMoveListener ms_listeners;
};

#endif // __TerrainMoveTool_h__

