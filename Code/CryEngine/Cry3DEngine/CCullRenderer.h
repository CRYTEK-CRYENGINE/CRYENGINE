// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CCULLRENDERER__
#define __CCULLRENDERER__

#include "VMath.hpp"

#include <CryThreading/IThreadManager.h>		// For CScopedFloatingPointException

//#define CULL_RENDERER_REPROJ_DEBUG
#define CULL_RENDERER_MINZ

// enable this define to allow ingame debugging of the coverage buffer
#define CULLING_ENABLE_DEBUG_OVERLAY

#pragma warning(push)
#pragma warning(disable:6262)

namespace NAsyncCull
{
namespace Debug
{

inline SAuxVertex* Generate2DBox(SAuxVertex* pVertices, float fX, float fY, float fHeigth, float fWidth, const ColorB& rColor)
{
	float fPosition[4][2] =
	{
		{ fX,          fY           },
		{ fX,          fY + fHeigth },
		{ fX + fWidth, fY + fHeigth },
		{ fX + fWidth, fY           }
	};

	SAuxVertex v = { { 0, 0, 0}, {{ rColor.pack_argb8888() }}, { 0, 0 } };

	pVertices[0] = v;
	pVertices[0].xyz = Vec3(fPosition[0][0], fPosition[0][1], 0.0f);

	pVertices[1] = v;
	pVertices[1].xyz = Vec3(fPosition[1][0], fPosition[1][1], 0.0f);

	pVertices[2] = v;
	pVertices[2].xyz = Vec3(fPosition[2][0], fPosition[2][1], 0.0f);

	pVertices[3] = v;
	pVertices[3].xyz = Vec3(fPosition[0][0], fPosition[0][1], 0.0f);

	pVertices[4] = v;
	pVertices[4].xyz = Vec3(fPosition[2][0], fPosition[2][1], 0.0f);

	pVertices[5] = v;
	pVertices[5].xyz = Vec3(fPosition[3][0], fPosition[3][1], 0.0f);

	return pVertices + 6;
}

} // namesapce Debug
} //namespace NasyncCull

namespace NAsyncCull
{

typedef float                   tdZexel;
typedef uint16                  tdIndex;

typedef PodArray<NVMath::vec4>& tdVertexCacheArg;
typedef PodArray<NVMath::vec4>  tdVertexCache;

enum {VERTEX_CACHE_COUNT = 64 * 1024};

extern const NVMath::vec4 MaskNot3;

template<uint32 SIZEX, uint32 SIZEY>
class CCullRenderer
{
public:
	enum { RESOLUTION_X = SIZEX };
	enum { RESOLUTION_Y = SIZEY };
private:

	CRY_ALIGN(16) NVMath::vec4 m_VMaxXY;
	static CRY_ALIGN(128) float m_ZBufferMainMemory[SIZEX * SIZEY];
	uint32    m_SizeX4;
	Matrix44A m_Reproject;
	uint32    m_nNumWorker;
	tdZexel*  m_ZInput;
	tdZexel*  m_ZOutput;

	tdZexel** m_ZBufferSwap;

	CRY_ALIGN(128) tdZexel m_ZBufferSwapMerged[SIZEX * SIZEY]; // 128 byte for XMemSet128

#ifdef CULL_RENDERER_REPROJ_DEBUG
	tdZexel m_ZBufferOrig[SIZEX * SIZEY];
#endif

	uint32 m_DrawCall;
	uint32 m_PolyCount;

	template<bool WRITE, bool CULL, bool CULL_BACKFACES>
	inline bool Triangle(
	  const NVMath::vec4& rV0,
	  const NVMath::vec4& rV1,
	  const NVMath::vec4& rV2)
	{
		using namespace NVMath;

		vec4 V0 = rV0;
		vec4 V1 = rV1;
		vec4 V2 = rV2;

		const uint32 Idx = SignMask(Shuffle<xzzz>(Shuffle<zzzz>(V0, V1), V2)) & (BitX | BitY | BitZ);
		if (Idx == (BitX | BitY | BitZ))
		{
			return false;
		}

		bool Visible = false;
		switch (Idx)
		{
		case 0:
			break;
		case BitX:
			{
				const vec4 F0 = Splat<2>(V0);
				const vec4 F1 = Splat<2>(V1);
				const vec4 F2 = Splat<2>(V2);
				const vec4 M0 = Div(F0, Sub(F0, F2));
				const vec4 M1 = Div(F0, Sub(F0, F1));
				const vec4 P0 = Madd(Sub(V2, V0), M0, V0);
				const vec4 P1 = Madd(Sub(V1, V0), M1, V0);
				Visible = Triangle2D<WRITE, CULL, true, CULL_BACKFACES>(P0, P1, V1);
				V0 = P0;
			}
			break;
		case BitY:
			{
				const vec4 F0 = Splat<2>(V0);
				const vec4 F1 = Splat<2>(V1);
				const vec4 F2 = Splat<2>(V2);
				const vec4 M0 = Div(F1, Sub(F1, F0));
				const vec4 M1 = Div(F1, Sub(F1, F2));
				const vec4 P0 = Madd(Sub(V0, V1), M0, V1);
				const vec4 P1 = Madd(Sub(V2, V1), M1, V1);
				Visible = Triangle2D<WRITE, CULL, true, CULL_BACKFACES>(P0, P1, V2);
				V1 = P0;
			}
			break;
		case BitX | BitY:
			{
				const vec4 F0 = Splat<2>(V0);
				const vec4 F1 = Splat<2>(V1);
				const vec4 F2 = Splat<2>(V2);
				const vec4 M0 = Div(F0, Sub(F0, F2));
				const vec4 M1 = Div(F1, Sub(F1, F2));
				V0 = Madd(Sub(V2, V0), M0, V0);
				V1 = Madd(Sub(V2, V1), M1, V1);
			}
			break;
		case BitZ:
			{
				const vec4 F0 = Splat<2>(V0);
				const vec4 F1 = Splat<2>(V1);
				const vec4 F2 = Splat<2>(V2);
				const vec4 M0 = Div(F2, Sub(F2, F1));
				const vec4 M1 = Div(F2, Sub(F2, F0));
				const vec4 P0 = Madd(Sub(V1, V2), M0, V2);
				const vec4 P1 = Madd(Sub(V0, V2), M1, V2);
				Visible = Triangle2D<WRITE, CULL, true, CULL_BACKFACES>(V0, P0, P1);
				V2 = P0;
			}
			break;
		case BitX | BitZ:
			{
				const vec4 F0 = Splat<2>(V0);
				const vec4 F1 = Splat<2>(V1);
				const vec4 F2 = Splat<2>(V2);
				const vec4 M0 = Div(F0, Sub(F0, F1));
				const vec4 M1 = Div(F2, Sub(F2, F1));
				V0 = Madd(Sub(V1, V0), M0, V0);
				V2 = Madd(Sub(V1, V2), M1, V2);
			}
			break;
		case BitY | BitZ:
			{
				const vec4 F0 = Splat<2>(V0);
				const vec4 F1 = Splat<2>(V1);
				const vec4 F2 = Splat<2>(V2);
				const vec4 M0 = Div(F1, Sub(F1, F0));
				const vec4 M1 = Div(F2, Sub(F2, F0));
				V1 = Madd(Sub(V0, V1), M0, V1);
				V2 = Madd(Sub(V0, V2), M1, V2);
			}
			break;
		case BitX | BitY | BitZ:
			break;
#if !CRY_PLATFORM_ORBIS && !CRY_PLATFORM_ANDROID
		default:
			__assume(0);
#endif
		}

		return Visible | Triangle2D<WRITE, CULL, true, CULL_BACKFACES>(V0, V1, V2);
	}

