// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DriverD3D.h"
#include "D3DRenderAuxGeom.h"
#include "GraphicsPipeline/StandardGraphicsPipeline.h"
#include <climits>

#include "Common/RenderDisplayContext.h"

#include "Common/ReverseDepth.h"

#if defined(ENABLE_RENDER_AUX_GEOM)

const float c_clipThres(0.1f);

enum EAuxGeomBufferSizes
{
	e_auxGeomVBSize = 0xffff,
	e_auxGeomIBSize = e_auxGeomVBSize * 2 * 3
};

struct SAuxObjVertex
{
	SAuxObjVertex() = default;

	SAuxObjVertex(const Vec3& pos, const Vec3& normal)
		: m_pos(pos)
		, m_normal(normal)
	{
	}

	Vec3 m_pos;
	Vec3 m_normal;
};


using AuxObjVertexBuffer = std::vector<SAuxObjVertex>;
using AuxObjIndexBuffer = std::vector<vtx_idx>;

CRenderAuxGeomD3D::CRenderAuxGeomD3D(CD3D9Renderer& renderer)
	: m_renderer(renderer)
	, m_geomPass(false)
	, m_textPass(false)
	, m_matrices()
	, m_curPrimType(CAuxGeomCB::e_PrimTypeInvalid)
	, m_curPointSize(1)
	, m_curTransMatrixIdx(-1)
	, m_curWorldMatrixIdx(-1)
	, m_pAuxGeomShader(nullptr)
	, m_curDrawInFrontMode(e_DrawInFrontOff)
	, m_auxSortedPushBuffer()
	, m_pCurCBRawData(nullptr)
	, CV_r_auxGeom(1)
{
	REGISTER_CVAR2("r_auxGeom", &CV_r_auxGeom, 1, VF_NULL, "");
}

CRenderAuxGeomD3D::~CRenderAuxGeomD3D()
{
	ClearCaches();
	ReleaseDeviceObjects();
}

void CRenderAuxGeomD3D::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject((char*)this - 16, sizeof(*this) + 16);  // adjust for new operator
	pSizer->AddObject(m_auxSortedPushBuffer);
}

void CRenderAuxGeomD3D::ReleaseDeviceObjects()
{
	for (uint32 i(0); i < e_auxObjNumLOD; ++i)
	{
		m_sphereObj[i].Release();
		m_coneObj[i].Release();
		m_cylinderObj[i].Release();
	}
}

void CRenderAuxGeomD3D::ReleaseResources()
{
	ClearCaches();

	m_geomPass.Reset();
	m_textPass.Reset();

	SAFE_RELEASE(m_pAuxGeomShader);
}

int CRenderAuxGeomD3D::GetDeviceDataSize()
{
	int nSize = 0;

	for (uint32 i = 0; i < e_auxObjNumLOD; ++i)
	{
		nSize += m_sphereObj[i].GetDeviceDataSize();
		nSize += m_coneObj[i].GetDeviceDataSize();
		nSize += m_cylinderObj[i].GetDeviceDataSize();
	}
	return nSize;
}

// function to generate a sphere mesh
static void CreateSphere(AuxObjVertexBuffer& vb, AuxObjIndexBuffer& ib, float radius, unsigned int rings, unsigned int sections)
{
	// calc required number of vertices/indices/triangles to build a sphere for the given parameters
	uint32 numVertices((rings - 1) * (sections + 1) + 2);
	uint32 numTriangles((rings - 2) * sections * 2 + 2 * sections);
	uint32 numIndices(numTriangles * 3);

	// setup buffers
	vb.clear();
	vb.reserve(numVertices);

	ib.clear();
	ib.reserve(numIndices);

	// 1st pole vertex
	vb.emplace_back(Vec3(0.0f, 0.0f, radius), Vec3(0.0f, 0.0f, 1.0f));

	// calculate "inner" vertices
	float sectionSlice(DEG2RAD(360.0f / (float) sections));
	float ringSlice(DEG2RAD(180.0f / (float) rings));

	for (uint32 a(1); a < rings; ++a)
	{
		float w(sinf(a * ringSlice));
		for (uint32 i(0); i <= sections; ++i)
		{
			Vec3 v;
			v.x = radius * cosf(i * sectionSlice) * w;
			v.y = radius * sinf(i * sectionSlice) * w;
			v.z = radius * cosf(a * ringSlice);
			vb.emplace_back(v, v.GetNormalized());
		}
	}

	// 2nd vertex of pole (for end cap)
	vb.emplace_back(Vec3(0.0f, 0.0f, -radius), Vec3(0.0f, 0.0f, 1.0f));

	// build "inner" faces
	for (uint32 a(0); a < rings - 2; ++a)
	{
		for (uint32 i(0); i < sections; ++i)
		{
			ib.push_back((vtx_idx) (1 + a * (sections + 1) + i + 1));
			ib.push_back((vtx_idx) (1 + a * (sections + 1) + i));
			ib.push_back((vtx_idx) (1 + (a + 1) * (sections + 1) + i + 1));

			ib.push_back((vtx_idx) (1 + (a + 1) * (sections + 1) + i));
			ib.push_back((vtx_idx) (1 + (a + 1) * (sections + 1) + i + 1));
			ib.push_back((vtx_idx) (1 + a * (sections + 1) + i));
		}
	}

	// build faces for end caps (to connect "inner" vertices with poles)
	for (uint32 i(0); i < sections; ++i)
	{
		ib.push_back((vtx_idx) (1 + (0) * (sections + 1) + i));
		ib.push_back((vtx_idx) (1 + (0) * (sections + 1) + i + 1));
		ib.push_back((vtx_idx) 0);
	}

	for (uint32 i(0); i < sections; ++i)
	{
		ib.push_back((vtx_idx) (1 + (rings - 2) * (sections + 1) + i + 1));
		ib.push_back((vtx_idx) (1 + (rings - 2) * (sections + 1) + i));
		ib.push_back((vtx_idx) ((rings - 1) * (sections + 1) + 1));
	}
}

// function to generate a cone mesh
static void CreateCone(AuxObjVertexBuffer& vb, AuxObjIndexBuffer& ib, float radius, float height, unsigned int sections)
{
	// calc required number of vertices/indices/triangles to build a cone for the given parameters
	uint32 numVertices(2 * (sections + 1) + 2);
	uint32 numTriangles(2 * sections);
	uint32 numIndices(numTriangles * 3);

	// setup buffers
	vb.clear();
	vb.reserve(numVertices);

	ib.clear();
	ib.reserve(numIndices);

	// center vertex
	vb.emplace_back(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f));

	// create circle around it
	float sectionSlice(DEG2RAD(360.0f / (float) sections));
	for (uint32 i(0); i <= sections; ++i)
	{
		Vec3 v;
		v.x = radius * cosf(i * sectionSlice);
		v.y = 0.0f;
		v.z = radius * sinf(i * sectionSlice);
		vb.emplace_back(v, Vec3(0.0f, -1.0f, 0.0f));
	}

	// build faces for end cap
	for (uint16 i(0); i < sections; ++i)
	{
		ib.push_back((vtx_idx)(0));
		ib.push_back((vtx_idx)(1 + i));
		ib.push_back((vtx_idx)(1 + i + 1));
	}

	// top
	vb.emplace_back(Vec3(0.0f, height, 0.0f), Vec3(0.0f, 1.0f, 0.0f));

	for (uint32 i(0); i <= sections; ++i)
	{
		Vec3 v;
		v.x = radius * cosf(i * sectionSlice);
		v.y = 0.0f;
		v.z = radius * sinf(i * sectionSlice);

		Vec3 v1;
		v1.x = radius * cosf(i * sectionSlice + 0.01f);
		v1.y = 0.0f;
		v1.z = radius * sinf(i * sectionSlice + 0.01f);

		Vec3 d(v1 - v);
		Vec3 d1(Vec3(0.0, height, 0.0f) - v);

		Vec3 n((d1.Cross(d)).normalized());
		vb.emplace_back(v, n);
	}

	// build faces
	for (uint16 i(0); i < sections; ++i)
	{
		ib.push_back((vtx_idx)(sections + 2));
		ib.push_back((vtx_idx)(sections + 3 + i + 1));
		ib.push_back((vtx_idx)(sections + 3 + i));
	}
}

