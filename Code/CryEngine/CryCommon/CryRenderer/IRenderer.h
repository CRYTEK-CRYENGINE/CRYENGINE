// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

//
//  File:Renderer.h - API Independent
//
//  History:
//  -Jan 31,2001:Originally created by Marco Corbetta
//  -: Taken over by Andrey Khonich
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include <CryMath/Cry_Geo.h>
#include <CryRenderer/IFlares.h> // <> required for Interfuscator
#include <CryThreading/IJobManager.h>

#include <CryExtension/ClassWeaver.h>
#include <CrySystem/IEngineModule.h>

// forward declarations
struct SRenderingPassInfo;
struct IFoliage;
struct SRenderLight;
struct SWaterRippleInfo;

class CRenderView;

//! Callback used for DXTCompress.
typedef void (* MIPDXTcallback)(const void* buffer, size_t count, void* userData);

typedef void (* GpuCallbackFunc)(DWORD context);

//! Callback for shadercache miss.
typedef void (* ShaderCacheMissCallback)(const char* acShaderRequest);

struct ICaptureFrameListener
{
	virtual ~ICaptureFrameListener(){}

	virtual bool OnNeedFrameData(unsigned char*& pConvertedTextureBuf) = 0;
	virtual void OnFrameCaptured(void) = 0;
	virtual int  OnGetFrameWidth(void) = 0;
	virtual int  OnGetFrameHeight(void) = 0;
	virtual int  OnCaptureFrameBegin(int* pTexHandle) = 0;

	enum ECaptureFrameFlags
	{
		eCFF_NoCaptureThisFrame = (0 << 1),
		eCFF_CaptureThisFrame   = (1 << 1),
	};
};

// Forward declarations.
//////////////////////////////////////////////////////////////////////
typedef void* WIN_HWND;
typedef void* WIN_HINSTANCE;
typedef void* WIN_HDC;
typedef void* WIN_HGLRC;

class CREMesh;
class CMesh;
//class   CImage;
struct  CStatObj;
class CVegetation;
struct  ShadowMapFrustum;
struct  IStatObj;
class CObjManager;
struct  SPrimitiveGroup;
struct  ICharacterInstance;
class CRenderElement;
class CRenderObject;
class CTexMan;
//class   ColorF;
class CShadowVolEdge;
class CCamera;
class CDLight;
struct SDeferredLightVolume;
struct  ILog;
struct  IConsole;
struct  ICVar;
struct  ITimer;
struct  ISystem;
class ICrySizer;
struct IRenderAuxGeom;
struct SREPointSpriteCreateParams;
struct SPointSpriteVertex;
struct RenderLMData;
struct SShaderParam;
struct SSkyLightRenderParams;
struct SParticleRenderInfo;
struct SParticleAddJobCompare;
struct IFlashPlayer_RenderProxy;
struct IColorGradingController;
struct IStereoRenderer;
struct IFFont_RenderProxy;
struct STextDrawContext;
struct IRenderMesh;
class IOpticsManager;
struct SRendererCloakParams;
struct ShadowFrustumMGPUCache;
struct IAsyncTextureCompileListener;
struct IClipVolume;
struct SClipVolumeBlendInfo;
class IImageFile;
class CVrProjectionManager;

//////////////////////////////////////////////////////////////////////
struct IScaleformPlayback;
struct GRendererCommandBufferReadOnly;
struct IFlashLoadMovieImage;

//////////////////////////////////////////////////////////////////////
namespace CryOVR
{
struct RenderInitParams;
}

//////////////////////////////////////////////////////////////////////
typedef unsigned char bvec4[4];
typedef float         vec4_t[4];
typedef unsigned char byte;
typedef float         vec2_t[2];

#include <CryMath/Cry_Color.h>
#include "Tarray.h"

#include <CryFont/IFont.h>

#define MAX_NUM_VIEWPORTS 7

//! Query types for CryInd editor (used in EF_Query() function).
enum ERenderQueryTypes
{
	EFQ_DeleteMemoryArrayPtr = 1,
	EFQ_DeleteMemoryPtr,
	EFQ_GetShaderCombinations,
	EFQ_SetShaderCombinations,
	EFQ_CloseShaderCombinations,

	EFQ_MainThreadList,
	EFQ_RenderThreadList,
	EFQ_RenderMultithreaded,

	EFQ_RecurseLevel,
	EFQ_IncrementFrameID,
	EFQ_DeviceLost,

	EFQ_Alloc_APITextures,
	EFQ_Alloc_APIMesh,

	//! Memory allocated by meshes in system memory.
	EFQ_Alloc_Mesh_SysMem,
	EFQ_Mesh_Count,

	EFQ_HDRModeEnabled,
	EFQ_ParticlesTessellation,
	EFQ_WaterTessellation,
	EFQ_MeshTessellation,
	EFQ_GetShadowPoolFrustumsNum,
	EFQ_GetShadowPoolAllocThisFrameNum,
	EFQ_GetShadowMaskChannelsNum,
	EFQ_GetTiledShadingSkippedLightsNum,

	//! Query will return all textures in the renderer,
	//! pass pointer to an SRendererQueryGetAllTexturesParam instance
	EFQ_GetAllTextures,

	//! Release resources allocated by GetAllTextures query.
	//! Pass pointer to an SRendererQueryGetAllTexturesParam instance, populated by EFQ_GetAllTextures
	EFQ_GetAllTexturesRelease,

	//! Query will return all IRenderMesh objects in the renderer.
	//! Pass an array pointer to be allocated and filled with the IRendermesh pointers. The calling function is responsible for freeing this memory.
	//! This was originally a two pass process, but proved to be non-thread-safe, leading to buffer overruns and underruns.
	EFQ_GetAllMeshes,

	//! Multigpu (crossfire/sli) is enabled.
	EFQ_MultiGPUEnabled,
	EFQ_SetDrawNearFov,
	EFQ_GetDrawNearFov,
	EFQ_TextureStreamingEnabled,
	EFQ_MSAAEnabled,
	EFQ_AAMode,

	EFQ_Fullscreen,
	EFQ_GetTexStreamingInfo,
	EFQ_GetMeshPoolInfo,

	//! True when shading is done in linear space, de-gamma on texture lookup, gamma on frame buffer writing (sRGB), false otherwise.
	EFQ_sLinearSpaceShadingEnabled,

	//! The percentages of overscan borders for left/right and top/bottom to adjust the title safe area.
	EFQ_OverscanBorders,

	//! Get num active post effects.
	EFQ_NumActivePostEffects,

	//! Get size of textures memory pool.
	EFQ_TexturesPoolSize,
	EFQ_RenderTargetPoolSize,

	EFQ_GetShaderCacheInfo,

	EFQ_GetFogCullDistance,
	EFQ_GetMaxRenderObjectsNum,

	EFQ_IsRenderLoadingThreadActive,

	EFQ_GetParticleVertexBufferSize,
	EFQ_GetParticleIndexBufferSize,
	EFQ_GetMaxParticleContainer,
	EFQ_GetSkinningDataPoolSize,

	EFQ_SetDynTexSourceLayerInfo,
	EFQ_SetDynTexSourceSharedRTDim,
	EFQ_GetViewportDownscaleFactor,
	EFQ_ReverseDepthEnabled,

	EFQ_GetLastD3DDebugMessage
};

// Interface to the graphics constant buffers
struct IGraphicsDeviceConstantBuffer : public CMultiThreadRefCount
{
	// Set contents of the device buffer
	virtual void SetData(const uint8* data, size_t size) = 0;
};
typedef _smart_ptr<IGraphicsDeviceConstantBuffer> IGraphicsDeviceConstantBufferPtr;

/*
   template <typename T>
   class CTypedGraphicsDeviceConstantBuffer
   {
   T m_hostBuffer;
   IGraphicsDeviceConstantBuffer m_constantBuffer;

   public:
   CTypedConstantBuffer() : m_constantBuffer(nullptr) {}
   CTypedConstantBuffer(const CTypedConstantBuffer<T> &cb) : m_constantBuffer(nullptr) { m_hostBuffer = cb.m_hostBuffer; }

   bool IsDeviceBufferAllocated() { return m_constantBuffer != nullptr; }
   IGraphicsDeviceConstantBuffer GetDeviceConstantBuffer()
   {
    if (!m_constantBuffer)
    {
      CreateDeviceBuffer();
    }
    return m_constantBuffer;
   }

   void CreateDeviceBuffer()
   {
    int size = sizeof(T);
    m_constantBuffer = gcpRendD3D->m_DevBufMan.CreateConstantBuffer(size);
    CopyToDevice();
   }
   void CopyToDevice()
   {
    m_constantBuffer->SetData(&m_hostBuffer, sizeof(m_hostBuffer));
   }

   T* operator->() { return &m_hostBuffer; }
   T& operator=(const T& hostData)
   {
    return m_hostBuffer = hostData;
   }
   };
 */

struct SWindGrid
{
	int       m_nWidth;
	int       m_nHeight;
	float     m_fCellSize;

	Vec3      m_vCentr;
	CryHalf2* m_pData;

	SWindGrid()
	{
		m_pData = NULL;
	}
};

struct ID3DDebugMessage
{
public:
	virtual void        Release() = 0;
	virtual const char* GetMsg() const = 0;

protected:
	ID3DDebugMessage() {}
	virtual ~ID3DDebugMessage() {}
};

enum EScreenAspectRatio
{
	eAspect_Unknown,
	eAspect_4_3,
	eAspect_16_9,
	eAspect_16_10,
};

enum EScreenShotMode
{
	eScreenShotMode_Normal,
	eScreenShotMode_AppCrash
};
namespace gpu_pfx2 {
class IManager;
}

class CRenderCamera
{
public:
	CRenderCamera();
	CRenderCamera(const CRenderCamera& Cam);
	void        Copy(const CRenderCamera& Cam);

	void        LookAt(const Vec3& Eye, const Vec3& ViewRefPt, const Vec3& ViewUp);
	void        Perspective(float Yfov, float Aspect, float Ndist, float Fdist);
	void        Frustum(float l, float r, float b, float t, float Ndist, float Fdist);
	const Vec3& wCOP() const;
	Vec3        ViewDir() const;
	Vec3        ViewDirOffAxis() const;

	float*      GetXform_Screen2Obj(float* M, int WW, int WH) const;
	float*      GetXform_Obj2Screen(float* M, int WW, int WH) const;

	float*      GetModelviewMatrix(float* M) const;
	float*      GetProjectionMatrix(float* M) const;
	float*      GetViewportMatrix(float* M, int WW, int WH) const;

	void        SetModelviewMatrix(const float* M);

	void        GetLookAtParams(Vec3* Eye, Vec3* ViewRefPt, Vec3* ViewUp) const;
	void        GetPerspectiveParams(float* Yfov, float* Xfov, float* Aspect, float* Ndist, float* Fdist) const;
	void        GetFrustumParams(float* l, float* r, float* b, float* t, float* Ndist, float* Fdist) const;

	float*      GetInvModelviewMatrix(float* M) const;
	float*      GetInvProjectionMatrix(float* M) const;
	float*      GetInvViewportMatrix(float* M, int WW, int WH) const;

	Vec3        WorldToCam(const Vec3& wP) const;
	float       WorldToCamZ(const Vec3& wP) const;
	Vec3        CamToWorld(const Vec3& cP) const;

	void        LoadIdentityXform();
	void        Xform(const float M[16]);

	void        Translate(const Vec3& trans);
	void        Rotate(const float M[9]);

	void        GetPixelRay(float sx, float sy, int ww, int wh, Vec3* Start, Vec3* Dir) const;

	void        CalcVerts(Vec3* V) const;
	void        CalcTileVerts(Vec3* V, f32 nPosX, f32 nPosY, f32 nGridSizeX, f32 nGridSizeY) const;
	void        CalcRegionVerts(Vec3* V, const Vec2& vMin, const Vec2& vMax) const;
	void        CalcTiledRegionVerts(Vec3* V, Vec2& vMin, Vec2& vMax, f32 nPosX, f32 nPosY, f32 nGridSizeX, f32 nGridSizeY) const;

	Vec3  vX, vY, vZ;
	Vec3  vOrigin;
	float fWL, fWR, fWB, fWT;
	float fNear, fFar;
};

inline float* Frustum16fv(float* M, float l, float r, float b, float t, float n, float f)
{
	M[0] = (2 * n) / (r - l);
	M[4] = 0;
	M[8] = (r + l) / (r - l);
	M[12] = 0;
	M[1] = 0;
	M[5] = (2 * n) / (t - b);
	M[9] = (t + b) / (t - b);
	M[13] = 0;
	M[2] = 0;
	M[6] = 0;
	M[10] = -(f + n) / (f - n);
	M[14] = (-2 * f * n) / (f - n);
	M[3] = 0;
	M[7] = 0;
	M[11] = -1;
	M[15] = 0;
	return M;
}

inline float* Viewing16fv(float* M, const Vec3 X, const Vec3 Y, const Vec3 Z, const Vec3 O)
{
	M[0] = X.x;
	M[4] = X.y;
	M[8] = X.z;
	M[12] = -X | O;
	M[1] = Y.x;
	M[5] = Y.y;
	M[9] = Y.z;
	M[13] = -Y | O;
	M[2] = Z.x;
	M[6] = Z.y;
	M[10] = Z.z;
	M[14] = -Z | O;
	M[3] = 0;
	M[7] = 0;
	M[11] = 0;
	M[15] = 1;
	return M;
}

inline CRenderCamera::CRenderCamera()
{
	vX.Set(1, 0, 0);
	vY.Set(0, 1, 0);
	vZ.Set(0, 0, 1);
	vOrigin.Set(0, 0, 0);
	fNear = 1.4142f;
	fFar = 10;
	fWL = -1;
	fWR = 1;
	fWT = 1;
	fWB = -1;
}

inline CRenderCamera::CRenderCamera(const CRenderCamera& Cam)
{
	Copy(Cam);
}

inline void CRenderCamera::Copy(const CRenderCamera& Cam)
{
	vX = Cam.vX;
	vY = Cam.vY;
	vZ = Cam.vZ;
	vOrigin = Cam.vOrigin;
	fNear = Cam.fNear;
	fFar = Cam.fFar;
	fWL = Cam.fWL;
	fWR = Cam.fWR;
	fWT = Cam.fWT;
	fWB = Cam.fWB;
}

inline void CRenderCamera::LookAt(const Vec3& Eye, const Vec3& ViewRefPt, const Vec3& ViewUp)
{
	vZ = Eye - ViewRefPt;
	vZ.NormalizeSafe();
	vX = ViewUp % vZ;
	vX.NormalizeSafe();
	vY = vZ % vX;
	vY.NormalizeSafe();
	vOrigin = Eye;
}

