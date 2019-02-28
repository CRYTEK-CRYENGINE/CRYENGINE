// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "WaterVolumeRenderNode.h"
#include "VisAreas.h"
#include "MatMan.h"
#include "TimeOfDay.h"
#include <CryMath/Cry_Geo.h>
#include <CryThreading/IJobManager_JobDelegator.h>

DECLARE_JOB("CWaterVolume_Render", TCWaterVolume_Render, CWaterVolumeRenderNode::Render_JobEntry);

//////////////////////////////////////////////////////////////////////////
// helpers

inline static Vec3 MapVertexToFogPlane(const Vec3& v, const Plane& p)
{
	const Vec3 projDir(0, 0, 1);
	float perpdist = p | v;
	float cosine = p.n | projDir;
	assert(fabs(cosine) > 1e-4);
	float pd_c = -perpdist / cosine;
	return v + projDir * pd_c;
}

//////////////////////////////////////////////////////////////////////////
// CWaterVolumeRenderNode implementation

CWaterVolumeRenderNode::CWaterVolumeRenderNode()
	: m_volumeType(IWaterVolumeRenderNode::eWVT_Unknown)
	, m_volumeID(~0)
	, m_volumeDepth(0)
	, m_streamSpeed(0)
	, m_wvParams()
	, m_pSerParams(nullptr)
	, m_pPhysAreaInput(nullptr)
	, m_pPhysArea(nullptr)
	, m_parentEntityWorldTM(IDENTITY)
	, m_nLayerId(0)
	, m_fogDensity(0.0f)
	, m_fogColor(0.2f, 0.5f, 0.7f)
	, m_fogColorAffectedBySun(true)
	, m_fogShadowing(0.5f)
	, m_fogPlane(Vec3(0, 0, 1), 0)
	, m_fogPlaneBase(Vec3(0, 0, 1), 0)
	, m_vOffset(ZERO)
	, m_center(ZERO)
	, m_WSBBox(Vec3(-1), Vec3(1))
	, m_capFogAtVolumeDepth(false)
	, m_attachedToEntity(false)
	, m_caustics(true)
	, m_causticIntensity(1.0f)
	, m_causticTiling(1.0f)
	, m_causticShadow(0.0f)
	, m_causticHeight(0.5f)
{
	GetInstCount(GetRenderNodeType())++;

	m_pWaterBodyIntoMat = GetMatMan()->LoadMaterial("%ENGINE%/EngineAssets/Materials/Fog/WaterFogVolumeInto", false);
	m_pWaterBodyOutofMat = GetMatMan()->LoadMaterial("%ENGINE%/EngineAssets/Materials/Fog/WaterFogVolumeOutof", false);
	for (int i = 0; i < RT_COMMAND_BUF_COUNT; ++i)
	{
		m_pVolumeRE[i] = GetRenderer() ?
		                 static_cast<CREWaterVolume*>(GetRenderer()->EF_CreateRE(eDATA_WaterVolume)) : nullptr;
		if (m_pVolumeRE[i])
		{
			m_pVolumeRE[i]->m_drawWaterSurface = false;
			m_pVolumeRE[i]->m_pParams = &m_wvParams[i];
		}
	}
	for (int i = 0; i < RT_COMMAND_BUF_COUNT; ++i)
	{
		m_pSurfaceRE[i] = GetRenderer() ?
		                  static_cast<CREWaterVolume*>(GetRenderer()->EF_CreateRE(eDATA_WaterVolume)) : nullptr;
		if (m_pSurfaceRE[i])
		{
			m_pSurfaceRE[i]->m_drawWaterSurface = true;
			m_pSurfaceRE[i]->m_pParams = &m_wvParams[i];
		}
	}
}

CWaterVolumeRenderNode::~CWaterVolumeRenderNode()
{
	GetInstCount(GetRenderNodeType())--;

	Dephysicalize();

	for (int i = 0; i < RT_COMMAND_BUF_COUNT; ++i)
	{
		if (m_pVolumeRE[i])
		{
			m_pVolumeRE[i]->Release(false);
			m_pVolumeRE[i] = 0;
		}
		if (m_pSurfaceRE[i])
		{
			m_pSurfaceRE[i]->Release(false);
			m_pSurfaceRE[i] = 0;
		}
	}
	SAFE_DELETE(m_pSerParams);
	SAFE_DELETE(m_pPhysAreaInput);

	Get3DEngine()->FreeRenderNodeState(this);
}

void CWaterVolumeRenderNode::SetAreaAttachedToEntity()
{
	m_attachedToEntity = true;
}

void CWaterVolumeRenderNode::SetFogDensity(float fogDensity)
{
	m_fogDensity = fogDensity;
}

float CWaterVolumeRenderNode::GetFogDensity() const
{
	return m_fogDensity;
}

void CWaterVolumeRenderNode::SetFogColor(const Vec3& fogColor)
{
	m_fogColor = fogColor;
}

void CWaterVolumeRenderNode::SetFogColorAffectedBySun(bool enable)
{
	m_fogColorAffectedBySun = enable;
}

void CWaterVolumeRenderNode::SetFogShadowing(float fogShadowing)
{
	m_fogShadowing = fogShadowing;
}

void CWaterVolumeRenderNode::SetCapFogAtVolumeDepth(bool capFog)
{
	m_capFogAtVolumeDepth = capFog;
}

void CWaterVolumeRenderNode::SetVolumeDepth(float volumeDepth)
{
	m_volumeDepth = volumeDepth;

	UpdateBoundingBox();
}

void CWaterVolumeRenderNode::SetStreamSpeed(float streamSpeed)
{
	m_streamSpeed = streamSpeed;
}

void CWaterVolumeRenderNode::SetCaustics(bool caustics)
{
	m_caustics = caustics;
}

void CWaterVolumeRenderNode::SetCausticIntensity(float causticIntensity)
{
	m_causticIntensity = causticIntensity;
}

void CWaterVolumeRenderNode::SetCausticTiling(float causticTiling)
{
	m_causticTiling = causticTiling;
}

void CWaterVolumeRenderNode::SetCausticHeight(float causticHeight)
{
	m_causticHeight = causticHeight;
}

void CWaterVolumeRenderNode::CreateOcean(uint64 volumeID, /* TBD */ bool keepSerializationParams)
{
}