// function to generate a cylinder mesh
static void CreateCylinder(AuxObjVertexBuffer& vb, AuxObjIndexBuffer& ib, float radius, float height, unsigned int sections)
{
	// calc required number of vertices/indices/triangles to build a cylinder for the given parameters
	uint32 numVertices(4 * (sections + 1) + 2);
	uint32 numTriangles(4 * sections);
	uint32 numIndices(numTriangles * 3);

	// setup buffers
	vb.clear();
	vb.reserve(numVertices);

	ib.clear();
	ib.reserve(numIndices);

	float sectionSlice(DEG2RAD(360.0f / (float) sections));

	// bottom cap
	{
		// center bottom vertex
		vb.emplace_back(Vec3(0.0f, -0.5f * height, 0.0f), Vec3(0.0f, -1.0f, 0.0f));

		// create circle around it
		for (uint32 i(0); i <= sections; ++i)
		{
			Vec3 v;
			v.x = radius * cosf(i * sectionSlice);
			v.y = -0.5f * height;
			v.z = radius * sinf(i * sectionSlice);
			vb.emplace_back(v, Vec3(0.0f, -1.0f, 0.0f));
		}

		// build faces
		for (uint16 i(0); i < sections; ++i)
		{
			ib.push_back((vtx_idx)(0));
			ib.push_back((vtx_idx)(1 + i));
			ib.push_back((vtx_idx)(1 + i + 1));
		}
	}

	// side
	{
		int vIdx(vb.size());

		for (uint32 i(0); i <= sections; ++i)
		{
			Vec3 v;
			v.x = radius * cosf(i * sectionSlice);
			v.y = -0.5f * height;
			v.z = radius * sinf(i * sectionSlice);

			Vec3 n(v.normalized());
			vb.emplace_back(v, n);
			vb.emplace_back(Vec3(v.x, -v.y, v.z), n);
		}

		// build faces
		for (uint16 i(0); i < sections; ++i, vIdx += 2)
		{
			ib.push_back((vtx_idx)(vIdx));
			ib.push_back((vtx_idx)(vIdx + 1));
			ib.push_back((vtx_idx)(vIdx + 2));

			ib.push_back((vtx_idx)(vIdx + 1));
			ib.push_back((vtx_idx)(vIdx + 3));
			ib.push_back((vtx_idx)(vIdx + 2));

		}
	}

	// top cap
	{
		size_t vIdx(vb.size());

		// center top vertex
		vb.emplace_back(Vec3(0.0f, 0.5f * height, 0.0f), Vec3(0.0f, 1.0f, 0.0f));

		// create circle around it
		for (uint32 i(0); i <= sections; ++i)
		{
			Vec3 v;
			v.x = radius * cosf(i * sectionSlice);
			v.y = 0.5f * height;
			v.z = radius * sinf(i * sectionSlice);
			vb.emplace_back(v, Vec3(0.0f, 1.0f, 0.0f));
		}

		// build faces
		for (uint16 i(0); i < sections; ++i)
		{
			ib.push_back(vIdx);
			ib.push_back(vIdx + 1 + i + 1);
			ib.push_back(vIdx + 1 + i);
		}
	}
}

// Functor to generate a sphere mesh. To be used with generalized CreateMesh function.
struct SSphereMeshCreateFunc
{
	SSphereMeshCreateFunc(float radius, unsigned int rings, unsigned int sections)
		: m_radius(radius)
		, m_rings(rings)
		, m_sections(sections)
	{
	}

	void CreateMesh(AuxObjVertexBuffer& vb, AuxObjIndexBuffer& ib)
	{
		CreateSphere(vb, ib, m_radius, m_rings, m_sections);
	}

	float        m_radius;
	unsigned int m_rings;
	unsigned int m_sections;
};

// Functor to generate a cone mesh. To be used with generalized CreateMesh function.
struct SConeMeshCreateFunc
{
	SConeMeshCreateFunc(float radius, float height, unsigned int sections)
		: m_radius(radius)
		, m_height(height)
		, m_sections(sections)
	{
	}

	void CreateMesh(AuxObjVertexBuffer& vb, AuxObjIndexBuffer& ib)
	{
		CreateCone(vb, ib, m_radius, m_height, m_sections);
	}

	float        m_radius;
	float        m_height;
	unsigned int m_sections;
};

// Functor to generate a cylinder mesh. To be used with generalized CreateMesh function.
struct SCylinderMeshCreateFunc
{
	SCylinderMeshCreateFunc(float radius, float height, unsigned int sections)
		: m_radius(radius)
		, m_height(height)
		, m_sections(sections)
	{
	}

	void CreateMesh(AuxObjVertexBuffer& vb, AuxObjIndexBuffer& ib)
	{
		CreateCylinder(vb, ib, m_radius, m_height, m_sections);
	}

	float        m_radius;
	float        m_height;
	unsigned int m_sections;
};

void CRenderAuxGeomD3D::SDrawObjMesh::Release()
{
	if( m_pVB != ~0u ) gcpRendD3D->m_DevBufMan.Destroy(m_pVB);
	if( m_pIB != ~0u ) gcpRendD3D->m_DevBufMan.Destroy(m_pIB);

	m_numVertices = 0;
	m_numFaces = 0;
}

int CRenderAuxGeomD3D::SDrawObjMesh::GetDeviceDataSize() const
{
	int nSize = 0;
	nSize += gcpRendD3D->m_DevBufMan.Size(m_pVB);
	nSize += gcpRendD3D->m_DevBufMan.Size(m_pIB);

	return nSize;
}

template<class T = vtx_idx, int size = sizeof(T)*CHAR_BIT> struct indexbuffer_type;

template<class T> struct indexbuffer_type<T, 16> { static const RenderIndexType type = Index16; };
template<class T> struct indexbuffer_type<T, 32> { static const RenderIndexType type = Index32; };