inline void CRenderCamera::Perspective(float Yfov, float Aspect, float Ndist, float Fdist)
{
	fNear = Ndist;
	fFar = Fdist;
	fWT = tanf(Yfov * 0.5f) * fNear;
	fWB = -fWT;
	fWR = fWT * Aspect;
	fWL = -fWR;
}

inline void CRenderCamera::Frustum(float l, float r, float b, float t, float Ndist, float Fdist)
{
	fNear = Ndist;
	fFar = Fdist;
	fWR = r;
	fWL = l;
	fWB = b;
	fWT = t;
}

inline void CRenderCamera::GetLookAtParams(Vec3* Eye, Vec3* ViewRefPt, Vec3* ViewUp) const
{
	*Eye = vOrigin;
	*ViewRefPt = vOrigin - vZ;
	*ViewUp = vY;
}

inline void CRenderCamera::GetPerspectiveParams(float* Yfov, float* Xfov, float* Aspect, float* Ndist, float* Fdist) const
{
	*Yfov = atanf(fWT / fNear) * 57.29578f * 2.0f;
	*Xfov = atanf(fWR / fNear) * 57.29578f * 2.0f;
	*Aspect = fWT / fWR;
	*Ndist = fNear;
	*Fdist = fFar;
}

inline void CRenderCamera::GetFrustumParams(float* l, float* r, float* b, float* t, float* Ndist, float* Fdist) const
{
	*l = fWL;
	*r = fWR;
	*b = fWB;
	*t = fWT;
	*Ndist = fNear;
	*Fdist = fFar;
}

inline const Vec3& CRenderCamera::wCOP() const
{
	return(vOrigin);
}

inline Vec3 CRenderCamera::ViewDir() const
{
	return(-vZ);
}

inline Vec3 CRenderCamera::ViewDirOffAxis() const
{
	float fX = (fWL + fWR) * 0.5f, fY = (fWT + fWB) * 0.5f;  // MIDPOINT ON VIEWPLANE WINDOW
	Vec3 ViewDir = vX * fX + vY * fY - vZ * fNear;
	ViewDir.Normalize();
	return ViewDir;
}

inline Vec3 CRenderCamera::WorldToCam(const Vec3& wP) const
{
	Vec3 sP(wP - vOrigin);
	Vec3 cP(vX | sP, vY | sP, vZ | sP);
	return cP;
}

inline float CRenderCamera::WorldToCamZ(const Vec3& wP) const
{
	Vec3 sP(wP - vOrigin);
	float zdist = vZ | sP;
	return zdist;
}

inline Vec3 CRenderCamera::CamToWorld(const Vec3& cP) const
{
	Vec3 wP(vX * cP.x + vY * cP.y + vZ * cP.z + vOrigin);
	return wP;
}

inline void CRenderCamera::LoadIdentityXform()
{
	vX.Set(1, 0, 0);
	vY.Set(0, 1, 0);
	vZ.Set(0, 0, 1);
	vOrigin.Set(0, 0, 0);
}

inline void CRenderCamera::Xform(const float M[16])
{
	vX.Set(vX.x * M[0] + vX.y * M[4] + vX.z * M[8],
	       vX.x * M[1] + vX.y * M[5] + vX.z * M[9],
	       vX.x * M[2] + vX.y * M[6] + vX.z * M[10]);
	vY.Set(vY.x * M[0] + vY.y * M[4] + vY.z * M[8],
	       vY.x * M[1] + vY.y * M[5] + vY.z * M[9],
	       vY.x * M[2] + vY.y * M[6] + vY.z * M[10]);
	vZ.Set(vZ.x * M[0] + vZ.y * M[4] + vZ.z * M[8],
	       vZ.x * M[1] + vZ.y * M[5] + vZ.z * M[9],
	       vZ.x * M[2] + vZ.y * M[6] + vZ.z * M[10]);
	vOrigin.Set(vOrigin.x * M[0] + vOrigin.y * M[4] + vOrigin.z * M[8] + M[12],
	            vOrigin.x * M[1] + vOrigin.y * M[5] + vOrigin.z * M[9] + M[13],
	            vOrigin.x * M[2] + vOrigin.y * M[6] + vOrigin.z * M[10] + M[14]);

	float Scale = vX.GetLength();
	vX /= Scale;
	vY /= Scale;
	vZ /= Scale;

	fWL *= Scale;
	fWR *= Scale;
	fWB *= Scale;
	fWT *= Scale;
	fNear *= Scale;
	fFar *= Scale;
};

inline void CRenderCamera::Translate(const Vec3& trans)
{
	vOrigin += trans;
}

inline void CRenderCamera::Rotate(const float M[9])
{
	vX.Set(vX.x * M[0] + vX.y * M[3] + vX.z * M[6],
	       vX.x * M[1] + vX.y * M[4] + vX.z * M[7],
	       vX.x * M[2] + vX.y * M[5] + vX.z * M[8]);
	vY.Set(vY.x * M[0] + vY.y * M[3] + vY.z * M[6],
	       vY.x * M[1] + vY.y * M[4] + vY.z * M[7],
	       vY.x * M[2] + vY.y * M[5] + vY.z * M[8]);
	vZ.Set(vZ.x * M[0] + vZ.y * M[3] + vZ.z * M[6],
	       vZ.x * M[1] + vZ.y * M[4] + vZ.z * M[7],
	       vZ.x * M[2] + vZ.y * M[5] + vZ.z * M[8]);
}

inline float* CRenderCamera::GetModelviewMatrix(float* M) const
{
	Viewing16fv(M, vX, vY, vZ, vOrigin);
	return M;
}

inline float* CRenderCamera::GetProjectionMatrix(float* M) const
{
	Frustum16fv(M, fWL, fWR, fWB, fWT, fNear, fFar);
	return(M);
}

inline void CRenderCamera::GetPixelRay(float sx, float sy, int ww, int wh, Vec3* Start, Vec3* Dir) const
{
	Vec3 wTL = vOrigin + (vX * fWL) + (vY * fWT) - (vZ * fNear); // FIND LOWER-LEFT
	Vec3 dX = (vX * (fWR - fWL)) / (float)ww;                    // WORLD WIDTH OF PIXEL
	Vec3 dY = (vY * (fWT - fWB)) / (float)wh;                    // WORLD HEIGHT OF PIXEL
	wTL += (dX * sx - dY * sy);                                  // INCR TO WORLD PIXEL
	wTL += (dX * 0.5f - dY * 0.5f);                              // INCR TO PIXEL CNTR
	*Start = vOrigin;
	*Dir = wTL - vOrigin;
}

inline void CRenderCamera::CalcVerts(Vec3* V)  const
{
	float NearZ = -fNear;
	V[0].Set(fWR, fWT, NearZ);
	V[1].Set(fWL, fWT, NearZ);
	V[2].Set(fWL, fWB, NearZ);
	V[3].Set(fWR, fWB, NearZ);

	float FarZ = -fFar, FN = fFar / fNear;
	float fwL = fWL * FN, fwR = fWR * FN, fwB = fWB * FN, fwT = fWT * FN;
	V[4].Set(fwR, fwT, FarZ);
	V[5].Set(fwL, fwT, FarZ);
	V[6].Set(fwL, fwB, FarZ);
	V[7].Set(fwR, fwB, FarZ);

	for (int i = 0; i < 8; i++)
		V[i] = CamToWorld(V[i]);
}

inline void CRenderCamera::CalcTileVerts(Vec3* V, f32 nPosX, f32 nPosY, f32 nGridSizeX, f32 nGridSizeY)  const
{
	float NearZ = -fNear;

	float TileWidth = abs(fWR - fWL) / nGridSizeX;
	float TileHeight = abs(fWT - fWB) / nGridSizeY;
	float TileL = fWL + TileWidth * nPosX;
	float TileR = fWL + TileWidth * (nPosX + 1);
	float TileB = fWB + TileHeight * nPosY;
	float TileT = fWB + TileHeight * (nPosY + 1);

	V[0].Set(TileR, TileT, NearZ);
	V[1].Set(TileL, TileT, NearZ);
	V[2].Set(TileL, TileB, NearZ);
	V[3].Set(TileR, TileB, NearZ);

	float FarZ = -fFar, FN = fFar / fNear;
	float fwL = fWL * FN, fwR = fWR * FN, fwB = fWB * FN, fwT = fWT * FN;

	float TileFarWidth = abs(fwR - fwL) / nGridSizeX;
	float TileFarHeight = abs(fwT - fwB) / nGridSizeY;
	float TileFarL = fwL + TileFarWidth * nPosX;
	float TileFarR = fwL + TileFarWidth * (nPosX + 1);
	float TileFarB = fwB + TileFarHeight * nPosY;
	float TileFarT = fwB + TileFarHeight * (nPosY + 1);

	V[4].Set(TileFarR, TileFarT, FarZ);
	V[5].Set(TileFarL, TileFarT, FarZ);
	V[6].Set(TileFarL, TileFarB, FarZ);
	V[7].Set(TileFarR, TileFarB, FarZ);

	for (int i = 0; i < 8; i++)
		V[i] = CamToWorld(V[i]);
}

inline void CRenderCamera::CalcTiledRegionVerts(Vec3* V, Vec2& vMin, Vec2& vMax, f32 nPosX, f32 nPosY, f32 nGridSizeX, f32 nGridSizeY) const
{
	float NearZ = -fNear;

	Vec2 vTileMin, vTileMax;

	vMin.x = max(vMin.x, nPosX / nGridSizeX);
	vMax.x = min(vMax.x, (nPosX + 1) / nGridSizeX);

	vMin.y = max(vMin.y, nPosY / nGridSizeY);
	vMax.y = min(vMax.y, (nPosY + 1) / nGridSizeY);

	vTileMin.x = abs(fWR - fWL) * vMin.x;
	vTileMin.y = abs(fWT - fWB) * vMin.y;
	vTileMax.x = abs(fWR - fWL) * vMax.x;
	vTileMax.y = abs(fWT - fWB) * vMax.y;

	float TileWidth = abs(fWR - fWL) / nGridSizeX;
	float TileHeight = abs(fWT - fWB) / nGridSizeY;

	float TileL = fWL + vTileMin.x;
	float TileR = fWL + vTileMax.x;
	float TileB = fWB + vTileMin.y;
	float TileT = fWB + vTileMax.y;

	V[0].Set(TileR, TileT, NearZ);
	V[1].Set(TileL, TileT, NearZ);
	V[2].Set(TileL, TileB, NearZ);
	V[3].Set(TileR, TileB, NearZ);

	float FarZ = -fFar, FN = fFar / fNear;
	float fwL = fWL * FN, fwR = fWR * FN, fwB = fWB * FN, fwT = fWT * FN;

	Vec2 vTileFarMin, vTileFarMax;

	vTileFarMin.x = abs(fwR - fwL) * vMin.x;
	vTileFarMin.y = abs(fwT - fwB) * vMin.y;
	vTileFarMax.x = abs(fwR - fwL) * vMax.x;
	vTileFarMax.y = abs(fwT - fwB) * vMax.y;

	float TileFarWidth = abs(fwR - fwL) / nGridSizeX;
	float TileFarHeight = abs(fwT - fwB) / nGridSizeY;

	float TileFarL = fwL + vTileFarMin.x;
	float TileFarR = fwL + vTileFarMax.x;
	float TileFarB = fwB + vTileFarMin.y;
	float TileFarT = fwB + vTileFarMax.y;

	V[4].Set(TileFarR, TileFarT, FarZ);
	V[5].Set(TileFarL, TileFarT, FarZ);
	V[6].Set(TileFarL, TileFarB, FarZ);
	V[7].Set(TileFarR, TileFarB, FarZ);

	for (int i = 0; i < 8; i++)
		V[i] = CamToWorld(V[i]);
}

inline void CRenderCamera::CalcRegionVerts(Vec3* V, const Vec2& vMin, const Vec2& vMax) const
{
	float NearZ = -fNear;

	Vec2 vTileMin, vTileMax;

	vTileMin.x = abs(fWR - fWL) * vMin.x;
	vTileMin.y = abs(fWT - fWB) * vMin.y;
	vTileMax.x = abs(fWR - fWL) * vMax.x;
	vTileMax.y = abs(fWT - fWB) * vMax.y;

	float TileL = fWL + vTileMin.x;
	float TileR = fWL + vTileMax.x;
	float TileB = fWB + vTileMin.y;
	float TileT = fWB + vTileMax.y;

	V[0].Set(TileR, TileT, NearZ);
	V[1].Set(TileL, TileT, NearZ);
	V[2].Set(TileL, TileB, NearZ);
	V[3].Set(TileR, TileB, NearZ);

	float FarZ = -fFar, FN = fFar / fNear;
	float fwL = fWL * FN, fwR = fWR * FN, fwB = fWB * FN, fwT = fWT * FN;

	Vec2 vTileFarMin, vTileFarMax;

	vTileFarMin.x = abs(fwR - fwL) * vMin.x;
	vTileFarMin.y = abs(fwT - fwB) * vMin.y;
	vTileFarMax.x = abs(fwR - fwL) * vMax.x;
	vTileFarMax.y = abs(fwT - fwB) * vMax.y;

	float TileFarL = fwL + vTileFarMin.x;
	float TileFarR = fwL + vTileFarMax.x;
	float TileFarB = fwB + vTileFarMin.y;
	float TileFarT = fwB + vTileFarMax.y;

	V[4].Set(TileFarR, TileFarT, FarZ);
	V[5].Set(TileFarL, TileFarT, FarZ);
	V[6].Set(TileFarL, TileFarB, FarZ);
	V[7].Set(TileFarR, TileFarB, FarZ);

	for (int i = 0; i < 8; i++)
		V[i] = CamToWorld(V[i]);
}

class SBoundingVolume
{
public:
	SBoundingVolume() : m_vCenter(0, 0, 0), m_fRadius(0) {}
	~SBoundingVolume() {}

	void        SetCenter(const Vec3& center) { m_vCenter = center; }
	void        SetRadius(float radius)       { m_fRadius = radius; }
	const Vec3& GetCenter() const             { return m_vCenter;   }
	float       GetRadius() const             { return m_fRadius;   }

protected:
	Vec3  m_vCenter;
	float m_fRadius;
};

class SMinMaxBox : public SBoundingVolume
{
public:
	SMinMaxBox()
	{
		Clear();
	}
	virtual ~SMinMaxBox() {}