void CWaterVolumeRenderNode::CreateArea(uint64 volumeID, const Vec3* pVertices, unsigned int numVertices, const Vec2& surfUVScale, const Plane& fogPlane, bool keepSerializationParams)
{
	const bool serializeWith3DEngine = keepSerializationParams && !IsAttachedToEntity();

	assert(fabs(fogPlane.n.GetLengthSquared() - 1.0f) < 1e-4 && "CWaterVolumeRenderNode::CreateArea(...) -- Fog plane normal doesn't have unit length!");
	if (fogPlane.n.Dot(Vec3(0, 0, 1)) <= 1e-4f)
	{
		// CWaterVolumeRenderNode::CreateArea(...) -- Invalid fog plane specified!
		return;
	}

	assert(numVertices >= 3);
	if (numVertices < 3)
		return;

	m_volumeID = volumeID;
	m_fogPlane = fogPlane;
	m_fogPlaneBase = fogPlane;
	m_volumeType = IWaterVolumeRenderNode::eWVT_Area;

	// copy volatile creation params to be able to serialize water volume if needed (only in editor)
	if (serializeWith3DEngine)
		CopyVolatileAreaSerParams(pVertices, numVertices, surfUVScale);

	// remove form 3d engine
	Get3DEngine()->UnRegisterEntityAsJob(this);

	// Edges pre-pass - break into smaller edges, in case distance threshold too big
	PodArray<Vec3> pTessVertList;
	PodArray<SVF_P3F_C4B_T2F> pVertsTemp;
	PodArray<uint16> pIndicesTemp;

	for (uint32 v(0); v < numVertices; ++v)
	{
		Vec3 in_a = pVertices[v];
		Vec3 in_b = (v < numVertices - 1) ? pVertices[v + 1] : pVertices[0]; // close mesh

		Vec3 vAB = in_b - in_a;
		float fLenAB = vAB.len();
		vAB.normalize();

		pTessVertList.push_back(in_a);

		const float fLenThreshold = 100.0f;  // break every 100 meters
		Vec3 vNewVert = Vec3(in_a + (vAB * fLenThreshold));
		while (fLenAB > fLenThreshold)
		{
			pTessVertList.push_back(vNewVert);

			vNewVert = Vec3(vNewVert + (vAB * fLenThreshold));
			vAB = in_b - vNewVert;
			fLenAB = vAB.len();
			vAB.normalize();
		}
	}

	m_waterSurfaceVertices.resize(pTessVertList.size());
	for (uint32 i = 0; i < pTessVertList.size(); ++i)
	{
		// project input vertex onto fog plane
		m_waterSurfaceVertices[i].xyz = MapVertexToFogPlane(pTessVertList[i], fogPlane);

		// generate texture coordinates
		m_waterSurfaceVertices[i].st = Vec2(surfUVScale.x * (pTessVertList[i].x - pTessVertList[0].x), surfUVScale.y * (pTessVertList[i].y - pTessVertList[0].y));

		pVertsTemp.push_back(m_waterSurfaceVertices[i]);
	}

	// generate indices.
	//	Note: triangulation code not robust, relies on contour/vertices to be declared sequentially and no holes -> too many vertices will lead to stretched results
	TPolygon2D<SVF_P3F_C4B_T2F, Vec3>(m_waterSurfaceVertices).Triangulate(m_waterSurfaceIndices);

	// update bounding info
	UpdateBoundingBox();

	// Safety check.
	if (m_waterSurfaceIndices.empty())
		return;

	// Pre-tessellate mesh further
	uint32 nIterations = 4;
	for (uint32 i = 0; i < nIterations; ++i)
	{
		uint32 nIndices = m_waterSurfaceIndices.size();
		for (uint32 t = 0; t < nIndices; t += 3)
		{
			// Get triangle, compute median edge vertex, insert to vertex list
			uint16 id_a = m_waterSurfaceIndices[t + 0];
			uint16 id_b = m_waterSurfaceIndices[t + 1];
			uint16 id_c = m_waterSurfaceIndices[t + 2];

			SVF_P3F_C4B_T2F& vtx_a = m_waterSurfaceVertices[id_a];
			SVF_P3F_C4B_T2F& vtx_b = m_waterSurfaceVertices[id_b];
			SVF_P3F_C4B_T2F& vtx_c = m_waterSurfaceVertices[id_c];

			SVF_P3F_C4B_T2F vtx_m_ab;
			vtx_m_ab.xyz = (vtx_a.xyz + vtx_b.xyz) * 0.5f;
			vtx_m_ab.st = (vtx_a.st + vtx_b.st) * 0.5f;
			vtx_m_ab.color = vtx_a.color;

			pVertsTemp.push_back(vtx_m_ab);
			uint16 id_d = (uint16) pVertsTemp.size() - 1;

			SVF_P3F_C4B_T2F vtx_m_bc;
			vtx_m_bc.xyz = (vtx_b.xyz + vtx_c.xyz) * 0.5f;
			vtx_m_bc.st = (vtx_b.st + vtx_c.st) * 0.5f;
			vtx_m_bc.color = vtx_a.color;

			pVertsTemp.push_back(vtx_m_bc);
			uint16 id_e = (uint16) pVertsTemp.size() - 1;

			SVF_P3F_C4B_T2F vtx_m_ca;
			vtx_m_ca.xyz = (vtx_a.xyz + vtx_c.xyz) * 0.5f;
			vtx_m_ca.st = (vtx_a.st + vtx_c.st) * 0.5f;
			vtx_m_ca.color = vtx_a.color;

			pVertsTemp.push_back(vtx_m_ca);
			uint16 id_f = (uint16) pVertsTemp.size() - 1;

			// build new indices

			// aed
			pIndicesTemp.push_back(id_a);
			pIndicesTemp.push_back(id_d);
			pIndicesTemp.push_back(id_f);

			// ebd
			pIndicesTemp.push_back(id_d);
			pIndicesTemp.push_back(id_b);
			pIndicesTemp.push_back(id_e);

			// bfd
			pIndicesTemp.push_back(id_f);
			pIndicesTemp.push_back(id_d);
			pIndicesTemp.push_back(id_e);

			// fcd
			pIndicesTemp.push_back(id_f);
			pIndicesTemp.push_back(id_e);
			pIndicesTemp.push_back(id_c);
		}

		// update index list for new iteration
		m_waterSurfaceIndices.resize(pIndicesTemp.size());
		memcpy(&m_waterSurfaceIndices[0], &pIndicesTemp[0], sizeof(uint16) * pIndicesTemp.size());
		m_waterSurfaceVertices.resize(pVertsTemp.size());
		memcpy(&m_waterSurfaceVertices[0], &pVertsTemp[0], sizeof(SVF_P3F_C4B_T2F) * pVertsTemp.size());
		pIndicesTemp.clear();
	}

	// update reference to vertex and index buffer
	for (int i = 0; i < RT_COMMAND_BUF_COUNT; ++i)
	{
		m_wvParams[i].m_pVertices = &m_waterSurfaceVertices[0];
		m_wvParams[i].m_numVertices = m_waterSurfaceVertices.size();
		m_wvParams[i].m_pIndices = &m_waterSurfaceIndices[0];
		m_wvParams[i].m_numIndices = m_waterSurfaceIndices.size();
	}

	// add to 3d engine
	Get3DEngine()->RegisterEntity(this);
}

