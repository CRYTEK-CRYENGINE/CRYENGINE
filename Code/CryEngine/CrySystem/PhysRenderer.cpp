// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "System.h"
#include "PhysRenderer.h"
#include <CryRenderer/IRenderAuxGeom.h>
#include <CryEntitySystem/IEntity.h>
#include <Cry3DEngine/IRenderNode.h>
#include <Cry3DEngine/IStatObj.h>

#pragma warning(push)
#pragma warning(disable: 4244)

ColorB CPhysRenderer::g_colorTab[9] = {
	ColorB(136, 141, 162, 255), ColorB(212, 208, 200, 255), ColorB(255, 255, 255, 255), ColorB(214, 222, 154, 128),
	ColorB(231, 192, 188, 255), ColorB(164, 0,   0,   80),  ColorB(164, 0,   0,   255), ColorB(168, 224, 251, 255),ColorB(0,0, 164, 255)
};

CPhysRenderer::CPhysRenderer()
	: m_cullDist(100.0f)
	, m_wireframeDist(40.0f)
	, m_timeRayFadein(0.2f)
	, m_rayPeakTime(0.0f)
	, m_maxTris(200)
	, m_maxTrisRange(800)
	, m_pRenderer(nullptr)
	, m_rayBuf(nullptr)
	, m_szRayBuf(0)
	, m_iFirstRay(0)
	, m_iLastRay(-1)
	, m_nRays(0)
	, m_geomBuf(nullptr)
	, m_szGeomBuf(0)
	, m_iFirstGeom(0)
	, m_iLastGeom(-1)
	, m_nGeoms(0)
	, m_pRayGeom(nullptr)
	, m_pRay(nullptr)
	, m_offset(ZERO)
	, m_qrot(IDENTITY)
	, m_lockDrawGeometry(0)
{
}

void CPhysRenderer::Init()
{
	primitives::ray aray;
	aray.dir.Set(0, 0, 1);
	aray.origin.zero();
	m_pRayGeom = gEnv->pPhysicalWorld->GetGeomManager()->CreatePrimitive(primitives::ray::type, &aray);
	m_pRay = (primitives::ray*)m_pRayGeom->GetData();
	m_rayBuf = new SRayRec[m_szRayBuf = 64];
	m_geomBuf = new SGeomRec[m_szGeomBuf = 64];
	m_iFirstRay = 0;
	m_iLastRay = -1;
	m_nRays = 0;
	m_pRenderer = gEnv->pRenderer;
}

CPhysRenderer::~CPhysRenderer()
{
	if (m_pRayGeom) m_pRayGeom->Release();
	if (m_rayBuf) delete[] m_rayBuf;
}

const char* CPhysRenderer::GetPhysForeignName(void* pForeignData, int iForeignData, int iForeignFlags)
{
	if (!pForeignData)
		return "*invalid foreign data*";
	if (iForeignData == PHYS_FOREIGN_ID_ENTITY)
	{
		static void* pIEntityVMT = 0;
		if (!pIEntityVMT)
		{
			IEntityItPtr pIter = gEnv->pEntitySystem->GetEntityIterator();
			if (pIter->This())
				pIEntityVMT = *(void**)pIter->This();
		}
		if (!pIEntityVMT || *(void**)pForeignData != pIEntityVMT)
			return "ERROR: dangling entity ptr";
		return ((IEntity*)pForeignData)->GetName();
	}
	if (iForeignData == PHYS_FOREIGN_ID_STATIC)
	{
		const char* name = ((IRenderNode*)pForeignData)->GetName(), * ptr;
		while (true)
		{
			for (ptr = name; *ptr && *ptr != '%'; ptr++)
				;
			if (*ptr)
				for (name = ptr; *name && *name != '\\'; name++)
					;
			else
				break;
		}
		return name;
	}
	if (iForeignData == PHYS_FOREIGN_ID_FOLIAGE)
		return "**foliage rope**";
	if (iForeignData == PHYS_FOREIGN_ID_ROPE)
		if (IEntity* pEntity = (((IRopeRenderNode*)pForeignData)->GetOwnerEntity()))
			return pEntity->GetName();
		else
			return "Rope";
	if (iForeignData == PHYS_FOREIGN_ID_RIGID_PARTICLE)
		return *((IStatObj*)pForeignData)->GetGeoName() ? ((IStatObj*)pForeignData)->GetGeoName() : ((IStatObj*)pForeignData)->GetFilePath();

	if (iForeignData == PHYS_FOREIGN_ID_WATERVOLUME)
		return ((IWaterVolumeRenderNode*)pForeignData)->GetName();
	return "[Static]";
}

