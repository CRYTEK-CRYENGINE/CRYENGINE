// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "UVElement.h"
#include "Core/UVIslandManager.h"

namespace Designer {
namespace UVMapping
{

class Model;

class UVIslandUndo : public IUndoObject
{
public:
	UVIslandUndo();

	const char* GetDescription() override { return "UV Mapping Editor : Edit"; }
	void        Undo(bool bUndo) override;
	void        Redo() override;

protected:
	struct UndoData
	{
		_smart_ptr<UVIslandManager> pUVIslandMgr;
		int                         subMatID;
	};

	void Apply(const UndoData& undoData);
	void Record(CryGUID& objGUID, UndoData& undoData);

	CryGUID  m_ObjGUID;
	UndoData m_undoData;
	UndoData m_redoData;
};

class UVProjectionUndo : public IUndoObject
{
public:
	UVProjectionUndo();

	const char* GetDescription() override { return "UV Mapping Editor : UVProjectionUndo"; }
	void        Undo(bool bUndo) override;
	void        Redo() override;

private:

	void SavePolygons(std::vector<PolygonPtr>& polygons);
	void UndoPolygons(const std::vector<PolygonPtr>& polygons);

	CryGUID                 m_ObjGUID;
	std::vector<PolygonPtr> m_UndoPolygons;
	std::vector<PolygonPtr> m_RedoPolygons;
};

class UVSelectionUndo : public IUndoObject
{
public:
	UVSelectionUndo();

	const char* GetDescription() override { return "UV Mapping Editor : Selection"; }
	void        Undo(bool bUndo) override;
	void        Redo() override;

private:

	void RefreshUVIslands(UVElementSetPtr pElementSet);
	void Apply(UVElementSetPtr pElementSet);

	CryGUID         m_ObjGUID;
	UVElementSetPtr m_UndoSelections;
	UVElementSetPtr m_RedoSelections;
};

class UVMoveUndo : public IUndoObject
{
public:
	UVMoveUndo();

	const char* GetDescription() override { return "UV Mapping Editor : Move"; }
	void        Undo(bool bUndo) override;
	void        Redo() override;

private:
	void GatherInvolvedPolygons(std::vector<PolygonPtr>& outInvolvedPolygons);
	void GatherInvolvedPolygons(std::vector<PolygonPtr>& involvedPolygons, std::vector<PolygonPtr>& outInvolvedPolygons);
	void ApplyInvolvedPolygons(std::vector<PolygonPtr>& involvedPolygons);

	CryGUID                 m_ObjGUID;
	std::vector<PolygonPtr> m_UndoPolygons;
	std::vector<PolygonPtr> m_RedoPolygons;
};

}
}