void CWaterVolumeRenderNode::CreateRiver(uint64 volumeID, const Vec3* pVertices, unsigned int numVertices, float uTexCoordBegin, float uTexCoordEnd, const Vec2& surfUVScale, const Plane& fogPlane, bool keepSerializationParams)
{
	assert(fabs(fogPlane.n.GetLengthSquared() - 1.0f) < 1e-4 && "CWaterVolumeRenderNode::CreateRiver(...) -- Fog plane normal doesn't have unit length!");
	assert(fogPlane.n.Dot(Vec3(0, 0, 1)) > 1e-4f && "CWaterVolumeRenderNode::CreateRiver(...) -- Invalid fog plane specified!");
	if (fogPlane.n.Dot(Vec3(0, 0, 1)) <= 1e-4f)
		return;

	assert(numVertices == 4);
	if (numVertices != 4 || !_finite(pVertices[0].x) || !_finite(pVertices[1].x) || !_finite(pVertices[2].x) || !_finite(pVertices[3].x))
		return;

	m_volumeID = volumeID;
	m_fogPlane = fogPlane;
	m_fogPlaneBase = fogPlane;
	m_volumeType = IWaterVolumeRenderNode::eWVT_River;

	// copy volatile creation params to be able to serialize water volume if needed (only in editor)
	if (keepSerializationParams)
		CopyVolatileRiverSerParams(pVertices, numVertices, uTexCoordBegin, uTexCoordEnd, surfUVScale);

	// remove form 3d engine
	Get3DEngine()->UnRegisterEntityAsJob(this);

	// generate vertices
	m_waterSurfaceVertices.resize(5);
	m_waterSurfaceVertices[0].xyz = pVertices[0];
	m_waterSurfaceVertices[1].xyz = pVertices[1];
	m_waterSurfaceVertices[2].xyz = pVertices[2];
	m_waterSurfaceVertices[3].xyz = pVertices[3];
	m_waterSurfaceVertices[4].xyz = 0.25f * (pVertices[0] + pVertices[1] + pVertices[2] + pVertices[3]);

	Vec3 tv0 = Vec3(0, 0, 1.f);
	Vec3 tv1 = Vec3(0, 0, -1.f);
	Plane planes[4];
	planes[0].SetPlane(pVertices[0], pVertices[1], pVertices[1] + tv0);
	planes[1].SetPlane(pVertices[2], pVertices[3], pVertices[3] + tv1);
	planes[2].SetPlane(pVertices[0], pVertices[2], pVertices[2] + tv1);
	planes[3].SetPlane(pVertices[1], pVertices[3], pVertices[3] + tv0);

	for (uint32 i(0); i < 5; ++i)
	{
		// map input vertex onto fog plane
		m_waterSurfaceVertices[i].xyz = MapVertexToFogPlane(m_waterSurfaceVertices[i].xyz, fogPlane);

		// generate texture coordinates
		float d0(planes[0].DistFromPlane(m_waterSurfaceVertices[i].xyz));
		float d1(planes[1].DistFromPlane(m_waterSurfaceVertices[i].xyz));
		float d2(planes[2].DistFromPlane(m_waterSurfaceVertices[i].xyz));
		float d3(planes[3].DistFromPlane(m_waterSurfaceVertices[i].xyz));
		float t(fabsf(d0 + d1) < FLT_EPSILON ? 0.0f : d0 / (d0 + d1));

		Vec2 st = Vec2((1 - t) * fabsf(uTexCoordBegin) + t * fabsf(uTexCoordEnd), fabsf(d2 + d3) < FLT_EPSILON ? 0.0f : d2 / (d2 + d3));
		st[0] *= surfUVScale.x;
		st[1] *= surfUVScale.y;

		m_waterSurfaceVertices[i].st = st;
	}

	// generate indices
	m_waterSurfaceIndices.resize(12);
	m_waterSurfaceIndices[0] = 0;
	m_waterSurfaceIndices[1] = 1;
	m_waterSurfaceIndices[2] = 4;

	m_waterSurfaceIndices[3] = 1;
	m_waterSurfaceIndices[4] = 3;
	m_waterSurfaceIndices[5] = 4;

	m_waterSurfaceIndices[6] = 3;
	m_waterSurfaceIndices[7] = 2;
	m_waterSurfaceIndices[8] = 4;

	m_waterSurfaceIndices[9] = 0;
	m_waterSurfaceIndices[10] = 4;
	m_waterSurfaceIndices[11] = 2;

	// update bounding info
	UpdateBoundingBox();

	// update reference to vertex and index buffer
	for (int i = 0; i < RT_COMMAND_BUF_COUNT; ++i)
	{
		m_wvParams[i].m_pVertices = &m_waterSurfaceVertices[0];
		m_wvParams[i].m_numVertices = m_waterSurfaceVertices.size();
		m_wvParams[i].m_pIndices = &m_waterSurfaceIndices[0];
		m_wvParams[i].m_numIndices = m_waterSurfaceIndices.size();
	}

	// add to 3d engine
	Get3DEngine()->RegisterEntity(this);
}

void CWaterVolumeRenderNode::SetAreaPhysicsArea(const Vec3* pVertices, unsigned int numVertices, bool keepSerializationParams)
{
	const bool serializeWith3DEngine = keepSerializationParams && !IsAttachedToEntity();

	assert(pVertices && numVertices > 3 && m_volumeType == IWaterVolumeRenderNode::eWVT_Area);
	if (!pVertices || numVertices <= 3 || m_volumeType != IWaterVolumeRenderNode::eWVT_Area)
		return;

	if (!m_pPhysAreaInput)
		m_pPhysAreaInput = new SWaterVolumePhysAreaInput;

	const Plane& fogPlane(m_fogPlane);

	// generate contour vertices
	m_pPhysAreaInput->m_contour.resize(numVertices);

	// map input vertices onto fog plane
	if (TPolygon2D<Vec3>(pVertices, numVertices).Area() > 0.0f)
	{
		for (unsigned int i(0); i < numVertices; ++i)
			m_pPhysAreaInput->m_contour[i] = MapVertexToFogPlane(pVertices[i], fogPlane);   // flip vertex order as physics expects them CCW
	}
	else
	{
		for (unsigned int i(0); i < numVertices; ++i)
			m_pPhysAreaInput->m_contour[i] = MapVertexToFogPlane(pVertices[numVertices - 1 - i], fogPlane);
	}

	// triangulate contour
	TPolygon2D<Vec3>(m_pPhysAreaInput->m_contour).Triangulate(m_pPhysAreaInput->m_indices);

	// reset flow
	m_pPhysAreaInput->m_flowContour.resize(0);

	if (serializeWith3DEngine)
		CopyVolatilePhysicsAreaContourSerParams(pVertices, numVertices);
}