	template<bool WRITE, bool CULL, bool PROJECT, bool CULL_BACKFACES>
#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_32BIT
	inline bool Triangle2D(NVMath::vec4 rV0, NVMath::vec4 rV1, NVMath::vec4 rV2, uint32 MinX = 0, uint32 MinY = 0, uint32 MaxX = 0, uint32 MaxY = 0, NVMath::vec4& VMinMax = NVMath::Vec4Zero(), NVMath::vec4& V210 = NVMath::Vec4Zero())
#else
	inline bool Triangle2D(NVMath::vec4 rV0, NVMath::vec4 rV1, NVMath::vec4 rV2, uint32 MinX = 0, uint32 MinY = 0, uint32 MaxX = 0, uint32 MaxY = 0, NVMath::vec4 VMinMax = NVMath::Vec4Zero (), NVMath::vec4 V210 = NVMath::Vec4Zero ())
#endif
	{
		using namespace NVMath;

		vec4 V0, V1, V2;
		if (PROJECT)
		{
			const vec4 WWW = Shuffle<xzww>(Shuffle<wwww>(rV0, rV1), rV2);
			const vec4 iWWW = Rcp(WWW);
			V0 = Mul(rV0, Splat<0>(iWWW));
			V1 = Mul(rV1, Splat<1>(iWWW));
			V2 = Mul(rV2, Splat<2>(iWWW));
			V210 = Sub(Shuffle<xyxy>(V1, V2), Swizzle<xyxy>(V0));
			vec4 Det = Mul(V210, Swizzle<wzwz>(V210));
			Det = Sub(Det, Splat<1>(Det));
			if (CULL_BACKFACES)
			{
				if ((SignMask(CmpLE(Det, Vec4Epsilon())) & BitX) != 0)
				{
					return false;
				}
			}

			Det = Select(Det, NVMath::Vec4(-FLT_EPSILON), CmpEq(Det, Vec4Zero()));
			V210 = Div(V210, Swizzle<xxxx>(Det));

			vec4 VMax = Max(Max(V0, V1), V2);
			vec4 VMin = Min(Min(V0, V1), V2);
			VMax = Add(VMax, Vec4One());
			VMinMax = Shuffle<xyxy>(VMin, VMax);
			VMinMax = Max(VMinMax, Vec4Zero());
			VMinMax = Min(VMinMax, m_VMaxXY);
			VMinMax = floatToint32(VMinMax);

			const uint32* pMM = reinterpret_cast<uint32*>(&VMinMax);
			MinX = pMM[0];
			MinY = pMM[1];
			MaxX = pMM[2];
			MaxY = pMM[3];
			if (MinX >= MaxX || MinY >= MaxY)
			{
				return false;
			}
		}
		else
		{
			V0 = rV0;
			V1 = rV1;
			V2 = rV2;
		}

		MinX &= ~3;

		VMinMax = And(VMinMax, MaskNot3);

#ifdef CULL_RENDERER_MINZ
		const vec4 VMinZ = Splat<2>(Min(Min(rV0, rV1), rV2));
#endif
		const vec4 V0z = Splat<2>(rV0);
		const vec4 Z10 = Sub(Splat<2>(rV1), V0z);
		const vec4 Z20 = Sub(Splat<2>(rV2), V0z);

		const vec4 X20 = Splat<0>(V210);
		const vec4 Y20 = Splat<1>(V210);
		const vec4 X10 = Sub(Vec4Zero(), Splat<2>(V210));
		const vec4 Y10 = Splat<3>(V210);

		VMinMax = Sub(int32Tofloat(VMinMax), V0);
		const vec4 dx4 = Add(Splat<0>(VMinMax), Vec4ZeroOneTwoThree());
		const vec4 Y1x = Mul(Y10, dx4);
		const vec4 Y2x = Sub(Vec4Zero(), Mul(Y20, dx4));
		vec4 dy4 = Splat<1>(VMinMax);
		const vec4 Y14 = Mul(Y10, Vec4Four());
		const vec4 Y24 = Sub(Vec4Zero(), Mul(Y20, Vec4Four()));
		const vec4 Y34 = Add(Y14, Y24);

		vec4 Visible = Vec4FFFFFFFF();
		uint16 y = MinY;
		do
		{
			vec4 Px = Madd(X10, dy4, Y1x);
			vec4 Py = Madd(X20, dy4, Y2x);
			vec4 Pz = Sub(Sub(Vec4One(), Py), Px);

			vec4* pDstZ = reinterpret_cast<vec4*>(&m_ZOutput[MinX + y * (uint16)SIZEX]);
			y++;
			uint16 x = MinX;
			do
			{
				Prefetch<ECL_LVL1>(pDstZ);
				x += 4;
				vec4 Mask = Or(Or(Px, Py), Pz);
				vec4 Z, rZ = *pDstZ;
#ifdef CULL_RENDERER_MINZ
				if (!WRITE)                       //compile time
				{
					Mask = Or(Mask, CmpLE(rZ, VMinZ));
				}
				else
#endif
				{
					Z = Madd(Z10, Px, Madd(Z20, Py, V0z));
					Mask = Or(Mask, CmpLE(rZ, Z));
				}
				Px = Add(Px, Y14);
				Py = Add(Py, Y24);
				Pz = Sub(Pz, Y34);
				if (CULL)                       //compile time
				{
					Visible = And(Visible, Mask);
				}
				if (WRITE)                      //compile time
				{
					*pDstZ = SelectSign(Z, rZ, Mask);
				}
				pDstZ++;
			}
			while (x < MaxX);

			if (!WRITE && CULL && (SignMask(Visible) & (BitX | BitY | BitZ | BitW)) != (BitX | BitY | BitZ | BitW))
			{
				return true;
			}

			dy4 = Add(dy4, Vec4One());
		}
		while (y < MaxY);

		return CULL && (SignMask(Visible) & (BitX | BitY | BitZ | BitW)) != (BitX | BitY | BitZ | BitW);
	}

	inline bool Quad2D(const NVMath::vec4& rV0, const NVMath::vec4& rV1, const NVMath::vec4& rV3, const NVMath::vec4& rV2)
	{
		using namespace NVMath;
		const vec4 WWW = Shuffle<xzxz>(Shuffle<wwww>(rV0, rV1), Shuffle<wwww>(rV2, rV3));
		const vec4 iWWW = Rcp(WWW);

		vec4 V0 = Mul(rV0, Splat<0>(iWWW));
		vec4 V1 = Mul(rV1, Splat<1>(iWWW));
		vec4 V2 = Mul(rV2, Splat<2>(iWWW));
		vec4 V3 = Mul(rV3, Splat<3>(iWWW));

		vec4 V210 = Sub(Shuffle<xyxy>(V1, V2), Swizzle<xyxy>(V0));
		vec4 V213 = Sub(Shuffle<xyxy>(V1, V2), Swizzle<xyxy>(V3));
		vec4 Det = Mul(V210, Swizzle<wzwz>(V210));
		Det = Sub(Det, Splat<1>(Det));

		vec4 VMax = Max(Max(V0, V1), Max(V2, V3));
		vec4 VMin = Min(Min(V0, V1), Min(V2, V3));
		VMax = Add(VMax, Vec4One());
		//saturate to 0 - ScreenSize cause it's assigned to uin16
		VMin = Min(VMin, m_VMaxXY);
		VMax = Min(VMax, m_VMaxXY);

		vec4 VMinMax = floatToint32(Max(Shuffle<xyxy>(VMin, VMax), Vec4Zero()));
		uint16 MinX = Vec4int32(VMinMax, 0);
		const uint16 MinY = Vec4int32(VMinMax, 1);
		const uint16 MaxX = Vec4int32(VMinMax, 2);
		const uint16 MaxY = Vec4int32(VMinMax, 3);
		if (MinX >= MaxX || MinY >= MaxY)
		{
			return false;
		}
		MinX &= ~3;

		const vec4 VMinZ = Splat<2>(Min(Min(rV0, rV1), Min(rV2, rV3)));
		Det = Rcp(Splat<0>(Det));
		V210 = Mul(V210, Det);
		V213 = Mul(V213, Det);
		const vec4 X20 = Splat<0>(V210);
		const vec4 Y20 = Splat<1>(V210);
		const vec4 X10 = Splat<2>(V210);
		const vec4 Y10 = Splat<3>(V210);
		const vec4 X23 = Splat<0>(V213);
		const vec4 Y23 = Splat<1>(V213);
		const vec4 X13 = Splat<2>(V213);
		const vec4 Y13 = Splat<3>(V213);

		const vec4 dx4 = Sub(Add(NVMath::Vec4(static_cast<float>(MinX)), Vec4ZeroOneTwoThree()), Splat<0>(V0));
		const vec4 Y10x = Mul(Y10, dx4);
		const vec4 Y20x = Mul(Y20, dx4);
		const vec4 Y13x = Mul(Y13, dx4);
		const vec4 Y23x = Mul(Y23, dx4);
		vec4 dy4 = Sub(NVMath::Vec4(static_cast<float>(MinY)), Splat<1>(V0));
		const vec4 Y104 = Mul(Y10, Vec4Four());
		const vec4 Y204 = Mul(Y20, Vec4Four());
		const vec4 Y134 = Mul(Y13, Vec4Four());
		const vec4 Y234 = Mul(Y23, Vec4Four());
		const vec4 Y304 = Sub(Y104, Y204);
		const vec4 Y334 = Sub(Y134, Y234);

		vec4 Visible = Vec4FFFFFFFF();
		uint16 y = MinY;
		do
		{
			vec4 P0x = Sub(Y10x, Mul(X10, dy4));
			vec4 P0y = Sub(Mul(X20, dy4), Y20x);
			vec4 P3x = Sub(Y13x, Mul(X13, dy4));
			vec4 P3y = Sub(Mul(X23, dy4), Y23x);
			uint16 x = MinX;
			vec4* pDstZ = reinterpret_cast<vec4*>(&m_ZOutput[MinX + y * (uint16)SIZEX]);
			do
			{
				Prefetch<ECL_LVL1>(pDstZ);
				vec4 Mask = Or(Or(P0x, P0y), Or(P3x, P3y));
				vec4 rZ = *pDstZ++;
				Mask = Or(Mask, CmpLE(rZ, VMinZ));
				x += 4;
				Visible = And(Visible, Mask);
				P0x = Add(P0x, Y104);
				P0y = Sub(P0y, Y204);
				P3x = Add(P3x, Y134);
				P3y = Sub(P3y, Y234);
			}
			while (x < MaxX);

			if (SignMask(Visible) != (BitX | BitY | BitZ | BitW))
			{
				return true;
			}

			y++;
			dy4 = Add(dy4, Vec4One());
		}
		while (y < MaxY);

		return false;
	}

