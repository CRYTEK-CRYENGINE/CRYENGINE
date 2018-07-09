// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MiscEntities.h"
#include "GameEngine.h"
#include "Objects/DisplayContext.h"

REGISTER_CLASS_DESC(CWindAreaEntityClassDesc);
REGISTER_CLASS_DESC(CConstraintEntityClassDesc);
REGISTER_CLASS_DESC(CGeomCacheEntityClassDesc);
REGISTER_CLASS_DESC(CJointGenEntityClassDesc);

IMPLEMENT_DYNCREATE(CConstraintEntity, CEntityObject)
IMPLEMENT_DYNCREATE(CWindAreaEntity, CEntityObject)
#if defined(USE_GEOM_CACHES)
IMPLEMENT_DYNCREATE(CGeomCacheEntity, CEntityObject)
#endif
IMPLEMENT_DYNCREATE(CJointGenEntity, CEntityObject)

//////////////////////////////////////////////////////////////////////////
// CConstraintEntity
//////////////////////////////////////////////////////////////////////////
static inline void DrawHingeQuad(DisplayContext& dc, float angle)
{
	const float len = 1.0f;
	Vec3 zero(0, 0, 0);
	const float halfLen = len * 0.5f;
	Vec3 vDest(0, len * cos_tpl(angle), len * sin_tpl(angle));
	Vec3 p1 = zero;
	p1.x += halfLen;
	Vec3 p2 = vDest;
	p2.x += halfLen;
	Vec3 p3 = vDest;
	p3.x -= halfLen;
	Vec3 p4 = zero;
	p4.x -= halfLen;
	dc.DrawQuad(p1, p2, p3, p4);
	dc.DrawQuad(p4, p3, p2, p1);
}