void CWaterVolumeRenderNode::SetRiverPhysicsArea(const Vec3* pVertices, unsigned int numVertices, bool keepSerializationParams)
{
	assert(pVertices && numVertices > 3 && !(numVertices & 1) && m_volumeType == IWaterVolumeRenderNode::eWVT_River);
	if (!pVertices || numVertices <= 3 || (numVertices & 1) || m_volumeType != IWaterVolumeRenderNode::eWVT_River)
		return;

	if (!m_pPhysAreaInput)
		m_pPhysAreaInput = new SWaterVolumePhysAreaInput;

	const Plane& fogPlane(m_fogPlane);

	// generate contour vertices
	m_pPhysAreaInput->m_contour.resize(numVertices);

	// map input vertices onto fog plane
	if (TPolygon2D<Vec3>(pVertices, numVertices).Area() > 0.0f)
	{
		for (unsigned int i(0); i < numVertices; ++i)
			m_pPhysAreaInput->m_contour[i] = MapVertexToFogPlane(pVertices[i], fogPlane);   // flip vertex order as physics expects them CCW
	}
	else
	{
		for (unsigned int i(0); i < numVertices; ++i)
			m_pPhysAreaInput->m_contour[i] = MapVertexToFogPlane(pVertices[numVertices - 1 - i], fogPlane);
	}

	// generate flow along contour
	unsigned int h(numVertices / 2);
	unsigned int h2(numVertices);
	m_pPhysAreaInput->m_flowContour.resize(numVertices);
	for (unsigned int i(0); i < h; ++i)
	{
		if (!i)
			m_pPhysAreaInput->m_flowContour[i] = (m_pPhysAreaInput->m_contour[i + 1] - m_pPhysAreaInput->m_contour[i]).GetNormalizedSafe() * m_streamSpeed;
		else if (i == h - 1)
			m_pPhysAreaInput->m_flowContour[i] = (m_pPhysAreaInput->m_contour[i] - m_pPhysAreaInput->m_contour[i - 1]).GetNormalizedSafe() * m_streamSpeed;
		else
			m_pPhysAreaInput->m_flowContour[i] = (m_pPhysAreaInput->m_contour[i + 1] - m_pPhysAreaInput->m_contour[i - 1]).GetNormalizedSafe() * m_streamSpeed;
	}

	for (unsigned int i(0); i < h; ++i)
	{
		if (!i)
			m_pPhysAreaInput->m_flowContour[h2 - 1 - i] = (m_pPhysAreaInput->m_contour[h2 - 1 - i - 1] - m_pPhysAreaInput->m_contour[h2 - 1 - i]).GetNormalizedSafe() * m_streamSpeed;
		else if (i == h - 1)
			m_pPhysAreaInput->m_flowContour[h2 - 1 - i] = (m_pPhysAreaInput->m_contour[h2 - 1 - i] - m_pPhysAreaInput->m_contour[h2 - 1 - i + 1]).GetNormalizedSafe() * m_streamSpeed;
		else
			m_pPhysAreaInput->m_flowContour[h2 - 1 - i] = (m_pPhysAreaInput->m_contour[h2 - 1 - i - 1] - m_pPhysAreaInput->m_contour[h2 - 1 - i + 1]).GetNormalizedSafe() * m_streamSpeed;
	}

	// triangulate contour
	m_pPhysAreaInput->m_indices.resize(3 * 2 * (numVertices / 2 - 1));
	for (unsigned int i(0); i < h - 1; ++i)
	{
		m_pPhysAreaInput->m_indices[6 * i + 0] = i;
		m_pPhysAreaInput->m_indices[6 * i + 1] = i + 1;
		m_pPhysAreaInput->m_indices[6 * i + 2] = h2 - 1 - i - 1;

		m_pPhysAreaInput->m_indices[6 * i + 3] = h2 - 1 - i - 1;
		m_pPhysAreaInput->m_indices[6 * i + 4] = h2 - 1 - i;
		m_pPhysAreaInput->m_indices[6 * i + 5] = i;
	}

	if (keepSerializationParams)
		CopyVolatilePhysicsAreaContourSerParams(pVertices, numVertices);
}

IPhysicalEntity* CWaterVolumeRenderNode::SetAndCreatePhysicsArea(const Vec3* pVertices, unsigned int numVertices)
{
	SetAreaPhysicsArea(pVertices, numVertices, false);

	return CreatePhysicsAreaFromSettings();
}

const char* CWaterVolumeRenderNode::GetEntityClassName() const
{
	return "WaterVolume";
}

const char* CWaterVolumeRenderNode::GetName() const
{
	return "WaterVolume";
}