	void Show();
public:

	// Note: Arrays are not initialized for performance reasons
	// cppcheck-suppress uninitMemberVar
	inline CCullRenderer()
	{
		m_ZInput = m_ZBufferMainMemory;
		m_ZOutput = m_ZBufferMainMemory;
		m_DebugRender = 0;
		m_nNumWorker = 0;
		m_ZBufferSwap = NULL;
	}

	~CCullRenderer()
	{
		for (uint32 i = 0; i < m_nNumWorker; ++i)
		{
			CryModuleMemalignFree(m_ZBufferSwap[i]);
		}
		delete[] m_ZBufferSwap;
	}

	void Prepare()
	{
		if (m_nNumWorker)
		{
			return;
		}

		m_nNumWorker = gEnv->pJobManager->GetNumWorkerThreads();
		m_ZBufferSwap = new tdZexel*[m_nNumWorker];
		for (uint32 i = 0; i < m_nNumWorker; ++i)
		{
			m_ZBufferSwap[i] = (tdZexel*)CryModuleMemalign(sizeof(tdZexel) * SIZEX * SIZEY, 128);
		}
	}

	inline void Clear()
	{
		m_VMaxXY = NVMath::int32Tofloat(NVMath::Vec4(SIZEX, SIZEY, SIZEX, SIZEY));
		for (uint32 a = 0, S = SIZEX * SIZEY; a < S; a++)
		{
			m_ZOutput[a] = 9999999999.f;
		}
		m_DrawCall = 0;
		m_PolyCount = 0;
	}

	bool DownLoadHWDepthBuffer(float nearPlane, float farPlane, float nearestMax, float Bias)
	{
		m_VMaxXY = NVMath::int32Tofloat(NVMath::Vec4(SIZEX, SIZEY, SIZEX, SIZEY));

		tdZexel* pPinned = gEnv->pRenderer->PinOcclusionBuffer(m_Reproject);
		if (!pPinned)
		{
			m_ZInput = m_ZBufferMainMemory;
			return false;
		}

		m_ZInput = pPinned;
		for (uint32 i = 0; i < m_nNumWorker; ++i)
		{
			memset(m_ZBufferSwap[i], 0, SIZEX * SIZEY * sizeof(float));
		}
		memset(m_ZBufferSwapMerged, 0, SIZEX * SIZEY * sizeof(float));

		return true;
	}

	void DetachHWDepthBuffer()
	{
		if (m_ZInput != m_ZBufferMainMemory)
		{
			gEnv->pRenderer->UnpinOcclusionBuffer();
			m_ZInput = m_ZBufferMainMemory;
		}
	}

	void ReprojectHWDepthBuffer(const Matrix44A& rCurrent, float nearPlane, float farPlane, float nearestMax, float Bias, int nStartLine, int nNumLines)
	{
		//#define USE_W_DEPTH
		//#define SCALE_DEPTH

		uint32 nWorkerThreadID = JobManager::GetWorkerThreadId();
		float* pZBufferSwap = m_ZBufferSwap[nWorkerThreadID];

		int sizeX = SIZEX;
		int sizeY = SIZEY;

		float fWidth = (float) sizeX;
		float fHeight = (float) sizeY;

		const float a = farPlane / (farPlane - nearPlane);
		const float b = farPlane * nearPlane / (nearPlane - farPlane);

		Matrix44A fromScreen;
		fromScreen.SetIdentity();
		fromScreen.SetTranslation(Vec3(-1.0f + 0.5f / fWidth, 1.0f - 0.5f / fHeight, 0.0f));
		fromScreen.m00 = 2.0f / fWidth;
		fromScreen.m11 = -2.0f / fHeight;  // Y flipped
		fromScreen.Transpose();

		Matrix44A Reproject = m_Reproject;
		Reproject.Invert();
		Matrix44A mToWorld = fromScreen * Reproject;

		{
			int x, y;
			float fY;
			using namespace NVMath;

#ifdef USE_W_DEPTH
			Matrix44A mReproject = mToWorld * rCurrent;
			const vec4 MR0 = reinterpret_cast<vec4*>(&mReproject)[0];
			const vec4 MR1 = reinterpret_cast<vec4*>(&mReproject)[1];
			const vec4 MR2 = reinterpret_cast<vec4*>(&mReproject)[2];
			const vec4 MR3 = reinterpret_cast<vec4*>(&mReproject)[3];

			const vec4 vA = NVMath::Vec4(a);
			const vec4 vB = NVMath::Vec4(b);
#else
			const vec4 MW0 = reinterpret_cast<vec4*>(&mToWorld)[0];
			const vec4 MW1 = reinterpret_cast<vec4*>(&mToWorld)[1];
			const vec4 MW2 = reinterpret_cast<vec4*>(&mToWorld)[2];
			const vec4 MW3 = reinterpret_cast<vec4*>(&mToWorld)[3];

			const vec4 MS0 = reinterpret_cast<const vec4*>(&rCurrent)[0];
			const vec4 MS1 = reinterpret_cast<const vec4*>(&rCurrent)[1];
			const vec4 MS2 = reinterpret_cast<const vec4*>(&rCurrent)[2];
			const vec4 MS3 = reinterpret_cast<const vec4*>(&rCurrent)[3];
#endif

			const vec4 vXOffsets = NVMath::Vec4(0.0f, 1.0f, 2.0f, 3.0f);
			const vec4 vXIncrement = NVMath::Vec4(4.0f);

			const float nearestLinear = b / (nearestMax - a);
			const vec4 vfEpsilon = NVMath::Vec4Epsilon();
			const vec4 vfOne = NVMath::Vec4One();
			const vec4 vZero = NVMath::Vec4Zero();

			vec4* pSrcZ = reinterpret_cast<vec4*>(&m_ZInput[nStartLine * sizeX]);

			// Disable fp exceptions in the loop
			CScopedFloatingPointException fpExceptionScope(eFPE_None);
			for (y = nStartLine, fY = static_cast<float>(nStartLine); y < nStartLine + nNumLines; y++, fY += 1.0f)
			{
				const vec4 vYYYY = NVMath::Vec4(fY);

				vec4 vXCoords = vXOffsets;

				for (x = 0; x < sizeX; x += 4)
				{
					const vec4 vNonLinearDepth = *pSrcZ;

					vec4 vXXXX[4];
					vXXXX[0] = Splat<0>(vXCoords);
					vXXXX[1] = Splat<1>(vXCoords);
					vXXXX[2] = Splat<2>(vXCoords);
					vXXXX[3] = Splat<3>(vXCoords);

					vec4 vZZZZ[4];
					vZZZZ[0] = Splat<0>(vNonLinearDepth);
					vZZZZ[1] = Splat<1>(vNonLinearDepth);
					vZZZZ[2] = Splat<2>(vNonLinearDepth);
					vZZZZ[3] = Splat<3>(vNonLinearDepth);

					for (int i = 0; i < 4; i++)
					{
#ifdef USE_W_DEPTH
						vec4 vScreenPos = Madd(MR0, vXXXX[i], Madd(MR1, vYYYY, Madd(MR2, vZZZZ[i], MR3)));

						vec4 vScreenPosH = Div(vScreenPos, Splat<3>(vScreenPos));

						vec4 vNewDepth = Div(vB, Sub(Splat<2>(vScreenPosH), vA));

						float newDepth = Vec4float<2>(vNewDepth);
#else
						vec4 vWorldPos = Madd(MW0, vXXXX[i], Madd(MW1, vYYYY, Madd(MW2, vZZZZ[i], MW3)));

						vec4 vWorldPosH = Div(vWorldPos, Max(Splat<3>(vWorldPos), vfEpsilon));

						vec4 vScreenPos = Madd(MS0, Splat<0>(vWorldPosH), Madd(MS1, Splat<1>(vWorldPosH), Madd(MS2, Splat<2>(vWorldPosH), MS3)));

						vec4 vNewDepth = Splat<2>(vScreenPos);

						vec4 vScreenPosH = Div(vScreenPos, Max(Splat<3>(vScreenPos), vfEpsilon));

						float newDepth = Vec4float<2>(vNewDepth);
#endif
						// It is faster to use simple non-vectorized code to write the depth in the buffer
						// The comparison will always test false if newDepth is NaN
						if (newDepth > 0.f)
						{
							int X;
							int Y;
							if (Vec4float<0>(vZZZZ[i]) < nearestMax)
							{
								X = x + i;
								Y = y;
								newDepth = nearestLinear;
							}
							else
							{
								vec4 vFinalScreenPosU = floatToint32(vScreenPosH);

								X = Vec4int32<0>(vFinalScreenPosU);
								Y = Vec4int32<1>(vFinalScreenPosU);
							}

							if (X >= 0 && Y >= 0 && X < sizeX && Y < sizeY)
							{
								float* pDstZ = &pZBufferSwap[X + (Y * sizeX)];
								float depth = *pDstZ;

								depth = depth <= 0.f ? farPlane : depth;
								*pDstZ = min(depth, newDepth);
							}
						}
					}
					vXCoords = Add(vXIncrement, vXCoords);

					pSrcZ++;
				}
			}
		}
	}