void CPhysRenderer::DrawGeometry(IGeometry* pGeom, geom_world_data* pgwd, int idxColor, int bSlowFadein, const Vec3& sweepDir, const ColorF& color)
{
	WriteLock lock(m_lockDrawGeometry);
	if (!bSlowFadein)
	{
		ColorB clr;

		if (!isneg(idxColor))
		{
			clr = g_colorTab[idxColor & 7];
			clr.a >>= idxColor >> 8;
		}
		else
		{
			clr = color;
		}

		DrawGeometry(pGeom, pgwd, clr);
	}
	else
	{
		if (pGeom->GetType() == GEOM_RAY)
		{
			primitives::ray* pray = (primitives::ray*)pGeom->GetData();
			if (m_nRays == m_szRayBuf)
			{
				int i;
				SRayRec* prevbuf = m_rayBuf;
				m_rayBuf = new SRayRec[m_szRayBuf += 64];
				memcpy(m_rayBuf + m_iFirstRay, prevbuf + m_iFirstRay, (m_nRays - m_iFirstRay) * sizeof(SRayRec));
				if (m_iFirstRay > m_iLastRay)
				{
					memcpy(m_rayBuf + m_nRays, prevbuf, (i = min(m_szRayBuf - m_nRays, m_iLastRay + 1)) * sizeof(SRayRec));
					PREFAST_SUPPRESS_WARNING(6386)
					memcpy(m_rayBuf, prevbuf + i, (m_iLastRay + 1 - i) * sizeof(SRayRec));
					m_iLastRay -= i - (m_nRays + i & (m_iLastRay - i) >> 31);
				}
			}
			m_iLastRay += 1 - (m_szRayBuf & (m_szRayBuf - 2 - m_iLastRay) >> 31);
			m_nRays++;
			if (!pgwd)
			{
				PREFAST_SUPPRESS_WARNING(6386)
				m_rayBuf[m_iLastRay].origin = pray->origin;
				m_rayBuf[m_iLastRay].dir = pray->dir;
			}
			else
			{
				m_rayBuf[m_iLastRay].origin = pgwd->R * pray->origin * pgwd->scale + pgwd->offset;
				m_rayBuf[m_iLastRay].dir = pgwd->R * pray->dir;
			}
			m_rayBuf[m_iLastRay].time = m_timeRayFadein;
			m_rayBuf[m_iLastRay].idxColor = idxColor;
		}
		else
		{
			if (m_nGeoms == m_szGeomBuf)
			{
				int i;
				SGeomRec* prevbuf = m_geomBuf;
				m_geomBuf = new SGeomRec[m_szGeomBuf += 64];
				memcpy(m_geomBuf + m_iFirstGeom, prevbuf + m_iFirstGeom, (m_nGeoms - m_iFirstGeom) * sizeof(SGeomRec));
				if (m_iFirstGeom > m_iLastGeom)
				{
					memcpy(m_geomBuf + m_nGeoms, prevbuf, (i = min(m_szGeomBuf - m_nGeoms, m_iLastGeom + 1)) * sizeof(SGeomRec));
					PREFAST_SUPPRESS_WARNING(6386)
					memcpy(m_geomBuf, prevbuf + i, (m_iLastGeom + 1 - i) * sizeof(SGeomRec));
					m_iLastGeom -= i - (m_nGeoms + i & (m_iLastGeom - i) >> 31);
				}
			}
			m_iLastGeom += 1 - (m_szGeomBuf & (m_szGeomBuf - 2 - m_iLastGeom) >> 31);
			m_nGeoms++;
			switch (m_geomBuf[m_iLastGeom].itype = pGeom->GetType())
			{
				PREFAST_SUPPRESS_WARNING(6385 6386)
			case GEOM_BOX:
				*(primitives::box*)m_geomBuf[m_iLastGeom].buf = *(primitives::box*)pGeom->GetData();
				break;
			case GEOM_SPHERE:
				*(primitives::sphere*)m_geomBuf[m_iLastGeom].buf = *(primitives::sphere*)pGeom->GetData();
				break;
			case GEOM_CYLINDER:
			case GEOM_CAPSULE:
				*(primitives::cylinder*)m_geomBuf[m_iLastGeom].buf = *(primitives::cylinder*)pGeom->GetData();
				break;
			}
			if (!pgwd)
				m_geomBuf[m_iLastGeom].offset.zero(), m_geomBuf[m_iLastGeom].R.SetIdentity(), m_geomBuf[m_iLastGeom].scale = 1.0f;
			else
				m_geomBuf[m_iLastGeom].offset = pgwd->offset, m_geomBuf[m_iLastGeom].R = pgwd->R, m_geomBuf[m_iLastGeom].scale = pgwd->scale;
			m_geomBuf[m_iLastGeom].sweepDir = sweepDir;
			m_geomBuf[m_iLastGeom].time = m_timeRayFadein;
		}
	}
}