void CConstraintEntity::Display(CObjectRenderHelper& objRenderHelper)
{
	// CRYIII-1928: disabled drawing while in AI/Physics mode so it doesn't crash anymore
	if (GetIEditorImpl()->GetGameEngine()->GetSimulationMode())
		return;

	DisplayContext& dc = objRenderHelper.GetDisplayContextRef();

	// The pre-allocated array is used as an optimization, trying to avoid the physics system from allocating an entity list.
	const int nPreAllocatedListSize(1024);                            //Magic number, probably big enough for the list.
	IPhysicalEntity* pPreAllocatedEntityList[nPreAllocatedListSize];  //Pre-allocated list.
	IPhysicalEntity** pEnts = pPreAllocatedEntityList;                // Container for the list of entities.

	CEntityObject::Display(objRenderHelper);

	// get the entities within the proximity radius of the constraint (only when not simulated)
	Vec3 center(GetWorldPos());
	Vec3 radVec(m_proximityRadius);
	IPhysicalWorld* pWorld = GetIEditorImpl()->GetSystem()->GetIPhysicalWorld();
	// ent_allocate_list is needed to avoid possible crashes, as it could return an internal list (modifiable by other threads).
	// Notice that in case of refactoring, there is the need to call the DeletePointer method before returning from this function, or memory will leak.
	int objtypes = ent_static | ent_rigid | ent_sleeping_rigid | ent_sort_by_mass | ent_allocate_list;
	int nEnts = pWorld->GetEntitiesInBox(center - radVec, center + radVec, pEnts, objtypes, nPreAllocatedListSize);

	m_body0 = NULL;
	m_body1 = NULL;

	if (
	  (pEnts)
	  &&
	  (pEnts != (IPhysicalEntity**)(-1))
	  )
	{
		m_body0 = nEnts > 0 ? pEnts[0] : NULL;
		m_body1 = nEnts > 1 ? pEnts[1] : NULL;

		if (m_body0 == (IPhysicalEntity*)-1)
		{
			m_body0 = NULL;
		}

		if (m_body1 == (IPhysicalEntity*)-1)
		{
			m_body1 = NULL;
		}
	}

	// determine the reference frame of the constraint and push it onto the display context stack
	bool useEntityFrame = GetEntityPropertyBool("bUseEntityFrame");
	quaternionf qbody0;
	qbody0.SetIdentity();
	Vec3 posBody0;
	if (m_body0)
	{
		pe_status_pos pos;
		m_body0->GetStatus(&pos);
		qbody0 = pos.q;
		posBody0 = pos.pos;
	}
	quaternionf qbody1;
	qbody1.SetIdentity();
	Vec3 posBody1;
	if (m_body1)
	{
		pe_status_pos pos;
		m_body1->GetStatus(&pos);
		qbody1 = pos.q;
		posBody1 = pos.pos;
	}

	if (!GetIEditorImpl()->GetGameEngine()->GetSimulationMode())
	{
		if (useEntityFrame)
		{
			m_qframe = qbody0;
		}
		else
		{
			m_qframe = quaternionf(GetWorldTM());
		}
		m_qloc0 = !qbody0 * m_qframe;
		m_qloc1 = !qbody1 * m_qframe;
		m_posLoc = !qbody0 * (GetWorldPos() - posBody0);
	}
	quaternionf qframe0 = qbody0 * m_qloc0;
	quaternionf qframe1 = qbody1 * m_qloc1;
	// construct 3 orthonormal vectors using the constraint X axis and the constraint position in body0 space (the link vector)
	Matrix33 rot;
	Vec3 u = qframe0 * Vec3(1, 0, 0);
	Vec3 posFrame = posBody0 + qbody0 * m_posLoc;
	Vec3 l = posFrame - posBody0;
	Vec3 v = l - (l * u) * u;
	v.Normalize();
	Vec3 w = v ^ u;
	rot.SetFromVectors(u, v, w);
	Matrix34 transform(rot);
	transform.SetTranslation(posFrame);
	dc.PushMatrix(transform);

	// X limits (hinge limits)
	float minLimit = GetEntityPropertyFloat("x_min");
	float maxLimit = GetEntityPropertyFloat("x_max");
	if (minLimit < maxLimit)
	{
		// the limits
		dc.SetColor(0.5f, 0, 0.5f);
		DrawHingeQuad(dc, DEG2RAD(minLimit));
		DrawHingeQuad(dc, DEG2RAD(maxLimit));
		// the current X angle
		quaternionf qrel = !qframe1 * qframe0;
		Ang3 angles(qrel);
		dc.SetColor(0, 0, 1);
		DrawHingeQuad(dc, angles.x);
	}
	// YZ limits
	minLimit = GetEntityPropertyFloat("yz_min");
	maxLimit = GetEntityPropertyFloat("yz_max");
	if (maxLimit > 0 && minLimit < maxLimit)
	{
		// draw the cone
		float height, radius;
		if (maxLimit == 90)
		{
			height = 0;
			radius = 1;
		}
		else
		{
			const float tanAngle = fabs(tan_tpl(DEG2RAD(maxLimit)));
			if (tanAngle < 1)
			{
				height = 1;
				radius = height * tanAngle;
			}
			else
			{
				radius = 1;
				height = radius / tanAngle;
			}
			if (sin(DEG2RAD(maxLimit)) < 0)
			{
				height = -height;
			}
		}
		const int divs = 20;
		const float delta = 2.0f * gf_PI / divs;
		Vec3 apex(0, 0, 0);
		Vec3 p0(height, radius, 0);
		float angle = delta;
		dc.SetColor(0, 0.5f, 0.5f);
		dc.SetFillMode(e_FillModeWireframe);
		for (int i = 0; i < divs; i++, angle += delta)
		{
			Vec3 p(height, radius * cos_tpl(angle), radius * sin_tpl(angle));
			dc.DrawTri(apex, p0, p);
			dc.DrawTri(apex, p, p0);
			p0 = p;
		}
		dc.SetFillMode(e_FillModeSolid);
	}
	dc.PopMatrix();

	// draw the body 1 constraint X axis
	if (minLimit < maxLimit)
	{
		dc.SetColor(1.0f, 0.0f, 0.0f);
		Vec3 x1 = qbody1 * m_qloc1 * Vec3(1, 0, 0);
		dc.DrawLine(center, center + 2.0f * x1);
	}

	// If we have an entity list pointer, and this list was allocated in the physics system (not our default list)...
	if (
	  (pEnts)
	  &&
	  (pEnts != pPreAllocatedEntityList)
	  &&
	  (pEnts != (IPhysicalEntity**)(-1))
	  )
	{
		// Delete it from the physics system.
		gEnv->pPhysicalWorld->GetPhysUtils()->DeletePointer(pEnts);
	}
}