	void MergeReprojectHWDepthBuffer(int nStartLine, int nNumLines)
	{
		const int sizeX = SIZEX;
		using namespace NVMath;

		const vec4 zero = Vec4Zero();

		for (uint32 i = 0; i < m_nNumWorker; ++i)
		{
			for (int y = nStartLine; y < nStartLine + nNumLines; y++)
			{
				for (int x = 0; x < sizeX; x += 4)
				{
					vec4* pDstZ = reinterpret_cast<vec4*>(&m_ZBufferSwapMerged[x + (y * sizeX)]);
					vec4 vDstZ = *pDstZ;

					vec4* pSrcZ = reinterpret_cast<vec4*>(&m_ZBufferSwap[i][x + (y * sizeX)]);
					vec4 vSrcZ = *pSrcZ;

					// remove zeros so Min doesn't select them
					vDstZ = Select(vDstZ, vSrcZ, CmpLE(vDstZ, zero));
					vSrcZ = Select(vSrcZ, vDstZ, CmpLE(vSrcZ, zero));

					const vec4 vNewDepth = Min(vSrcZ, vDstZ);

					*pDstZ = vNewDepth;
				}
			}
		}
	}

	void ReprojectHWDepthBufferAfterMerge(const Matrix44A& rCurrent, float nearPlane, float farPlane, float nearestMax, float Bias, int nStartLine, int nNumLines)
	{
		using namespace NVMath;
		int sizeX = SIZEX;
		int sizeY = SIZEY;
		const vec4 vFarPlane = NVMath::Vec4(farPlane);

		float* pZBufferSwap = m_ZBufferSwapMerged;
		vec4* pSwap = reinterpret_cast<vec4*>(&pZBufferSwap[0]);
		vec4* pDst = reinterpret_cast<vec4*>(&m_ZOutput[nStartLine * sizeX]);

		const vec4 vBiasAdd = NVMath::Vec4(Bias < 0.f ? -Bias : 0.f);
		const vec4 vBiasMul = NVMath::Vec4(Bias > 0.f ? Bias : 0.f);
		const int pitchX = SIZEX / 4;

		vec4 zero = Vec4Zero();

		for (int y = nStartLine; y < nStartLine + nNumLines; y++)
		{
			int minY = max((int)0, (int)y - 1);
			int maxY = min((int)sizeY - 1, (int)y + 1);
			int maxX = min(pitchX - 1, 0 + 1);

			vec4 src[3];
			vec4 srcMax[3];
			vec4 srcCenter;

			// left, no data available yet
			srcMax[0] = zero;

			// center
			src[0] = pSwap[0 + minY * pitchX];
			src[1] = pSwap[0 + y * pitchX];
			src[2] = pSwap[0 + maxY * pitchX];
			srcMax[1] = Max(Max(src[0], src[1]), src[2]);
			srcCenter = src[1];

			// right
			src[0] = pSwap[maxX + minY * pitchX];
			src[1] = pSwap[maxX + y * pitchX];
			src[2] = pSwap[maxX + maxY * pitchX];
			srcMax[2] = Max(Max(src[0], src[1]), src[2]);

			int vecX = 0;
			for (int x = 0; x < sizeX; x += 4) //todo, fix edge cases
			{
				vec4 vDst;
				vec4 vSrcIsZero = CmpLE(srcCenter, zero);

				// 0
				{
					vec4 vLeft, vCenter;
					vLeft = SelectStatic<0x8>(zero, srcMax[0]);
					vCenter = SelectStatic<0x3>(zero, srcMax[1]);

					vec4 _vMax;
					_vMax = Max(vLeft, vCenter);
					_vMax = Max(_vMax, Swizzle<zwxy>(_vMax));
					_vMax = Max(_vMax, Swizzle<wzyx>(_vMax));

					vDst = _vMax;
				}

				// 1
				{
					vec4 vCenter;

					vCenter = SelectStatic<0x7>(zero, srcMax[1]);

					vec4 _vMax;
					_vMax = Max(vCenter, Swizzle<zwxy>(vCenter));
					_vMax = Max(_vMax, Swizzle<wzyx>(_vMax));

					vDst = SelectStatic<0x2>(vDst, _vMax);
				}

				// 2
				{
					vec4 vCenter;

					vCenter = SelectStatic<0xE>(zero, srcMax[1]);

					vec4 _vMax;
					_vMax = Max(vCenter, Swizzle<zwxy>(vCenter));
					_vMax = Max(_vMax, Swizzle<wzyx>(_vMax));

					vDst = SelectStatic<0x4>(vDst, _vMax);
				}

				// 3
				{
					vec4 vRight, vCenter;

					vRight = SelectStatic<0x1>(zero, srcMax[2]);
					vCenter = SelectStatic<0xC>(zero, srcMax[1]);

					vec4 _vMax;
					_vMax = Max(vRight, vCenter);

					_vMax = Max(_vMax, Swizzle<zwxy>(_vMax));
					_vMax = Max(_vMax, Swizzle<wzyx>(_vMax));

					vDst = SelectStatic<0x8>(vDst, _vMax);
				}

				vec4 vDstIsZero = CmpLE(vDst, zero);
				vDst = Select(vDst, vFarPlane, vDstIsZero);

				vDst = Select(srcCenter, vDst, vSrcIsZero);

				vDst = Add(vDst, vBiasAdd);  //linear bias
				vDst = Add(vDst, Madd(vBiasMul, vDst, vBiasMul));// none-linear bias
#ifdef SCALE_DEPTH
				//*pDst = Mul(vDst, NVMath::Vec4(1.2f));
				*pDst = Add(vDst, NVMath::Vec4(0.5f));
#else
				*pDst = vDst;
#endif

				//next loop
				++pDst;
				++vecX;

				// shift to the left
				srcMax[0] = srcMax[1];
				srcMax[1] = srcMax[2];
				srcCenter = src[1];

				// load right data
				maxX = min(pitchX - 1, vecX + 1);
				src[0] = pSwap[maxX + minY * pitchX];
				src[1] = pSwap[maxX + y * pitchX];
				src[2] = pSwap[maxX + maxY * pitchX];
				srcMax[2] = Max(Max(src[0], src[1]), src[2]);
			}
		}

		//for(int a=0;a<128;a+=16)
		//	printf("%2.2f %2.2f %2.2f %2.2f %2.2f %2.2f %2.2f %2.2f %2.2f %2.2f %2.2f %2.2f %2.2f %2.2f %2.2f %2.2f\n",
		//	m_ZBuffer[a+0],m_ZBuffer[a+1],m_ZBuffer[a+2],m_ZBuffer[a+3],
		//	m_ZBuffer[a+4],m_ZBuffer[a+5],m_ZBuffer[a+6],m_ZBuffer[a+7],
		//	m_ZBuffer[a+8],m_ZBuffer[a+9],m_ZBuffer[a+10],m_ZBuffer[a+11],
		//	m_ZBuffer[a+12],m_ZBuffer[a+13],m_ZBuffer[a+14],m_ZBuffer[a+15]);

#ifdef CULL_RENDERER_REPROJ_DEBUG
		memcpy(&pZBufferSwap[nStartLine * sizeX], &m_ZOutput[nStartLine * sizeX], sizeX * nNumLines * sizeof(float));
#endif

#ifdef SCALE_DEPTH
	#undef SCALE_DEPTH
#endif

#ifdef USE_W_DEPTH
	#undef USE_W_DEPTH
#endif

	}