void CPhysRenderer::DrawFrame(const Vec3& pnt, const Vec3* axes, const float scale, const Vec3* limits, const int axes_locked)
{
	IRenderAuxGeom* aux = IRenderAuxGeom::GetAux();
	SAuxGeomRenderFlags oldFlags = aux->GetRenderFlags();
	SAuxGeomRenderFlags renderFlags(oldFlags);
	renderFlags.SetDepthWriteFlag(e_DepthWriteOn);
	renderFlags.SetDepthTestFlag(e_DepthTestOn);
	renderFlags.SetDrawInFrontMode(e_DrawInFrontOff);
	renderFlags.SetAlphaBlendMode(e_AlphaNone);
	renderFlags.SetCullMode(e_CullModeNone);
	aux->SetRenderFlags(renderFlags);

	ColorB clr[3] = { ColorB(255, 0, 0), ColorB(0, 255, 0), ColorB(0, 0, 255) };
	float fclr[4][4] = {
		{ 1.f, 0.f, 0.f, 1.f }, { 0.f, 1.f, 0.f, 1.f }, { 0.f, 0.f, 1.f, 1.f }, { 1.f, 1.f, 1.f, 1.f }
	};

	for (int j = 0; j < 3; ++j)
		if (axes_locked & 1 << j)
		{
			float lim[2] = { max(-gf_PI*2, limits[0][j]), min(gf_PI*2, limits[1][j]) };
			const float step = 0.2f;
			float cosa, sina, cstep = 0.499f * step / max(0.001f, lim[1] - lim[0]);
			sincos_tpl(step, &sina, &cosa);
			// axes0 is conventionally the "twist", aka "prinpical" axis for a bone
			// use axes0 to show rotation ranges around axes1 and 2, and axes1 for rotation around axes0
			Vec3 axort = axes[(j | j >> 1) & 1 ^ 1], axis = axort.GetRotated(axes[j], lim[0]), axis1;
			for(float a = lim[0], c = 0.5f; a < lim[1]; a += step, axis = axis1, c += cstep)
			{
				axis1 = a + step < lim[1] ? axis.GetRotated(axes[j], cosa, sina) : axort.GetRotated(axes[j], lim[1]);
				ColorB cb = ColorB(ColorF(clr[j]) * c);
				aux->DrawTriangle(pnt, cb, pnt + axis * scale, cb, pnt + axis1 * scale, cb);
			}
		}

	aux->SetRenderFlags(oldFlags);
}

void CPhysRenderer::DrawLine(const Vec3& pt0, const Vec3& pt1, int idxColor, int bSlowFadein)
{
	m_pRay->origin = pt0;
	m_pRay->dir = pt1 - pt0;
	int bPeaked = isneg(max(1e-6f - m_rayPeakTime, m_rayPeakTime - gEnv->pTimer->TicksToSeconds(bSlowFadein >> 1) * 1000));
	DrawGeometry(m_pRayGeom, 0, idxColor - bPeaked * 2, bSlowFadein & 1);
}