// Generalized CreateMesh function to create any mesh via passed-in functor.
// The functor needs to provide a CreateMesh function which accepts an
// AuxObjVertexBuffer and AuxObjIndexBuffer to stored the resulting mesh.
template<typename TMeshFunc>
HRESULT CRenderAuxGeomD3D::CreateMesh(SDrawObjMesh& mesh, TMeshFunc meshFunc)
{
	// create mesh
	AuxObjVertexBuffer vb;
	AuxObjIndexBuffer  ib;
	meshFunc.CreateMesh(vb, ib);

	// create vertex buffer and copy data
	mesh.m_pVB = gcpRendD3D->m_DevBufMan.Create(BBT_VERTEX_BUFFER, BU_STATIC, vb.size() * sizeof(SAuxObjVertex));
	mesh.m_pIB = gcpRendD3D->m_DevBufMan.Create(BBT_INDEX_BUFFER, BU_STATIC, ib.size() * sizeof(vtx_idx));

	gcpRendD3D->m_DevBufMan.UpdateBuffer(mesh.m_pVB, &vb[0], vb.size() * sizeof(SAuxObjVertex));
	gcpRendD3D->m_DevBufMan.UpdateBuffer(mesh.m_pIB, &ib[0], ib.size() * sizeof(vtx_idx));

#if !defined(RELEASE) && CRY_PLATFORM_WINDOWS
	{
		buffer_size_t vbOffset = 0, ibOffset = 0;
		D3DVertexBuffer* vbAux = gcpRendD3D->m_DevBufMan.GetD3DVB(mesh.m_pVB, &vbOffset);
		D3DIndexBuffer * ibAux = gcpRendD3D->m_DevBufMan.GetD3DIB(mesh.m_pIB, &ibOffset);

		vbAux->SetPrivateData(WKPDID_D3DDebugObjectName, strlen("Aux Geom Mesh"), "Aux Geom Mesh");
		ibAux->SetPrivateData(WKPDID_D3DDebugObjectName, strlen("Aux Geom Mesh"), "Aux Geom Mesh");
	}
#endif

	// write mesh info
	mesh.m_numVertices = vb.size();
	mesh.m_numFaces = ib.size() / 3;

	SStreamInfo vertexStreamInfo;
	vertexStreamInfo.hStream = mesh.m_pVB;
	vertexStreamInfo.nStride = sizeof(SAuxObjVertex);
	vertexStreamInfo.nSlot = 0;
	mesh.m_primitive.m_pVertexInputSet = GetDeviceObjectFactory().CreateVertexStreamSet(1, &vertexStreamInfo);

	SStreamInfo indexStreamInfo;
	indexStreamInfo.hStream = mesh.m_pIB;
	indexStreamInfo.nStride = indexbuffer_type<vtx_idx>::type;
	indexStreamInfo.nSlot = 0;
	mesh.m_primitive.m_pIndexInputSet = GetDeviceObjectFactory().CreateIndexStreamSet(&indexStreamInfo);

	mesh.m_primitive.m_drawInfo.vertexOrIndexCount = ib.size();
	mesh.m_primitive.m_drawInfo.vertexOrIndexOffset = 0;

	return S_OK;
}

HRESULT CRenderAuxGeomD3D::RestoreDeviceObjects()
{
	HRESULT hr(S_OK);


	// recreate aux objects
	for (uint32 i(0); i < e_auxObjNumLOD; ++i)
	{
		m_sphereObj[i].Release();
		if (FAILED(hr = CreateMesh(m_sphereObj[i], SSphereMeshCreateFunc(1.0f, 9 + 4 * i, 9 + 4 * i))))
		{
			return(hr);
		}

		m_coneObj[i].Release();
		if (FAILED(hr = CreateMesh(m_coneObj[i], SConeMeshCreateFunc(1.0f, 1.0f, 10 + i * 6))))
		{
			return(hr);
		}

		m_cylinderObj[i].Release();
		if (FAILED(hr = CreateMesh(m_cylinderObj[i], SCylinderMeshCreateFunc(1.0f, 1.0f, 10 + i * 6))))
		{
			return(hr);
		}
	}
	return(hr);
}

CRenderAuxGeomD3D::CBufferManager::~CBufferManager()
{
	if( vbAux != ~0u ) gcpRendD3D->m_DevBufMan.Destroy(vbAux);
	if( ibAux != ~0u ) gcpRendD3D->m_DevBufMan.Destroy(ibAux);
}

buffer_handle_t CRenderAuxGeomD3D::CBufferManager::update(BUFFER_BIND_TYPE type, const void* data, size_t size)
{
	buffer_handle_t buf = gcpRendD3D->m_DevBufMan.Create(type, BU_TRANSIENT, size);//BU_DYNAMIC

	gcpRendD3D->m_DevBufMan.UpdateBuffer(buf, data, size);

	return buf;
}

buffer_handle_t CRenderAuxGeomD3D::CBufferManager::fill(buffer_handle_t buf, BUFFER_BIND_TYPE type, const void* data, size_t size)
{
	if( buf != ~0u ) gcpRendD3D->m_DevBufMan.Destroy(buf);

	return size ? update(type, data, size) : ~0u;
}

bool CRenderAuxGeomD3D::PreparePass(const CCamera& camera, const SDisplayContextKey& displayContextKey, CPrimitiveRenderPass& pass, SRenderViewport* getViewport)
{
	CRenderDisplayContext* displayContext = gcpRendD3D.FindDisplayContext(displayContextKey);
	if (!displayContext)
		return false;

	// Toggle current back-buffer if the output is connected to a swap-chain
	displayContext->PostPresent();

	// update transformation matrices
	m_matrices.UpdateMatrices(camera);

	const SRenderViewport& vp = displayContext->GetViewport();
	D3DViewPort viewport = { float(vp.x), float(vp.y), float(vp.x) + vp.width, float(vp.y) + vp.height, vp.zmin, vp.zmax };
	*getViewport = vp;

	CTexture* pTargetTexture = displayContext->GetCurrentColorOutput();
	CTexture* pDepthTexture = displayContext->GetCurrentDepthOutput();
	pass.SetRenderTarget(0, pTargetTexture);
	pass.SetDepthTarget(pDepthTexture);
	pass.SetViewport(viewport);

	return true;
}

//////////////////////////////////////////////////////////////////////////
CDeviceGraphicsPSOPtr CRenderAuxGeomD3D::GetGraphicsPSO(const SAuxGeomRenderFlags& flags, const CCryNameTSCRC& techique, ERenderPrimitiveType topology, InputLayoutHandle format)
{
	CDeviceGraphicsPSODesc psoDesc;

	psoDesc.m_pResourceLayout = m_currentState.m_pResourceLayout_WithTexture;
	psoDesc.m_pShader = m_pAuxGeomShader;
	psoDesc.m_technique = techique;
	psoDesc.m_ShaderFlags_RT = 0;
	psoDesc.m_ShaderFlags_MD = 0;
	psoDesc.m_ShaderFlags_MDV = 0;
	psoDesc.m_PrimitiveType = topology;
	psoDesc.m_VertexFormat = format;
	psoDesc.m_RenderState = 0;
	psoDesc.m_StencilState = STENC_FUNC(FSS_STENCFUNC_ALWAYS) | STENCOP_FAIL(FSS_STENCOP_KEEP) | STENCOP_ZFAIL(FSS_STENCOP_KEEP) | STENCOP_PASS(FSS_STENCOP_KEEP);
	psoDesc.m_StencilReadMask = 0xFF;
	psoDesc.m_StencilWriteMask = 0xFF;
	psoDesc.m_CullMode = eCULL_None;
	psoDesc.m_bDepthClip = true; //??
	psoDesc.m_pRenderPass = m_geomPass.GetRenderPass();
	
	const bool bReverseDepth = true;
	uint32 gsFunc = bReverseDepth ? GS_DEPTHFUNC_GEQUAL : GS_DEPTHFUNC_LEQUAL;

	const bool bThickLine = CAuxGeomCB::IsThickLine(flags);

	if (flags.GetDepthTestFlag() == e_DepthTestOff) gsFunc |= GS_NODEPTHTEST;
	if (flags.GetDepthWriteFlag() == e_DepthWriteOn) gsFunc |= GS_DEPTHWRITE;
	if (flags.GetFillMode() == e_FillModeWireframe) gsFunc |= GS_WIREFRAME;

	switch (flags.GetAlphaBlendMode())
	{
	case e_AlphaBlended: gsFunc |= GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA; break;
	case e_AlphaAdditive: gsFunc |= GS_BLSRC_ONE | GS_BLDST_ONE;
	}

	if (bThickLine)
	{
		psoDesc.m_CullMode = eCULL_None;
	}
	else
	{
		switch (flags.GetCullMode())
		{
		case e_CullModeFront: psoDesc.m_CullMode = eCULL_Front; break;
		case e_CullModeNone:  psoDesc.m_CullMode = eCULL_None;  break;
		default:              psoDesc.m_CullMode = eCULL_Back;
		}
	}
	psoDesc.m_RenderState = gsFunc;

	return m_auxPsoCache.GetOrCreatePSO(psoDesc);
}