	inline int AABBInFrustum(const NVMath::vec4* pViewProj, Vec3 Min, Vec3 Max, Vec3 ViewPos)
	{
		using namespace NVMath;
		const NVMath::vec4 M0 = pViewProj[0];
		const NVMath::vec4 M1 = pViewProj[1];
		const NVMath::vec4 M2 = pViewProj[2];
		const NVMath::vec4 M3 = pViewProj[3];
		const NVMath::vec4 MinX = NVMath::Vec4(Min.x);
		const NVMath::vec4 MinY = NVMath::Vec4(Min.y);
		const NVMath::vec4 MinZ = NVMath::Vec4(Min.z);
		const NVMath::vec4 MaxX = NVMath::Vec4(Max.x);
		const NVMath::vec4 MaxY = NVMath::Vec4(Max.y);
		const NVMath::vec4 MaxZ = NVMath::Vec4(Max.z);

		vec4 VB0 = Madd(MinX, M0, Madd(MinY, M1, Madd(MinZ, M2, M3)));
		vec4 VB1 = Madd(MinX, M0, Madd(MaxY, M1, Madd(MinZ, M2, M3)));
		vec4 VB2 = Madd(MaxX, M0, Madd(MinY, M1, Madd(MinZ, M2, M3)));
		vec4 VB3 = Madd(MaxX, M0, Madd(MaxY, M1, Madd(MinZ, M2, M3)));
		vec4 VB4 = Madd(MinX, M0, Madd(MinY, M1, Madd(MaxZ, M2, M3)));
		vec4 VB5 = Madd(MinX, M0, Madd(MaxY, M1, Madd(MaxZ, M2, M3)));
		vec4 VB6 = Madd(MaxX, M0, Madd(MinY, M1, Madd(MaxZ, M2, M3)));
		vec4 VB7 = Madd(MaxX, M0, Madd(MaxY, M1, Madd(MaxZ, M2, M3)));
		vec4 SMask = And(And(And(VB0, VB1), And(VB2, VB3)), And(Or(VB4, VB5), And(VB6, VB7)));
		if (SignMask(SMask) & BitZ)
		{
			return 0;
		}

		int Visible = 3;

		SMask = Or(Or(Or(VB0, VB1), Or(VB2, VB3)), Or(Or(VB4, VB5), Or(VB6, VB7)));
		if ((SignMask(SMask) & BitZ) == 0)
		{
			VB0 = Div(VB0, Splat<3>(VB0));
			VB1 = Div(VB1, Splat<3>(VB1));
			VB2 = Div(VB2, Splat<3>(VB2));
			VB3 = Div(VB3, Splat<3>(VB3));
			VB4 = Div(VB4, Splat<3>(VB4));
			VB5 = Div(VB5, Splat<3>(VB5));
			VB6 = Div(VB6, Splat<3>(VB6));
			VB7 = Div(VB7, Splat<3>(VB7));
			const vec4 VC0 = Madd(VB0, NVMath::Vec4(-1.f), m_VMaxXY);
			const vec4 VC1 = Madd(VB1, NVMath::Vec4(-1.f), m_VMaxXY);
			const vec4 VC2 = Madd(VB2, NVMath::Vec4(-1.f), m_VMaxXY);
			const vec4 VC3 = Madd(VB3, NVMath::Vec4(-1.f), m_VMaxXY);
			const vec4 VC4 = Madd(VB4, NVMath::Vec4(-1.f), m_VMaxXY);
			const vec4 VC5 = Madd(VB5, NVMath::Vec4(-1.f), m_VMaxXY);
			const vec4 VC6 = Madd(VB6, NVMath::Vec4(-1.f), m_VMaxXY);
			const vec4 VC7 = Madd(VB7, NVMath::Vec4(-1.f), m_VMaxXY);
			const vec4 SMaskB = And(And(And(VB0, VB1), And(VB2, VB3)), And(And(VB4, VB5), And(VB6, VB7)));
			const vec4 SMaskC = And(And(And(VC0, VC1), And(VC2, VC3)), And(And(VC4, VC5), And(VC6, VC7)));
			if ((SignMask(SMaskB) & (BitX | BitY)) || (SignMask(SMaskC) & (BitX | BitY)))
			{
				return 0;
			}
			Visible = 1;
		}
		//return true;
		if (Max.x < ViewPos.x)
		{
			if (Triangle<false, true, true>(VB3, VB2, VB7))
			{
				return Visible;    //MaxX
			}
			if (Triangle<false, true, true>(VB7, VB2, VB6))
			{
				return Visible;
			}
			Visible &= ~1;
		}
		else if (Min.x > ViewPos.x)
		{
			if (Triangle<false, true, true>(VB0, VB1, VB4))
			{
				return Visible;    //MinX
			}
			if (Triangle<false, true, true>(VB4, VB1, VB5))
			{
				return Visible;
			}
			Visible &= ~1;
		}
		if (Max.y < ViewPos.y)
		{
			if (Triangle<false, true, true>(VB1, VB3, VB5))
			{
				return Visible | 1;    //MaxY
			}
			if (Triangle<false, true, true>(VB5, VB3, VB7))
			{
				return Visible | 1;
			}
			Visible &= ~1;
		}
		else if (Min.y > ViewPos.y)
		{
			if (Triangle<false, true, true>(VB2, VB0, VB6))
			{
				return Visible | 1;    //MinY
			}
			if (Triangle<false, true, true>(VB6, VB0, VB4))
			{
				return Visible | 1;
			}
			Visible &= ~1;
		}
		if (Max.z < ViewPos.z)
		{
			if (Triangle<false, true, true>(VB4, VB5, VB6))
			{
				return Visible | 1;    //MaxZ
			}
			if (Triangle<false, true, true>(VB6, VB5, VB7))
			{
				return Visible | 1;
			}
			Visible = 0;
		}
		else if (Min.z > ViewPos.z)
		{
			if (Triangle<false, true, true>(VB1, VB0, VB3))
			{
				return Visible | 1;    //MinZ
			}
			if (Triangle<false, true, true>(VB3, VB0, VB2))
			{
				return Visible | 1;
			}
			Visible = 0;
		}
		return Visible & (Visible << 1);
	}

	inline bool TestQuad(const NVMath::vec4* pViewProj, const Vec3& vCenter, const Vec3& vAxisX, const Vec3& vAxisY)
	{
		const NVMath::vec4 M0 = pViewProj[0];
		const NVMath::vec4 M1 = pViewProj[1];
		const NVMath::vec4 M2 = pViewProj[2];
		const NVMath::vec4 M3 = pViewProj[3];

		const Vec3 v0 = vCenter - vAxisX - vAxisY;
		const Vec3 v1 = vCenter - vAxisX + vAxisY;
		const Vec3 v2 = vCenter + vAxisX + vAxisY;
		const Vec3 v3 = vCenter + vAxisX - vAxisY;

		const NVMath::vec4 VB0 = NVMath::Madd(NVMath::Vec4(v0.x), M0, NVMath::Madd(NVMath::Vec4(v0.y), M1, NVMath::Madd(NVMath::Vec4(v0.z), M2, M3)));
		const NVMath::vec4 VB1 = NVMath::Madd(NVMath::Vec4(v1.x), M0, NVMath::Madd(NVMath::Vec4(v1.y), M1, NVMath::Madd(NVMath::Vec4(v1.z), M2, M3)));
		const NVMath::vec4 VB2 = NVMath::Madd(NVMath::Vec4(v2.x), M0, NVMath::Madd(NVMath::Vec4(v2.y), M1, NVMath::Madd(NVMath::Vec4(v2.z), M2, M3)));
		const NVMath::vec4 VB3 = NVMath::Madd(NVMath::Vec4(v3.x), M0, NVMath::Madd(NVMath::Vec4(v3.y), M1, NVMath::Madd(NVMath::Vec4(v3.z), M2, M3)));

		// Note: Explicitly disabling backface culling here
		if (Triangle<false, true, false>(VB2, VB0, VB3))
		{
			return true;
		}
		if (Triangle<false, true, false>(VB1, VB0, VB2))
		{
			return true;
		}

		return false;
	}