	void AddPoint(const Vec3& pt)
	{
		if (pt.x > m_max.x)
			m_max.x = pt.x;
		if (pt.x < m_min.x)
			m_min.x = pt.x;

		if (pt.y > m_max.y)
			m_max.y = pt.y;
		if (pt.y < m_min.y)
			m_min.y = pt.y;

		if (pt.z > m_max.z)
			m_max.z = pt.z;
		if (pt.z < m_min.z)
			m_min.z = pt.z;

		// Updates the center and radius.
		UpdateSphere();
	}
	void AddPoint(float x, float y, float z)
	{
		AddPoint(Vec3(x, y, z));
	}

	void        Union(const SMinMaxBox& box) { AddPoint(box.GetMin()); AddPoint(box.GetMax()); }

	const Vec3& GetMin() const               { return m_min; }
	const Vec3& GetMax() const               { return m_max; }

	void        SetMin(const Vec3& min)      { m_min = min; UpdateSphere(); }
	void        SetMax(const Vec3& max)      { m_max = max; UpdateSphere(); }

	float       GetWidthInX() const          { return m_max.x - m_min.x; }
	float       GetWidthInY() const          { return m_max.y - m_min.y; }
	float       GetWidthInZ() const          { return m_max.z - m_min.z; }

	bool        PointInBBox(const Vec3& pt) const;

	bool        ViewFrustumCull(const CRenderCamera& cam, const Matrix44& mat);

	void        Transform(const Matrix34& mat)
	{
		Vec3 verts[8];
		CalcVerts(verts);
		Clear();
		for (int i = 0; i < 8; i++)
		{
			AddPoint(mat.TransformPoint(verts[i]));
		}
	}

	//! Reset the bounding box.
	void Clear()
	{
		m_min = Vec3(999999.0f, 999999.0f, 999999.0f);
		m_max = Vec3(-999999.0f, -999999.0f, -999999.0f);
	}

protected:
	void UpdateSphere()
	{
		m_vCenter = m_min;
		m_vCenter += m_max;
		m_vCenter *= 0.5f;

		Vec3 rad = m_max;
		rad -= m_vCenter;
		m_fRadius = rad.len();
	}
	void CalcVerts(Vec3 pVerts[8]) const
	{
		pVerts[0].Set(m_max.x, m_max.y, m_max.z);
		pVerts[4].Set(m_max.x, m_max.y, m_min.z);
		pVerts[1].Set(m_min.x, m_max.y, m_max.z);
		pVerts[5].Set(m_min.x, m_max.y, m_min.z);
		pVerts[2].Set(m_min.x, m_min.y, m_max.z);
		pVerts[6].Set(m_min.x, m_min.y, m_min.z);
		pVerts[3].Set(m_max.x, m_min.y, m_max.z);
		pVerts[7].Set(m_max.x, m_min.y, m_min.z);
	}

private:
	Vec3 m_min;   //!< Original object-space BV.
	Vec3 m_max;
};

//////////////////////////////////////////////////////////////////////
// All possible primitive types

enum PublicRenderPrimitiveType
{
	prtTriangleList,
	prtTriangleStrip,
	prtLineList,
	prtLineStrip
};

//////////////////////////////////////////////////////////////////////
#define R_CULL_DISABLE 0
#define R_CULL_NONE    0
#define R_CULL_FRONT   1
#define R_CULL_BACK    2

//////////////////////////////////////////////////////////////////////
#define R_DEFAULT_LODBIAS 0

//////////////////////////////////////////////////////////////////////
#define R_SOLID_MODE      0
#define R_WIREFRAME_MODE  1
#define R_POINT_MODE      2

//////////////////////////////////////////////////////////////////////
#define R_DX11_RENDERER   0
#define R_DX12_RENDERER   1
#define R_GL_RENDERER     2
#define R_VK_RENDERER     3
#define R_GNM_RENDERER    4

#define STR_DX11_RENDERER   "DX11"
#define STR_DX12_RENDERER   "DX12"
#define STR_GL_RENDERER     "GL"
#define STR_VK_RENDERER     "VK"
#define STR_GNM_RENDERER    "GNM"
#define STR_AUTO_RENDERER   "Auto"
//////////////////////////////////////////////////////////////////////
// Render features

#define RFT_FREE_0x1         0x1
#define RFT_ALLOW_RECTTEX    0x2
#define RFT_OCCLUSIONQUERY   0x4
#define RFT_FREE_0x8         0x8
#define RFT_HWGAMMA          0x10
#define RFT_FREE_0x20        0x20
#define RFT_COMPRESSTEXTURE  0x40
#define RFT_FREE_0x80        0x80
#define RFT_ALLOWANISOTROPIC 0x100       // Allows anisotropic texture filtering.
#define RFT_SUPPORTZBIAS     0x200
#define RFT_FREE_0x400       0x400
#define RFT_FREE_0x800       0x800
#define RFT_FREE_0x1000      0x1000
#define RFT_FREE_0x2000      0x2000
#define RFT_FREE_0x4000      0x4000
#define RFT_OCCLUSIONTEST    0x8000      // Support hardware occlusion test.

#define RFT_HW_INTEL         0x10000     // Unclassified intel hardware.
#define RFT_HW_ATI           0x20000     // Unclassified ATI hardware.
#define RFT_HW_NVIDIA        0x40000     // Unclassified NVidia hardware.
#define RFT_HW_MASK          0x70000     // Graphics chip mask.

#define RFT_HW_HDR           0x80000     // Hardware supports high dynamic range rendering.

#define RFT_HW_SM20          0x100000    // Shader model 2.0
#define RFT_HW_SM2X          0x200000    // Shader model 2.X
#define RFT_HW_SM30          0x400000    // Shader model 3.0
#define RFT_HW_SM40          0x800000    // Shader model 4.0
#define RFT_HW_SM50          0x1000000   // Shader model 5.0

#define RFT_FREE_0x2000000   0x2000000
#define RFT_FREE_0x4000000   0x4000000
#define RFT_FREE_0x8000000   0x8000000
#define RFT_FREE_0x10000000  0x10000000

#define RFT_RGBA             0x20000000  // RGBA order (otherwise BGRA).
#define RFT_FREE_0x40000000  0x40000000
#define RFT_FREE_0x80000000  0x80000000

//====================================================================
// PrecacheResources flags

#define FPR_NEEDLIGHT                    1
#define FPR_2D                           2
#define FPR_HIGHPRIORITY                 4
#define FPR_SYNCRONOUS                   8
#define FPR_STARTLOADING                 16
#define FPR_SINGLE_FRAME_PRIORITY_UPDATE 32

//=====================================================================
// SetRenderTarget flags
#define SRF_SCREENTARGET           1
#define SRF_USE_ORIG_DEPTHBUF      2
#define SRF_USE_ORIG_DEPTHBUF_MSAA 4

//====================================================================
// Draw shaders flags (EF_EndEf3d)
enum EShaderRenderingFlags
{
	SHDF_ALLOWHDR           = BIT(0),
	SHDF_CUBEMAPGEN         = BIT(1),
	SHDF_ZPASS              = BIT(2),
	SHDF_STEREO_LEFT_EYE    = BIT(3),
	SHDF_STEREO_RIGHT_EYE   = BIT(4),
	SHDF_ALLOWPOSTPROCESS   = BIT(5),
	SHDF_BILLBOARDS         = BIT(6),
	SHDF_ALLOW_AO           = BIT(8),
	SHDF_ALLOW_WATER        = BIT(9),
	SHDF_NOASYNC            = BIT(10),
	SHDF_NO_DRAWNEAR        = BIT(11),
	SHDF_STREAM_SYNC        = BIT(13),
	SHDF_NO_DRAWCAUSTICS    = BIT(14),
	SHDF_NO_SHADOWGEN       = BIT(15),
	SHDF_SECONDARY_VIEWPORT = BIT(16),
};

//////////////////////////////////////////////////////////////////////
// Virtual screen size
const float VIRTUAL_SCREEN_WIDTH  = 800.0f;
const float VIRTUAL_SCREEN_HEIGHT = 600.0f;

//////////////////////////////////////////////////////////////////////
// Object states
#define OS_ALPHA_BLEND         0x1
#define OS_ADD_BLEND           0x2
#define OS_MULTIPLY_BLEND      0x4
#define OS_TRANSPARENT         (OS_ALPHA_BLEND | OS_ADD_BLEND | OS_MULTIPLY_BLEND)
#define OS_NODEPTH_TEST        0x8
#define OS_NODEPTH_WRITE       0x10
#define OS_ANIM_BLEND          0x20
#define OS_ENVIRONMENT_CUBEMAP 0x40

// Render State flags (uint32)
#define GS_BLSRC_ZERO              0x01
#define GS_BLSRC_ONE               0x02
#define GS_BLSRC_DSTCOL            0x03
#define GS_BLSRC_ONEMINUSDSTCOL    0x04
#define GS_BLSRC_SRCALPHA          0x05
#define GS_BLSRC_ONEMINUSSRCALPHA  0x06
#define GS_BLSRC_DSTALPHA          0x07
#define GS_BLSRC_ONEMINUSDSTALPHA  0x08
#define GS_BLSRC_ALPHASATURATE     0x09
#define GS_BLSRC_SRCALPHA_A_ZERO   0x0A  // separate alpha blend state
#define GS_BLSRC_SRC1ALPHA         0x0B  // dual source blending
#define GS_BLSRC_MASK              0x0F
#define GS_BLSRC_SHIFT             0

#define GS_BLDST_ZERO              0x10
#define GS_BLDST_ONE               0x20
#define GS_BLDST_SRCCOL            0x30
#define GS_BLDST_ONEMINUSSRCCOL    0x40
#define GS_BLDST_SRCALPHA          0x50
#define GS_BLDST_ONEMINUSSRCALPHA  0x60
#define GS_BLDST_DSTALPHA          0x70
#define GS_BLDST_ONEMINUSDSTALPHA  0x80
#define GS_BLDST_ONE_A_ZERO        0x90 // separate alpha blend state
#define GS_BLDST_ONEMINUSSRC1ALPHA 0xA0 // dual source blending
#define GS_BLDST_MASK              0xF0
#define GS_BLDST_SHIFT             4

#define GS_DEPTHWRITE              0x00000100
#define GS_NODEPTHTEST             0x00000200
#define GS_STENCIL                 0x00000400
#define GS_ALPHATEST               0x00000800

#define GS_NOCOLMASK_R             0x00001000
#define GS_NOCOLMASK_G             0x00002000
#define GS_NOCOLMASK_B             0x00004000
#define GS_NOCOLMASK_A             0x00008000
#define GS_COLMASK_RGB             (GS_NOCOLMASK_A)
#define GS_COLMASK_A               (GS_NOCOLMASK_R | GS_NOCOLMASK_G | GS_NOCOLMASK_B)
#define GS_COLMASK_NONE            (GS_NOCOLMASK_R | GS_NOCOLMASK_G | GS_NOCOLMASK_B | GS_NOCOLMASK_A)
#define GS_COLMASK_MASK            GS_COLMASK_NONE
#define GS_COLMASK_SHIFT           12

#define GS_WIREFRAME               0x00010000
#define GS_POINTRENDERING          0x00020000

#define GS_DEPTHFUNC_LEQUAL        0x00000000
#define GS_DEPTHFUNC_EQUAL         0x00040000
#define GS_DEPTHFUNC_GREAT         0x00080000
#define GS_DEPTHFUNC_LESS          0x000C0000
#define GS_DEPTHFUNC_GEQUAL        0x00100000
#define GS_DEPTHFUNC_NOTEQUAL      0x00140000
#define GS_DEPTHFUNC_MASK          0x001C0000 // enum
#define GS_DEPTHFUNC_SHIFT         18

#define GS_BLEND_OP_ADD            0x00000000
#define GS_BLEND_OP_MAX            0x00200000
#define GS_BLEND_OP_MIN            0x00400000
#define GS_BLEND_OP_SUB            0x00600000
#define GS_BLEND_OP_SUBREV         0x00800000
#define GS_BLEND_OP_MASK           0x00E00000 // enum
#define GS_BLEND_OP_SHIFT          21

// Separate alpha blend mode
#define GS_BLALPHA_NONSEP 0x00000000          // same alpha blend op as color blend op (non-separate)
#define GS_BLALPHA_MAX    0x02000000
#define GS_BLALPHA_MIN    0x04000000
#define GS_BLALPHA_MASK   0x0E000000          // enum
#define GS_BLALPHA_SHIFT  25

#define GS_BLEND_MASK     (GS_BLSRC_MASK | GS_BLDST_MASK | GS_BLEND_OP_MASK | GS_BLALPHA_MASK)

#define FORMAT_8_BIT      8
#define FORMAT_24_BIT     24
#define FORMAT_32_BIT     32

//! Read FrameBuffer type.
enum ERB_Type
{
	eRB_BackBuffer,
	eRB_FrontBuffer,
	eRB_ShadowBuffer
};

enum EVertexCostTypes
{
	EVCT_STATIC = 0,
	EVCT_VEGETATION,
	EVCT_SKINNED,
	EVCT_NUM
};

//////////////////////////////////////////////////////////////////////

struct SDispFormat
{
	int m_Width;
	int m_Height;
	int m_BPP;
};

struct SAAFormat
{
	char szDescr[64];
	int  nSamples;
	int  nQuality;
};

//! Info about Terrain sector texturing.
struct SSectorTextureSet
{
	SSectorTextureSet()
	{
		ZeroStruct(*this);
		fTexScale = 1.f;
		nSlot0 = nSlot1 = nSlot2 = -1;
	}

	unsigned short nTex0, nTex1, nTex2, nSlot0, nSlot1, nSlot2;
	float          fTexOffsetX, fTexOffsetY, fTexScale;
};

struct IRenderNode;
struct SShaderItem;
struct IParticleVertexCreator;
struct IParticleComponentInstance;
namespace gpu_pfx2
{
	class IParticleComponentRuntime;
}

struct CRY_ALIGN(16) SAddParticlesToSceneJob
{
	void GetMemoryUsage(ICrySizer* pSizer) const {}

	SShaderItem* pShaderItem;
	CRenderObject* pRenderObject;
	IParticleVertexCreator* pVertexCreator = nullptr;
	gpu_pfx2::IParticleComponentRuntime* pGpuRuntime = nullptr;
	int16 nCustomTexId;
};

#ifdef SUPPORT_HW_MOUSE_CURSOR
class IHWMouseCursor
{
public:
	virtual ~IHWMouseCursor() {}
	virtual void SetPosition(int x, int y) = 0;
	virtual void Show() = 0;
	virtual void Hide() = 0;
};
#endif

//////////////////////////////////////////////////////////////////////
#include <CryRenderer/IShader.h> // <> required for Interfuscator
#include <CryRenderer/IRenderMesh.h>
#include "IMeshBaking.h"

