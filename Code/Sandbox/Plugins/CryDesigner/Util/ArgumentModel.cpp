// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ArgumentModel.h"
#include "Core/Polygon.h"
#include "Core/ModelDB.h"
#include "Util/UVUnwrapUtil.h"
#include "Core/LoopPolygons.h"

namespace Designer
{
ArgumentModel::ArgumentModel(Polygon* pPolygon,
                             BrushFloat fScale,
                             CBaseObject* pObject,
                             std::vector<PolygonPtr>* perpendicularPolygons,
                             ModelDB* pDB) :
	m_fHeight(0),
	m_fScale(fScale)
{
	DESIGNER_ASSERT(pPolygon);

	if (pPolygon)
	{
		m_pPolygon = pPolygon->Clone();
		m_pPolygon->RemoveBridgeEdges();
		m_pInitialPolygon = pPolygon->Clone();
		m_BasePlane = pPolygon->GetPlane();
		m_RaiseDir = m_BasePlane.Normal();
	}

	if (perpendicularPolygons)
	{
		for (int i = 0, iPerpendicularSize(perpendicularPolygons->size()); i < iPerpendicularSize; ++i)
		{
			PolygonPtr pPerpendicularPolygon = (*perpendicularPolygons)[i];
			if (pPerpendicularPolygon->CheckFlags(ePolyFlag_Mirrored))
				continue;
			for (int k = 0, iEdgeSize(pPolygon->GetEdgeCount()); k < iEdgeSize; ++k)
			{
				BrushEdge3D edge = pPolygon->GetEdge(k);
				if (pPerpendicularPolygon->IsEdgeOverlappedByBoundary(edge))
					m_EdgePlanePairs.push_back(EdgeBrushPlanePair(edge, pPerpendicularPolygon->GetPlane()));
			}
		}
	}

	m_pBaseObject = pObject;
	m_pModel = new Model;
	m_pCompiler = new ModelCompiler(eCompiler_CastShadow);
	m_pDB = pDB;
	if (m_pDB)
		m_pDB->AddRef();
}

ArgumentModel::~ArgumentModel()
{
	if (m_pDB)
		m_pDB->Release();
}

bool ArgumentModel::FindPlaneWithEdge(const BrushEdge3D& edge, const BrushPlane& hintPlane, BrushPlane& outPlane)
{
	for (int i = 0, iSize(m_EdgePlanePairs.size()); i < iSize; ++i)
	{
		if (m_EdgePlanePairs[i].first.IsEquivalent(edge))
		{
			if (m_EdgePlanePairs[i].second.IsSameFacing(hintPlane))
			{
				outPlane = m_EdgePlanePairs[i].second;
				return true;
			}
		}
	}
	return false;
}

void ArgumentModel::CompileModel()
{
	m_pCompiler->Compile(m_pBaseObject, m_pModel);
}

void ArgumentModel::Update(EBrushArgumentUpdate updateOp)
{
	if (m_pModel == NULL)
		return;

	if (std::abs(m_fScale) >= kDesignerEpsilon && !m_pPolygon->CheckFlags(ePolyFlag_NonplanarQuad))
	{
		UpdateWithScale(updateOp);
		return;
	}

	m_pModel->Clear();

	BrushFloat fPerpendicularDist = Comparison::IsEquivalent(m_BasePlane.Normal(), m_RaiseDir) ? m_fHeight : m_fHeight* m_BasePlane.Normal().Dot(m_RaiseDir);
	m_CapPlane = BrushPlane(m_BasePlane.Normal(), m_BasePlane.Distance() - fPerpendicularDist);

	AddCapPolygons();
	AddSidePolygons();

	if (updateOp == eBAU_UpdateBrush)
		CompileModel();
}

void ArgumentModel::UpdateWithScale(EBrushArgumentUpdate updateOp)
{
	m_pModel->Clear();

	m_CapPlane = BrushPlane(m_BasePlane.Normal(), m_BasePlane.Distance() - m_fHeight);
	AddCapPolygons();

	for (int i = 0, iEdgeSize(m_pInitialPolygon->GetEdgeCount()); i < iEdgeSize; ++i)
	{
		BrushEdge3D initEdge = m_pInitialPolygon->GetEdge(i);
		BrushEdge3D capEdge = m_pPolygon->GetEdge(i);

		std::vector<BrushVec3> v;
		v.resize(4);

		v[0] = capEdge.m_v[1];
		v[1] = capEdge.m_v[0];
		v[2] = initEdge.m_v[0];
		v[3] = initEdge.m_v[1];

		if (m_fHeight < 0.0f)
		{
			std::swap(v[0], v[3]);
			std::swap(v[1], v[2]);
		}

		BrushPlane sidePlane(v[0], v[1], v[2]);

		if (m_pDB)
			m_pDB->FindPlane(sidePlane, sidePlane);

		PolygonPtr pSidePolygon = new Polygon(v, sidePlane, m_pInitialPolygon->GetSubMatID(), &m_pInitialPolygon->GetTexInfo(), true);
		pSidePolygon->SetFlag(m_pInitialPolygon->GetFlag());
		if (m_pDB)
			m_pDB->UpdatePolygonVertices(pSidePolygon);

		m_pModel->AddPolygon(pSidePolygon->Clone(), false);
	}

	if (updateOp == eBAU_UpdateBrush)
		CompileModel();
}

void ArgumentModel::AddCapPolygons()
{
	MoveVertices2Plane(m_CapPlane, m_RaiseDir);
	m_pPolygon->Scale(m_fScale);
	m_pModel->AddPolygon(m_pPolygon->Clone(), false);
}

void ArgumentModel::AddSidePolygons()
{
	if (std::abs(m_fHeight) <= kDesignerEpsilon)
		return;

	std::vector<PolygonPtr> polygons;

	polygons.push_back(m_pInitialPolygon);

	for (int a = 0, iPolygonCount(polygons.size()); a < iPolygonCount; ++a)
	{
		PolygonPtr pPolygon = polygons[a];

		if (m_pInitialPolygon->CheckFlags(ePolyFlag_NonplanarQuad) && m_pInitialPolygon != pPolygon)
			continue;

		for (int i = 0, iEdgeSize(pPolygon->GetEdgeCount()); i < iEdgeSize; ++i)
		{
			BrushEdge3D e = pPolygon->GetEdge(i);

			if (IsEdgeExcluded(e))
				continue;

			std::vector<BrushVec3> vSideList(4);

			if (m_pInitialPolygon->CheckFlags(ePolyFlag_NonplanarQuad))
			{
				BrushEdge3D cap_e = m_pPolygon->GetEdge(i);
				vSideList[0] = cap_e.m_v[1];
				vSideList[1] = cap_e.m_v[0];
			}
			else
			{
				m_CapPlane.HitTest(BrushRay(e.m_v[1], -m_RaiseDir), NULL, &vSideList[0]);
				m_CapPlane.HitTest(BrushRay(e.m_v[0], -m_RaiseDir), NULL, &vSideList[1]);
			}

			vSideList[2] = e.m_v[0];
			vSideList[3] = e.m_v[1];

			if (m_fHeight < 0.0f)
			{
				std::swap(vSideList[0], vSideList[3]);
				std::swap(vSideList[1], vSideList[2]);
			}

			BrushPlane sidePlane(vSideList[0], vSideList[1], vSideList[2]);

			if (!FindPlaneWithEdge(e, sidePlane, sidePlane))
			{
				if (m_pDB)
					m_pDB->FindPlane(sidePlane, sidePlane);
			}

			PolygonPtr pSidePolygon = new Polygon(vSideList, sidePlane, m_pInitialPolygon->GetSubMatID(), &m_pInitialPolygon->GetTexInfo(), true);
			if (!pSidePolygon->IsOpen())
			{
				if (m_pDB)
					m_pDB->UpdatePolygonVertices(pSidePolygon);
				m_pModel->AddPolygon(pSidePolygon, false);
			}
		}
	}
}

void ArgumentModel::MoveVertices2Plane(const BrushPlane& targetPlane, const BrushVec3& dir)
{
	m_pPolygon = m_pInitialPolygon->Clone();

	if (m_pPolygon->CheckFlags(ePolyFlag_NonplanarQuad))
	{
		m_pPolygon->Translate(dir * m_fHeight);
	}
	else
	{
		for (int i = 0, iVertexSize(m_pPolygon->GetVertexCount()); i < iVertexSize; ++i)
		{
			const BrushVec3& in_v = m_pPolygon->GetPos(i);
			BrushVec3 out_v;
			if (targetPlane.HitTest(BrushRay(in_v, -dir), NULL, &out_v))
				m_pPolygon->SetPos(i, out_v);
		}
	}

	m_pPolygon->SetPlane(targetPlane);
}

bool ArgumentModel::GetPolygonList(std::vector<PolygonPtr>& outPolygons) const
{
	if (m_pModel->GetPolygonCount() < 1)
		return false;
	outPolygons.push_back(m_pModel->GetPolygon(0));
	GetSidePolygonList(outPolygons);
	return true;
}

bool ArgumentModel::GetSidePolygonList(std::vector<PolygonPtr>& outPolygons) const
{
	int iPolygonSize(m_pModel->GetPolygonCount());

	for (int i = 1; i < iPolygonSize; ++i)
		outPolygons.push_back(m_pModel->GetPolygon(i));

	return true;
}

PolygonPtr ArgumentModel::GetCapPolygon() const
{
	if (m_pModel->GetPolygonCount() <= 0)
		return NULL;
	return m_pModel->GetPolygon(0);
}

void ArgumentModel::SetHeight(BrushFloat fHeight)
{
	if (std::abs(fHeight) < kDesignerEpsilon)
		m_fHeight = 0;
	else
		m_fHeight = fHeight;
}

bool ArgumentModel::IsEdgeExcluded(const BrushEdge3D& edge) const
{
	for (auto ii = m_ExcludedEdges.begin(); ii != m_ExcludedEdges.end(); ++ii)
	{
		if (edge.IsEquivalent(*ii) || edge.IsEquivalent((*ii).GetInverted()))
			return true;
	}
	return false;
}
}