	inline bool TestAABB(const NVMath::vec4* pViewProj, Vec3 Min, Vec3 Max, Vec3 ViewPos)
	{
		using namespace NVMath;
		const NVMath::vec4 M0 = pViewProj[0];
		const NVMath::vec4 M1 = pViewProj[1];
		const NVMath::vec4 M2 = pViewProj[2];
		const NVMath::vec4 M3 = pViewProj[3];
		const NVMath::vec4 MinX = NVMath::Vec4(Min.x);
		const NVMath::vec4 MinY = NVMath::Vec4(Min.y);
		const NVMath::vec4 MinZ = NVMath::Vec4(Min.z);
		const NVMath::vec4 MaxX = NVMath::Vec4(Max.x);
		const NVMath::vec4 MaxY = NVMath::Vec4(Max.y);
		const NVMath::vec4 MaxZ = NVMath::Vec4(Max.z);

		const vec4 VB0 = Madd(MinX, M0, Madd(MinY, M1, Madd(MinZ, M2, M3)));
		const vec4 VB1 = Madd(MinX, M0, Madd(MaxY, M1, Madd(MinZ, M2, M3)));
		const vec4 VB2 = Madd(MaxX, M0, Madd(MinY, M1, Madd(MinZ, M2, M3)));
		const vec4 VB3 = Madd(MaxX, M0, Madd(MaxY, M1, Madd(MinZ, M2, M3)));
		const vec4 VB4 = Madd(MinX, M0, Madd(MinY, M1, Madd(MaxZ, M2, M3)));
		const vec4 VB5 = Madd(MinX, M0, Madd(MaxY, M1, Madd(MaxZ, M2, M3)));
		const vec4 VB6 = Madd(MaxX, M0, Madd(MinY, M1, Madd(MaxZ, M2, M3)));
		const vec4 VB7 = Madd(MaxX, M0, Madd(MaxY, M1, Madd(MaxZ, M2, M3)));
		vec4 SMask = Or(Or(Or(VB0, VB1), Or(VB2, VB3)), Or(Or(VB4, VB5), Or(VB6, VB7)));

		if (SignMask(SMask) & BitZ)
		{
			if (Max.x < ViewPos.x)
			{
				if (Triangle<false, true, true>(VB3, VB2, VB7))
				{
					return true;    //MaxX
				}
				if (Triangle<false, true, true>(VB7, VB2, VB6))
				{
					return true;
				}
			}
			if (Min.x > ViewPos.x)
			{
				if (Triangle<false, true, true>(VB0, VB1, VB4))
				{
					return true;    //MinX
				}
				if (Triangle<false, true, true>(VB4, VB1, VB5))
				{
					return true;
				}
			}
			if (Max.y < ViewPos.y)
			{
				if (Triangle<false, true, true>(VB1, VB3, VB5))
				{
					return true;    //MaxY
				}
				if (Triangle<false, true, true>(VB5, VB3, VB7))
				{
					return true;
				}
			}
			if (Min.y > ViewPos.y)
			{
				if (Triangle<false, true, true>(VB2, VB0, VB6))
				{
					return true;    //MinY
				}
				if (Triangle<false, true, true>(VB6, VB0, VB4))
				{
					return true;
				}
			}
			if (Max.z < ViewPos.z)
			{
				if (Triangle<false, true, true>(VB4, VB5, VB6))
				{
					return true;    //MaxZ
				}
				if (Triangle<false, true, true>(VB6, VB5, VB7))
				{
					return true;
				}
			}
			if (Min.z > ViewPos.z)
			{
				if (Triangle<false, true, true>(VB1, VB0, VB3))
				{
					return true;    //MinZ
				}
				if (Triangle<false, true, true>(VB3, VB0, VB2))
				{
					return true;
				}
			}
		}
		else
		{
			if (Max.x < ViewPos.x)
			{
				//if(Quad2D(VB3,VB2,VB6,VB7))return true;
				if (Triangle2D<false, true, true, true>(VB3, VB2, VB7))
				{
					return true;
				}
				if (Triangle2D<false, true, true, true>(VB7, VB2, VB6))
				{
					return true;
				}
			}
			if (Min.x > ViewPos.x)
			{
				//if(Quad2D(VB0,VB1,VB5,VB4))return true;
				if (Triangle2D<false, true, true, true>(VB0, VB1, VB4))
				{
					return true;
				}
				if (Triangle2D<false, true, true, true>(VB4, VB1, VB5))
				{
					return true;
				}
			}
			if (Max.y < ViewPos.y)
			{
				//if(Quad2D(VB1,VB3,VB7,VB5))return true;
				if (Triangle2D<false, true, true, true>(VB1, VB3, VB5))
				{
					return true;
				}
				if (Triangle2D<false, true, true, true>(VB5, VB3, VB7))
				{
					return true;
				}
			}
			if (Min.y > ViewPos.y)
			{
				//if(Quad2D(VB2,VB0,VB4,VB6))return true;
				if (Triangle2D<false, true, true, true>(VB2, VB0, VB6))
				{
					return true;
				}
				if (Triangle2D<false, true, true, true>(VB6, VB0, VB4))
				{
					return true;
				}
			}
			if (Max.z < ViewPos.z)
			{
				//if(Quad2D(VB4,VB5,VB7,VB6))return true;
				if (Triangle2D<false, true, true, true>(VB4, VB5, VB6))
				{
					return true;
				}
				if (Triangle2D<false, true, true, true>(VB6, VB5, VB7))
				{
					return true;
				}
			}
			if (Min.z > ViewPos.z)
			{
				//if(Quad2D(VB1,VB0,VB2,VB3))return true;
				if (Triangle2D<false, true, true, true>(VB1, VB0, VB3))
				{
					return true;
				}
				if (Triangle2D<false, true, true, true>(VB3, VB0, VB2))
				{
					return true;
				}
			}
		}
		return false;
	}