//////////////////////////////////////////////////////////////////////////
void CWindAreaEntity::Display(CObjectRenderHelper& objRenderHelper)
{
	DisplayContext& dc = objRenderHelper.GetDisplayContextRef();

	CEntityObject::Display(objRenderHelper);

	IEntity* pEntity = GetIEntity();
	if (pEntity == NULL)
	{
		return;
	}

	IPhysicalEntity* pPhysEnt = pEntity->GetPhysics();
	if (pPhysEnt == NULL || pPhysEnt->GetType() != PE_AREA)
	{
		return;
	}

	pe_status_pos pos;
	if (pPhysEnt->GetStatus(&pos) == 0)
	{
		return;
	}

	Vec3 samples[8][8][8];
	QuatT transform(pos.pos, pos.q);
	AABB bbox = AABB(pos.BBox[0], pos.BBox[1]);
	float frameTime = gEnv->pTimer->GetCurrTime();
	float theta = frameTime - floor(frameTime);

	float len[3] =
	{
		fabs_tpl(bbox.min.x - bbox.max.x),
		fabs_tpl(bbox.min.y - bbox.max.y),
		fabs_tpl(bbox.min.z - bbox.max.z)
	};

	pe_status_area area;
	for (size_t i = 0; i < 8; ++i)
		for (size_t j = 0; j < 8; ++j)
			for (size_t k = 0; k < 8; ++k)
			{
				area.ctr = transform * Vec3(
				  bbox.min.x + i * (len[0] / 8.f)
				  , bbox.min.y + j * (len[1] / 8.f)
				  , bbox.min.z + k * (len[2] / 8.f));
				samples[i][j][k] = (pPhysEnt->GetStatus(&area)) ? area.pb.waterFlow : Vec3(0, 0, 0);
			}

	for (size_t i = 0; i < 8; ++i)
		for (size_t j = 0; j < 8; ++j)
			for (size_t k = 0; k < 8; ++k)
			{
				Vec3 ctr = transform * Vec3(
				  bbox.min.x + i * (len[0] / 8.f),
				  bbox.min.y + j * (len[1] / 8.f),
				  bbox.min.z + k * (len[2] / 8.f));
				Vec3 dir = samples[i][j][k].GetNormalized();
				dc.SetColor(Col_Red);
				dc.DrawArrow(ctr, ctr + dir, 1.f);
			}
}

//////////////////////////////////////////////////////////////////////////

void AlignCutTemplate(const IStatObj* pSrcObj, const IStatObj* pObjTpl, int align, const Vec3& tplOffs, float tplScale, Matrix33& R, Vec3& offset, float& scale);

void DrawCutTemplate(DisplayContext& dc, IStatObj* pObj, const Matrix34& tmWorld, const Vec3& campos)
{
	for (int i = 0; i < pObj->GetSubObjectCount(); i++)
	{
		IStatObj::SSubObject* pSubObj = pObj->GetSubObject(i);
		if (pSubObj->pStatObj && pSubObj->pStatObj->GetPhysGeom())
		{
			IGeometry* pGeom = pSubObj->pStatObj->GetPhysGeom()->pGeom;
			Matrix34 tm = tmWorld * pSubObj->tm;
			Vec3 camposLoc = tm.GetInverted() * campos;
			switch (pGeom->GetType())
			{
			case GEOM_TRIMESH:
				{
					const mesh_data* md = (const mesh_data*)pGeom->GetData();
					for (int itri = 0; itri < md->nTris; itri++)
					{
						float dot = (md->pVertices[md->pIndices[itri * 3]] - camposLoc) * md->pNormals[itri];
						for (int j = 0, itri1; j < 3; j++)
							if ((itri1 = md->pTopology[itri].ibuddy[j]) < 0 || dot < 0 && (md->pVertices[md->pIndices[itri1 * 3]] - camposLoc) * md->pNormals[itri1] >= 0)
								dc.DrawLine(tm * md->pVertices[md->pIndices[itri * 3 + j]], tm * md->pVertices[md->pIndices[itri * 3 + incm3(j)]]);
					}
				} break;
			case GEOM_BOX:
				{
					const primitives::box* pbox = (const primitives::box*)pGeom->GetData();
					Vec3 axdot = pbox->Basis * (pbox->center - camposLoc);
					Vec3i iax(sgnnz(axdot.x), sgnnz(axdot.y), sgnnz(axdot.z));
					for (int j = 0, mask0 = 2, mask1, bit = 0; j < 6; j++, mask0 = mask1, bit = incm3(bit))
					{
						mask1 = mask0 ^ 1 << bit;
						Vec3 pt0(iax.x * (1 - (mask0 << 1 & 2)), iax.y * (1 - (mask0 & 2)), iax.z * (1 - (mask0 >> 1 & 2)));
						Vec3 pt1(iax.x * (1 - (mask1 << 1 & 2)), iax.y * (1 - (mask1 & 2)), iax.z * (1 - (mask1 >> 1 & 2)));
						dc.DrawLine(tm * (pbox->center + (Diag33(pbox->size) * pt0) * pbox->Basis), tm * (pbox->center + (Diag33(pbox->size) * pt1) * pbox->Basis));
					}
				} break;
			}
		}
	}
}