IRenderNode* CWaterVolumeRenderNode::Clone() const
{
	CWaterVolumeRenderNode* pWaterVol = new CWaterVolumeRenderNode();

	// CWaterVolumeRenderNode member vars
	pWaterVol->m_volumeType = m_volumeType;

	pWaterVol->m_volumeID = m_volumeID;

	pWaterVol->m_volumeDepth = m_volumeDepth;
	pWaterVol->m_streamSpeed = m_streamSpeed;

	pWaterVol->m_pMaterial = m_pMaterial;
	pWaterVol->m_pWaterBodyIntoMat = m_pWaterBodyIntoMat;
	pWaterVol->m_pWaterBodyOutofMat = m_pWaterBodyOutofMat;

	if (m_pPhysAreaInput != NULL)
	{
		pWaterVol->m_pPhysAreaInput = new SWaterVolumePhysAreaInput;
		*pWaterVol->m_pPhysAreaInput = *m_pPhysAreaInput;
	}
	else
	{
		pWaterVol->m_pPhysAreaInput = NULL;
	}

	pWaterVol->m_waterSurfaceVertices = m_waterSurfaceVertices;
	pWaterVol->m_waterSurfaceIndices = m_waterSurfaceIndices;

	pWaterVol->m_parentEntityWorldTM = m_parentEntityWorldTM;
	pWaterVol->m_nLayerId = m_nLayerId;

	pWaterVol->m_fogDensity = m_fogDensity;
	pWaterVol->m_fogColor = m_fogColor;
	pWaterVol->m_fogColorAffectedBySun = m_fogColorAffectedBySun;
	pWaterVol->m_fogShadowing = m_fogShadowing;

	pWaterVol->m_fogPlane = m_fogPlane;
	pWaterVol->m_fogPlaneBase = m_fogPlaneBase;

	pWaterVol->m_center = m_center;
	pWaterVol->m_WSBBox = m_WSBBox;

	pWaterVol->m_capFogAtVolumeDepth = m_capFogAtVolumeDepth;
	pWaterVol->m_attachedToEntity = m_attachedToEntity;
	pWaterVol->m_caustics = m_caustics;

	pWaterVol->m_causticIntensity = m_causticIntensity;
	pWaterVol->m_causticTiling = m_causticTiling;
	pWaterVol->m_causticShadow = m_causticShadow;
	pWaterVol->m_causticHeight = m_causticHeight;

	// update reference to vertex and index buffer
	for (int i = 0; i < RT_COMMAND_BUF_COUNT; ++i)
	{
		pWaterVol->m_wvParams[i].m_pVertices = &pWaterVol->m_waterSurfaceVertices[0];
		pWaterVol->m_wvParams[i].m_numVertices = pWaterVol->m_waterSurfaceVertices.size();
		pWaterVol->m_wvParams[i].m_pIndices = &pWaterVol->m_waterSurfaceIndices[0];
		pWaterVol->m_wvParams[i].m_numIndices = pWaterVol->m_waterSurfaceIndices.size();
	}

	//IRenderNode member vars
	//	We cannot just copy over due to issues with the linked list of IRenderNode objects
	CopyIRenderNodeData(pWaterVol);

	return pWaterVol;
}

inline void TransformPosition(Vec3& pos, const Vec3& localOrigin, const Matrix34& l2w)
{
	pos = pos - localOrigin;
	pos = l2w * pos;
}

void CWaterVolumeRenderNode::Transform(const Vec3& localOrigin, const Matrix34& l2w)
{
	CRY_ASSERT_MESSAGE(!IsAttachedToEntity(), "FIXME: Don't currently support transforming attached water volumes");

	if (m_pPhysAreaInput != NULL)
	{
		for (std::vector<Vec3>::iterator it = m_pPhysAreaInput->m_contour.begin(); it != m_pPhysAreaInput->m_contour.end(); ++it)
		{
			Vec3& pos = *it;
			TransformPosition(pos, localOrigin, l2w);
		}

		for (std::vector<Vec3>::iterator it = m_pPhysAreaInput->m_flowContour.begin(); it != m_pPhysAreaInput->m_flowContour.end(); ++it)
		{
			Vec3& pos = *it;
			TransformPosition(pos, localOrigin, l2w);
		}
	}

	for (WaterSurfaceVertices::iterator it = m_waterSurfaceVertices.begin(); it != m_waterSurfaceVertices.end(); ++it)
	{
		SVF_P3F_C4B_T2F& vert = *it;
		TransformPosition(vert.xyz, localOrigin, l2w);
	}

	Vec3 origFogPoint = m_fogPlane.n * m_fogPlane.d;
	TransformPosition(origFogPoint, localOrigin, l2w);
	m_fogPlane.SetPlane(l2w.TransformVector(m_fogPlaneBase.n).GetNormalized(), origFogPoint);

	TransformPosition(m_center, localOrigin, l2w);

	UpdateBoundingBox();
}

void CWaterVolumeRenderNode::SetMatrix(const Matrix34& mat)
{
	if (!IsAttachedToEntity())
		return;

	m_parentEntityWorldTM = mat;
	m_fogPlane.SetPlane(m_parentEntityWorldTM.TransformVector(m_fogPlaneBase.n).GetNormalized(), m_parentEntityWorldTM.GetTranslation());

	UpdateBoundingBox();
}

void CWaterVolumeRenderNode::Render(const SRendParams& rParam, const SRenderingPassInfo& passInfo)
{
	Render_JobEntry(rParam, passInfo);
}