	template<bool NEEDCLIPPING>
	inline void Rasterize(const NVMath::vec4* pViewProj, const NVMath::vec4* __restrict pTriangles, size_t TriCount)
	{
		using namespace NVMath;
		Prefetch<ECL_LVL1>(pTriangles);
		m_DrawCall++;
		m_PolyCount += TriCount;

		const vec4 M0 = pViewProj[0];
		const vec4 M1 = pViewProj[1];
		const vec4 M2 = pViewProj[2];
		const vec4 M3 = pViewProj[3];
		const size_t VCacheCount = 48;                       //16x3 vertices
		vec4 VTmp[VCacheCount];
		vec4 DetTmp[VCacheCount * 2 / 3];

		if (TriCount > 65535)
		{
			TriCount = 65535;
		}

		for (size_t a = 0, S = TriCount; a < S; a += VCacheCount)
		{
			vec4 ZMask = Vec4Zero();
			const size_t VTmpCount = VCacheCount + a > TriCount ? TriCount - a : VCacheCount;
			vec4* pVTmp = VTmp;
			for (size_t b = 0; b < VTmpCount; b += 3, pVTmp += 3, pTriangles += 3)
			{
				Prefetch<ECL_LVL1>(pTriangles + 48);

				const vec4 VA = reinterpret_cast<const vec4*>(pTriangles)[0];
				const vec4 VB = reinterpret_cast<const vec4*>(pTriangles)[1];
				const vec4 VC = reinterpret_cast<const vec4*>(pTriangles)[2];

				const vec4 V0 = Madd(Splat<0>(VA), M0, Madd(Splat<1>(VA), M1, Madd(Splat<2>(VA), M2, M3)));
				const vec4 V1 = Madd(Splat<0>(VB), M0, Madd(Splat<1>(VB), M1, Madd(Splat<2>(VB), M2, M3)));
				const vec4 V2 = Madd(Splat<0>(VC), M0, Madd(Splat<1>(VC), M1, Madd(Splat<2>(VC), M2, M3)));

				if (NEEDCLIPPING)
				{
					ZMask = Or(Or(ZMask, V0), Or(V1, V2));
				}

				pVTmp[0] = V0;
				pVTmp[1] = V1;
				pVTmp[2] = V2;
			}

			const uint32 Idx = SignMask(ZMask) & BitZ;
			if (NEEDCLIPPING && Idx == BitZ)
			{
				for (size_t b = 0; b < VTmpCount; b += 3)
				{
					Triangle<true, false, true>(VTmp[b], VTmp[b + 2], VTmp[b + 1]);
				}
			}
			else
			{
				const vec4 M = NVMath::Vec4(~0u, ~0u, 0u, ~0u);
				pVTmp = VTmp;
				vec4* pDetTmp = DetTmp;
				for (size_t b = 0; b < VTmpCount; b += 12, pVTmp += 12, pDetTmp += 8)
				{
					vec4 V0 = pVTmp[0];
					vec4 V1 = pVTmp[1];
					vec4 V2 = pVTmp[2];
					vec4 V3 = pVTmp[3];
					vec4 V4 = pVTmp[4];
					vec4 V5 = pVTmp[5];
					vec4 V6 = pVTmp[6];
					vec4 V7 = pVTmp[7];
					vec4 V8 = pVTmp[8];
					vec4 V9 = pVTmp[9];
					vec4 VA = pVTmp[10];
					vec4 VB = pVTmp[11];

					const vec4 W0123 = Shuffle<xzxz>(Shuffle<wwww>(V0, V1), Shuffle<wwww>(V2, V3));
					const vec4 W4567 = Shuffle<xzxz>(Shuffle<wwww>(V4, V5), Shuffle<wwww>(V6, V7));
					const vec4 W89AB = Shuffle<xzxz>(Shuffle<wwww>(V8, V9), Shuffle<wwww>(VA, VB));
					const vec4 iW0123 = Rcp(W0123);
					const vec4 iW4567 = Rcp(W4567);
					const vec4 iW89AB = Rcp(W89AB);
					const vec4 V0T = Mul(V0, Splat<0>(iW0123));
					const vec4 V1T = Mul(V1, Splat<1>(iW0123));
					const vec4 V2T = Mul(V2, Splat<2>(iW0123));
					const vec4 V3T = Mul(V3, Splat<3>(iW0123));
					const vec4 V4T = Mul(V4, Splat<0>(iW4567));
					const vec4 V5T = Mul(V5, Splat<1>(iW4567));
					const vec4 V6T = Mul(V6, Splat<2>(iW4567));
					const vec4 V7T = Mul(V7, Splat<3>(iW4567));
					const vec4 V8T = Mul(V8, Splat<0>(iW89AB));
					const vec4 V9T = Mul(V9, Splat<1>(iW89AB));
					const vec4 VAT = Mul(VA, Splat<2>(iW89AB));
					const vec4 VBT = Mul(VB, Splat<3>(iW89AB));

					V0 = SelectBits(V0, V0T, M);
					V1 = SelectBits(V1, V1T, M);
					V2 = SelectBits(V2, V2T, M);
					V3 = SelectBits(V3, V3T, M);
					V4 = SelectBits(V4, V4T, M);
					V5 = SelectBits(V5, V5T, M);
					V6 = SelectBits(V6, V6T, M);
					V7 = SelectBits(V7, V7T, M);
					V8 = SelectBits(V8, V8T, M);
					V9 = SelectBits(V9, V9T, M);
					VA = SelectBits(VA, VAT, M);
					VB = SelectBits(VB, VBT, M);

					vec4 V012 = Sub(Shuffle<xyxy>(V2T, V1T), Swizzle<xyxy>(V0T));
					vec4 V345 = Sub(Shuffle<xyxy>(V5T, V4T), Swizzle<xyxy>(V3T));
					vec4 V678 = Sub(Shuffle<xyxy>(V8T, V7T), Swizzle<xyxy>(V6T));
					vec4 V9AB = Sub(Shuffle<xyxy>(VBT, VAT), Swizzle<xyxy>(V9T));
					vec4 Det012 = Mul(V012, Swizzle<wzwz>(V012));
					vec4 Det345 = Mul(V345, Swizzle<wzwz>(V345));
					vec4 Det678 = Mul(V678, Swizzle<wzwz>(V678));
					vec4 Det9AB = Mul(V9AB, Swizzle<wzwz>(V9AB));

					Det012 = Sub(Det012, Splat<1>(Det012));
					Det345 = Sub(Det345, Splat<1>(Det345));
					Det678 = Sub(Det678, Splat<1>(Det678));
					Det9AB = Sub(Det9AB, Splat<1>(Det9AB));
					vec4 Det = Shuffle<xzxz>(Shuffle<xxxx>(Det012, Det345), Shuffle<xxxx>(Det678, Det9AB));

#if !CRY_PLATFORM_LINUX && !CRY_PLATFORM_ANDROID && !CRY_PLATFORM_APPLE //to avoid DivBy0 exception on PC
					Det = Select(Det, NVMath::Vec4(-FLT_EPSILON), CmpEq(Det, Vec4Zero()));
#endif
					Det = Rcp(Det);
					Det012 = Splat<0>(Det);
					Det345 = Splat<1>(Det);
					Det678 = Splat<2>(Det);
					Det9AB = Splat<3>(Det);

					vec4 VMax012 = Max(Max(V0T, V1T), V2T);
					vec4 VMax345 = Max(Max(V3T, V4T), V5T);
					vec4 VMax678 = Max(Max(V6T, V7T), V8T);
					vec4 VMax9AB = Max(Max(V9T, VAT), VBT);
					vec4 VMin012 = Min(Min(V0T, V1T), V2T);
					vec4 VMin345 = Min(Min(V3T, V4T), V5T);
					vec4 VMin678 = Min(Min(V6T, V7T), V8T);
					vec4 VMin9AB = Min(Min(V9T, VAT), VBT);
					VMax012 = Add(VMax012, Vec4One());
					VMax345 = Add(VMax345, Vec4One());
					VMax678 = Add(VMax678, Vec4One());
					VMax9AB = Add(VMax9AB, Vec4One());
					vec4 VMinMax012 = Shuffle<xyxy>(VMin012, VMax012);
					vec4 VMinMax345 = Shuffle<xyxy>(VMin345, VMax345);
					vec4 VMinMax678 = Shuffle<xyxy>(VMin678, VMax678);
					vec4 VMinMax9AB = Shuffle<xyxy>(VMin9AB, VMax9AB);
					VMinMax012 = Max(VMinMax012, Vec4Zero());
					VMinMax345 = Max(VMinMax345, Vec4Zero());
					VMinMax678 = Max(VMinMax678, Vec4Zero());
					VMinMax9AB = Max(VMinMax9AB, Vec4Zero());
					VMinMax012 = Min(VMinMax012, m_VMaxXY);
					VMinMax345 = Min(VMinMax345, m_VMaxXY);
					VMinMax678 = Min(VMinMax678, m_VMaxXY);
					VMinMax9AB = Min(VMinMax9AB, m_VMaxXY);
					VMinMax012 = floatToint32(VMinMax012);
					VMinMax345 = floatToint32(VMinMax345);
					VMinMax678 = floatToint32(VMinMax678);
					VMinMax9AB = floatToint32(VMinMax9AB);
					VMinMax012 = Or(VMinMax012, CmpLE(Det012, Vec4Zero()));                     //backface cull
					VMinMax345 = Or(VMinMax345, CmpLE(Det345, Vec4Zero()));
					VMinMax678 = Or(VMinMax678, CmpLE(Det678, Vec4Zero()));
					VMinMax9AB = Or(VMinMax9AB, CmpLE(Det9AB, Vec4Zero()));

					pVTmp[0] = V0;
					pVTmp[1] = V1;
					pVTmp[2] = V2;
					pVTmp[3] = V3;
					pVTmp[4] = V4;
					pVTmp[5] = V5;
					pVTmp[6] = V6;
					pVTmp[7] = V7;
					pVTmp[8] = V8;
					pVTmp[9] = V9;
					pVTmp[10] = VA;
					pVTmp[11] = VB;
					pDetTmp[0] = VMinMax012;
					pDetTmp[1] = Mul(V012, Det012);
					pDetTmp[2] = VMinMax345;
					pDetTmp[3] = Mul(V345, Det345);
					pDetTmp[4] = VMinMax678;
					pDetTmp[5] = Mul(V678, Det678);
					pDetTmp[6] = VMinMax9AB;
					pDetTmp[7] = Mul(V9AB, Det9AB);
				}

				pDetTmp = DetTmp;
				for (size_t b = 0; b < VTmpCount; b += 3, pDetTmp += 2)
				{
					const uint32* pMM = reinterpret_cast<uint32*>(pDetTmp);
					const uint16 MinX = pMM[0];
					const uint16 MinY = pMM[1];
					const uint16 MaxX = pMM[2];
					const uint16 MaxY = pMM[3];
					if (MinX < MaxX && MinY < MaxY)
					{
						Triangle2D<true, false, false, true>(VTmp[b], VTmp[b + 2], VTmp[b + 1], MinX, MinY, MaxX, MaxY, pDetTmp[0], pDetTmp[1]);
					}
				}
			}
		}

	}
	template<bool WRITE>
	inline bool Rasterize(const NVMath::vec4* pViewProj, tdVertexCacheArg vertexCache,
	                            const tdIndex* __restrict pIndices, const uint32 ICount,
	                            const uint8* __restrict pVertices, const uint32 VertexSize, const uint32 VCount)
	{
		using namespace NVMath;
		if (!VCount || !ICount)
		{
			return false;
		}

		m_DrawCall++;
		m_PolyCount += VCount / 3;

		const vec4 M0 = pViewProj[0];
		const vec4 M1 = pViewProj[1];
		const vec4 M2 = pViewProj[2];
		const vec4 M3 = pViewProj[3];

		if (VCount + 1 > vertexCache.size())
		{
			vertexCache.resize(VCount + 1);
		}

		vec4* pVCache = &vertexCache[0];
		pVCache = reinterpret_cast<vec4*>(((reinterpret_cast<size_t>(pVCache) + 15) & ~15));

		vec4 SMask = Vec4Zero();
		for (uint32 a = 0, S = VCount & ~3; a < S; a += 4)
		{
			const float* pV0 = reinterpret_cast<const float*>(pVertices + (a + 0) * VertexSize);
			const float* pV1 = reinterpret_cast<const float*>(pVertices + (a + 1) * VertexSize);
			const float* pV2 = reinterpret_cast<const float*>(pVertices + (a + 2) * VertexSize);
			const float* pV3 = reinterpret_cast<const float*>(pVertices + (a + 3) * VertexSize);

			const vec4 V0 = Madd(NVMath::Vec4(pV0[0]), M0, Madd(NVMath::Vec4(pV0[1]), M1, Madd(NVMath::Vec4(pV0[2]), M2, M3)));
			const vec4 V1 = Madd(NVMath::Vec4(pV1[0]), M0, Madd(NVMath::Vec4(pV1[1]), M1, Madd(NVMath::Vec4(pV1[2]), M2, M3)));
			const vec4 V2 = Madd(NVMath::Vec4(pV2[0]), M0, Madd(NVMath::Vec4(pV2[1]), M1, Madd(NVMath::Vec4(pV2[2]), M2, M3)));
			const vec4 V3 = Madd(NVMath::Vec4(pV3[0]), M0, Madd(NVMath::Vec4(pV3[1]), M1, Madd(NVMath::Vec4(pV3[2]), M2, M3)));

			SMask = Or(SMask, V0);
			SMask = Or(SMask, V1);
			SMask = Or(SMask, V2);
			SMask = Or(SMask, V3);

			pVCache[a + 0] = V0;
			pVCache[a + 1] = V1;
			pVCache[a + 2] = V2;
			pVCache[a + 3] = V3;
		}
		for (uint32 a = VCount & ~3, S = VCount; a < S; a++)
		{
			const float* pV = reinterpret_cast<const float*>(pVertices + a * VertexSize);
			const vec4 V = Madd(NVMath::Vec4(pV[0]), M0, Madd(NVMath::Vec4(pV[1]), M1, Madd(NVMath::Vec4(pV[2]), M2, M3)));
			SMask = Or(SMask, V);
			pVCache[a] = V;
		}

		bool Visible = false;
		if (SignMask(SMask) & BitZ)
		{
			for (uint32 a = 0; a < ICount; a += 3)
			{
				vec4 Pos0 = pVCache[pIndices[a + 0]];
				vec4 Pos2 = pVCache[pIndices[a + 1]];
				vec4 Pos1 = pVCache[pIndices[a + 2]];

				Visible |= Triangle<WRITE, true>(Pos0, Pos1, Pos2);
				if (!WRITE && Visible)
				{
					return true;
				}
			}
		}
		else
		{
			for (uint32 a = 0; a < ICount; a += 3)
			{
				vec4 Pos0 = pVCache[pIndices[a + 0]];
				vec4 Pos2 = pVCache[pIndices[a + 1]];
				vec4 Pos1 = pVCache[pIndices[a + 2]];

				Visible |= Triangle2D<WRITE, true>(Pos0, Pos1, Pos2);
				if (!WRITE && Visible)
				{
					return true;
				}
			}
		}

		return Visible;
	}