void CPhysRenderer::Flush(float dt)
{
	if (m_nRays > 0)
	{
		int i = m_iFirstRay, iprev;
		float rtime = 1 / m_timeRayFadein;
		ColorB clr;
		do
		{
			clr = g_colorTab[m_rayBuf[i].idxColor];
			clr.a = FtoI(clr.a * m_rayBuf[i].time * rtime);
			m_pRay->origin = m_rayBuf[i].origin;
			m_pRay->dir = m_rayBuf[i].dir;
			DrawGeometry(m_pRayGeom, 0, clr);
			iprev = i;
			i += 1 - (m_szRayBuf & (m_szRayBuf - 2 - i) >> 31);
			if ((m_rayBuf[iprev].time -= dt) < 0)
				m_iFirstRay = i, m_nRays = max(0, m_nRays - 1);
		}
		while (iprev != m_iLastRay);
	}

	if (m_nGeoms > 0)
	{
		int i = m_iFirstGeom, iprev;
		float rtime = 1 / m_timeRayFadein;
		ColorB clr;
		geom_world_data gwd;
		do
		{
			clr = g_colorTab[7];
			clr.a = FtoI(clr.a * m_geomBuf[i].time * rtime * 0.7f);
			gwd.offset = m_geomBuf[i].offset;
			gwd.R = m_geomBuf[i].R;
			gwd.scale = m_geomBuf[i].scale;
			DrawGeometry(m_geomBuf[i].itype, m_geomBuf[i].buf, &gwd, clr, m_geomBuf[i].sweepDir);
			iprev = i;
			i += 1 - (m_szGeomBuf & (m_szGeomBuf - 2 - i) >> 31);
			if ((m_geomBuf[iprev].time -= dt) < 0)
				m_iFirstGeom = i, m_nGeoms = max(0, m_nGeoms - 1);
		}
		while (iprev != m_iLastGeom);
	}
}

void CPhysRenderer::UpdateCamera(const CCamera& camera)
{
	m_camera = camera;
	m_camera.SetFrustum(camera.GetViewSurfaceX(), camera.GetViewSurfaceZ(), camera.GetFov(), camera.GetNearPlane(), m_cullDist, camera.GetPixelAspectRatio());
}

void CPhysRenderer::DrawAllHelpers(IPhysicalWorld* pWorld)
{
	pWorld->DrawPhysicsHelperInformation(this);
}

void CPhysRenderer::DrawEntityHelpers(IPhysicalWorld* pWorld, int entityId, int iDrawHelpers)
{
	pWorld->DrawEntityHelperInformation(this, entityId, iDrawHelpers);
}

void CPhysRenderer::DrawEntityHelpers(IPhysicalEntity* pEntity, int helperFlags)
{
	IPhysicalWorld* pWorld = pEntity->GetWorld();
	int entityId = pWorld->GetPhysicalEntityId(pEntity);
	pWorld->DrawEntityHelperInformation(this, entityId, helperFlags);
}

inline float getheight(primitives::heightfield* phf, int ix, int iy)
{
	return phf->fpGetHeightCallback ?
	       phf->getheight(ix, iy) :
	       ((float*)phf->fpGetSurfTypeCallback)[Vec2i(ix, iy) * phf->stride] * phf->heightscale;
}

void CPhysRenderer::DrawGeometry(IGeometry* pGeom, geom_world_data* pgwd, const ColorB& clr, const Vec3& sweepDir)
{
	Matrix33 R = Matrix33::CreateIdentity();
	Vec3 pos(ZERO), sz, center;
	float scale = 1.0f;
	int itype = pGeom->GetType();
	primitives::box bbox;
	if (pgwd)
	{
		R = pgwd->R;
		pos = pgwd->offset;
		scale = pgwd->scale;
	}
	pos = m_qrot*pos + m_offset;
	R = Matrix33(m_qrot)*R;

	pGeom->GetBBox(&bbox);
	sz = (bbox.size * (bbox.Basis *= R.T()).Fabs()) * scale;
	center = pos + R * bbox.center * scale;
	if (itype != GEOM_HEIGHTFIELD && !m_camera.IsAABBVisible_F(AABB(center - sz, center + sz)))
		return;

	// handle opacity
	ColorB clrNew = clr;
	float fAlpha = gEnv->pConsole->GetCVar("p_draw_helpers_opacity")->GetFVal();
	fAlpha = ::min(::max(0.0f, fAlpha), 1.0f);
	if (fAlpha < 0.99) clrNew.set(clr.r, clr.g, clr.b, (uint8)(fAlpha * (float)clr.a));

	IRenderAuxGeom::GetAux()->SetRenderFlags(e_Mode3D | e_AlphaBlended | e_FillModeSolid | e_CullModeBack | e_DepthTestOn |
	                               (clrNew.a == 255 ? e_DepthWriteOn : e_DepthWriteOff));

	DrawGeometry(itype, pGeom->GetData(), pgwd, clrNew, sweepDir);
}