void CWaterVolumeRenderNode::Render_JobEntry(SRendParams rParam, SRenderingPassInfo passInfo)
{
	FUNCTION_PROFILER_3DENGINE;

	// hack: special case for when inside amphibious vehicle
	if (Get3DEngine()->GetOceanRenderFlags() & OCR_NO_DRAW)
		return;

	// anything to render?
	if (passInfo.IsRecursivePass() || !m_pMaterial || !m_pWaterBodyIntoMat || !m_pWaterBodyOutofMat || !passInfo.RenderWaterVolumes() ||
	    m_waterSurfaceVertices.empty() || m_waterSurfaceIndices.empty())
		return;

	if (m_fogDensity == 0)
		return;

	IRenderer* pRenderer(GetRenderer());

	const int fillThreadID = passInfo.ThreadID();

	// get render objects
	CRenderObject* pROVol = passInfo.GetIRenderView()->AllocateTemporaryRenderObject();
	CRenderObject* pROSurf = passInfo.GetIRenderView()->AllocateTemporaryRenderObject();
	if (!pROVol || !pROSurf)
		return;

	if (!m_pSurfaceRE[fillThreadID])
		return;

	float distToWaterVolumeSurface(GetCameraDistToWaterVolumeSurface(passInfo));
	bool aboveWaterVolumeSurface(distToWaterVolumeSurface > 0.0f);
	bool belowWaterVolume(m_capFogAtVolumeDepth && distToWaterVolumeSurface < -m_volumeDepth);
	bool insideWaterVolume = false;
	if (!aboveWaterVolumeSurface && !belowWaterVolume) // check for z-range: early abort, since the following triangle check is more expensive
	{
		insideWaterVolume = IsCameraInsideWaterVolumeSurface2D(passInfo); // surface triangle check
	}


	// fill parameters to render elements
	m_wvParams[fillThreadID].m_viewerInsideVolume = insideWaterVolume;
	m_wvParams[fillThreadID].m_viewerCloseToWaterPlane = /*insideWaterVolumeSurface2D && */ fabsf(distToWaterVolumeSurface) < 0.5f;
	m_wvParams[fillThreadID].m_viewerCloseToWaterVolume = GetCameraDistSqToWaterVolumeAABB(passInfo) < 9.0f; //Sq dist

	const float hdrMultiplier = m_fogColorAffectedBySun ? 1.0f : ((CTimeOfDay*)Get3DEngine()->GetTimeOfDay())->GetHDRMultiplier();

	m_wvParams[fillThreadID].m_fogDensity = m_fogDensity;
	m_wvParams[fillThreadID].m_fogColor = m_fogColor * hdrMultiplier;
	m_wvParams[fillThreadID].m_fogColorAffectedBySun = m_fogColorAffectedBySun;
	m_wvParams[fillThreadID].m_fogPlane = m_fogPlane;
	m_wvParams[fillThreadID].m_fogShadowing = m_fogShadowing;

	m_wvParams[fillThreadID].m_caustics = m_caustics;
	m_wvParams[fillThreadID].m_causticIntensity = m_causticIntensity;
	m_wvParams[fillThreadID].m_causticTiling = m_causticTiling;
	m_wvParams[fillThreadID].m_causticHeight = m_causticHeight;

	m_wvParams[fillThreadID].m_center = m_center;
	m_wvParams[fillThreadID].m_WSBBox = m_WSBBox;

	// if above water render fog together with water surface
	bool isFastpath = GetCVars()->e_WaterVolumes == 2 && (distToWaterVolumeSurface > 0.5f || insideWaterVolume);
	m_pSurfaceRE[fillThreadID]->m_drawFastPath = isFastpath;

	// submit volume
	if (GetCVars()->e_Fog)
	{
		if ((insideWaterVolume || (!isFastpath && aboveWaterVolumeSurface)) && m_pVolumeRE[fillThreadID])
		{
			CRY_ASSERT(!(m_pVolumeRE[fillThreadID]->m_drawWaterSurface));

			// fill in data for render object
			if (!IsAttachedToEntity())
			{
				pROVol->SetMatrix(Matrix34::CreateIdentity(), passInfo);
			}
			else
			{
				pROVol->SetMatrix(m_parentEntityWorldTM, passInfo);
				pROVol->m_ObjFlags |= FOB_TRANS_MASK;
			}
			pROVol->m_fSort = 0;

			auto pMaterial = m_wvParams[fillThreadID].m_viewerInsideVolume ? m_pWaterBodyOutofMat.get() : m_pWaterBodyIntoMat.get();
			pROVol->m_pCurrMaterial = pMaterial;

			// get shader item
			SShaderItem& shaderItem(pMaterial->GetShaderItem(0));

			// add to renderer
			passInfo.GetIRenderView()->AddRenderObject(m_pVolumeRE[fillThreadID], shaderItem, pROVol, passInfo, EFSLIST_WATER_VOLUMES, aboveWaterVolumeSurface ? 0 : 1);
		}
	}

	// submit surface
	{
		CRY_ASSERT(m_pSurfaceRE[fillThreadID]->m_drawWaterSurface);

		// fill in data for render object
		if (!IsAttachedToEntity())
		{
			pROSurf->SetMatrix(Matrix34::CreateIdentity(), passInfo);
		}
		else
		{
			pROSurf->SetMatrix(m_parentEntityWorldTM, passInfo);
			pROSurf->m_ObjFlags |= FOB_TRANS_MASK;
		}
		pROSurf->m_fSort = 0;

		pROSurf->m_pCurrMaterial = m_pMaterial;

		// get shader item
		SShaderItem& shaderItem(m_pMaterial->GetShaderItem(0));

		// add to renderer
		passInfo.GetIRenderView()->AddRenderObject(m_pSurfaceRE[fillThreadID], shaderItem, pROSurf, passInfo, EFSLIST_WATER, 1);
	}
}

void CWaterVolumeRenderNode::SetMaterial(IMaterial* pMat)
{
	m_pMaterial = pMat;
}

void CWaterVolumeRenderNode::GetMemoryUsage(ICrySizer* pSizer) const
{
	SIZER_COMPONENT_NAME(pSizer, "WaterVolumeNode");
	pSizer->AddObject(this, sizeof(*this));
	pSizer->AddObject(m_pSerParams);
	pSizer->AddObject(m_pPhysAreaInput);
	pSizer->AddObject(m_waterSurfaceVertices);
	pSizer->AddObject(m_waterSurfaceIndices);
}

void CWaterVolumeRenderNode::Precache()
{
}

IPhysicalEntity* CWaterVolumeRenderNode::GetPhysics() const
{
	return m_pPhysArea;
}

void CWaterVolumeRenderNode::SetPhysics(IPhysicalEntity* pPhysArea)
{
	m_pPhysArea = pPhysArea;
}

void CWaterVolumeRenderNode::CheckPhysicalized()
{
	if (!GetPhysics())
		Physicalize();
}

void CWaterVolumeRenderNode::Physicalize(bool bInstant)
{
	if (IsAttachedToEntity())
		return;

	Dephysicalize();

	// setup physical area
	m_pPhysArea = CreatePhysicsAreaFromSettings();

	if (m_pPhysArea)
	{
		pe_status_pos sp;
		m_pPhysArea->GetStatus(&sp);

		pe_params_buoyancy pb;
		pb.waterPlane.n = sp.q * Vec3(0, 0, 1);
		pb.waterPlane.origin = m_pPhysAreaInput->m_contour[0];
		//pb.waterFlow = sp.q * Vec3( m_streamSpeed, 0, 0 );
		m_pPhysArea->SetParams(&pb);

		pe_params_foreign_data pfd;
		pfd.pForeignData = this;
		pfd.iForeignData = PHYS_FOREIGN_ID_WATERVOLUME;
		pfd.iForeignFlags = 0;
		m_pPhysArea->SetParams(&pfd);

		m_pPhysArea->SetParams(&m_auxPhysParams);
	}
}

void CWaterVolumeRenderNode::Dephysicalize(bool bKeepIfReferenced)
{
	if (m_pPhysArea)
	{
		GetPhysicalWorld()->DestroyPhysicalEntity(m_pPhysArea);
		m_pPhysArea = 0;
		m_attachedToEntity = false;
	}
}

float CWaterVolumeRenderNode::GetCameraDistToWaterVolumeSurface(const SRenderingPassInfo& passInfo) const
{
	const CCamera& cam(passInfo.GetCamera());
	Vec3 camPos(cam.GetPosition());
	return m_fogPlane.DistFromPlane(camPos);
}

float CWaterVolumeRenderNode::GetCameraDistSqToWaterVolumeAABB(const SRenderingPassInfo& passInfo) const
{
	const CCamera& cam(passInfo.GetCamera());
	Vec3 camPos(cam.GetPosition());
	return m_WSBBox.GetDistanceSqr(camPos);
}