//! Flags passed in function FreeResources.
#define FRR_DELETED_MESHES           BIT(0)
#define FRR_FLUSH_TEXTURESTREAMING   BIT(1)
#define FRR_RP_BUFFERS               BIT(2)
#define FRR_POST_EFFECTS             BIT(3)
#define FRR_OBJECTS                  BIT(4)
#define FRR_PERMANENT_RENDER_OBJECTS BIT(5)
#define FRR_SYSTEM                   BIT(6) // requires also FRR_OBJECTS
#define FRR_SYSTEM_RESOURCES         BIT(7) // requires also FRR_DELETED_MESHES |  FRR_FLUSH_TEXTURESTREAMING | FRR_RP_BUFFERS | FRR_POST_EFFECTS | FRR_OBJECTS | FRR_PERMANENT_RENDER_OBJECTS | FRR_SVOGI
#define FRR_TEXTURES                 BIT(8) // requires also FRR_DELETED_MESHES |  FRR_FLUSH_TEXTURESTREAMING | FRR_RP_BUFFERS | FRR_POST_EFFECTS | FRR_OBJECTS | FRR_PERMANENT_RENDER_OBJECTS | FRR_SYSTEM_RESOURCES | FRR_SVOGI
#define FRR_SVOGI                    BIT(9)
#define FRR_ALL                      -1

// Free resource flag combinations for commonly used operations
#define FRR_LEVEL_UNLOAD_SANDBOX  (FRR_DELETED_MESHES | FRR_FLUSH_TEXTURESTREAMING | FRR_OBJECTS | FRR_RP_BUFFERS | FRR_POST_EFFECTS | FRR_PERMANENT_RENDER_OBJECTS | FRR_SVOGI)
#define FRR_LEVEL_UNLOAD_LAUNCHER (FRR_DELETED_MESHES | FRR_FLUSH_TEXTURESTREAMING | FRR_OBJECTS | FRR_RP_BUFFERS | FRR_POST_EFFECTS | FRR_PERMANENT_RENDER_OBJECTS | FRR_SYSTEM_RESOURCES | FRR_SVOGI)
#define FRR_SHUTDOWN              (FRR_ALL)

// Refresh render resources flags.
// Flags passed in function RefreshResources.
#define FRO_SHADERS        1
#define FRO_SHADERTEXTURES 2
#define FRO_TEXTURES       4
#define FRO_GEOMETRY       8
#define FRO_FORCERELOAD    0x10

//=============================================================================
// Shaders render target stuff.

#define FRT_CLEAR_DEPTH                 BIT(0)  // equals CLEAR_ZBUFFER and D3D11_CLEAR_DEPTH
#define FRT_CLEAR_STENCIL               BIT(1)  // equals CLEAR_STENCIL and D3D11_CLEAR_STENCIL
#define FRT_CLEAR_COLOR                 BIT(2)  // equals CLEAR_RTARGET
#define FRT_CLEAR_FOGCOLOR              BIT(3)
#define FRT_CLEAR_IMMEDIATE             BIT(4)
#define FRT_CLEAR_COLORMASK             BIT(5)

#define FRT_CAMERA_REFLECTED_WATERPLANE BIT(6)
#define FRT_CAMERA_REFLECTED_PLANE      BIT(7)
#define FRT_CAMERA_CURRENT              BIT(8)

#define FRT_USE_FRONTCLIPPLANE          BIT(9)
#define FRT_USE_BACKCLIPPLANE           BIT(10)

#define FRT_GENERATE_MIPS               BIT(11)

#define FRT_RENDTYPE_CUROBJECT          BIT(12)
#define FRT_RENDTYPE_CURSCENE           BIT(13)
#define FRT_RENDTYPE_RECURSIVECURSCENE  BIT(14)
#define FRT_RENDTYPE_COPYSCENE          BIT(15)

#define FRT_CLEAR                       (FRT_CLEAR_COLOR | FRT_CLEAR_DEPTH | FRT_CLEAR_STENCIL)

//! Flags used in DrawText function.
enum EDrawTextFlags
{
	eDrawText_Default,
	eDrawText_Center         = BIT(0),  //!< Centered alignment, otherwise right or left.
	eDrawText_Right          = BIT(1),  //!< Right alignment, otherwise center or left.
	eDrawText_CenterV        = BIT(2),  //!< Center vertically, oterhwise top.
	eDrawText_Bottom         = BIT(3),  //!< Bottom alignment.

	eDrawText_2D             = BIT(4),  //!< 3-component vector is used for xy screen position, otherwise it's 3d world space position.

	eDrawText_FixedSize      = BIT(5),  //!< Font size is defined in the actual pixel resolution, otherwise it's in the virtual 800x600.
	eDrawText_800x600        = BIT(6),  //!< Position are specified in the virtual 800x600 resolution, otherwise coordinates are in pixels.

	eDrawText_Monospace      = BIT(7),  //!< Non proportional font rendering (Font width is same for all characters).

	eDrawText_Framed         = BIT(8),  //!< Draw a transparent, rectangular frame behind the text to ease readability independent from the background.

	eDrawText_DepthTest      = BIT(9),  //!< Text should be occluded by world geometry using the depth buffer.
	eDrawText_IgnoreOverscan = BIT(10), //!< Ignore the overscan borders, text should be drawn at the location specified.
	eDrawText_LegacyBehavior = BIT(11)  //!< Reserved for internal system use.
};

// Debug stats/views for Partial resolves
// if REFRACTION_PARTIAL_RESOLVE_DEBUG_VIEWS is enabled, make sure REFRACTION_PARTIAL_RESOLVE_STATS is too
#if defined(PERFORMANCE_BUILD)
	#define REFRACTION_PARTIAL_RESOLVE_STATS       1
	#define REFRACTION_PARTIAL_RESOLVE_DEBUG_VIEWS 0
#elif defined(_RELEASE) // note: _RELEASE is defined in PERFORMANCE_BUILD, so this check must come second
	#define REFRACTION_PARTIAL_RESOLVE_STATS       0
	#define REFRACTION_PARTIAL_RESOLVE_DEBUG_VIEWS 0
#else
	#define REFRACTION_PARTIAL_RESOLVE_STATS       1
	#define REFRACTION_PARTIAL_RESOLVE_DEBUG_VIEWS 1
#endif

#if REFRACTION_PARTIAL_RESOLVE_DEBUG_VIEWS
enum ERefractionPartialResolvesDebugViews
{
	eRPR_DEBUG_VIEW_2D_AREA = 1,
	eRPR_DEBUG_VIEW_3D_BOUNDS,
	eRPR_DEBUG_VIEW_2D_AREA_OVERLAY
};
#endif

//! This structure used in DrawText method of renderer.
//! It provide all necessary information of how to render text on screen.
struct SDrawTextInfo
{
	//! One of EDrawTextFlags flags.
	int flags;

	//! Text color, (r,g,b,a) all members must be specified.
	float color[4];
	Vec2  scale;
	IFFont* pFont;

	SDrawTextInfo()
	{
		flags = 0;
		color[0] = color[1] = color[2] = color[3] = 1;
		scale = ZERO;
		pFont = nullptr;
	}
};

#define UIDRAW_TEXTSIZEFACTOR (12.0f)

#if CRY_PLATFORM_DURANGO || CRY_PLATFORM_ORBIS
	#define MAX_GPU_NUM 1
#else
//! SLI/CROSSFIRE GPU maximum count.
	#define MAX_GPU_NUM 4
#endif

#define MAX_FRAME_ID_STEP_PER_FRAME 20
const int MAX_GSM_LODS_NUM = 16;

const f32 DRAW_NEAREST_MIN = 0.03f;
const f32 DRAW_NEAREST_MAX = 40.0f;

//===================================================================

//////////////////////////////////////////////////////////////////////
struct IRendererEventListener
{
	virtual void OnPostCreateDevice() = 0;
	virtual void OnPostResetDevice() = 0;
	virtual ~IRendererEventListener(){}
};

//////////////////////////////////////////////////////////////////////
struct ILoadtimeCallback
{
	virtual void LoadtimeUpdate(float fDeltaTime) = 0;
	virtual void LoadtimeRender() = 0;
	virtual ~ILoadtimeCallback(){}
};

//////////////////////////////////////////////////////////////////////
struct ISyncMainWithRenderListener
{
	virtual void SyncMainWithRender() = 0;
	virtual ~ISyncMainWithRenderListener(){}
};

//////////////////////////////////////////////////////////////////////
enum class ERenderType : uint8
{
	Undefined,
	Direct3D11,
	Direct3D12,
	OpenGL,
	Vulkan,
	GNM
};

//////////////////////////////////////////////////////////////////////
//! Enum for types of deferred lights.
enum eDeferredLightType
{
	eDLT_DeferredLight          = 0,

	eDLT_NumShadowCastingLights = eDLT_DeferredLight + 1,

	//! These lights cannot cast shadows.
	eDLT_DeferredCubemap = eDLT_NumShadowCastingLights,
	eDLT_DeferredAmbientLight,
	eDLT_DynamicLight,                                    // Not deferred light
	eDLT_NumLightTypes,
};

//! Scale factor between photometric and internal light units.
const float RENDERER_LIGHT_UNIT_SCALE = 10000.0f;

//////////////////////////////////////////////////////////////////////
struct SCustomRenderInitArgs
{
	bool appStartedFromMediaCenter;
};

#if CRY_PLATFORM_ANDROID
enum {CULL_SIZEX = 128};
enum {CULL_SIZEY = 64};
#else
enum {CULL_SIZEX = 256};
enum {CULL_SIZEY = 128};
#endif

class ITextureStreamListener
{
public:
	virtual void OnCreatedStreamedTexture(void* pHandle, const char* name, int nMips, int nMinMipAvailable) = 0;
	virtual void OnUploadedStreamedTexture(void* pHandle) = 0;
	virtual void OnDestroyedStreamedTexture(void* pHandle) = 0;
	virtual void OnTextureWantsMip(void* pHandle, int nMinMip) = 0;
	virtual void OnTextureHasMip(void* pHandle, int nMinMip) = 0;
	virtual void OnBegunUsingTextures(void** pHandles, size_t numHandles) = 0;
	virtual void OnEndedUsingTextures(void** pHandle, size_t numHandles) = 0;

protected:
	virtual ~ITextureStreamListener() {}
};

#if defined(CRY_ENABLE_RC_HELPER)
#include <CryCore/ToolsHelpers/ResourceCompilerHelper.h>
//! Listener for asynchronous texture compilation.
//! Connects the listener to the task-queue of pending compilation requests.
enum ERcExitCode;
struct IAsyncTextureCompileListener
{
public:
	virtual void OnCompilationStarted(const char* source, const char* target, int nPending) = 0;
	virtual void OnCompilationFinished(const char* source, const char* target, ERcExitCode eReturnCode) = 0;

	virtual void OnCompilationQueueTriggered(int nPending) = 0;
	virtual void OnCompilationQueueDepleted() = 0;

protected:
	virtual ~IAsyncTextureCompileListener() {}
};
#endif

enum ERenderPipelineProfilerStats
{
	eRPPSTATS_OverallFrame = 0,
	eRPPSTATS_Recursion,

	// Scene
	eRPPSTATS_SceneOverall,
	eRPPSTATS_SceneDecals,
	eRPPSTATS_SceneForward,
	eRPPSTATS_SceneWater,

	// Shadows
	eRPPSTATS_ShadowsOverall,
	eRPPSTATS_ShadowsSun,
	eRPPSTATS_ShadowsSunCustom,
	eRPPSTATS_ShadowsLocal,

	// Lighting
	eRPPSTATS_LightingOverall,
	eRPPSTATS_LightingGI,

	// VFX
	eRPPSTATS_VfxOverall,
	eRPPSTATS_VfxTransparent,
	eRPPSTATS_VfxFog,
	eRPPSTATS_VfxFlares,

	// Individual Total Illumination stats
	eRPPSTATS_TI_INJECT_CLEAR,
	eRPPSTATS_TI_VOXELIZE,
	eRPPSTATS_TI_INJECT_AIR,
	eRPPSTATS_TI_INJECT_LIGHT,
	eRPPSTATS_TI_INJECT_REFL0,
	eRPPSTATS_TI_INJECT_REFL1,
	eRPPSTATS_TI_INJECT_DYNL,
	eRPPSTATS_TI_NID_DIFF,
	eRPPSTATS_TI_GEN_DIFF,
	eRPPSTATS_TI_GEN_SPEC,
	eRPPSTATS_TI_GEN_AIR,
	eRPPSTATS_TI_DEMOSAIC_DIFF,
	eRPPSTATS_TI_DEMOSAIC_SPEC,
	eRPPSTATS_TI_UPSCALE_DIFF,
	eRPPSTATS_TI_UPSCALE_SPEC,

	RPPSTATS_NUM
};

struct RPProfilerStats
{
	float  gpuTime;
	float  gpuTimeSmoothed;
	float  gpuTimeMax;
	float  cpuTime;
	uint32 numDIPs;
	uint32 numPolys;

	// Internal
	float _gpuTimeMaxNew;
};

struct ISvoRenderer
{
	virtual bool IsShaderItemUsedForVoxelization(SShaderItem& rShaderItem, IRenderNode* pRN) { return false; }
	virtual void SetEditingHelper(const Sphere& sp)                                          {}
	virtual void Release()                                                                   {}
};

struct SRenderPolygonDescription
{
	SShaderItem            shaderItem;
	int                    numVertices = 0;
	const SVF_P3F_C4B_T2F* pVertices = nullptr;
	const SPipTangents*    pTangents = nullptr;
	uint16*                pIndices = nullptr;
	int                    numIndices = 0;
	int                    afterWater = 0;
	int                    renderListId = 0; // ERenderListID
	CRenderObject*         pRenderObject = nullptr;

	SRenderPolygonDescription() {}
	SRenderPolygonDescription(CRenderObject* pRendObj, SShaderItem& si, int numPts, const SVF_P3F_C4B_T2F* verts, const SPipTangents* tangs, uint16* inds, int ninds, ERenderListID _renderListId, int nAW)
		: numVertices(numPts)
		, pVertices(verts)
		, pTangents(tangs)
		, pIndices(inds)
		, numIndices(ninds)
		, renderListId(_renderListId)
		, afterWater(nAW)
		, pRenderObject(pRendObj)
		, shaderItem(si)
	{}
};

// Interface to the render view.
struct IRenderView : public CMultiThreadRefCount
{
	enum EViewType
	{
		eViewType_Default,
		eViewType_Recursive,
		eViewType_Shadow,
		eViewType_BillboardGen,
		eViewType_Count,
	};