void CPhysRenderer::DrawGeometry(int itype, const void* pGeomData, geom_world_data* pgwd, const ColorB& clr0, const Vec3& sweepDir)
{
	Matrix33 R = Matrix33::CreateIdentity();
	Vec3 pos(ZERO), center, sz, ldir0, ldir1(0, 0, 1), n, pt[5], campos;
	float scale = 1.0f, t, l, sx, dist;
	primitives::box bbox;
	ColorB clrlit[4], clr = clr0;
	IRenderAuxGeom* aux = IRenderAuxGeom::GetAux();
	int i, j;
#define FIAT_LUX(color)                                                                                                  \
  t = (n * ldir0) * -0.5f; l = t + fabs_tpl(t); t = (n * ldir1) * -0.1f; l += t + fabs_tpl(t); l = min(1.0f, l + 0.05f); \
  color = ColorB(FtoI(l * clr.r), FtoI(l * clr.g), FtoI(l * clr.b), clr.a)

	if (pgwd)
	{
		R = pgwd->R;
		pos = pgwd->offset;
		scale = pgwd->scale;
	}
	pos = m_qrot*pos + m_offset;
	R = Matrix33(m_qrot)*R;
	campos = m_camera.GetPosition() * 3;
	(ldir0 = m_camera.GetViewdir()).z = 0;
	(ldir0 = ldir0.normalized() * 0.5f).z = (float)sqrt3 * -0.5f;

	switch (itype)
	{
	case GEOM_TRIMESH:
	case GEOM_VOXELGRID:
		{
			mesh_data* pmesh = (mesh_data*)pGeomData;
			char* pmats = pmesh->pMats, dummyMat = 0;
			int matmask = -1;
			if (!pmats)
				pmats = &dummyMat, matmask = 0;
			float curTime = gEnv->pTimer->GetCurrTime();
			int icurTime = (int)curTime;
			float alpha = sin_tpl(((icurTime & 1) + (curTime - icurTime) * (1 - (icurTime & 1) * 2)) * gf_PI * 0.5f);
			alpha *= min(m_maxTrisRange, max(0, pmesh->nTris - m_maxTris)) / (float)m_maxTrisRange;
			ColorF clrf = ColorF(clr0.r, clr0.g, clr0.b, clr0.a) * (1.0f - alpha) + ColorF(180.0f, 0.0f, 0.0f, (float)clr0.a) * alpha;
			clr = ColorB(FtoI(clrf.r), FtoI(clrf.g), FtoI(clrf.b), FtoI(clrf.a));
			for (i = 0; i < pmesh->nTris; i++)
			{
				for (j = 0; j < 3; j++)
					pt[j] = R * pmesh->pVertices[pmesh->pIndices[i * 3 + j]] * scale + pos;
				n = R * pmesh->pNormals[i];
				FIAT_LUX(clrlit[0]);
				if (pmats[i & matmask] >= 0)
				{
					aux->DrawTriangle(pt[0], clrlit[0], pt[1], clrlit[0], pt[2], clrlit[0]);
					if ((pt[0] + pt[1] + pt[2] - campos).len2() < sqr(m_wireframeDist) * 9)
						aux->DrawPolyline(pt, 3, true, clr);
				}
			}
			break;
		}

	case GEOM_HEIGHTFIELD:
		{
			primitives::heightfield* phf = (primitives::heightfield*)pGeomData;
			center = phf->Basis * (((m_camera.GetPosition() - pos) * R) / scale - phf->origin);
			Vec2 c(center.x * phf->stepr.x, center.y * phf->stepr.y);
			ColorB clrhf[2];
			dist = min(100 / scale, 70 * phf->step.x); //m_camera.GetFarPlane()/scale;
			clrhf[0] = ColorB(FtoI(clr.r * 0.8f), FtoI(clr.g * 0.8f), FtoI(clr.b * 0.8f), clr.a);
			clrhf[1] = ColorB(FtoI(clr.r * 0.4f), FtoI(clr.g * 0.4f), FtoI(clr.b * 0.4f), clr.a);
			R *= phf->Basis.T();
			pos += R * phf->origin * scale;
			for (j = max(0, FtoI(c.y - dist * phf->stepr.y - 0.5f)); j <= min(phf->size.y - 1, FtoI(c.y + dist * phf->stepr.y - 0.5f)); j++)
			{
				sx = sqrt_tpl(max(0.0f, sqr(dist * phf->stepr.y) - sqr(j + 0.5f - c.y)));
				i = max(0, FtoI(c.x - sx - 0.5f));
				pt[0] = R * Vec3(i * phf->step.x, j * phf->step.y, getheight(phf, i, j)) * scale + pos;
				pt[1] = R * Vec3(i * phf->step.x, (j + 1) * phf->step.y, getheight(phf, i, j + 1)) * scale + pos;
				for (; i <= min(phf->size.x - 1, FtoI(c.x + sx - 0.5f)); i++)
				{
					clrlit[0] = clrhf[(i ^ j) & 1];
					pt[2] = R * Vec3((i + 1) * phf->step.x, j * phf->step.y, getheight(phf, i + 1, j)) * scale + pos;
					pt[3] = R * Vec3((i + 1) * phf->step.x, (j + 1) * phf->step.y, getheight(phf, i + 1, j + 1)) * scale + pos;
					if (!phf->fpGetSurfTypeCallback || !phf->fpGetHeightCallback || phf->fpGetSurfTypeCallback(i, j) != phf->typehole)
					{
						aux->DrawTriangle(pt[0], clrlit[0], pt[2], clrlit[0], pt[1], clrlit[0]);
						aux->DrawTriangle(pt[1], clrlit[0], pt[2], clrlit[0], pt[3], clrlit[0]);
					}
					pt[0] = pt[2];
					pt[1] = pt[3];
				}
			}
			break;
		}

	case GEOM_BOX:
		{
			primitives::box* pbox = (primitives::box*)pGeomData;
			bbox.Basis = pbox->Basis * R.T();
			bbox.center = pos + R * pbox->center * scale;
			bbox.size = pbox->size * scale;
			for (i = 0; i < 6; i++)
			{
				n = bbox.Basis.GetRow(i >> 1) * float((i * 2 & 2) - 1);
				FIAT_LUX(clrlit[0]);
				pt[4] = bbox.center + n * bbox.size[i >> 1];
				for (j = 0; j < 4; j++)
					pt[j] = pt[4] + bbox.Basis.GetRow(incm3(i >> 1)) * bbox.size[incm3(i >> 1)] * float(((j ^ i) * 2 & 2) - 1) +
					        bbox.Basis.GetRow(decm3(i >> 1)) * bbox.size[decm3(i >> 1)] * float((j & 2) - 1);
				aux->DrawTriangle(pt[0], clrlit[0], pt[2], clrlit[0], pt[3], clrlit[0]);
				aux->DrawTriangle(pt[0], clrlit[0], pt[3], clrlit[0], pt[1], clrlit[0]);
			}
			if (sweepDir.len2() > 0)
			{
				int ix, iy, iz;
				for (i = 0; i < 6; i++)
				{
					ix = i >> 1;
					iy = incm3(ix);
					iz = decm3(ix);
					pt[0][ix] = pt[1][ix] = bbox.size[ix] * ((i * 2 & 2) - 1) * sgnnz(sweepDir * bbox.Basis.GetRow(ix));
					pt[0][iy] = pt[1][iy] = bbox.size[iy] * (1 - (i * 2 & 2)) * sgnnz(sweepDir * bbox.Basis.GetRow(iy));
					pt[0][iz] = -bbox.size[iz];
					n = (bbox.Basis.GetRow(iz) ^ sweepDir).normalized();
					j = isneg(n * (pt[0] * bbox.Basis));
					pt[j ^ 1][iz] = -bbox.size[iz];
					pt[j][iz] = bbox.size[iz];
					n *= 1 - j * 2;
					pt[0] = bbox.center + pt[0] * bbox.Basis;
					pt[1] = bbox.center + pt[1] * bbox.Basis;
					FIAT_LUX(clrlit[0]);
					aux->DrawTriangle(pt[0], clrlit[0], pt[0] + sweepDir, clrlit[0], pt[1] + sweepDir, clrlit[0]);
					aux->DrawTriangle(pt[0], clrlit[0], pt[1] + sweepDir, clrlit[0], pt[1], clrlit[0]);
				}
				bbox.center += sweepDir;
				DrawGeometry(GEOM_BOX, &bbox, 0, clr);
			}
			break;
		}

	case GEOM_SPHERE:
		{
			assert(pgwd);
			primitives::sphere* psph = (primitives::sphere*)pGeomData;
			if (pgwd->iStartNode >= 0 || (pgwd->offset - campos * (1.0f / 3)).len2() < sqr(pgwd->iStartNode))
				aux->DrawSphere(pgwd->offset + pgwd->R * psph->center * pgwd->scale, psph->r * pgwd->scale, clr);
			if (sweepDir.len2() > 0)
			{
				pgwd->offset += sweepDir * 0.5f;
				primitives::cylinder cyl;
				cyl.center = psph->center;
				cyl.axis = sweepDir.normalized() * pgwd->R;
				cyl.r = psph->r;
				cyl.hh = sweepDir.len() * 0.5f;
				DrawGeometry(GEOM_CYLINDER, &cyl, pgwd, clr);
				pgwd->offset += sweepDir * 0.5f;
				DrawGeometry(GEOM_SPHERE, psph, pgwd, clr);
				pgwd->offset -= sweepDir;
			}
			break;
		}

	case GEOM_CYLINDER:
		{
			primitives::cylinder* pcyl = (primitives::cylinder*)pGeomData;
			const float cos15 = 0.96592582f, sin15 = 0.25881904f, cos7 = 0.99144486f, sin7 = 0.13052619f;
			float x, y;
			Vec3 axes[3];
			axes[2] = R * pcyl->axis;
			axes[0] = axes[2].GetOrthogonal().normalized();
			axes[1] = axes[2] ^ axes[0];
			center = R * pcyl->center * scale + pos;
			pt[0] = pt[2] = center + axes[0] * (pcyl->r * scale);
			n = axes[0];
			FIAT_LUX(clrlit[0]);
			n = axes[2];
			FIAT_LUX(clrlit[2]);
			n = -axes[2];
			FIAT_LUX(clrlit[3]);
			axes[2] *= pcyl->hh * scale;
			for (i = 0, x = cos15, y = sin15; i < 24; i++, pt[0] = pt[1], clrlit[0] = clrlit[1])
			{
				n = axes[0] * x + axes[1] * y;
				FIAT_LUX(clrlit[1]);
				pt[1] = center + n * (pcyl->r * scale);
				aux->DrawTriangle(pt[0] - axes[2], clrlit[0], pt[1] - axes[2], clrlit[1], pt[0] + axes[2], clrlit[0]);
				aux->DrawTriangle(pt[1] + axes[2], clrlit[1], pt[0] + axes[2], clrlit[0], pt[1] - axes[2], clrlit[1]);
				aux->DrawTriangle(pt[2] + axes[2], clrlit[2], pt[0] + axes[2], clrlit[2], pt[1] + axes[2], clrlit[2]);
				aux->DrawTriangle(pt[2] - axes[2], clrlit[3], pt[1] - axes[2], clrlit[3], pt[0] - axes[2], clrlit[3]);
				t = x;
				x = x * cos15 - y * sin15;
				y = y * cos15 + t * sin15;
			}
			if (sweepDir.len2() > 0)
			{
				assert(pgwd);
				int sg0, sg1, sgax = isneg(axes[2] * sweepDir);
				pt[0] = pt[2] = center + axes[0] * (pcyl->r * scale);
				n = axes[0] * cos7 + axes[1] * sin7;
				sg0 = isneg(n * sweepDir);
				for (i = 0, x = cos15, y = sin15; i < 24; i++, pt[0] = pt[1], sg0 = sg1)
				{
					n = axes[0] * (x * cos7 - y * sin7) + axes[1] * (y * cos7 + x * sin7);
					sg1 = isneg(n * sweepDir);
					pt[1] = center + n * (pcyl->r * scale);
					pt[2 + (sgax)] = pt[0] + axes[2] * ((sg0 ^ sgax) * 2 - 1);
					pt[3 - (sgax)] = pt[1] + axes[2] * ((sg0 ^ sgax) * 2 - 1);
					n = (pt[3] - pt[2] ^ sweepDir).normalized();
					FIAT_LUX(clrlit[0]);
					aux->DrawTriangle(pt[2], clrlit[0], pt[3], clrlit[0], pt[3] + sweepDir, clrlit[0]);
					aux->DrawTriangle(pt[2], clrlit[0], pt[3] + sweepDir, clrlit[0], pt[2] + sweepDir, clrlit[0]);
					if (sg0 ^ sg1)
					{
						n = (sweepDir ^ axes[2]).normalized();
						j = isneg(n * (pt[1] - center));
						n *= 1 - j * 2;
						FIAT_LUX(clrlit[0]);
						pt[3 - j] = pt[1] - axes[2];
						pt[2 + j] = pt[1] + axes[2];
						aux->DrawTriangle(pt[2], clrlit[0], pt[3], clrlit[0], pt[3] + sweepDir, clrlit[0]);
						aux->DrawTriangle(pt[2], clrlit[0], pt[3] + sweepDir, clrlit[0], pt[2] + sweepDir, clrlit[0]);
					}
					t = x;
					x = x * cos15 - y * sin15;
					y = y * cos15 + t * sin15;
				}
				pgwd->offset += sweepDir;
				DrawGeometry(GEOM_CYLINDER, pcyl, pgwd, clr);
				pgwd->offset -= sweepDir;
			}
			break;
		}

	case GEOM_CAPSULE:
		{
			primitives::cylinder* pcyl = (primitives::cylinder*)pGeomData;
			if (sweepDir.len2() == 0)
			{
				const float cos15 = 0.96592582f, sin15 = 0.25881904f;
				float x, y, cost, sint, costup, sintup;
				Vec3 axes[3], haxis, nxy;
				int icap;
				axes[2] = R * pcyl->axis;
				axes[0] = axes[2].GetOrthogonal().normalized();
				axes[1] = axes[2] ^ axes[0];
				center = R * pcyl->center * scale + pos;
				pt[0] = pt[2] = center + axes[0] * (pcyl->r * scale);
				haxis = axes[2] * (pcyl->hh * scale);
				n = axes[0];
				FIAT_LUX(clrlit[0]);
				for (i = 0, x = cos15, y = sin15; i < 24; i++, pt[0] = pt[1], clrlit[0] = clrlit[1])
				{
					n = axes[0] * x + axes[1] * y;
					FIAT_LUX(clrlit[1]);
					pt[1] = center + n * (pcyl->r * scale);
					aux->DrawTriangle(pt[0] - haxis, clrlit[0], pt[1] - haxis, clrlit[1], pt[0] + haxis, clrlit[0]);
					aux->DrawTriangle(pt[1] + haxis, clrlit[1], pt[0] + haxis, clrlit[0], pt[1] - haxis, clrlit[1]);
					t = x;
					x = x * cos15 - y * sin15;
					y = y * cos15 + t * sin15;
				}
				for (icap = 0; icap < 2; icap++, haxis.Flip(), axes[2].Flip())
					for (j = 0, cost = 1, sint = 0, costup = cos15, sintup = sin15; j < 6; j++)
					{
						n = axes[0] * cost + axes[2] * sint;
						FIAT_LUX(clrlit[0]);
						pt[0] = center + haxis + n * (pcyl->r * scale);
						n = axes[0] * costup + axes[2] * sintup;
						FIAT_LUX(clrlit[2]);
						pt[2] = center + haxis + n * (pcyl->r * scale);
						for (i = 0, x = cos15, y = sin15; i < 24; i++, pt[0] = pt[1], pt[2] = pt[3], clrlit[0] = clrlit[1], clrlit[2] = clrlit[3])
						{
							nxy = axes[0] * x + axes[1] * y;
							n = nxy * costup + axes[2] * sintup;
							FIAT_LUX(clrlit[3]);
							pt[3] = center + haxis + n * (pcyl->r * scale);
							n = nxy * cost + axes[2] * sint;
							FIAT_LUX(clrlit[1]);
							pt[1] = center + haxis + n * (pcyl->r * scale);
							aux->DrawTriangle(pt[0], clrlit[0], pt[1 + icap], clrlit[1 + icap], pt[2 - icap], clrlit[2 - icap]);
							aux->DrawTriangle(pt[1], clrlit[1], pt[3 - icap], clrlit[3 - icap], pt[2 + icap], clrlit[2 + icap]);
							t = x;
							x = x * cos15 - y * sin15;
							y = y * cos15 + t * sin15;
						}
						cost = costup;
						sint = sintup;
						costup = cost * cos15 - sint * sin15;
						sintup = sint * cos15 + cost * sin15;
					}
			}
			else
			{
				primitives::sphere sph;
				DrawGeometry(GEOM_CYLINDER, pcyl, pgwd, clr, sweepDir);
				sph.center = pcyl->center + pcyl->axis * pcyl->hh;
				sph.r = pcyl->r;
				DrawGeometry(GEOM_SPHERE, &sph, pgwd, clr, sweepDir);
				sph.center = pcyl->center - pcyl->axis * pcyl->hh;
				DrawGeometry(GEOM_SPHERE, &sph, pgwd, clr, sweepDir);
			}
			break;
		}

	case GEOM_RAY:
		{
			primitives::ray* pray = (primitives::ray*)pGeomData;
			pt[0] = pos + R * pray->origin * scale;
			pt[1] = pt[0] + R * pray->dir * scale;
			aux->DrawLine(pt[0], clr, pt[1], ColorB(0, 0, 0, clr.a));
			break;
		}
	}
}

#pragma warning(pop)