namespace
{

	// aabb check - only for xy-axis
	bool IsPointInsideAABB_2d(const AABB& aabb, const Vec3& pos)
	{
		if (pos.x < aabb.min.x) return false;
		if (pos.y < aabb.min.y) return false;
		if (pos.x > aabb.max.x) return false;
		if (pos.y > aabb.max.y) return false;
		return true;
	}

}

bool CWaterVolumeRenderNode::IsCameraInsideWaterVolumeSurface2D(const SRenderingPassInfo& passInfo) const
{
	const CCamera& cam(passInfo.GetCamera());
	const Vec3 camPosWS(cam.GetPosition());

	pe_status_area sa;
	sa.bUniformOnly = true;
	MARK_UNUSED sa.ctr;
	if (m_pPhysArea && m_pPhysArea->GetStatus(&sa) && sa.pSurface)
	{
		pe_status_contains_point scp;
		scp.pt = camPosWS;
		return m_pPhysArea->GetStatus(&scp) != 0;
	}

	// bounding box test in world space to abort early
	if (!IsPointInsideAABB_2d(m_WSBBox, camPosWS)) return false;

	// check triangles in entity space (i.e., water volume space), to avoid transformation of each single water volume vertex - thus, transform camera pos into entity space
	const Vec3 camPos_entitySpace = m_parentEntityWorldTM.GetInvertedFast() * camPosWS;
	TPolygon2D<SVF_P3F_C4B_T2F, Vec3> ca(m_waterSurfaceVertices);
	for (size_t i(0); i < m_waterSurfaceIndices.size(); i += 3)
	{
		const Vec3& v0 = ca[m_waterSurfaceIndices[i]];
		const Vec3& v1 = ca[m_waterSurfaceIndices[i + 1]];
		const Vec3& v2 = ca[m_waterSurfaceIndices[i + 2]];

		if (ca.InsideTriangle(v0.x, v0.y, v1.x, v1.y, v2.x, v2.y, camPos_entitySpace.x, camPos_entitySpace.y))
		{
			return true;
		}
	}

	return false;
}

void CWaterVolumeRenderNode::UpdateBoundingBox()
{
	m_WSBBox.Reset();
	for (size_t i(0); i < m_waterSurfaceVertices.size(); ++i)
		m_WSBBox.Add(m_parentEntityWorldTM.TransformPoint(m_waterSurfaceVertices[i].xyz));

	if (IVisArea* pArea = Get3DEngine()->GetVisAreaFromPos(m_WSBBox.GetCenter()))
	{
		if (m_WSBBox.min.z > pArea->GetAABBox()->min.z)
			m_WSBBox.min.z = pArea->GetAABBox()->min.z;
		return;
	}

	m_WSBBox.min.z -= m_volumeDepth;
	m_center = m_WSBBox.GetCenter();
}

const SWaterVolumeSerialize* CWaterVolumeRenderNode::GetSerializationParams()
{
	if (!m_pSerParams)
		return 0;

	// before returning, copy non-volatile serialization params
	m_pSerParams->m_volumeType = m_volumeType;
	m_pSerParams->m_volumeID = m_volumeID;

	m_pSerParams->m_pMaterial = m_pMaterial;

	m_pSerParams->m_fogDensity = m_fogDensity;
	m_pSerParams->m_fogColor = m_fogColor;
	m_pSerParams->m_fogColorAffectedBySun = m_fogColorAffectedBySun;
	m_pSerParams->m_fogPlane = m_fogPlane;
	m_pSerParams->m_fogShadowing = m_fogShadowing;

	m_pSerParams->m_volumeDepth = m_volumeDepth;
	m_pSerParams->m_streamSpeed = m_streamSpeed;
	m_pSerParams->m_capFogAtVolumeDepth = m_capFogAtVolumeDepth;

	m_pSerParams->m_caustics = m_caustics;
	m_pSerParams->m_causticIntensity = m_causticIntensity;
	m_pSerParams->m_causticTiling = m_causticTiling;
	m_pSerParams->m_causticHeight = m_causticHeight;

	return m_pSerParams;
}

void CWaterVolumeRenderNode::CopyVolatilePhysicsAreaContourSerParams(const Vec3* pVertices, unsigned int numVertices)
{
	if (!m_pSerParams)
		m_pSerParams = new SWaterVolumeSerialize;

	m_pSerParams->m_physicsAreaContour.resize(numVertices);
	for (unsigned int i(0); i < numVertices; ++i)
		m_pSerParams->m_physicsAreaContour[i] = pVertices[i];
}

void CWaterVolumeRenderNode::CopyVolatileRiverSerParams(const Vec3* pVertices, unsigned int numVertices, float uTexCoordBegin, float uTexCoordEnd, const Vec2& surfUVScale)
{
	if (!m_pSerParams)
		m_pSerParams = new SWaterVolumeSerialize;

	m_pSerParams->m_uTexCoordBegin = uTexCoordBegin;
	m_pSerParams->m_uTexCoordEnd = uTexCoordEnd;

	m_pSerParams->m_surfUScale = surfUVScale.x;
	m_pSerParams->m_surfVScale = surfUVScale.y;

	m_pSerParams->m_vertices.resize(numVertices);
	for (uint32 i(0); i < numVertices; ++i)
		m_pSerParams->m_vertices[i] = pVertices[i];
}

void CWaterVolumeRenderNode::CopyVolatileAreaSerParams(const Vec3* pVertices, unsigned int numVertices, const Vec2& surfUVScale)
{
	if (!m_pSerParams)
		m_pSerParams = new SWaterVolumeSerialize;

	m_pSerParams->m_uTexCoordBegin = 1.0f;
	m_pSerParams->m_uTexCoordEnd = 1.0f;

	m_pSerParams->m_surfUScale = surfUVScale.x;
	m_pSerParams->m_surfVScale = surfUVScale.y;

	m_pSerParams->m_vertices.resize(numVertices);
	for (uint32 i(0); i < numVertices; ++i)
		m_pSerParams->m_vertices[i] = pVertices[i];
}