//////////////////////////////////////////////////////////////////////////
void CRenderAuxGeomD3D::DrawAuxPrimitives(const CAuxGeomCB::SAuxGeomCBRawData& rawData,CAuxGeomCB::AuxSortedPushBuffer::const_iterator itBegin, CAuxGeomCB::AuxSortedPushBuffer::const_iterator itEnd, const Matrix44& mViewProj, const SRenderViewport& vp, int texID)
{
	CryStackAllocWithSize(HLSL_AuxGeomObjectConstantBuffer, cbObj, CDeviceBufferManager::AlignBufferSizeForStreaming);
	CryStackAllocWithSize(HLSL_AuxGeomConstantBuffer, cbPrimObj, CDeviceBufferManager::AlignBufferSizeForStreaming);

	ERenderPrimitiveType topology = eptTriangleList;
	
	const SAuxGeomRenderFlags& flags = (*itBegin)->m_renderFlags;
	CAuxGeomCB::EPrimType primType(CAuxGeomCB::GetPrimType(flags));

	InputLayoutHandle vertexFormat = EDefaultInputLayouts::P3F_C4B_T2F;
	
	bool bIndices = false;
	bool bTextured = texID != -1;
	bool bDrawObjects = false;
	CAuxGeomCB::EAuxDrawObjType objType = CAuxGeomCB::EAuxDrawObjType::eDOT_Sphere;

	switch (primType)
	{
	case CAuxGeomCB::e_LineList: topology = eptLineList; break;
	case CAuxGeomCB::e_LineStrip: topology = eptLineStrip; break;
	case CAuxGeomCB::e_TriList:  topology = eptTriangleList; break;
	case CAuxGeomCB::e_PtList:   topology = eptPointList; break;
	case CAuxGeomCB::e_LineListInd: topology = eptLineList; bIndices = true; break;
	case CAuxGeomCB::e_LineStripInd: topology = eptLineStrip; bIndices = true; break;
	case CAuxGeomCB::e_TriListInd:  topology = eptTriangleList; bIndices = true; break;
	case CAuxGeomCB::e_Obj:
		{
			bDrawObjects = true;
			objType = CAuxGeomCB::GetAuxObjType(flags);
			topology = eptTriangleList;
			vertexFormat = EDefaultInputLayouts::P3F_T3F;
			bIndices = true;
			texID = -1;
			bTextured = false;
		}
		break;
	default:
		CRY_ASSERT(false);
	}

	static CCryNameTSCRC tGeomText("AuxText");
	static CCryNameTSCRC tGeom("AuxGeometry");
	static CCryNameTSCRC tGeomTexture("AuxGeometryTexture");
	static CCryNameTSCRC tGeomThickLines("AuxGeometryThickLines");
	static CCryNameTSCRC tGeomObject("AuxGeometryObj");

	CCryNameTSCRC technique;
	if (bDrawObjects)
	{
		technique = tGeomObject;
	}
	else if (e_ModeText == flags.GetMode2D3DFlag())
	{
		technique = tGeomText;
	}
	else
	{
		technique	= CAuxGeomCB::IsThickLine(flags) ? tGeomThickLines : (texID != -1 ? tGeomTexture : tGeom);
	}

	CDeviceGraphicsPSOPtr pso = GetGraphicsPSO(flags, technique, topology, vertexFormat);
	if (!pso->IsValid())
		return;

	CDeviceResourceSetPtr pTextureResources = m_auxTextureCache.GetOrCreateResourceSet(texID);
	if (!pTextureResources->IsValid())
		return;

	CConstantBuffer* pCB = nullptr;

	if (!bDrawObjects)
	{
		pCB = m_auxConstantBufferHeap.GetUsableConstantBuffer();
		{
			cbPrimObj->matViewProj = mViewProj;
			cbPrimObj->invScreenDim.x = 1.f / static_cast<float>(vp.width);
			cbPrimObj->invScreenDim.y = 1.f / static_cast<float>(vp.height);
			pCB->UpdateBuffer(cbPrimObj, cbPrimObjSize);
		}
	}

	for (auto it = itBegin; it != itEnd; ++it)
	{
		const CAuxGeomCB::SAuxPushBufferEntry& entry = *(*it);

		SCompiledRenderPrimitive* pPrimitive = m_auxRenderPrimitiveHeap.GetUsablePrimitive();

		pPrimitive->m_pResourceLayout = m_currentState.m_pResourceLayout_WithTexture;
		pPrimitive->m_pVertexInputSet = m_currentState.m_pVertexInputSet;
		pPrimitive->m_pIndexInputSet = (bIndices) ? m_currentState.m_pIndexInputSet : nullptr;
		pPrimitive->m_stencilRef = m_currentState.m_stencilRef;
		pPrimitive->m_pPipelineState = pso;
		pPrimitive->m_pResources = pTextureResources;
		pPrimitive->m_inlineConstantBuffers[0].shaderSlot = (EConstantBufferShaderSlot)0;
		pPrimitive->m_inlineConstantBuffers[0].shaderStages = EShaderStage_Vertex | ((technique == tGeomThickLines) ? EShaderStage_Geometry : EShaderStage_None);
		pPrimitive->m_inlineConstantBuffers[0].pBuffer = pCB;

		if (!bIndices)
		{
			pPrimitive->m_drawInfo.vertexBaseOffset = 0;
			pPrimitive->m_drawInfo.vertexOrIndexOffset = entry.m_vertexOffs;
			pPrimitive->m_drawInfo.vertexOrIndexCount = entry.m_numVertices;
		}
		else
		{
			pPrimitive->m_drawInfo.vertexBaseOffset = entry.m_vertexOffs;
			pPrimitive->m_drawInfo.vertexOrIndexOffset = entry.m_indexOffs;
			pPrimitive->m_drawInfo.vertexOrIndexCount = entry.m_numIndices;
		}

		if (bDrawObjects)
		{
			// Do a setup for an object
			// CRY_ASSERT than all objects in this batch are of same type
			CRY_ASSERT(CAuxGeomCB::GetAuxObjType(entry.m_renderFlags) == objType);
			CRY_ASSERT(rawData.m_auxDrawObjParamBuffer.size() > 0);

			if (rawData.m_auxDrawObjParamBuffer.size() == 0) return;

			uint32 drawParamOffs;

			if (!entry.GetDrawParamOffs(drawParamOffs))
			{
				CRY_ASSERT(false);
				continue;
			}

			// get draw params
			const CAuxGeomCB::SAuxDrawObjParams& drawParams(rawData.m_auxDrawObjParamBuffer[drawParamOffs]);

			// Prepare d3d world space matrix in draw param structure
			// Attention: in d3d terms matWorld is actually matWorld^T

			const Matrix44A& matView = GetCurrentView();
			Matrix44A        matWorld = drawParams.m_matWorld;

			// LOD calculation
			Vec3 objCenterWorld = matWorld.GetRow(3);
			Vec3 objOuterRightWorld = objCenterWorld + matView.GetTranslation() * drawParams.m_size;

			Vec4 v0 = Vec4(objCenterWorld, 1) * *m_matrices.m_pCurTransMat;
			Vec4 v1 = Vec4(objOuterRightWorld, 1) * *m_matrices.m_pCurTransMat;

			float scale;
			CRY_ASSERT(fabs(v0.w - v0.w) < 1e-4);
			if (fabs(v0.w) < 1e-2)
				scale = 0.5f;
			else
				scale = ((v1.x - v0.x) / v0.w) * std::max(static_cast<float>(vp.width), static_cast<float>(vp.height)) / 500.0f;

			// map scale to detail level
			uint32 lodLevel((uint32)((scale / 0.5f) * (e_auxObjNumLOD - 1)));
			if (lodLevel >= e_auxObjNumLOD)
			{
				lodLevel = e_auxObjNumLOD - 1;
			}

			// get appropriate mesh
			CRY_ASSERT(lodLevel >= 0 && lodLevel < e_auxObjNumLOD);

			SDrawObjMesh* pMesh = m_sphereObj;
			switch (objType)
			{
			case CAuxGeomCB::eDOT_Cylinder: pMesh = m_cylinderObj;  break;
			case CAuxGeomCB::eDOT_Cone:     pMesh = m_coneObj;      break;

			default:
			case CAuxGeomCB::eDOT_Sphere:   pMesh = m_sphereObj;
			}

			CRY_ASSERT(pMesh != nullptr);

			pMesh = &pMesh[lodLevel];

			//////////////////////////////////////////////////////////////////////////
			// Fill Constant Buffer
			pCB = m_auxConstantBufferHeap.GetUsableConstantBuffer();
			{
				Matrix44A matWorldViewProj;

				// calculate transformation matrices
				if (m_curDrawInFrontMode == e_DrawInFrontOn)
				{
					Matrix44A matScale(Matrix34::CreateScale(Vec3(0.999f, 0.999f, 0.999f)));

					Matrix44A& matWorldViewScaleProjT = matWorldViewProj;
					matWorldViewScaleProjT = matView * matScale;
					matWorldViewScaleProjT = matWorldViewScaleProjT * GetCurrentProj();

					matWorldViewScaleProjT = matWorldViewScaleProjT.GetTransposed();
					matWorldViewScaleProjT = matWorldViewScaleProjT * matWorld;
				}
				else
				{
					Matrix44A& matWorldViewProjT = matWorldViewProj;
					matWorldViewProjT = m_matrices.m_pCurTransMat->GetTransposed();
					matWorldViewProjT = matWorldViewProjT *matWorld;
				}

				// set color
				ColorF col(drawParams.m_color);

				// set light vector (rotate back into local space)
				Matrix33 matWorldInv(drawParams.m_matWorld.GetInverted());
				Vec3 lightLocalSpace(matWorldInv * Vec3(0.5773f, 0.5773f, 0.5773f));

				// normalize light vector (matWorld could contain non-uniform scaling)
				lightLocalSpace.Normalize();

				{
					cbObj->matViewProj = matWorldViewProj;
					cbObj->auxGeomObjColor = Vec4(col.b, col.g, col.r, col.a); // need to flip r/b as drawParams.m_color was originally argb
					cbObj->globalLightLocal = lightLocalSpace;  // normalize light vector (matWorld could contain non-uniform scaling)
					cbObj->auxGeomObjShading = Vec2(drawParams.m_shaded ? 0.4f : 0, drawParams.m_shaded ? 0.6f : 1); // set shading flag
					pCB->UpdateBuffer(cbObj, cbObjSize);
				}
			}

			pPrimitive->m_inlineConstantBuffers[0].pBuffer = pCB;
			pPrimitive->m_pVertexInputSet = pMesh->m_primitive.m_pVertexInputSet;
			pPrimitive->m_pIndexInputSet  = pMesh->m_primitive.m_pIndexInputSet;
			pPrimitive->m_stencilRef      = pMesh->m_primitive.m_stencilRef;
			pPrimitive->m_drawInfo        = pMesh->m_primitive.m_drawInfo;
		}

		m_geomPass.AddPrimitive(pPrimitive);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CRenderAuxGeomD3D::PrepareRendering(const CAuxGeomCB::SAuxGeomCBRawData *pAuxGeomData, SRenderViewport* viewportOut)
{
	// reset DrawInFront mode
	m_curDrawInFrontMode = e_DrawInFrontOff;

	// reset current prim type
	m_curPrimType = CAuxGeomCB::e_PrimTypeInvalid;

	if (!PreparePass(pAuxGeomData->m_camera, pAuxGeomData->displayContextKey, m_geomPass, viewportOut))
	{
		return false;
	}

	m_geomPass.BeginAddingPrimitives();

	m_currentState.m_stencilRef = 0;

	SStreamInfo vertexStreamInfo;
	vertexStreamInfo.hStream = m_bufman.GetVB();
	vertexStreamInfo.nStride = sizeof(SAuxVertex);
	vertexStreamInfo.nSlot = 0;
	m_currentState.m_pVertexInputSet = GetDeviceObjectFactory().CreateVertexStreamSet(1, &vertexStreamInfo);

	SStreamInfo indexStreamInfo;
	indexStreamInfo.hStream = m_bufman.GetIB();
	indexStreamInfo.nStride = indexbuffer_type<vtx_idx>::type;
	indexStreamInfo.nSlot = 0;
	m_currentState.m_pIndexInputSet = GetDeviceObjectFactory().CreateIndexStreamSet(&indexStreamInfo);

	{
		SDeviceResourceLayoutDesc resourceLayoutDesc;
		CDeviceResourceSetDesc auxResourcesSetDesc;
		auxResourcesSetDesc.SetTexture(0, nullptr, EDefaultResourceViews::Default, EShaderStage_Pixel);
		auxResourcesSetDesc.SetSampler(0, EDefaultSamplerStates::TrilinearClamp, EShaderStage_Pixel);
		resourceLayoutDesc.SetResourceSet(0, auxResourcesSetDesc);
		resourceLayoutDesc.SetConstantBuffer(1, (EConstantBufferShaderSlot)0, EShaderStage_AllWithoutCompute);
		m_currentState.m_pResourceLayout_WithTexture = GetDeviceObjectFactory().CreateResourceLayout(resourceLayoutDesc);
	}

	return true;
}

void CRenderAuxGeomD3D::FinishRendering()
{
	m_currentState = SAuxCurrentState();

	m_auxConstantBufferHeap.FreeUsedConstantBuffers();
	m_auxRenderPrimitiveHeap.FreeUsedPrimitives();
}

//////////////////////////////////////////////////////////////////////////
void CRenderAuxGeomD3D::ClearCaches()
{
	m_auxTextureCache.Clear();
	m_auxPsoCache.Clear();

	m_auxConstantBufferHeap.FreeUsedConstantBuffers();
	m_auxRenderPrimitiveHeap.FreeUsedPrimitives();
}

//////////////////////////////////////////////////////////////////////////
void CRenderAuxGeomD3D::Prepare(const SAuxGeomRenderFlags& renderFlags, Matrix44A& mat, const SDisplayContextKey& displayContextKey)
{
	// mode 2D/3D -- set new transformation matrix
	const Matrix44A* pNewTransMat(&GetCurrentTrans3D());
	const EAuxGeomPublicRenderflags_Mode2D3D eMode = renderFlags.GetMode2D3DFlag();
	
	if (e_Mode2D == eMode || e_ModeText == eMode)
	{
		pNewTransMat = &GetCurrentTrans2D();
	}
	else if (e_ModeUnit == eMode)
	{
		pNewTransMat = &GetCurrentTransUnit();
	}

	if (m_matrices.m_pCurTransMat != pNewTransMat)
	{
		m_matrices.m_pCurTransMat = pNewTransMat;
	}

	m_curPointSize = (CAuxGeomCB::e_PtList == CAuxGeomCB::GetPrimType(renderFlags)) ? CAuxGeomCB::GetPointSize(renderFlags) : 1;

	EAuxGeomPublicRenderflags_DrawInFrontMode newDrawInFrontMode(renderFlags.GetDrawInFrontMode());
	CAuxGeomCB::EPrimType newPrimType(CAuxGeomCB::GetPrimType(renderFlags));

	m_curDrawInFrontMode = (e_DrawInFrontOn == renderFlags.GetDrawInFrontMode() && e_Mode3D == eMode) ? e_DrawInFrontOn : e_DrawInFrontOff;

	if( CAuxGeomCB::e_Obj != newPrimType )
	{
		if( m_curDrawInFrontMode == e_DrawInFrontOn )
		{
			Matrix44A matScale(Matrix34::CreateScale(Vec3(0.999f, 0.999f, 0.999f)));

			mat = GetCurrentView();

			if( HasWorldMatrix() )
			{
				mat = Matrix44A(GetAuxWorldMatrix(m_curWorldMatrixIdx)).GetTransposed() * mat;
			}

			mat = mat * matScale;
			mat = mat * GetCurrentProj();
			mat = mat.GetTransposed();
		}
		else
		{
			mat = *m_matrices.m_pCurTransMat;

			if( HasWorldMatrix() )
			{
				mat = Matrix44(GetAuxWorldMatrix(m_curWorldMatrixIdx)).GetTransposed() *  mat;
			}

			//Matrix44A matProj;
			//mathMatrixOrthoOffCenter(&matProj, 0.0f, (float)viewport.width, (float)viewport.height, 0.0f, depth, -depth);

			//mat = mat * GetCurrentProj();
			mat = mat.GetTransposed();
		}
	}

	m_curPrimType = newPrimType;

}


void CRenderAuxGeomD3D::RT_Flush(const SAuxGeomCBRawDataPackagedConst& data)
{
	if (!CV_r_auxGeom)
		return;

	if (m_renderer.IsDeviceLost())
		return;

	PROFILE_LABEL_SCOPE("AuxGeom_Flush");

	// should only be called from render thread
	CRY_ASSERT(m_renderer.m_pRT->IsRenderThread());

	CRY_ASSERT(data.m_pData);
	if (!data.m_pData->IsUsed())
		return;

	if (!m_pAuxGeomShader)
	{
		// allow invalid file access for this shader because it shouldn't be used in the final build anyway
		SCOPED_ALLOW_FILE_ACCESS_FROM_THIS_THREAD();

		m_pAuxGeomShader = m_renderer.m_cEF.mfForName("AuxGeom", 0);
		CRY_ASSERT(m_pAuxGeomShader != nullptr);
	}

	Matrix44 mViewProj;

	m_pCurCBRawData = data.m_pData;	

	// get push buffer to process all submitted auxiliary geometries
	m_pCurCBRawData->GetSortedPushBuffer(0, m_pCurCBRawData->m_auxPushBuffer.size(), m_auxSortedPushBuffer);

	const CAuxGeomCB::AuxVertexBuffer& auxVertexBuffer(GetAuxVertexBuffer());
	const CAuxGeomCB::AuxIndexBuffer&  auxIndexBuffer (GetAuxIndexBuffer());

	m_bufman.FillVB(auxVertexBuffer.data(), auxVertexBuffer.size() * sizeof(SAuxVertex));
	m_bufman.FillIB(auxIndexBuffer.data(),  auxIndexBuffer.size()  * sizeof(vtx_idx));

	// prepare rendering
	SRenderViewport vp;
	if (PrepareRendering(data.m_pData, &vp))
	{
		CD3DStereoRenderer& stereoRenderer = gcpRendD3D->GetS3DRend();
		const bool bStereoEnabled = stereoRenderer.IsStereoEnabled();
		const bool bStereoSequentialMode = stereoRenderer.RequiresSequentialSubmission();

		// process push buffer
		for (CAuxGeomCB::AuxSortedPushBuffer::const_iterator it(m_auxSortedPushBuffer.begin()), itEnd(m_auxSortedPushBuffer.end()); it != itEnd; )
		{
			// mark current push buffer position
			CAuxGeomCB::AuxSortedPushBuffer::const_iterator itCur(it);

			// get current render flags
			const SAuxGeomRenderFlags& curRenderFlags((*it)->m_renderFlags);
			int curTexture = (*it)->m_textureID;
			const SDisplayContextKey& displayContextKey = (*it)->m_displayContextKey;
			m_curTransMatrixIdx = (*it)->m_transMatrixIdx;
			m_curWorldMatrixIdx = (*it)->m_worldMatrixIdx;

			// get prim type
			CAuxGeomCB::EPrimType primType(CAuxGeomCB::GetPrimType(curRenderFlags));

			// find all entries sharing the same render flags and texture
			while (true)
			{
				++it;
				if ((it == itEnd) ||
					((*it)->m_renderFlags != curRenderFlags) ||
					((*it)->m_transMatrixIdx != m_curTransMatrixIdx) ||
					((*it)->m_worldMatrixIdx != m_curWorldMatrixIdx) ||
					((*it)->m_textureID != curTexture) ||
					((*it)->m_displayContextKey != displayContextKey)
					)
				{
					break;
				}
			}

			// set appropriate rendering data
			Prepare(curRenderFlags, mViewProj, displayContextKey);

			if (!CAuxGeomCB::IsTextured(curRenderFlags))
			{
				curTexture = -1;
			}

			// draw push buffer entries
			switch (primType)
			{
			case CAuxGeomCB::e_PtList:
			case CAuxGeomCB::e_LineList:
			case CAuxGeomCB::e_TriList:
			case CAuxGeomCB::e_LineListInd:
			case CAuxGeomCB::e_LineStrip:
			case CAuxGeomCB::e_LineStripInd:
			case CAuxGeomCB::e_TriListInd:
			case CAuxGeomCB::e_Obj:
				DrawAuxPrimitives(*m_pCurCBRawData, itCur, it, mViewProj, vp, curTexture);
			break;
			default:
				assert(0);
			}
		}

		m_geomPass.Execute();

		FinishRendering();
	}

	m_pCurCBRawData = nullptr;
	m_curTransMatrixIdx = -1;
	m_curWorldMatrixIdx = -1;
}

void CRenderAuxGeomD3D::RT_Render(const CAuxGeomCBCollector::AUXJobs& auxGeoms)
{
	for (auto& pJob : auxGeoms)
	{
		RenderAuxGeom(pJob);
	}

	GetDeviceObjectFactory().GetCoreCommandList().Reset();
}

void CRenderAuxGeomD3D::RT_Reset(CAuxGeomCBCollector::AUXJobs& auxGeoms)
{
	for (auto& pJob : auxGeoms)
	{
		if (auto* processed = pJob->AccessData())
			processed->Reset();
	}
}

void CRenderAuxGeomD3D::RenderAuxGeom(const CAuxGeomCB* pAuxGeom)
{
	if (const auto* processed = pAuxGeom->AccessData())
	{
		if (processed->m_auxPushBuffer.size() > 0)
		{
			const auto processedRawDataPacked = SAuxGeomCBRawDataPackagedConst(processed);
			RT_Flush(processedRawDataPacked);
		}
	}
}

const Matrix44A& CRenderAuxGeomD3D::GetCurrentView() const
{
	return IsOrthoMode() ? gRenDev->m_IdentityMatrix : m_matrices.m_matView;
}

const Matrix44A& CRenderAuxGeomD3D::GetCurrentViewInv() const
{
	return IsOrthoMode() ? gRenDev->m_IdentityMatrix : m_matrices.m_matViewInv;
}

const Matrix44A& CRenderAuxGeomD3D::GetCurrentProj() const
{
	return IsOrthoMode() ? GetAuxOrthoMatrix(m_curTransMatrixIdx) : m_matrices.m_matProj;
}

const Matrix44A& CRenderAuxGeomD3D::GetCurrentTrans3D() const
{
	return IsOrthoMode() ? GetAuxOrthoMatrix(m_curTransMatrixIdx) : m_matrices.m_matTrans3D;
}

const Matrix44A& CRenderAuxGeomD3D::GetCurrentTrans2D() const
{
	return m_matrices.m_matTrans2D;
}

const Matrix44A& CRenderAuxGeomD3D::GetCurrentTransUnit() const
{
	static Matrix44A mU = Matrix44A(
		 2,  0, 0, 0,
		 0, -2, 0, 0,
		 0,  0, 0, 0,
		-1,  1, 0, 1);

	return mU;
}

bool CRenderAuxGeomD3D::IsOrthoMode() const
{
	return m_curTransMatrixIdx != -1;
}

bool CRenderAuxGeomD3D::HasWorldMatrix() const
{
	return m_curWorldMatrixIdx != -1;
}

void CRenderAuxGeomD3D::SMatrices::UpdateMatrices(const CCamera& camera)
{
	m_matProj.SetIdentity();
	m_matView.SetIdentity();

	const Vec2i resolution = { camera.GetViewSurfaceX(), camera.GetViewSurfaceZ() };
	const bool depthreversed = true;

	//float depth = depthreversed ? 1.0f : -1.0f;
	//float depth = depthreversed ? 1.0f : -1.0f;
	float depth = -1e10f;
	//mathMatrixOrthoOffCenter(&m_matProj, 0.0f, (float)resolution.x, (float)resolution.y,0.0f, depth, -depth);

	/*
	float xScale = 1.0f / (float)resolution.x;
	float yScale = 1.0f / (float)resolution.y;
	m_matTrans2D = Matrix44A(
		2*xScale, 0, 0, 0,
		0, -2*yScale, 0, 0,
		0, 0, 0, 0,
		-1, 1, 0, 1
	);
	*/

	if (depthreversed)
	{
		mathMatrixOrthoOffCenterLHReverseDepth(&m_matTrans2D, 0.0f, (float)resolution.x, (float)resolution.y, 0.0f, 0.0f, 1.0f);
	}
	else
	{
		mathMatrixOrthoOffCenterLH(&m_matTrans2D, 0.0f, (float)resolution.x, (float)resolution.y, 0.0f, 0.0f, 1.0f);
	}

	//m_matProj = ReverseDepthHelper::Convert(m_matProj);

	if (depthreversed)
	{
		float wl, wr, wb, wt;
		camera.GetAsymmetricFrustumParams(wl, wr, wb, wt);
		mathMatrixPerspectiveOffCenterReverseDepth((Matrix44A*)&m_matProj, wl, wr, wb, wt, camera.GetNearPlane(), camera.GetFarPlane());
	}
	else
	{
		m_matProj = camera.GetRenderProjectionMatrix();
	}

	m_matView = camera.GetRenderViewMatrix();

	m_matViewInv = m_matView.GetInverted();
	m_matTrans3D = m_matView * m_matProj;

	m_pCurTransMat = nullptr;
}

void CRenderAuxGeomD3D::FreeMemory()
{
	stl::free_container(m_auxSortedPushBuffer);
}

//////////////////////////////////////////////////////////////////////////
inline const CAuxGeomCB::AuxVertexBuffer& CRenderAuxGeomD3D::GetAuxVertexBuffer() const
{
	CRY_ASSERT(m_pCurCBRawData);
	return m_pCurCBRawData->m_auxVertexBuffer;
}

inline const CAuxGeomCB::AuxIndexBuffer& CRenderAuxGeomD3D::GetAuxIndexBuffer() const
{
	CRY_ASSERT(m_pCurCBRawData);
	return m_pCurCBRawData->m_auxIndexBuffer;
}

inline const CAuxGeomCB::AuxDrawObjParamBuffer& CRenderAuxGeomD3D::GetAuxDrawObjParamBuffer() const
{
	CRY_ASSERT(m_pCurCBRawData);
	return m_pCurCBRawData->m_auxDrawObjParamBuffer;
}

inline const Matrix44A& CRenderAuxGeomD3D::GetAuxOrthoMatrix(int idx) const
{
	CRY_ASSERT(m_pCurCBRawData && idx >= 0 && idx < (int)m_pCurCBRawData->m_auxOrthoMatrices.size());
	return m_pCurCBRawData->m_auxOrthoMatrices[idx];
}

inline const Matrix34A& CRenderAuxGeomD3D::GetAuxWorldMatrix(int idx) const
{
	CRY_ASSERT(m_pCurCBRawData && idx >= 0 && idx < (int)m_pCurCBRawData->m_auxWorldMatrices.size());
	return m_pCurCBRawData->m_auxWorldMatrices[idx];
}

//////////////////////////////////////////////////////////////////////////
SCompiledRenderPrimitive* CRenderAuxGeomD3D::CAuxPrimitiveHeap::GetUsablePrimitive()
{
	if (m_freeList.begin() == m_freeList.end())
		m_useList.emplace_front();
	else
		m_useList.splice_after(m_useList.before_begin(), m_freeList, m_freeList.before_begin());

	return &*m_useList.begin();
}

void CRenderAuxGeomD3D::CAuxPrimitiveHeap::FreeUsedPrimitives()
{
	for (auto& prim : m_useList)
	{
		prim.Reset();
	}
	m_freeList.splice_after(m_freeList.before_begin(), m_useList);
}

void CRenderAuxGeomD3D::CAuxPrimitiveHeap::Clear()
{
	m_useList.clear();
	m_freeList.clear();
}

//////////////////////////////////////////////////////////////////////////
CDeviceGraphicsPSOPtr CRenderAuxGeomD3D::CAuxPSOCache::GetOrCreatePSO(const CDeviceGraphicsPSODesc &psoDesc)
{
	auto it = m_cache.find(psoDesc);
	if (it != m_cache.end())
		return it->second;

	auto pPipelineState = GetDeviceObjectFactory().CreateGraphicsPSO(psoDesc);
	m_cache[psoDesc] = pPipelineState;
	return pPipelineState;
}

void CRenderAuxGeomD3D::CAuxPSOCache::Clear()
{
	m_cache.clear();
}

//////////////////////////////////////////////////////////////////////////
CRenderAuxGeomD3D::CAuxDeviceResourceSetCacheForTexture::~CAuxDeviceResourceSetCacheForTexture()
{
	Clear();
}

bool CRenderAuxGeomD3D::CAuxDeviceResourceSetCacheForTexture::OnTextureInvalidated(void* pListener, SResourceBindPoint bindPoint, UResourceReference resource, uint32 invalidationFlags)
{
	CRY_ASSERT(gRenDev->m_pRT->IsRenderThread());

	CAuxDeviceResourceSetCacheForTexture* pCache = static_cast<CAuxDeviceResourceSetCacheForTexture*>(pListener);
	pCache->m_cache.erase(resource.pTexture->GetID());

	return false; // remove callback now
}

CDeviceResourceSetPtr CRenderAuxGeomD3D::CAuxDeviceResourceSetCacheForTexture::GetOrCreateResourceSet(int textureId)
{
	if (textureId == -1 && m_pDefaultWhite)
		return m_pDefaultWhite;

	const auto& it = m_cache.find(textureId);
	if (it != m_cache.end())
		return (it->second).first;

	CTexture *pTexture = CTexture::GetByID(textureId);
	if (!pTexture || !CTexture::IsTextureExist(pTexture))
		pTexture = CRendererResources::s_ptexWhite;

	CDeviceResourceSetDesc auxResourcesSetDesc;
	auxResourcesSetDesc.SetTexture(0, pTexture, EDefaultResourceViews::Default, EShaderStage_Pixel);
	auxResourcesSetDesc.SetSampler(0, EDefaultSamplerStates::TrilinearClamp, EShaderStage_Pixel);

	CDeviceResourceSetPtr pResourceSet = GetDeviceObjectFactory().CreateResourceSet(CDeviceResourceSet::EFlags_ForceSetAllState);
	if (pResourceSet->Update(auxResourcesSetDesc))
	{
		m_cache[textureId] = std::make_pair(pResourceSet, _smart_ptr<CTexture>(pTexture));
		pTexture->AddInvalidateCallback(this, SResourceBindPoint(), OnTextureInvalidated);
		
		if (!m_pDefaultWhite && pTexture == CRendererResources::s_ptexWhite)
			m_pDefaultWhite = pResourceSet;

	}
	
	return pResourceSet;
}

void CRenderAuxGeomD3D::CAuxDeviceResourceSetCacheForTexture::Clear()
{
	for (auto& entry : m_cache)
		entry.second.second->RemoveInvalidateCallbacks(this);

	m_cache.clear();
}

//////////////////////////////////////////////////////////////////////////

void CAuxGeomCBCollector::GetMemoryUsage(ICrySizer* pSizer) const
{
	m_rwGlobal.RLock();
	for (auto const& it : m_auxThreadMap)
		it.second->GetMemoryUsage(pSizer);
	m_rwGlobal.RUnlock();
}

CAuxGeomCBCollector::AUXJobs CAuxGeomCBCollector::SubmitAuxGeomsAndPrepareForRendering()
{
	FUNCTION_PROFILER_RENDERER();

	CAuxGeomCBCollector::AUXJobs auxJobs;
	std::vector<SThread*>    tmpThreads;	

	m_rwGlobal.RLock();
	for (AUXThreadMap::const_iterator it = m_auxThreadMap.begin(); it != m_auxThreadMap.end(); ++it)
		tmpThreads.push_back(it->second);
	m_rwGlobal.RUnlock();

	for (auto const pTmpThread : tmpThreads)
	{
		pTmpThread->m_rwlLocal.RLock();
		for (CAuxGeomCBCollector::SThread::AUXJobMap::const_iterator job = pTmpThread->m_auxJobMap.begin(); job != pTmpThread->m_auxJobMap.end(); ++job)
			auxJobs.push_back(*job);
		pTmpThread->m_rwlLocal.RUnlock();
	}

	for (auto const pJob : auxJobs)
		pJob->Submit();

	tmpThreads.clear();

	return auxJobs;
}

void CAuxGeomCBCollector::SetDefaultCamera(const CCamera& camera)
{
	m_camera = camera;

	for (AUXThreadMap::iterator it = m_auxThreadMap.begin(); it != m_auxThreadMap.end(); ++it)
		it->second->SetDefaultCamera(camera);
}

void CAuxGeomCBCollector::SetDisplayContextKey(const SDisplayContextKey &displayContextKey)
{
	m_displayContextKey = displayContextKey;

	for (AUXThreadMap::iterator it = m_auxThreadMap.begin(); it != m_auxThreadMap.end(); ++it)
		it->second->SetDisplayContextKey(displayContextKey);
}

CCamera CAuxGeomCBCollector::GetCamera() const
{
	return m_camera;
}

const SDisplayContextKey& CAuxGeomCBCollector::GetDisplayContextKey() const
{
	return m_displayContextKey;
}

void CAuxGeomCBCollector::FreeMemory()
{
	m_rwGlobal.WLock();
	for (AUXThreadMap::const_iterator cbit = m_auxThreadMap.begin(); cbit != m_auxThreadMap.end(); ++cbit)
	{
		cbit->second->FreeMemory();
	}
	m_rwGlobal.WUnlock();
}

CAuxGeomCB* CAuxGeomCBCollector::Get(int jobID)
{
	threadID tid = CryGetCurrentThreadId();

	m_rwGlobal.RLock();

	AUXThreadMap::const_iterator it = m_auxThreadMap.find(tid);
	SThread* auxThread = m_auxThreadMap.end() != it ? it->second : nullptr;

	m_rwGlobal.RUnlock();

	if (auxThread == nullptr)
	{
		auxThread = new SThread;
		auxThread->SetDefaultCamera(m_camera);
		auxThread->SetDisplayContextKey(m_displayContextKey);

		m_rwGlobal.WLock();
		m_auxThreadMap.insert(AUXThreadMap::value_type(tid, auxThread));
		m_rwGlobal.WUnlock();
	}

	return auxThread->Get(jobID, tid);
}

void CAuxGeomCBCollector::Add(CAuxGeomCB* newAuxGeomCB)
{
	threadID tid = CryGetCurrentThreadId();

	m_rwGlobal.RLock();

	AUXThreadMap::const_iterator it = m_auxThreadMap.find(tid);
	SThread* auxThread = m_auxThreadMap.end() != it ? it->second : nullptr;

	m_rwGlobal.RUnlock();

	if (auxThread == nullptr)
	{
		auxThread = new SThread;
		auxThread->SetDefaultCamera(m_camera);
		auxThread->SetDisplayContextKey(m_displayContextKey);

		m_rwGlobal.WLock();
		m_auxThreadMap.insert(AUXThreadMap::value_type(tid, auxThread));
		m_rwGlobal.WUnlock();
	}

	m_rwGlobal.WLock();
	newAuxGeomCB->SetCurrentDisplayContext(m_displayContextKey);
	m_auxThreadMap[tid]->Add(newAuxGeomCB);
	m_rwGlobal.WUnlock();
}

CAuxGeomCBCollector::~CAuxGeomCBCollector()
{
	for (auto const& cbit : m_auxThreadMap)
	{
		delete cbit.second;
	}
	m_auxThreadMap.clear();
}

void CAuxGeomCBCollector::SThread::GetMemoryUsage(ICrySizer* pSizer) const
{
	m_rwlLocal.RLock();
	for (auto const& job : m_auxJobMap)
	{
		// MUST BE called after final CAuxGeomCB::Commit()
		// adding data (issuing render commands) is not thread safe !!!
		job->GetMemoryUsage(pSizer);
	}
	m_rwlLocal.RUnlock();
}

void CAuxGeomCBCollector::SThread::SetDefaultCamera(const CCamera & camera)
{
	m_camera = camera;

	for (auto& auxGeomCB : m_auxJobMap)
	{
		if(!auxGeomCB->IsUsingCustomCamera())
			auxGeomCB->SetCamera(camera);
	}
}

void CAuxGeomCBCollector::SThread::SetDisplayContextKey(const SDisplayContextKey& displayContextKey)
{
	this->displayContextKey = displayContextKey;

	for (auto& auxGeomCB : m_auxJobMap)
		auxGeomCB->SetCurrentDisplayContext(displayContextKey);
}


void CAuxGeomCBCollector::SThread::FreeMemory()
{
	m_rwlLocal.WLock();
	for (AUXJobMap::const_iterator job = m_auxJobMap.begin(); job != m_auxJobMap.end(); ++job)
	{
		// MUST BE called after final CAuxGeomCB::Commit()
		// adding data (issuing render commands) is not thread safe !!!
		(*job)->FreeMemory();
		gEnv->pRenderer->DeleteAuxGeom(*job);
	}
	m_auxJobMap.resize(0);
	m_rwlLocal.WUnlock();
}

CAuxGeomCBCollector::SThread::~SThread()
{
	for (auto const& cbit : m_auxJobMap)
	{
		delete cbit;
	}
}

CAuxGeomCB* CAuxGeomCBCollector::SThread::Get(int jobID, threadID tid)
{
	m_rwlLocal.RLock();

	CAuxGeomCB* pAuxGeomCB = jobID < m_auxJobMap.size() ? m_auxJobMap[jobID] : nullptr;

	m_rwlLocal.RUnlock();

	if (!pAuxGeomCB)
	{
		threadID mainThreadID, renderThreadID;
		gRenDev->GetThreadIDs(mainThreadID, renderThreadID);

		pAuxGeomCB = static_cast<CAuxGeomCB*>(gEnv->pRenderer->GetOrCreateIRenderAuxGeom());
		pAuxGeomCB->SetCamera(m_camera);
		pAuxGeomCB->SetUsingCustomCamera(false);
		pAuxGeomCB->SetCurrentDisplayContext(displayContextKey);

		m_rwlLocal.WLock();
		m_auxJobMap.push_back(pAuxGeomCB);
		m_rwlLocal.WUnlock();
	}

	return pAuxGeomCB;
}

void CAuxGeomCBCollector::SThread::Add(CAuxGeomCB* newAuxGeomCB)
{
	m_rwlLocal.WLock();
	m_auxJobMap.push_back(newAuxGeomCB);
	m_rwlLocal.WUnlock();
}

CAuxGeomCBCollector::SThread::SThread() : m_cbCurrent()
{

}

#endif // #if defined(ENABLE_RENDER_AUX_GEOM)