void CJointGenEntity::Display(CObjectRenderHelper& objRenderHelper)
{
	DisplayContext& dc = objRenderHelper.GetDisplayContextRef();

	CEntityObject::Display(objRenderHelper);

	Vec3 campos = dc.camera->GetPosition();
	IEntity* pent = GetIEntity();
	if (GetIEditorImpl()->GetGameEngine()->GetSimulationMode() || (pent->GetWorldPos() - campos).len2() > 100)
		return;

	SmartScriptTable props;
	pent->GetScriptTable()->GetValue("Properties", props);
	string str;
	if (props->GetValue("object_BreakTemplate", str))
	{
		if (str != m_fname)
		{
			m_pObj = gEnv->p3DEngine->LoadStatObj(str);
			m_fname = str;
		}
		if (!m_pObj)
			return;

		Vec3 tplOffs(ZERO);
		props->GetValue("Offset", tplOffs);
		float tplScale = 1.0f;
		props->GetValue("Scale", tplScale);
		int align;
		props->GetValue("ei_Alignment", align);
		IPhysicalWorld::SPWIParams pp;
		pp.entTypes = ent_static | ent_rigid | ent_sleeping_rigid;
		pp.itype = primitives::sphere::type;
		primitives::sphere sph;
		sph.center = pent->GetWorldPos();
		props->GetValue("Radius", sph.r);
		pp.pprim = &sph;
		geom_contact* pcont;
		pp.ppcontact = &pcont;
		intersection_params ip;
		ip.bNoAreaContacts = ip.bNoBorder = true;
		pp.pip = &ip;
		dc.SetColor(RGB(100, 166, 255));

		int ncont = (int)gEnv->pPhysicalWorld->PrimitiveWorldIntersection(pp);
		for (int i = 0; i < ncont; i++)
		{
			IPhysicalEntity* pentPhys = gEnv->pPhysicalWorld->GetPhysicalEntityById(pcont[i].iPrim[0]);
			if (!pentPhys)
				continue;
			IRenderNode* pRN = 0;
			switch (pentPhys->GetiForeignData())
			{
			case PHYS_FOREIGN_ID_ENTITY:
				pRN = (((IEntity*)pentPhys->GetForeignData(PHYS_FOREIGN_ID_ENTITY))->GetRenderInterface())->GetRenderNode();
				break;
			case PHYS_FOREIGN_ID_STATIC:
				pRN = ((IRenderNode*)pentPhys->GetForeignData(PHYS_FOREIGN_ID_STATIC));
				break;
			default:
				continue;
			}
			Matrix34A tmW;
			IStatObj* pObj = pRN->GetEntityStatObj(0, &tmW);
			if (!pObj)
				continue;
			for (; pObj->GetCloneSourceObject(); pObj = pObj->GetCloneSourceObject())
				;
			if (!m_pSph)
				(m_pSph = gEnv->pPhysicalWorld->GetGeomManager()->CreatePrimitive(primitives::sphere::type, &sph))->Release();
			m_pSph->SetData(&sph);
			geom_world_data gwd;
			IStatObj::SSubObject subObj;
			subObj.pStatObj = pObj;
			subObj.tm.SetIdentity();

			for (int j = 0; j < max(1, pObj->GetSubObjectCount()); j++)
			{
				IStatObj::SSubObject* pSubObj = pObj->GetSubObject(j) ? pObj->GetSubObject(j) : &subObj;
				if (!pSubObj->pStatObj || !pSubObj->pStatObj->GetPhysGeom())
					continue;
				QuatTS qtsW(tmW * pSubObj->tm);
				gwd.R = Matrix33(qtsW.q);
				gwd.offset = qtsW.t;
				gwd.scale = qtsW.s;
				if (m_pSph->Intersect(pSubObj->pStatObj->GetPhysGeom()->pGeom, 0, &gwd, 0, pcont))
				{
					gwd.R = align == -2 ? Matrix33(!pent->GetRotation() * qtsW.q) : Matrix33(IDENTITY);
					AlignCutTemplate(pSubObj->pStatObj, m_pObj, align > 23 ? -1 : align, tplOffs, tplScale, gwd.R, gwd.offset, gwd.scale);
					gwd.R = gwd.R.T() / gwd.scale;
					DrawCutTemplate(dc, m_pObj, Matrix34(qtsW) * Matrix34(gwd.R, gwd.R * -gwd.offset), campos);
				}
			}
			break;
		}
		if (!ncont)
		{
			dc.SetColor(RGB(160, 160, 160));
			DrawCutTemplate(dc, m_pObj, pent->GetWorldTM(), campos);
		}
	}
}