	int m_DebugRender;

	void DrawDebug(IRenderer* pRenderer, int32 nStep)
	{
		// project buffer to the screen
#if defined(CULLING_ENABLE_DEBUG_OVERLAY)
		IRenderAuxGeom* pAux = pRenderer->GetIRenderAuxGeom();

		nStep %= 32;
		if (!nStep)
		{
			return;
		}

		//if(!m_DebugRender)
		//	return;

		const float FarPlaneInv = 255.f / GetISystem()->GetViewCamera().GetFarPlane();

		SAuxGeomRenderFlags oFlags(e_Def2DPublicRenderflags);
		oFlags.SetDepthTestFlag(e_DepthTestOff);
		oFlags.SetDepthWriteFlag(e_DepthWriteOff);
		oFlags.SetCullMode(e_CullModeNone);
		oFlags.SetAlphaBlendMode(e_AlphaNone);
		SAuxGeomRenderFlags prevFlags = pAux->SetRenderFlags(oFlags);

		const float fScreenWidth  = float(pAux->GetCamera().GetViewSurfaceX());
		const float fScreenHeight = float(pAux->GetCamera().GetViewSurfaceZ());

		float fTopOffSet = 35.0f;
		float fSideOffSet = 35.0f;

		std::vector<SAuxVertex> vertices;
		vertices.resize(SIZEX*SIZEY * 6);
		
		SAuxVertex* __restrict pVertices = vertices.data();

		// draw z-buffer after reprojection (unknown parts are red)
		fTopOffSet += 200.0f;
		for (uint32 y = 0; y < SIZEY; y += 1)
		{
			const float* __restrict pVMemZ = &m_ZOutput[y * SIZEX];
			float fY = fTopOffSet + (y * 3);
			for (uint32 x = 0; x < SIZEX; x += 4)
			{
				float fX0 = fSideOffSet + ((x + 0) * 3);
				float fX1 = fSideOffSet + ((x + 1) * 3);
				float fX2 = fSideOffSet + ((x + 2) * 3);
				float fX3 = fSideOffSet + ((x + 3) * 3);

				uint32 ValueColor0 = (uint32)(pVMemZ[x + 0]);
				uint32 ValueColor1 = (uint32)(pVMemZ[x + 1]);
				uint32 ValueColor2 = (uint32)(pVMemZ[x + 2]);
				uint32 ValueColor3 = (uint32)(pVMemZ[x + 3]);
				ColorB Color0(ValueColor0, ValueColor0 * 16, ValueColor0 * 256, 222);
				ColorB Color1(ValueColor1, ValueColor1 * 16, ValueColor1 * 256, 222);
				ColorB Color2(ValueColor2, ValueColor2 * 16, ValueColor2 * 256, 222);
				ColorB Color3(ValueColor3, ValueColor3 * 16, ValueColor3 * 256, 222);

				pVertices = NAsyncCull::Debug::Generate2DBox(pVertices, fX0, fY, 3.0f, 3.0f, Color0);
				pVertices = NAsyncCull::Debug::Generate2DBox(pVertices, fX1, fY, 3.0f, 3.0f, Color1);
				pVertices = NAsyncCull::Debug::Generate2DBox(pVertices, fX2, fY, 3.0f, 3.0f, Color2);
				pVertices = NAsyncCull::Debug::Generate2DBox(pVertices, fX3, fY, 3.0f, 3.0f, Color3);

				CRY_ASSERT(pVertices <= vertices.data() + vertices.size());
			}
		}

		pAux->DrawBuffer(vertices.data(), vertices.size(), false);
		pAux->SetRenderFlags(prevFlags);
#endif
	}

	inline uint32 SizeX() const
	{
		return SIZEX;
	}
	inline uint32 SizeY() const
	{
		return SIZEY;
	}

};
}

#pragma warning(pop)

template<uint32 SIZEX, uint32 SIZEY>
CRY_ALIGN(128) float NAsyncCull::CCullRenderer<SIZEX, SIZEY>::m_ZBufferMainMemory[SIZEX * SIZEY];
#endif