	// View can be in either reading or writing modes.
	enum EUsageMode
	{
		eUsageModeUndefined,
		eUsageModeReading,
		eUsageModeReadingDone,
		eUsageModeWriting,
		eUsageModeWritingDone,
	};
	virtual void   SetFrameId(uint64 frameId) = 0;
	virtual uint64 GetFrameId() const = 0;

	/// @See SRenderingPassInfo::ESkipRenderingFlags
	virtual void   SetSkipRenderingFlags(uint32 nFlags) = 0;
	virtual uint32 GetSkipRenderingFlags() const = 0;

	/// @see EShaderRenderingFlags
	virtual void   SetShaderRenderingFlags(uint32 nFlags) = 0;
	virtual uint32 GetShaderRenderingFlags() const = 0;

	virtual void   SetCameras(const CCamera* pCameras, int cameraCount) = 0;
	virtual void   SetPreviousFrameCameras(const CCamera* pCameras, int cameraCount) = 0;

	virtual void   SwitchUsageMode(EUsageMode mode) = 0;

	// All jobs that write items to render view should share and use this synchronization mutex.
	virtual CryJobState* GetWriteMutex() = 0;

	//////////////////////////////////////////////////////////////////////////
	// Access to dynamic lights
	virtual void          AddDynamicLight(SRenderLight& light) = 0;
	virtual int           GetDynamicLightsCount() const = 0;
	virtual SRenderLight& GetDynamicLight(int nLightId) = 0;

	virtual void          AddLight(eDeferredLightType lightType, SRenderLight& light) = 0;
	virtual int           GetLightsCount(eDeferredLightType lightType) const = 0;
	virtual SRenderLight& GetLight(eDeferredLightType lightType, int nLightId) = 0;
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Interface for 3d engine
	//////////////////////////////////////////////////////////////////////////
	virtual void AddPermanentObject(CRenderObject* pRenderObject, const SRenderingPassInfo& passInfo) = 0;

	//////////////////////////////////////////////////////////////////////////
	// Clip Volumes
	virtual uint8 AddClipVolume(const IClipVolume* pClipVolume) = 0;
	virtual void  SetClipVolumeBlendInfo(const IClipVolume* pClipVolume, int blendInfoCount, IClipVolume** blendVolumes, Plane* blendPlanes) = 0;

	//////////////////////////////////////////////////////////////////////////
	// Fog Volumes
	virtual void AddFogVolume(const class CREFogVolume* pFogVolume) = 0;

	//////////////////////////////////////////////////////////////////////////
	// Cloud blockers
	virtual void AddCloudBlocker(const Vec3& pos, const Vec3& param, int32 flags) = 0;

	//////////////////////////////////////////////////////////////////////////
	// Water ripples
	virtual void AddWaterRipple(const SWaterRippleInfo& waterRippleInfo) = 0;
	virtual void AddPolygon(const SRenderPolygonDescription& poly, const SRenderingPassInfo& passInfo) = 0;
};

struct IRendererEngineModule : public Cry::IDefaultModule
{
	CRYINTERFACE_DECLARE_GUID(IRendererEngineModule, "516d044b-d626-4283-b08d-9ad9558204e0"_cry_guid);
};

//////////////////////////////////////////////////////////////////////
struct IRenderer//: public IRendererCallbackServer
{
	enum EViewportType
	{
		eViewportType_Default,
		eViewportType_Secondary,
	};

	virtual ~IRenderer(){}
	virtual void        AddListener(IRendererEventListener* pRendererEventListener) = 0;
	virtual void        RemoveListener(IRendererEventListener* pRendererEventListener) = 0;

	virtual ERenderType GetRenderType() const = 0;

	//! Initializes the renderer, parameters are self-explanatory.
	virtual WIN_HWND Init(int x, int y, int width, int height, unsigned int cbpp, int zbpp, int sbits, bool fullscreen, WIN_HWND Glhwnd = 0, bool bReInit = false, const SCustomRenderInitArgs* pCustomArgs = 0, bool bShaderCacheGen = false) = 0;
	virtual void     PostInit() = 0;

	//! Start active rendering of the intro movies while initializing the rest of the engine.
	virtual void StartRenderIntroMovies() = 0;
	virtual void StopRenderIntroMovies(bool bWaitForFinished) = 0;
	virtual bool IsRenderingIntroMovies() const = 0;

	virtual bool IsPost3DRendererEnabled() const { return false; }

	virtual void ExecuteAsyncDIP() = 0;

	virtual int  GetFeatures() = 0;
	virtual void GetVideoMemoryUsageStats(size_t& vidMemUsedThisFrame, size_t& vidMemUsedRecently, bool bGetPoolsSizes = false) = 0;
	virtual int  GetNumGeomInstances() = 0;
	virtual int  GetNumGeomInstanceDrawCalls() = 0;
	virtual int  GetCurrentNumberOfDrawCalls() = 0;
	virtual void GetCurrentNumberOfDrawCalls(int& nGeneral, int& nShadowGen) = 0;

	//! Sums DIP counts for the EFSLIST_* passes that match the submitted mask.
	//! Compose the mask with bitwise arithmetic, use (1 << EFSLIST_*) per list.
	//! E.g. to sum general and transparent, pass in ( (1 << EFSLIST_GENERAL) | (1 << EFSLIST_TRANSP) ).
	virtual int  GetCurrentNumberOfDrawCalls(const uint32 EFSListMask) = 0;

	virtual void SetDebugRenderNode(IRenderNode* pRenderNode) = 0;
	virtual bool IsDebugRenderNode(IRenderNode* pRenderNode) const = 0;

	/////////////////////////////////////////////////////////////////////////////////
	// Render-context management
	/////////////////////////////////////////////////////////////////////////////////
	virtual bool     DeleteContext(WIN_HWND hWnd) = 0;
	virtual bool     CreateContext(WIN_HWND hWnd, bool bMainViewport, int SSX = 1, int SSY = 1) = 0;
	virtual bool     SetCurrentContext(WIN_HWND hWnd) = 0;
	virtual void     MakeMainContextActive() = 0;
	virtual WIN_HWND GetCurrentContextHWND() = 0;
	virtual bool     IsCurrentContextMainVP() = 0;

	//! Gets height of the current viewport.
	virtual int GetCurrentContextViewportHeight() const = 0;

	//! Gets width of the current viewport.
	virtual int GetCurrentContextViewportWidth() const = 0;
	/////////////////////////////////////////////////////////////////////////////////

#if CRY_PLATFORM_DURANGO
	virtual void SuspendDevice() = 0;
	virtual void ResumeDevice() = 0;
#endif

	//! Shuts down the renderer.
	virtual void ShutDown(bool bReInit = false) = 0;
	virtual void ShutDownFast() = 0;

	//! Creates array of all supported video formats (except low resolution formats).
	//! \return Number of formats in memory.
	virtual int EnumDisplayFormats(SDispFormat* Formats) = 0;

	//! Returns all supported by video card video AA formats.
	virtual int EnumAAFormats(SAAFormat* Formats) = 0;

	//! Changes resolution of the window/device (doesn't require to reload the level.
	virtual bool ChangeResolution(int nNewWidth, int nNewHeight, int nNewColDepth, int nNewRefreshHZ, bool bFullScreen, bool bForceReset) = 0;

	//! Should be called at the beginning of every frame.
	virtual void BeginFrame() = 0;

	//! Creates default system shaders and textures.
	virtual void InitSystemResources(int nFlags) = 0;

	//! Frees the allocated resources.
	virtual void FreeResources(int nFlags) = 0;

	//! Shuts down the renderer.
	virtual void Release() = 0;

	//! \see r_ShowDynTextures
	virtual void RenderDebug(bool bRenderStats = true) = 0;

	//! Should be called at the end of every frame.
	virtual void EndFrame() = 0;

	//! Force a swap on the backbuffer.
	virtual void ForceSwapBuffers() = 0;

	//! Try to flush the render thread commands to keep the render thread active during level loading, but simpy return if the render thread is still busy
	virtual void               TryFlush() = 0;

	virtual void               GetViewport(int* x, int* y, int* width, int* height) = 0;
	virtual void               SetViewport(int x, int y, int width, int height, int id = 0) = 0;
	virtual void               SetRenderTile(f32 nTilesPosX = 0.f, f32 nTilesPosY = 0.f, f32 nTilesGridSizeX = 1.f, f32 nTilesGridSizeY = 1.f) = 0;
	virtual void               SetScissor(int x = 0, int y = 0, int width = 0, int height = 0) = 0;

	virtual EScreenAspectRatio GetScreenAspect(int nWidth, int nHeight) = 0;

	virtual Vec2               SetViewportDownscale(float xscale, float yscale) = 0;

	//! Draw user primitives.
	virtual void DrawDynVB(SVF_P3F_C4B_T2F* pBuf, uint16* pInds, int nVerts, int nInds, const PublicRenderPrimitiveType nPrimType) = 0;

	//! Set the renderer camera.
	virtual void SetCamera(const CCamera& cam) = 0;

	//! Get the renderer camera.
	virtual const CCamera& GetCamera() = 0;

	virtual CRenderView*   GetRenderViewForThread(int nThreadID, IRenderView::EViewType Type=IRenderView::eViewType_Default) = 0;

	//! Set delta gamma.
	virtual bool SetGammaDelta(const float fGamma) = 0;

	//! Reset gamma setting if not in fullscreen mode.
	virtual void RestoreGamma(void) = 0;

	//! Changes display size.
	virtual bool ChangeDisplay(unsigned int width, unsigned int height, unsigned int cbpp) = 0;

	//! Changes viewport size.
	virtual void ChangeViewport(unsigned int x, unsigned int y, unsigned int width, unsigned int height) = 0;

	// Should not be here.
	//! Saves source data to a Tga file.
	virtual bool SaveTga(unsigned char* sourcedata, int sourceformat, int w, int h, const char* filename, bool flip) const = 0;

	//! Sets the current binded texture.
	virtual void SetTexture(int tnum) = 0;

	//! Sets the white texture.
	virtual void SetWhiteTexture() = 0;

	//! Gets the white texture Id.
	virtual int GetWhiteTextureId() const = 0;

	//! Draws a 2d image on the screen.
	//! Example: Hud etc.
	virtual void Draw2dImage(float xpos, float ypos, float w, float h, int texture_id, float s0 = 0, float t0 = 0, float s1 = 1, float t1 = 1, float angle = 0, float r = 1, float g = 1, float b = 1, float a = 1, float z = 1) = 0;

	virtual void Draw2dImageStretchMode(bool stretch) = 0;

	//! Adds a 2D image that should be drawn on the screen to an internal render list. The list can be drawn with Draw2dImageList.
	//! If several images will be drawn, using this function is more efficient than calling Draw2dImage as it allows better batching.
	//! The function supports placing images in stereo 3D space.
	//! The stereo params are the same that are used for the scene. A value of 0 is handled as a special case and always places the image on the screen plane.
	//! \param stereoDepth - Places image in stereo 3d space. The depth is specified in camera space.
	virtual void Push2dImage(float xpos, float ypos, float w, float h, int texture_id, float s0 = 0, float t0 = 0, float s1 = 1, float t1 = 1, float angle = 0, float r = 1, float g = 1, float b = 1, float a = 1, float z = 1, float stereoDepth = 0) = 0;

	//! Used for pushing texture to texture render tasks in UI system.
	//! Will post render in Render Thread.
	virtual void PushUITexture(int srcId, int dstId, float x0, float y0, float w, float h, float u0, float v0, float u1, float v1, float r, float g, float b, float a) = 0;

	//! Used for calculating the hit uv-coordinates from a raycast.
	virtual int	GetDetailedRayHitInfo(IPhysicalEntity* pCollider, const Vec3& vOrigin, const Vec3& vDirection, const float maxRayDist, float* pUOut, float* pVOut) = 0;

	//! Turns screen point into vector that is projected into scene, from the current view
	virtual Vec3 UnprojectFromScreen(int x, int y) = 0;

	//! Draws all images to the screen that were collected with Push2dImage.
	virtual void Draw2dImageList() = 0;

	//! Draws a image using the current matrix.
	virtual void DrawImage(float xpos, float ypos, float w, float h, int texture_id, float s0, float t0, float s1, float t1, float r, float g, float b, float a, bool filtered = true) = 0;

	//! Draws a image using the current matrix.
	//! More flexible than DrawImage order for s and t: 0=left_top, 1=right_top, 2=right_bottom, 3=left_bottom.
	virtual void DrawImageWithUV(float xpos, float ypos, float z, float width, float height, int texture_id, float* s, float* t, float r = 1, float g = 1, float b = 1, float a = 1, bool filtered = true) = 0;

	//!	Sets the polygon mode with Push, Pop restores the last used one
	//! Example: Wireframe, solid.
	virtual void PushWireframeMode(int mode) = 0;
	virtual void PopWireframeMode() = 0;

	//! Gets height of the main rendering resolution.
	virtual int GetHeight() = 0;

	//! Gets width of the main rendering resolution.
	virtual int GetWidth() = 0;

	//! Gets Pixel Aspect Ratio.
	virtual float GetPixelAspectRatio() const = 0;

	//! Gets width of the height of the overlay viewport where UI and debug output are rendered.
	virtual int GetOverlayHeight() = 0;

	//! Gets width of the width of the overlay viewport where UI and debug output are rendered.
	virtual int GetOverlayWidth() = 0;

	//! Switches subsequent rendering from the internal backbuffer to the native resolution backbuffer if available.
	virtual void SwitchToNativeResolutionBackbuffer() = 0;

	//! Gets memory status information
	virtual void GetMemoryUsage(ICrySizer* Sizer) = 0;

	//! Gets textures streaming bandwidth information
	virtual void GetBandwidthStats(float* fBandwidthRequested) = 0;

	//! Sets an event listener for texture streaming updates
	virtual void SetTextureStreamListener(ITextureStreamListener* pListener) = 0;

#if defined(CRY_ENABLE_RC_HELPER)
	//! Sets an event listener for triggered asynchronous resource compilation.
	//! The listener is called asynchronously as well, thus doesn't block the renderer/streaming,
	//! but it does block the worker-queue, no resources will be compiled for as long as the listener executes.
	//! The listener doesn't need to be thread-safe, there is always just one notified.
	virtual void AddAsyncTextureCompileListener(IAsyncTextureCompileListener* pListener) = 0;
	virtual void RemoveAsyncTextureCompileListener(IAsyncTextureCompileListener* pListener) = 0;
#endif