int OnWaterUpdate(const EventPhysAreaChange* pEvent)
{
	if (pEvent->iForeignData == PHYS_FOREIGN_ID_WATERVOLUME)
	{
		CWaterVolumeRenderNode* pWVRN = (CWaterVolumeRenderNode*)pEvent->pForeignData;
		pe_status_area sa;
		sa.bUniformOnly = true;
		MARK_UNUSED sa.ctr;
		if (pWVRN->GetPhysics() != pEvent->pEntity || !pEvent->pEntity->GetStatus(&sa))
			return 1;
		if (pEvent->pContainer)
		{
			pWVRN->SetAreaAttachedToEntity();
			pWVRN->SetMatrix(Matrix34(Vec3(1), pEvent->qContainer, pEvent->posContainer));
		}
		pWVRN->SyncToPhysMesh(QuatT(pEvent->q, pEvent->pos), sa.pSurface, pEvent->depth);
	}
	return 1;
}

IPhysicalEntity* CWaterVolumeRenderNode::CreatePhysicsAreaFromSettings()
{
	if (!m_pPhysAreaInput)
		return NULL;

	Vec3* pFlow(!m_pPhysAreaInput->m_flowContour.empty() ? &m_pPhysAreaInput->m_flowContour[0] : 0);
	//assert( m_pPhysAreaInput->m_contour.size() >= 3 && ( !pFlow || m_pPhysAreaInput->m_contour.size() == m_pPhysAreaInput->m_flowContour.size() ) && m_pPhysAreaInput->m_indices.size() >= 3 && m_pPhysAreaInput->m_indices.size() % 3 == 0 );
	if (m_pPhysAreaInput->m_contour.size() < 3 || (pFlow && m_pPhysAreaInput->m_contour.size() != m_pPhysAreaInput->m_flowContour.size()) || m_pPhysAreaInput->m_indices.size() < 3 || m_pPhysAreaInput->m_indices.size() % 3 != 0)
		return NULL;

	GetPhysicalWorld()->AddEventClient(EventPhysAreaChange::id, (int (*)(const EventPhys*))OnWaterUpdate, 1);

	// setup physical area
	return GetPhysicalWorld()->AddArea(&m_pPhysAreaInput->m_contour[0], m_pPhysAreaInput->m_contour.size(), min(0.0f, -m_volumeDepth), 10.0f,
	                                   Vec3(ZERO), Quat(IDENTITY), 1.0f, Vec3(ZERO), &m_pPhysAreaInput->m_indices[0], m_pPhysAreaInput->m_indices.size() / 3, pFlow);
}

void CWaterVolumeRenderNode::SyncToPhysMesh(const QuatT& qtSurface, IGeometry* pSurface, float depth)
{
	mesh_data* pmd;
	if (!pSurface || pSurface->GetType() != GEOM_TRIMESH || !(pmd = (mesh_data*)pSurface->GetData()))
		return;
	bool bResized = false;
	if (bResized = m_waterSurfaceVertices.size() != pmd->nVertices)
		m_waterSurfaceVertices.resize(pmd->nVertices);
	Vec2 uvScale = m_pSerParams ? Vec2(m_pSerParams->m_surfUScale, m_pSerParams->m_surfVScale) : Vec2(1.0f, 1.0f);
	for (int i = 0; i < pmd->nVertices; i++)
	{
		m_waterSurfaceVertices[i].xyz = qtSurface * pmd->pVertices[i];
		m_waterSurfaceVertices[i].st = Vec2(pmd->pVertices[i].x * uvScale.x, pmd->pVertices[i].y * uvScale.y);
	}
	if (m_waterSurfaceIndices.size() != pmd->nTris * 3)
		m_waterSurfaceIndices.resize(pmd->nTris * 3), bResized = true;
	for (int i = 0; i < pmd->nTris * 3; i++)
		m_waterSurfaceIndices[i] = pmd->pIndices[i];

	if (bResized)
		for (int i = 0; i < RT_COMMAND_BUF_COUNT; ++i)
		{
			m_wvParams[i].m_pVertices = &m_waterSurfaceVertices[0];
			m_wvParams[i].m_numVertices = m_waterSurfaceVertices.size();
			m_wvParams[i].m_pIndices = &m_waterSurfaceIndices[0];
			m_wvParams[i].m_numIndices = m_waterSurfaceIndices.size();
		}

	m_fogPlane.SetPlane(qtSurface.q * Vec3(0, 0, 1), qtSurface.t);
	m_volumeDepth = depth;
	UpdateBoundingBox();
}

void CWaterVolumeRenderNode::OffsetPosition(const Vec3& delta)
{
	if (const auto pTempData = m_pTempData.load()) pTempData->OffsetPosition(delta);
	m_vOffset += delta;
	m_center += delta;
	m_WSBBox.Move(delta);
	for (int i = 0; i < (int)m_waterSurfaceVertices.size(); ++i)
		m_waterSurfaceVertices[i].xyz += delta;

	if (m_pPhysAreaInput)
	{
		for (std::vector<Vec3>::iterator it = m_pPhysAreaInput->m_contour.begin(); it != m_pPhysAreaInput->m_contour.end(); ++it)
			*it += delta;
		for (std::vector<Vec3>::iterator it = m_pPhysAreaInput->m_flowContour.begin(); it != m_pPhysAreaInput->m_flowContour.end(); ++it)
			*it += delta;
	}

	if (m_pPhysArea)
	{
		pe_params_pos par_pos;
		m_pPhysArea->GetParams(&par_pos);
		par_pos.bRecalcBounds |= 2;
		par_pos.pos = m_vOffset;
		m_pPhysArea->SetParams(&par_pos);
	}
}

void CWaterVolumeRenderNode::FillBBox(AABB& aabb)
{
	aabb = CWaterVolumeRenderNode::GetBBox();
}

EERType CWaterVolumeRenderNode::GetRenderNodeType()
{
	return eERType_WaterVolume;
}

float CWaterVolumeRenderNode::GetMaxViewDist()
{
	if (GetMinSpecFromRenderNodeFlags(m_dwRndFlags) == CONFIG_DETAIL_SPEC)
		return max(GetCVars()->e_ViewDistMin, CWaterVolumeRenderNode::GetBBox().GetRadius() * GetCVars()->e_ViewDistRatioDetail * GetViewDistRatioNormilized());

	return max(GetCVars()->e_ViewDistMin, CWaterVolumeRenderNode::GetBBox().GetRadius() * GetCVars()->e_ViewDistRatio * GetViewDistRatioNormilized());
}

Vec3 CWaterVolumeRenderNode::GetPos(bool bWorldOnly) const
{
	return m_center;
}

IMaterial* CWaterVolumeRenderNode::GetMaterial(Vec3* pHitPos) const
{
	return m_pMaterial;
}

bool CWaterVolumeRenderNode::CanExecuteRenderAsJob()
{
	return !gEnv->IsEditor() && GetCVars()->e_ExecuteRenderAsJobMask & BIT(GetRenderNodeType());
}
