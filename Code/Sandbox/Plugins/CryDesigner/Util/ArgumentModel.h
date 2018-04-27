// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Core/Polygon.h"
#include "Core/ModelCompiler.h"
#include "Core/Model.h"

namespace Designer
{
class ModelDB;

class ArgumentModel : public _i_reference_target_t
{
public:
	ArgumentModel(Polygon* pPolygon,
	              BrushFloat fScale,
	              CBaseObject* pObject,
	              std::vector<PolygonPtr>* perpendicularPolygons,
	              ModelDB* pDB);
	~ArgumentModel();

	enum EBrushArgumentUpdate
	{
		eBAU_None        = 0,
		eBAU_UpdateBrush = 1,
	};

	void              Update(EBrushArgumentUpdate updateOp = eBAU_None);

	void              SetRaiseDir(const BrushVec3& dir)                       { m_RaiseDir = dir; }
	void              SetExcludedEdges(const std::vector<BrushEdge3D>& edges) { m_ExcludedEdges = edges; }
	void              SetHeight(BrushFloat fHeight);
	BrushFloat        GetHeight() const                                       { return m_fHeight; }
	const BrushPlane& GetCurrentCapPlane()                                    { return m_CapPlane; }
	const BrushPlane& GetBasePlane()                                          { return m_BasePlane; }
	PolygonPtr        GetCapPolygon() const;
	bool              GetSidePolygonList(std::vector<PolygonPtr>& outPolygons) const;
	bool              GetPolygonList(std::vector<PolygonPtr>& outPolygons) const;
	AABB              GetBoundBox() const { return m_pModel->GetBoundBox(); }

protected:
	void AddCapPolygons();
	void AddSidePolygons();

	void MoveVertices2Plane(const BrushPlane& targetPlane, const BrushVec3& dir);
	void CompileModel();
	bool FindPlaneWithEdge(const BrushEdge3D& edge, const BrushPlane& hintPlane, BrushPlane& outPlane);
	bool IsEdgeExcluded(const BrushEdge3D& edge) const;

	void UpdateWithScale(EBrushArgumentUpdate updateOp);

protected:
	_smart_ptr<CBaseObject>   m_pBaseObject;
	_smart_ptr<ModelCompiler> m_pCompiler;
	_smart_ptr<Model>         m_pModel;

	PolygonPtr                m_pPolygon;
	PolygonPtr                m_pInitialPolygon;

	f64                       m_fHeight;
	BrushVec3                 m_RaiseDir;
	BrushPlane                m_BasePlane;
	BrushPlane                m_CapPlane;
	BrushFloat                m_fScale;

	typedef std::pair<BrushEdge3D, BrushPlane> EdgeBrushPlanePair;
	std::vector<EdgeBrushPlanePair> m_EdgePlanePairs;
	std::vector<BrushEdge3D>        m_ExcludedEdges;

	ModelDB*                        m_pDB;
};

typedef _smart_ptr<ArgumentModel> ArgumentModelPtr;
}