	//! Pins an occlusion buffer into CPU memory, from where it can be used until UnpinOcclusionBuffer is called.
	//! The occlusion buffer is of type 'float[CULL_SIZEX * CULL_SIZEY]', and contains non-linearized depth values from 0 (near) to 1 (far).
	//! While at least one client has a buffer pinned, all other clients are guaranteed to pin the same buffer.
	//! Note: This implies all pins have to be released at some point or all clients will forever be stuck with old data.
	//! (Un)pinning can be arbitrarily nested from any thread and will never block forward progress of the render thread or GPU.
	//! The function may fail (by returning nullptr) if no data is available, in which case you must not call UnpinOcclusionBuffer.
	virtual float* PinOcclusionBuffer(Matrix44A& camera) = 0;
	virtual void   UnpinOcclusionBuffer() = 0;

	//! Take a screenshot and save it to a file
	//! \return true on success
	virtual bool ScreenShot(const char* filename = NULL, int width = 0, EScreenShotMode eScreenShotMode = eScreenShotMode_Normal) = 0;

	//! Get current bpp.
	virtual int GetColorBpp() = 0;

	//! Get current z-buffer depth.
	virtual int GetDepthBpp() = 0;

	//! Get current stencil bits.
	virtual int GetStencilBpp() = 0;

	//! Returns true if stereo rendering is enabled.
	virtual bool IsStereoEnabled() const = 0;

	//! Returns values of nearest rendering z-range max
	virtual float GetNearestRangeMax() const = 0;

	//! Projects to screen.
	//! Returns true if successful.
	virtual bool ProjectToScreen(
	  float ptx, float pty, float ptz,
	  float* sx, float* sy, float* sz) = 0;

	//! Unprojects to screen.
	virtual int UnProject(
	  float sx, float sy, float sz,
	  float* px, float* py, float* pz,
	  const float modelMatrix[16],
	  const float projMatrix[16],
	  const int viewport[4]) = 0;

	//! Unprojects from screen.
	virtual int UnProjectFromScreen(
	  float sx, float sy, float sz,
	  float* px, float* py, float* pz) = 0;

	//! \note For editor.
	virtual void GetModelViewMatrix(float* mat) = 0;

	//! \note For editor.
	virtual void GetProjectionMatrix(float* mat) = 0;

	virtual void GetCameraZeroMatrix(float* mat) = 0;

	virtual bool WriteDDS(byte* dat, int wdt, int hgt, int Size, const char* name, ETEX_Format eF, int NumMips) = 0;
	virtual bool WriteTGA(byte* dat, int wdt, int hgt, const char* name, int src_bits_per_pixel, int dest_bits_per_pixel) = 0;
	virtual bool WriteJPG(byte* dat, int wdt, int hgt, char* name, int src_bits_per_pixel, int nQuality = 100) = 0;

	/////////////////////////////////////////////////////////////////////////////////
	//Replacement functions for Font

	virtual bool FontUploadTexture(class CFBitmap*, ETEX_Format eTF = eTF_R8G8B8A8) = 0;
	virtual int  FontCreateTexture(int Width, int Height, byte* pData, ETEX_Format eTF = eTF_R8G8B8A8, bool genMips = false) = 0;
	virtual bool FontUpdateTexture(int nTexId, int X, int Y, int USize, int VSize, byte* pData) = 0;
	virtual void FontReleaseTexture(class CFBitmap* pBmp) = 0;
	virtual void FontSetTexture(class CFBitmap*, int nFilterMode) = 0;
	virtual void FontSetTexture(int nTexId, int nFilterMode) = 0;
	virtual void FontSetRenderingState(unsigned int nVirtualScreenWidth, unsigned int nVirtualScreenHeight) = 0;
	virtual void FontSetBlending(int src, int dst) = 0;
	virtual void FontRestoreRenderingState() = 0;

	virtual bool FlushRTCommands(bool bWait, bool bImmediatelly, bool bForce) = 0;

	virtual int  RT_CurThreadList() = 0;
	virtual void RT_FlashRender(IFlashPlayer_RenderProxy* pPlayer, bool stereo) = 0;
	virtual void RT_FlashRenderPlaybackLockless(IFlashPlayer_RenderProxy* pPlayer, int cbIdx, bool stereo, bool finalPlayback) = 0;
	virtual void RT_FlashRemoveTexture(ITexture* pTexture) = 0;

	/////////////////////////////////////////////////////////////////////////////////
	// External interface for shaders
	/////////////////////////////////////////////////////////////////////////////////
	virtual bool           EF_PrecacheResource(SShaderItem *pSI, int iScreenTexels, float fTimeToReady, int Flags, int nUpdateId, int nCounter = 1) = 0;
	virtual bool           EF_PrecacheResource(SShaderItem* pSI, float fMipFactor, float fTimeToReady, int Flags, int nUpdateId, int nCounter = 1) = 0;
	virtual bool           EF_PrecacheResource(IShader* pSH, float fMipFactor, float fTimeToReady, int Flags) = 0;
	virtual bool           EF_PrecacheResource(ITexture* pTP, float fMipFactor, float fTimeToReady, int Flags, int nUpdateId, int nCounter = 1) = 0;
	virtual bool           EF_PrecacheResource(IRenderMesh* pPB, IMaterial* pMaterial, float fMipFactor, float fTimeToReady, int Flags, int nUpdateId) = 0;
	virtual bool           EF_PrecacheResource(CDLight* pLS, float fMipFactor, float fTimeToReady, int Flags, int nUpdateId) = 0;

	virtual ITexture*      EF_CreateCompositeTexture(int type, const char* szName, int nWidth, int nHeight, int nDepth, int nMips, int nFlags, ETEX_Format eTF, const STexComposition* pCompositions, size_t nCompositions, int8 nPriority = -1) = 0;

	virtual void           PostLevelLoading() = 0;
	virtual void           PostLevelUnload() = 0;

	virtual void           EF_AddMultipleParticlesToScene(const SAddParticlesToSceneJob* jobs, size_t numJobs, const SRenderingPassInfo& passInfo) = 0;
	virtual void           GetMemoryUsageParticleREs(ICrySizer* pSizer) {}

	/////////////////////////////////////////////////////////////////////////////////
	// Shaders/Shaders management /////////////////////////////////////////////////////////////////////////////////

	virtual void        EF_SetShaderMissCallback(ShaderCacheMissCallback callback) = 0;
	virtual const char* EF_GetShaderMissLogPath() = 0;

	/////////////////////////////////////////////////////////////////////////////////
	virtual string* EF_GetShaderNames(int& nNumShaders) = 0;

	//! Reloads file
	virtual bool EF_ReloadFile(const char* szFileName) = 0;

	//! Reloads file at any time the renderer feels to do so (no guarantees, but likely on next frame update).
	//! \note This function is is thread safe.
	virtual bool                   EF_ReloadFile_Request(const char* szFileName) = 0;

	virtual _smart_ptr<IImageFile> EF_LoadImage(const char* szFileName, uint32 nFlags) = 0;

	//! Remaps shader gen mask to common global mask.
	virtual uint64                EF_GetRemapedShaderMaskGen(const char* name, uint64 nMaskGen = 0, bool bFixup = 0) = 0;

	virtual uint64                EF_GetShaderGlobalMaskGenFromString(const char* szShaderName, const char* szShaderGen, uint64 nMaskGen = 0) = 0;
	virtual const char*           EF_GetStringFromShaderGlobalMaskGen(const char* szShaderName, uint64 nMaskGen = 0) = 0;

	virtual const SShaderProfile& GetShaderProfile(EShaderType eST) const = 0;
	virtual void                  EF_SetShaderQuality(EShaderType eST, EShaderQuality eSQ) = 0;

	//! Submit window texture info.
	virtual void EF_SubmitWind(const SWindGrid* pWind) = 0;

	//! Get renderer quality.
	virtual ERenderQuality EF_GetRenderQuality() const = 0;

	//! Get shader type quality.
	virtual EShaderQuality EF_GetShaderQuality(EShaderType eST) = 0;

	//! Load shader item for name (name).
	struct SLoadShaderItemArgs
	{
		SLoadShaderItemArgs(IMaterial* pMtlSrc, IMaterial* pMtlSrcParent) : m_pMtlSrc(pMtlSrc), m_pMtlSrcParent(pMtlSrcParent) {}
		IMaterial* m_pMtlSrc;
		IMaterial* m_pMtlSrcParent;
	};
	virtual SShaderItem EF_LoadShaderItem(const char* szName, bool bShare, int flags = 0, SInputShaderResources* Res = NULL, uint64 nMaskGen = 0, const SLoadShaderItemArgs* pArgs = 0) = 0;

	//! Load shader for name (name).
	virtual IShader* EF_LoadShader(const char* name, int flags = 0, uint64 nMaskGen = 0) = 0;

	//! Reinitialize all shader files (build hash tables).
	virtual void EF_ReloadShaderFiles(int nCategory) = 0;

	//! Reload all texture files.
	virtual void EF_ReloadTextures() = 0;

	//! Get texture object by ID.
	virtual ITexture* EF_GetTextureByID(int Id) = 0;

	//! Get texture object by Name.
	virtual ITexture* EF_GetTextureByName(const char* name, uint32 flags = 0) = 0;

	//! Load the texture for name(nameTex).
	virtual ITexture* EF_LoadTexture(const char* nameTex, const uint32 flags = 0) = 0;

	//! Load the texture for name(nameTex).
	virtual IDynTextureSource* EF_LoadDynTexture(const char* dynsourceName, bool sharedRT = false) = 0;

	//! Load lightmap for name.
	virtual int  EF_LoadLightmap(const char* name) = 0;
	virtual bool EF_RenderEnvironmentCubeHDR(int size, Vec3& Pos, TArray<unsigned short>& vecData) = 0;
	// Writes a TIF file to the system with the requested preset
	// Intended for use with EF_RenderEnvironmentCubeHDR
	virtual bool WriteTIFToDisk(const void* pData, int width, int height, int bytesPerChannel, int numChannels, bool bFloat, const char* szPreset, const char* szFileName) = 0;

	//! Stores GBuffers region to atlas textures.
	virtual bool StoreGBufferToAtlas(const RectI& rcDst, int nSrcWidth, int nSrcHeight, int nDstWidth, int nDstHeight, ITexture *pDataD, ITexture *pDataN) = 0;

	//! Create new RE (RenderElement) of type (edt).
	virtual CRenderElement*  EF_CreateRE(EDataType edt) = 0;

	//! Start using of the shaders (return first index for allow recursions).
	virtual void EF_StartEf(const SRenderingPassInfo& passInfo) = 0;

	//! Get CRenderObject for RE transformation.
	//! Get temporary RenderObject
	virtual CRenderObject* EF_GetObject_Temp(int nThreadID) = 0;

	//! Get permanent RenderObject
	virtual CRenderObject* EF_GetObject() = 0;

	//! Release permanent RenderObject
	virtual void EF_FreeObject(CRenderObject* pObj) = 0;

	//Get permanent RenderObject
	virtual CRenderObject* EF_DuplicateRO(CRenderObject* pObj, const SRenderingPassInfo& passInfo) = 0;

	//! Add shader to the list.
	virtual void EF_AddEf(CRenderElement* pRE, SShaderItem& pSH, CRenderObject* pObj, const SRenderingPassInfo& passInfo, int nList, int nAW) = 0;

	//! Draw all shaded REs in the list
	virtual void EF_EndEf3D(const int nFlags, const int nPrecacheUpdateId, const int nNearPrecacheUpdateId, const SRenderingPassInfo& passInfo) = 0;

	virtual void EF_InvokeShadowMapRenderJobs(CRenderView* pRenderView, const int nFlags) = 0;

	// Dynamic lights.
	virtual bool EF_IsFakeDLight(const CDLight* Source) = 0;
	virtual void EF_ADDDlight(CDLight* Source, const SRenderingPassInfo& passInfo) = 0;
	virtual bool EF_UpdateDLight(SRenderLight* pDL) = 0;
	virtual bool EF_AddDeferredDecal(const SDeferredDecal& rDecal, const SRenderingPassInfo& passInfo) { return true; }

	// Deferred lights/vis areas

	virtual int EF_AddDeferredLight(const CDLight& pLight, float fMult, const SRenderingPassInfo& passInfo) = 0;

	// Deferred clip volumes
	virtual void Ef_AddDeferredGIClipVolume(const IRenderMesh* pClipVolume, const Matrix34& mxTransform) = 0;

	//! Called in between levels to free up memory.
	virtual void EF_ReleaseDeferredData() = 0;

	//! Called in between levels to free up memory.
	virtual SInputShaderResources* EF_CreateInputShaderResource(IRenderShaderResources* pOptionalCopyFrom = 0) = 0;

	//////////////////////////////////////////////////////////////////////////
	// Post-processing effects interfaces.

	virtual void  EF_SetPostEffectParam(const char* pParam, float fValue, bool bForceValue = false) = 0;
	virtual void  EF_SetPostEffectParamVec4(const char* pParam, const Vec4& pValue, bool bForceValue = false) = 0;
	virtual void  EF_SetPostEffectParamString(const char* pParam, const char* pszArg) = 0;

	virtual void  EF_GetPostEffectParam(const char* pParam, float& fValue) = 0;
	virtual void  EF_GetPostEffectParamVec4(const char* pParam, Vec4& pValue) = 0;
	virtual void  EF_GetPostEffectParamString(const char* pParam, const char*& pszArg) = 0;

	virtual int32 EF_GetPostEffectID(const char* pPostEffectName) = 0;

	virtual void  EF_ResetPostEffects(bool bOnSpecChange = false) = 0;

	virtual void  EF_DisableTemporalEffects() = 0;

	/////////////////////////////////////////////////////////////////////////////////
	// 2D interface for the shaders.
	/////////////////////////////////////////////////////////////////////////////////
	virtual void EF_EndEf2D(const bool bSort) = 0;

	//! Returns various Renderer Settings, see ERenderQueryTypes.
	//! \param Query e.g. EFQ_GetShaderCombinations.
	//! \param rInOut Input/Output Parameter, depends on the query if written to/read from, or both.
	void EF_Query(ERenderQueryTypes eQuery)
	{
		EF_QueryImpl(eQuery, NULL, 0, NULL, 0);
	}
	template<typename T>
	void EF_Query(ERenderQueryTypes eQuery, T& rInOut)
	{
		EF_QueryImpl(eQuery, static_cast<void*>(&rInOut), sizeof(T), NULL, 0);
	}
	template<typename T0, typename T1>
	void EF_Query(ERenderQueryTypes eQuery, T0& rInOut0, T1& rInOut1)
	{
		EF_QueryImpl(eQuery, static_cast<void*>(&rInOut0), sizeof(T0), static_cast<void*>(&rInOut1), sizeof(T1));
	}

	//! Toggles render mesh garbage collection.
	virtual void ForceGC() = 0;

	//! \note For stats.
	virtual int  GetPolyCount() = 0;
	virtual void GetPolyCount(int& nPolygons, int& nShadowVolPolys) = 0;

	//! \note 3D engine set this color to fog color.
	virtual void SetClearColor(const Vec3& vColor) = 0;

	//! Creates/deletes RenderMesh object.
	virtual _smart_ptr<IRenderMesh> CreateRenderMesh(
	  const char* szType
	  , const char* szSourceName
	  , IRenderMesh::SInitParamerers* pInitParams = NULL
	  , ERenderMeshType eBufType = eRMT_Static
	  ) = 0;

	virtual _smart_ptr<IRenderMesh> CreateRenderMeshInitialized(
	  const void* pVertBuffer, int nVertCount, InputLayoutHandle eVF,
	  const vtx_idx* pIndices, int nIndices,
	  const PublicRenderPrimitiveType nPrimetiveType, const char* szType, const char* szSourceName, ERenderMeshType eBufType = eRMT_Static,
	  int nMatInfoCount = 1, int nClientTextureBindID = 0,
	  bool (* PrepareBufferCallback)(IRenderMesh*, bool) = NULL,
	  void* CustomData = NULL,
	  bool bOnlyVideoBuffer = false,
	  bool bPrecache = true,
	  const SPipTangents* pTangents = NULL, bool bLockForThreadAcc = false, Vec3* pNormals = NULL) = 0;

	virtual int  GetFrameID(bool bIncludeRecursiveCalls = true) = 0;

	virtual void MakeMatrix(const Vec3& pos, const Vec3& angles, const Vec3& scale, Matrix34* mat) = 0;

	//////////////////////////////////////////////////////////////////////

	virtual float ScaleCoordX(float value) const = 0;
	virtual float ScaleCoordY(float value) const = 0;
	virtual void  ScaleCoord(float& x, float& y) const = 0;

	virtual void  SetState(int State, int AlphaRef = -1) = 0;
	virtual void  SetCullMode(int mode = R_CULL_BACK) = 0;

	virtual void  PushProfileMarker(char* label) = 0;
	virtual void  PopProfileMarker(char* label) = 0;

	virtual bool  EnableFog(bool enable) = 0;
	virtual void  SetFogColor(const ColorF& color) = 0;

	virtual void  SetColorOp(byte eCo, byte eAo, byte eCa, byte eAa) = 0;

	//! For one frame allows disabling limit of texture streaming requests.
	virtual void RequestFlushAllPendingTextureStreamingJobs(int nFrames) {}

	//! Allow dynamic adjustment texture streaming load depending on game conditions.
	virtual void SetTexturesStreamingGlobalMipFactor(float fFactor) {}

	//////////////////////////////////////////////////////////////////////
	//! Interface for auxiliary geometry (for debugging, editor purposes, etc.)
	virtual IRenderAuxGeom* GetIRenderAuxGeom(EViewportType viewport = eViewportType_Default) = 0;
	//////////////////////////////////////////////////////////////////////

	//! Interface for renderer side SVO.
	virtual ISvoRenderer*            GetISvoRenderer() { return 0; }

	virtual IColorGradingController* GetIColorGradingController() = 0;
	virtual IStereoRenderer*         GetIStereoRenderer() = 0;

	virtual void                     ResetToDefault() = 0;
	virtual void                     SetMaterialColor(float r, float g, float b, float a) = 0;

	virtual void                     Graph(byte* g, int x, int y, int wdt, int hgt, int nC, int type, char* text, ColorF& color, float fScale) = 0;

	// NOTE: deprecated
	virtual void ClearTargetsImmediately(uint32 nFlags) = 0;
	virtual void ClearTargetsImmediately(uint32 nFlags, const ColorF& Colors, float fDepth) = 0;
	virtual void ClearTargetsImmediately(uint32 nFlags, const ColorF& Colors) = 0;
	virtual void ClearTargetsImmediately(uint32 nFlags, float fDepth) = 0;

	virtual void ReadFrameBuffer(unsigned char* pRGB, int nImageX, int nSizeX, int nSizeY, ERB_Type eRBType, bool bRGBA, int nScaledX = -1, int nScaledY = -1) = 0;
	virtual void ReadFrameBufferFast(uint32* pDstARGBA8, int dstWidth, int dstHeight) = 0;

	// NB: The following functions will be removed.
	virtual void         EnableVSync(bool enable) = 0;
	virtual void         PopMatrix() = 0;
	virtual void         PushMatrix() = 0;

	virtual void         CreateResourceAsync(SResourceAsync* Resource) = 0;
	virtual void         ReleaseResourceAsync(SResourceAsync* Resource) = 0;
	virtual unsigned int DownLoadToVideoMemory(unsigned char* data, int w, int h, ETEX_Format eTFSrc, ETEX_Format eTFDst, int nummipmap, bool repeat = true, int filter = FILTER_BILINEAR, int Id = 0, const char* szCacheName = NULL, int flags = 0, EEndian eEndian = eLittleEndian, RectI* pRegion = NULL, bool bAsynDevTexCreation = false) = 0;
	virtual unsigned int DownLoadToVideoMemory3D(unsigned char* data, int w, int h, int d, ETEX_Format eTFSrc, ETEX_Format eTFDst, int nummipmap, bool repeat = true, int filter = FILTER_BILINEAR, int Id = 0, const char* szCacheName = NULL, int flags = 0, EEndian eEndian = eLittleEndian, RectI* pRegion = NULL, bool bAsynDevTexCreation = false) = 0;
	virtual unsigned int DownLoadToVideoMemoryCube(unsigned char* data, int w, int h, ETEX_Format eTFSrc, ETEX_Format eTFDst, int nummipmap, bool repeat = true, int filter = FILTER_BILINEAR, int Id = 0, const char* szCacheName = NULL, int flags = 0, EEndian eEndian = eLittleEndian, RectI* pRegion = NULL, bool bAsynDevTexCreation = false) = 0;
	virtual void         UpdateTextureInVideoMemory(uint32 tnum, unsigned char* newdata, int posx, int posy, int w, int h, ETEX_Format eTFSrc = eTF_B8G8R8, int posz = 0, int sizez = 1) = 0;

	// NB: Without header.
	virtual bool DXTCompress(byte* raw_data, int nWidth, int nHeight, ETEX_Format eTF, bool bUseHW, bool bGenMips, int nSrcBytesPerPix, MIPDXTcallback callback) = 0;
	virtual bool DXTDecompress(byte* srcData, const size_t srcFileSize, byte* dstData, int nWidth, int nHeight, int nMips, ETEX_Format eSrcTF, bool bUseHW, int nDstBytesPerPix) = 0;
	virtual void RemoveTexture(unsigned int TextureId) = 0;

	virtual bool BakeMesh(const SMeshBakingInputParams* pInputParams, SMeshBakingOutput* pReturnValues) = 0;

	//! This routines uses 2 destination surfaces.  It triggers a backbuffer copy to one of its surfaces, and then copies the other surface to system memory.
	//! This hopefully will remove any CPU stalls due to the rect lock call since the buffer will already be in system memory when it is called.
	//! \param pDstARGBA8			Pointer to a buffer that will hold the captured frame (should be at least 4*dstWidth*dstHieght for RGBA surface).
	//! \param destinationWidth	Width of the frame to copy.
	//! \param destinationHeight	Height of the frame to copy.
	//! \note If dstWidth or dstHeight is larger than the current surface dimensions, the dimensions of the surface are used for the copy.
	virtual bool CaptureFrameBufferFast(unsigned char* pDstRGBA8, int destinationWidth, int destinationHeight) = 0;

	//! Copy a captured surface to a buffer.
	//! \param pDstARGBA8			Pointer to a buffer that will hold the captured frame (should be at least 4*dstWidth*dstHieght for RGBA surface).
	//! \param destinationWidth	Width of the frame to copy.
	//! \param destinationHeight	Height of the frame to copy.
	//! \note If dstWidth or dstHeight is larger than the current surface dimensions, the dimensions of the surface are used for the copy.
	virtual bool CopyFrameBufferFast(unsigned char* pDstRGBA8, int destinationWidth, int destinationHeight) = 0;

	//! This routine registers a callback address that is called when a new frame is available.
	//! \param pCapture			Address of the ICaptureFrameListener object.
	//! \return true if successful, otherwise false.
	virtual bool RegisterCaptureFrame(ICaptureFrameListener* pCapture) = 0;

	//! This routine unregisters a callback address that was previously registered.
	//! \param pCapture			Address of the ICaptureFrameListener object to unregister.
	//! \returns true if successful, otherwise false.
	virtual bool UnRegisterCaptureFrame(ICaptureFrameListener* pCapture) = 0;

	//! This routine initializes 2 destination surfaces for use by the CaptureFrameBufferFast routine.
	//! It also captures the current backbuffer into one of the created surfaces.
	//! \param bufferWidth	Width of capture buffer, on consoles the scaling is done on the GPU. Pass in 0 (the default) to use backbuffer dimensions.
	//! \param bufferHeight	Height of capture buffer.
	//! \return true if surfaces were created otherwise returns false.
	virtual bool InitCaptureFrameBufferFast(uint32 bufferWidth = 0, uint32 bufferHeight = 0) = 0;

	//! This routine releases the 2 surfaces used for frame capture by the CaptureFrameBufferFast routine.
	virtual void CloseCaptureFrameBufferFast(void) = 0;

	//! This routine checks for any frame buffer callbacks that are needed and calls them.
	virtual void                CaptureFrameBufferCallBack(void) = 0;

	virtual gpu_pfx2::IManager* GetGpuParticleManager() = 0;

	virtual void                RegisterSyncWithMainListener(ISyncMainWithRenderListener* pListener) = 0;
	virtual void                RemoveSyncWithMainListener(const ISyncMainWithRenderListener* pListener) = 0;

	virtual void                Set2DMode(bool enable, int ortox, int ortoy, float znear = -1e10f, float zfar = 1e10f) = 0;
	virtual int                 ScreenToTexture(int nTexID) = 0;
	virtual void                EnableSwapBuffers(bool bEnable) = 0;

	//! Stop renderer at the end of the frame.
	//! E.g. ensures the renderer is not holding a lock when crash handler is trying to take a screenshot.
	virtual bool StopRendererAtFrameEnd(uint timeoutMilliseconds) = 0;

	//! Resume renderer that was stopped at the end of the frame with StopRendererAtFrameEnd().
	virtual void     ResumeRendererFromFrameEnd() = 0;

	virtual WIN_HWND GetHWND() = 0;

	//! Set the window icon to be displayed on the output window.
	//! The parameter is the path to a DDS texture file to be used as the icon.
	//! For best results, pass a square power-of-two sized texture, with a mip-chain.
	virtual bool                SetWindowIcon(const char* path) = 0;

	virtual void                OnEntityDeleted(struct IRenderNode* pRenderNode) = 0;

	virtual int                 CreateRenderTarget(int nWidth, int nHeight, const ColorF& cClear, ETEX_Format eTF) = 0;
	virtual bool                DestroyRenderTarget(int nHandle) = 0;
	virtual bool                SetRenderTarget(int nHandle, int nFlags = 0) = 0;
	virtual float               EF_GetWaterZElevation(float fX, float fY) = 0;

	virtual IOpticsElementBase* CreateOptics(EFlareType type) const = 0;
	virtual void                ReleaseOptics(IOpticsElementBase* pOpticsElement) const = 0;

	//! Used for pausing timer related stuff.
	//! Example: For texture animations, and shader 'time' parameter.
	virtual void PauseTimer(bool bPause) = 0;

	//! Creates an Interface to the public params container.
	//! \return Created IShaderPublicParams interface.
	virtual IShaderPublicParams* CreateShaderPublicParams() = 0;

	virtual void                 SetLevelLoadingThreadId(threadID threadId) = 0;

	virtual void                 GetThreadIDs(threadID& mainThreadID, threadID& renderThreadID) const = 0;

	struct SUpdateRect
	{
		uint32 dstX, dstY;
		uint32 srcX, srcY;
		uint32 width, height;

		void   Set(uint32 dx, uint32 dy, uint32 sx, uint32 sy, uint32 w, uint32 h)
		{
			dstX = dx;
			dstY = dy;
			srcX = sx;
			srcY = sy;
			width = w;
			height = h;
		}
	};

	virtual IScaleformPlayback*                       SF_CreatePlayback() const = 0;
	virtual int                                       SF_CreateTexture(int width, int height, int numMips, const unsigned char* pData, ETEX_Format eTF, int flags) = 0;
	virtual bool                                      SF_UpdateTexture(int texId, int mipLevel, int numRects, const SUpdateRect* pRects, const unsigned char* pData, size_t pitch, size_t size, ETEX_Format eTF) = 0;
	virtual bool                                      SF_ClearTexture(int texId, int mipLevel, int numRects, const SUpdateRect* pRects, const unsigned char* pData) = 0;
	virtual void                                      SF_Playback(IScaleformPlayback* pRenderer, GRendererCommandBufferReadOnly* pBuffer) const = 0;
	virtual void                                      SF_Drain(GRendererCommandBufferReadOnly* pBuffer) const = 0;
	virtual void                                      SF_GetMeshMaxSize(int& numVertices, int& numIndices) const = 0;

	virtual ITexture*                                 CreateTexture(const char* name, int width, int height, int numMips, unsigned char* pData, ETEX_Format eTF, int flags) = 0;
	virtual ITexture*                                 CreateTextureArray(const char* name, ETEX_Type eType, uint32 nWidth, uint32 nHeight, uint32 nArraySize, int nMips, uint32 nFlags, ETEX_Format eTF, int nCustomID) = 0;

	virtual const RPProfilerStats*                    GetRPPStats(ERenderPipelineProfilerStats eStat, bool bCalledFromMainThread = true) = 0;
	virtual const RPProfilerStats*                    GetRPPStatsArray(bool bCalledFromMainThread = true) = 0;

	virtual int                                       GetPolygonCountByType(uint32 EFSList, EVertexCostTypes vct, uint32 z, bool bCalledFromMainThread = true) = 0;

	virtual void                                      StartLoadtimeFlashPlayback(ILoadtimeCallback* pCallback) = 0;
	virtual void                                      StopLoadtimeFlashPlayback() = 0;

	virtual void                                      SetCloudShadowsParams(int nTexID, const Vec3& speed, float tiling, bool invert, float brightness) = 0;
	virtual void                                      SetVolumetricCloudParams(int nTexID) = 0;
	virtual void                                      SetVolumetricCloudNoiseTex(int cloudNoiseTexId, int edgeNoiseTexId) = 0;
	virtual uint16                                    PushFogVolumeContribution(const ColorF& fogVolumeContrib, const SRenderingPassInfo& passInfo) = 0;

	virtual int                                       GetMaxTextureSize() = 0;

	virtual const char*                               GetTextureFormatName(ETEX_Format eTF) = 0;
	virtual int                                       GetTextureFormatDataSize(int nWidth, int nHeight, int nDepth, int nMips, ETEX_Format eTF) = 0;
	virtual bool                                      IsTextureFormatSupported(ETEX_Format eTF) = 0;

	virtual void                                      SetDefaultMaterials(IMaterial* pDefMat, IMaterial* pTerrainDefMat) = 0;

	virtual uint32                                    GetActiveGPUCount() const = 0;
	virtual ShadowFrustumMGPUCache*                   GetShadowFrustumMGPUCache() = 0;
	virtual const StaticArray<int, MAX_GSM_LODS_NUM>& GetCachedShadowsResolution() const = 0;
	virtual void                                      SetCachedShadowsResolution(const StaticArray<int, MAX_GSM_LODS_NUM>& arrResolutions) = 0;
	virtual void                                      UpdateCachedShadowsLodCount(int nGsmLods) const = 0;

	virtual void                                      SetTexturePrecaching(bool stat) = 0;

	//! Platform-specific.
	virtual void RT_InsertGpuCallback(uint32 context, GpuCallbackFunc callback) = 0;
	virtual void EnablePipelineProfiler(bool bEnable) = 0;

	struct SGpuInfo
	{
		const char* name;
		unsigned int nNodeCount;
		UINT VendorId;
		UINT DeviceId;
		UINT SubSysId;
		UINT Revision;
	};

	virtual void QueryActiveGpuInfo(SGpuInfo& info) const = 0;

	struct SRenderTimes
	{
		float fWaitForMain;
		float fWaitForRender;
		float fWaitForGPU;
		float fTimeProcessedRT;
		float fTimeProcessedRTScene;  //!< The part of the render thread between the "SCENE" profiler labels.
		float fTimeProcessedGPU;
		float fTimeGPUIdlePercent;
	};
	virtual void  GetRenderTimes(SRenderTimes& outTimes) = 0;
	virtual float GetGPUFrameTime() = 0;

	//! Enable the batch mode if the meshpools are used to enable quick and dirty flushes.
	virtual void EnableBatchMode(bool enable) = 0;

	//! Flag level unloading in progress to disable f.i. rendermesh creation requests.
	virtual void EnableLevelUnloading(bool enable) = 0;

	struct SDrawCallCountInfo
	{
		static const uint32 MESH_NAME_LENGTH = 32;
		static const uint32 TYPE_NAME_LENGTH = 16;

		SDrawCallCountInfo() : pPos(0, 0, 0), nZpass(0), nShadows(0), nGeneral(0), nTransparent(0), nMisc(0)
		{
			meshName[0] = '\0';
			typeName[0] = '\0';
		}

#if !defined(_RELEASE)
		void Update(CRenderObject* pObj, IRenderMesh* pRM); // legacy support
		void Update(CRenderObject* pObj, IRenderMesh* pRM, EShaderTechniqueID techniqueID);
#endif

		Vec3  pPos;
		uint8 nZpass, nShadows, nGeneral, nTransparent, nMisc;
		char  meshName[MESH_NAME_LENGTH];
		char  typeName[TYPE_NAME_LENGTH];
	};

	//! Debug draw call info (per node).
	typedef std::map<IRenderNode*, IRenderer::SDrawCallCountInfo> RNDrawcallsMapNode;
	typedef RNDrawcallsMapNode::iterator                          RNDrawcallsMapNodeItor;

	//! Debug draw call info (per mesh).
	typedef std::map<IRenderMesh*, IRenderer::SDrawCallCountInfo> RNDrawcallsMapMesh;
	typedef RNDrawcallsMapMesh::iterator                          RNDrawcallsMapMeshItor;

#if !defined(_RELEASE)
	//! Get draw call info for frame.
	virtual RNDrawcallsMapMesh& GetDrawCallsInfoPerMesh(bool mainThread = true) = 0;
	virtual int                 GetDrawCallsPerNode(IRenderNode* pRenderNode) = 0;
	virtual void                ForceRemoveNodeFromDrawCallsMap(IRenderNode* pNode) = 0;
#endif

	virtual void CollectDrawCallsInfo(bool status) = 0;
	virtual void CollectDrawCallsInfoPerNode(bool status) = 0;

	// Summary:
	virtual SSkinningData* EF_CreateSkinningData(uint32 nNumBones, bool bNeedJobSyncVar) = 0;
	virtual SSkinningData* EF_CreateRemappedSkinningData(uint32 nNumBones, SSkinningData* pSourceSkinningData, uint32 nCustomDataSize, uint32 pairGuid) = 0;
	virtual void           EF_EnqueueComputeSkinningData(SSkinningData* pData) = 0;
	virtual int            EF_GetSkinningPoolID() = 0;

	virtual void           UpdateShaderItem(SShaderItem* pShaderItem, IMaterial* pMaterial) = 0;
	virtual void           ForceUpdateShaderItem(SShaderItem* pShaderItem, IMaterial* pMaterial) = 0;
	virtual void           RefreshShaderResourceConstants(SShaderItem* pShaderItem, IMaterial* pMaterial) = 0;

	//! Used for editor highlights, sets the highlight color
	virtual void           SetHighlightColor(ColorF color) = 0;
	//! Used for editor highlights, sets the selection color
	virtual void           SetSelectionColor(ColorF color) = 0;
	//! Used for editor highlights, sets outline thickness and ghost alpha parameters for the effect
	virtual void           SetHighlightParams(float outlineThickness, float fGhostAlpha) = 0;

	//! Determine if a switch to stereo mode will occur at the start of the next frame.
	virtual bool IsStereoModeChangePending() = 0;

	//! Wait for all particle ComputeVertices jobs to finish.
	virtual void SyncComputeVerticesJobs() = 0;

	//! Lock/Unlock the video memory buffer used by particles when using the jobsystem.
	virtual void  LockParticleVideoMemory() = 0;
	virtual void  UnLockParticleVideoMemory() = 0;

	virtual void  ActivateLayer(const char* pLayerName, bool activate) = 0;

	virtual void  FlushPendingTextureTasks() = 0;

	virtual void  SetCloakParams(const SRendererCloakParams& cloakParams) = 0;
	virtual float GetCloakFadeLightScale() const = 0;
	virtual void  SetCloakFadeLightScale(float fColorScale) = 0;
	virtual void  SetShadowJittering(float fShadowJittering) = 0;
	virtual float GetShadowJittering() const = 0;

	virtual bool  LoadShaderStartupCache() = 0;
	virtual void  UnloadShaderStartupCache() = 0;

	virtual bool  LoadShaderLevelCache() = 0;
	virtual void  UnloadShaderLevelCache() = 0;

	virtual void  OffsetPosition(const Vec3& delta) = 0;

	virtual void  SetRendererCVar(ICVar* pCVar, const char* pArgText, const bool bSilentMode = false) = 0;

#ifdef SUPPORT_HW_MOUSE_CURSOR
	virtual IHWMouseCursor* GetIHWMouseCursor() = 0;
#endif

	// Creates a constant buffer on the device.
	virtual IGraphicsDeviceConstantBufferPtr CreateGraphiceDeviceConstantBuffer() = 0;

private:
	//! Use private for EF_Query to prevent client code to submit arbitary combinations of output data/size.
	virtual void EF_QueryImpl(ERenderQueryTypes eQuery, void* pInOut0, uint32 nInOutSize0, void* pInOut1, uint32 nInOutSize1) = 0;
};

//! Util class to change wireframe mode.
class CScopedWireFrameMode
{
public:
	CScopedWireFrameMode(IRenderer* pRenderer, int nMode) : m_pRenderer(pRenderer), m_nMode(nMode)
	{
		(void) m_nMode;     // removes not used warning
		pRenderer->PushWireframeMode(nMode);
	}
	~CScopedWireFrameMode()
	{
		m_pRenderer->PopWireframeMode();
	}
private:
	IRenderer* m_pRenderer;
	int        m_nMode;
};

struct SShaderCacheStatistics
{
	size_t m_nTotalLevelShaderCacheMisses;
	size_t m_nGlobalShaderCacheMisses;
	size_t m_nNumShaderAsyncCompiles;
	bool   m_bShaderCompileActive;

	SShaderCacheStatistics() : m_nTotalLevelShaderCacheMisses(0),
		m_nGlobalShaderCacheMisses(0),
		m_nNumShaderAsyncCompiles(0), m_bShaderCompileActive(false)
	{}
};

//! The statistics about the pool for render mesh data.
struct SMeshPoolStatistics
{
	//! The size of the mesh data size in bytes.
	size_t nPoolSize;

	//! The amount of memory currently in use in the pool.
	size_t nPoolInUse;

	//! The highest amount of memory allocated within the mesh data pool.
	size_t nPoolInUsePeak;

	//! The size of the mesh data size in bytes.
	size_t nInstancePoolSize;

	//! The amount of memory currently in use in the pool.
	size_t nInstancePoolInUse;

	//! The highest amount of memory allocated within the mesh data pool.
	size_t nInstancePoolInUsePeak;

	size_t nFallbacks;
	size_t nInstanceFallbacks;
	size_t nFlushes;

	SMeshPoolStatistics()
		: nPoolSize(),
		nPoolInUse(),
		nInstancePoolSize(),
		nInstancePoolInUse(),
		nInstancePoolInUsePeak(),
		nFallbacks(),
		nInstanceFallbacks(),
		nFlushes()
	{}
};

struct SRendererQueryGetAllTexturesParam
{
	SRendererQueryGetAllTexturesParam()
		: pTextures(NULL)
		, numTextures(0)
	{
	}

	_smart_ptr<ITexture>* pTextures;
	uint32                numTextures;
};

//////////////////////////////////////////////////////////////////////

#define STRIPTYPE_NONE           0
#define STRIPTYPE_ONLYLISTS      1
#define STRIPTYPE_SINGLESTRIP    2
#define STRIPTYPE_MULTIPLESTRIPS 3
#define STRIPTYPE_DEFAULT        4

/////////////////////////////////////////////////////////////////////

struct IRenderMesh;

#include "VertexFormats.h"

struct SRestLightingInfo
{
	SRestLightingInfo()
	{
		averDir.zero();
		averCol = Col_Black;
		refPoint.zero();
	}
	Vec3   averDir;
	ColorF averCol;
	Vec3   refPoint;
};

class CLodValue
{
public:
	CLodValue()
	{
		m_nLodA = -1;
		m_nLodB = -1;
		m_nDissolveRef = 0;
	}

	CLodValue(int nLodA)
	{
		m_nLodA = nLodA;
		m_nLodB = -1;
		m_nDissolveRef = 0;
	}

	CLodValue(int nLodA, uint8 nDissolveRef, int nLodB)
	{
		m_nLodA = nLodA;
		m_nLodB = nLodB;
		m_nDissolveRef = nDissolveRef;
	}

	int  LodA() const         { return m_nLodA; }
	int  LodSafeA() const     { return m_nLodA == -1 ? 0 : m_nLodA; }
	int  LodB() const         { return m_nLodB; }

	uint DissolveRefA() const { return m_nDissolveRef; }
	uint DissolveRefB() const { return 255 - m_nDissolveRef; }

private:
	int16 m_nLodA;
	int16 m_nLodB;
	uint8 m_nDissolveRef;
};

//! Structure used to pass render parameters to Render() functions of IStatObj and ICharInstance.
struct SRendParams
{
	SRendParams()
	{
		memset(this, 0, sizeof(SRendParams));
		fAlpha = 1.f;
		nAfterWater = 1;
	}

	// object transformations.
	Matrix34*                 pMatrix;

	Matrix34*                 pPrevMatrix; //!< object previous transformations - motion blur specific.

	IVisArea*                 m_pVisArea; //!< VisArea that contains this object, used for RAM-ambient cube query.

	IMaterial*                pMaterial; //!< Override material.

	IFoliage*                 pFoliage; //!< Skeleton implementation for bendable foliage.

	struct IRenderNode*       pRenderNode; //!< Object Id for objects identification in renderer.

	void*                     pInstance; //!< Unique object Id for objects identification in renderer.

	struct SSectorTextureSet* pTerrainTexInfo;   //!< TerrainTexInfo for grass.

	DynArray<SShaderParam>*   pShaderParams; //!< dynamic render data object which can be set by the game

	ColorF                    AmbientColor; //!< Ambient color for the object.

	float                     fAlpha; //!< Object alpha.

	float                     fDistance; //!< Distance from camera.

	uint64                    dwFObjFlags; //!< Approximate information about the lights not included into nDLightMask.

	uint32                    nMaterialLayersBlend; //!< Material layers blending amount

	uint32                    nVisionParams; //!< Vision modes params

	uint32                    nHUDSilhouettesParams; //!< Vision modes params

	uint32                    pLayerEffectParams; //!< Layer effects.

	hidemask                  nSubObjHideMask; //!< Defines what peaces of pre-broken geometry has to be rendered.

	float                     fCustomData[4]; //!< Defines per object float custom data.

	int16                     nTextureID; //!< Custom TextureID.

	uint16                    nCustomFlags; //!< Defines per object custom flags.

	CLodValue                 lodValue; //!< The LOD value compute for rendering.

	uint8                     nCustomData; //!< Defines per object custom data.

	uint8                     nDissolveRef; //!< Defines per object DissolveRef value if used by shader.

	uint8                     nClipVolumeStencilRef; //!< Per-instance vis area stencil ref id.

	uint8                     nAfterWater; //!< Custom offset for sorting by distance.

	uint8                     nMaterialLayers; //!< Material layers bitmask -> which material layers are active.

	uint32                    nEditorSelectionID; //!< Selection ID and information for editor
};

struct SRendererCloakParams
{
	SRendererCloakParams()
	{
		memset(this, 0, sizeof(SRendererCloakParams));
	}

	float fCloakLightScale;
	float fCloakTransitionLightScale;
	int   cloakFadeByDist;
	float fCloakFadeLightScale;
	float fCloakFadeMinDistSq;
	float fCloakFadeMaxDistSq;
	float fCloakFadeMinValue;
	int   cloakRefractionFadeByDist;
	float fCloakRefractionFadeMinDistSq;
	float fCloakRefractionFadeMaxDistSq;
	float fCloakRefractionFadeMinValue;
	float fCloakMinLightValue;
	float fCloakHeatScale;
	int   cloakRenderInThermalVision;
	float fCloakMinAmbientOutdoors;
	float fCloakMinAmbientIndoors;
	float fCloakSparksAlpha;
	float fCloakInterferenceSparksAlpha;
	float fCloakHighlightStrength;
	float fThermalVisionViewDistance;
	float fArmourPulseSpeedMultiplier;
	float fMaxSuitPulseSpeedMultiplier;
};
