// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "VertexCommand.h"

#include <CryMath/QTangent.h>
#include "../ModelMesh.h"
#include "VertexData.h"

#pragma warning(push)
#pragma warning(disable:4700) // uninitialized local variable
#pragma warning(disable:6326) // potential comparison of a constant with another constant

#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_DURANGO || CRY_PLATFORM_ORBIS
	#define USE_VERTEXCOMMAND_SSE
	#if CRY_PLATFORM_DURANGO || CRY_PLATFORM_ORBIS
		#define USE_VERTEXCOMMAND_SSE4
	#endif
#endif

#ifdef USE_VERTEXCOMMAND_SSE

	#define vec4f_swizzle(v, p, q, r, s) (_mm_shuffle_ps((v), (v), ((s) << 6 | (r) << 4 | (q) << 2 | (p))))

// Even on Durango and Orbis we don't use _mm_dp_ps because it's slower than the _mm_hadd_ps way (_mm_dp_ps is a microcoded instruction).
ILINE __m128 _mm_dp_ps_emu(const __m128& a, const __m128& b)
{
	__m128 tmp = _mm_mul_ps(a, b);
	tmp = _mm_hadd_ps(tmp, tmp);
	return _mm_hadd_ps(tmp, tmp);
}

void VertexCommandTangents::Execute(VertexCommandTangents& command, CVertexData& vertexData)
{
	CRY_PROFILE_FUNCTION(PROFILE_ANIMATION);

	strided_pointer<Vec3> pPositions = vertexData.GetPositions();
	strided_pointer<SPipTangents> pTangents = vertexData.GetTangents();

	const __m128 _xyzMask = _mm_castsi128_ps(_mm_set_epi32(0, 0xffffffff, 0xffffffff, 0xffffffff));

	for (uint i = 0; i < command.tangetUpdateDataCount; ++i)
	{
		const uint base = i * 3;

		const STangentUpdateTriangles& data = command.pTangentUpdateData[i];

		const vtx_idx idx1 = data.idx1;
		const vtx_idx idx2 = data.idx2;
		const vtx_idx idx3 = data.idx3;

		//    const Vec3 v1 = pPositions[idx1];
		//    const Vec3 v2 = pPositions[idx2];
		//    const Vec3 v3 = pPositions[idx3];

		const float* __restrict p1 = (const float*)&pPositions[idx1];
		__m128 _v1 = _mm_loadu_ps(p1);
		_v1 = _mm_and_ps(_v1, _xyzMask);

		const float* __restrict p2 = (const float*)&pPositions[idx2];
		__m128 _v2 = _mm_loadu_ps(p2);
		_v2 = _mm_and_ps(_v2, _xyzMask);

		const float* __restrict p3 = (const float*)&pPositions[idx3];
		__m128 _v3 = _mm_loadu_ps(p3);
		_v3 = _mm_and_ps(_v3, _xyzMask);

		//    const float ux = v2.x - v1.x;
		//    const float uy = v2.y - v1.y;
		//    const float uz = v2.z - v1.z;

		const __m128 _u = _mm_sub_ps(_v2, _v1);

		//    const float vx = v3.x - v1.x;
		//    const float vy = v3.y - v1.y;
		//    const float vz = v3.z - v1.z;

		const __m128 _v = _mm_sub_ps(_v3, _v1);

		//		Vec3 n = u ^ v;
		//    const float nx = uy*vz - uz*vy;
		//    const float ny = uz*vx - ux*vz;
		//    const float nz = ux*vy - uy*vx;
		const __m128 _uYZX = vec4f_swizzle(_u, 1, 2, 0, 0);
		const __m128 _vZXY = vec4f_swizzle(_v, 2, 0, 1, 0);
		const __m128 _uZXY = vec4f_swizzle(_u, 2, 0, 1, 0);
		const __m128 _vYZX = vec4f_swizzle(_v, 1, 2, 0, 0);
		const __m128 _n = _mm_sub_ps(_mm_mul_ps(_uYZX, _vZXY), _mm_mul_ps(_uZXY, _vYZX));

		//    const Vec3 sdir(
		//      (data.t2 * ux - data.t1 * vx) * data.r,
		//      (data.t2 * uy - data.t1 * vy) * data.r,
		//      (data.t2 * uz - data.t1 * vz) * data.r);
		const __m128 _t1 = _mm_set_ps1(data.t1);
		const __m128 _t2 = _mm_set_ps1(data.t2);
		const __m128 _r = _mm_set_ps1(data.r);
		__m128 _dir = _mm_mul_ps(_mm_sub_ps(_mm_mul_ps(_u, _t2), _mm_mul_ps(_v, _t1)), _r);

		//    RecTangents& rt1 = pRecTangets[idx1];
		//    rt1.t.x += sdir.x; rt1.t.y += sdir.y; rt1.t.z += sdir.z;
		//		rt1.n.x += nx; rt1.n.y += ny; rt1.n.z += nz;
		__m128 _rt1_t = _mm_load_ps(command.pRecTangets[idx1].t);
		__m128 _rt1_n = _mm_load_ps(command.pRecTangets[idx1].n);
		_rt1_t = _mm_add_ps(_rt1_t, _dir);
		_rt1_n = _mm_add_ps(_rt1_n, _n);
		_mm_store_ps(command.pRecTangets[idx1].t, _rt1_t);
		_mm_store_ps(command.pRecTangets[idx1].n, _rt1_n);

		//    RecTangents& rt2 = pRecTangets[idx2];
		//    rt2.t.x += sdir.x; rt2.t.y += sdir.y; rt2.t.z += sdir.z;
		//		rt2.n.x += nx; rt2.n.y += ny; rt2.n.z += nz;
		__m128 _rt2_t = _mm_load_ps(command.pRecTangets[idx2].t);
		__m128 _rt2_n = _mm_load_ps(command.pRecTangets[idx2].n);
		_rt2_t = _mm_add_ps(_rt2_t, _dir);
		_rt2_n = _mm_add_ps(_rt2_n, _n);
		_mm_store_ps(command.pRecTangets[idx2].t, _rt2_t);
		_mm_store_ps(command.pRecTangets[idx2].n, _rt2_n);

		//    RecTangents& rt3 = pRecTangets[idx3];
		//    rt3.t.x += sdir.x; rt3.t.y += sdir.y; rt3.t.z += sdir.z;
		//		rt3.n.x += nx; rt3.n.y += ny; rt3.n.z += nz;
		__m128 _rt3_t = _mm_load_ps(command.pRecTangets[idx3].t);
		__m128 _rt3_n = _mm_load_ps(command.pRecTangets[idx3].n);
		_rt3_t = _mm_add_ps(_rt3_t, _dir);
		_rt3_n = _mm_add_ps(_rt3_n, _n);
		_mm_store_ps(command.pRecTangets[idx3].t, _rt3_t);
		_mm_store_ps(command.pRecTangets[idx3].n, _rt3_n);
	}

	const __m128 _32767 = _mm_set1_ps(32767.0f);
	CRY_ALIGN(16) Vec4sf tangentBitangent[2];

	for (uint i = 0; i < command.tangetUpdateVertIdsCount; ++i)
	{
		const uint idx = command.pTangentUpdateVertIds[i];

		//RecTangents& rec = command.pRecTangets[idx];
		__m128 _rec_t = _mm_load_ps(command.pRecTangets[idx].t);
		_rec_t = _mm_and_ps(_rec_t, _xyzMask);
		__m128 _rec_n = _mm_load_ps(command.pRecTangets[idx].n);
		_rec_n = _mm_and_ps(_rec_n, _xyzMask);

		// clear in-place (to avoid memset the entire array)
		_mm_store_ps(command.pRecTangets[idx].t, _mm_setzero_ps());
		_mm_store_ps(command.pRecTangets[idx].n, _mm_setzero_ps());

		//Vec3 n(rec.n[0], rec.n[1], rec.n[2]);
		//n = n.normalized();
		__m128 _invLength = _mm_rsqrt_ps(_mm_dp_ps_emu(_rec_n, _rec_n));
		//__m128 _invLength = _mm_rcp_ps(_mm_sqrt_ps(_mm_dp_ps_emu(_rec_n, _rec_n)));
		_rec_n = _mm_mul_ps(_rec_n, _invLength);

		//const Vec3 t(rec.t[0], rec.t[1], rec.t[2]);
		//Vec3 tangent = (t - n * (n * t)).normalized();
		__m128 _tangent = _mm_sub_ps(_rec_t, _mm_mul_ps(_mm_dp_ps_emu(_rec_t, _rec_n), _rec_n));
		_invLength = _mm_rsqrt_ps(_mm_dp_ps_emu(_tangent, _tangent));
		_tangent = _mm_mul_ps(_tangent, _invLength);

		//Vec3 bitangent = tangent ^ n;
		const __m128 _tangentYZX = vec4f_swizzle(_tangent, 1, 2, 0, 0);
		const __m128 _nZXY = vec4f_swizzle(_rec_n, 2, 0, 1, 0);
		const __m128 _tangentZXY = vec4f_swizzle(_tangent, 2, 0, 1, 0);
		const __m128 _nYZX = vec4f_swizzle(_rec_n, 1, 2, 0, 0);
		const __m128 _bitangent = _mm_sub_ps(_mm_mul_ps(_tangentYZX, _nZXY), _mm_mul_ps(_tangentZXY, _nYZX));

		//pTangents[idx] = Vec4sf(tPackF2B(tangent.x), tPackF2B(tangent.y), tPackF2B(tangent.z), pTangents[idx].w);
		//pBitangents[idx] = Vec4sf(tPackF2B(bitangent.x), tPackF2B(bitangent.y), tPackF2B(bitangent.z), pBitangents[idx].w);
		__m128i _tangenti = _mm_cvtps_epi32(_mm_mul_ps(_tangent, _32767));
		__m128i _bitangenti = _mm_cvtps_epi32(_mm_mul_ps(_bitangent, _32767));
		__m128i _compressed = _mm_packs_epi32(_tangenti, _bitangenti);

		_mm_store_si128((__m128i*)&tangentBitangent[0], _compressed);

		pTangents[idx] = SPipTangents(tangentBitangent[0], tangentBitangent[1], pTangents[idx]);
	}
}

#else

void VertexCommandTangents::Execute(VertexCommandTangents& command, CVertexData& vertexData)
{
	CRY_PROFILE_FUNCTION(PROFILE_ANIMATION);

	strided_pointer<Vec3> pPositions = vertexData.GetPositions();
	strided_pointer<SPipTangents> pTangents = vertexData.GetTangents();

	for (uint i = 0; i < command.tangetUpdateDataCount; ++i)
	{
		const uint base = i * 3;

		const STangentUpdateTriangles& data = command.pTangentUpdateData[i];

		const vtx_idx idx1 = data.idx1;
		const vtx_idx idx2 = data.idx2;
		const vtx_idx idx3 = data.idx3;

		const Vec3 v1 = pPositions[idx1];
		const Vec3 v2 = pPositions[idx2];
		const Vec3 v3 = pPositions[idx3];

		//    Vec3 u = v2 - v1;
		const float ux = v2.x - v1.x;
		const float uy = v2.y - v1.y;
		const float uz = v2.z - v1.z;

		//    Vec3 v = v3 - v1;
		const float vx = v3.x - v1.x;
		const float vy = v3.y - v1.y;
		const float vz = v3.z - v1.z;

		//		Vec3 n = u ^ v;
		const float nx = uy * vz - uz * vy;
		const float ny = uz * vx - ux * vz;
		const float nz = ux * vy - uy * vx;

		const Vec3 sdir((data.t2 * ux - data.t1 * vx) * data.r, (data.t2 * uy - data.t1 * vy) * data.r, (data.t2 * uz - data.t1 * vz) * data.r);

		RecTangents& rt1 = command.pRecTangets[idx1];
		RecTangents& rt2 = command.pRecTangets[idx2];
		RecTangents& rt3 = command.pRecTangets[idx3];

		rt1.t[0] += sdir.x;
		rt1.t[1] += sdir.y;
		rt1.t[2] += sdir.z;
		rt1.n[0] += nx;
		rt1.n[1] += ny;
		rt1.n[2] += nz;

		rt2.t[0] += sdir.x;
		rt2.t[1] += sdir.y;
		rt2.t[2] += sdir.z;
		rt2.n[0] += nx;
		rt2.n[1] += ny;
		rt2.n[2] += nz;

		rt3.t[0] += sdir.x;
		rt3.t[1] += sdir.y;
		rt3.t[2] += sdir.z;
		rt3.n[0] += nx;
		rt3.n[1] += ny;
		rt3.n[2] += nz;
	}

	for (uint i = 0; i < command.tangetUpdateVertIdsCount; ++i)
	{
		const uint idx = command.pTangentUpdateVertIds[i];

		RecTangents& rec = command.pRecTangets[idx];

		//		Vec3 n = rec.n.normalized();
		Vec3 n(rec.n[0], rec.n[1], rec.n[2]);
		n = n.normalized();

		//    const Vec3& t = rec.t;
		const Vec3 t(rec.t[0], rec.t[1], rec.t[2]);

		Vec3 tangent = (t - n * (n * t)).normalized();
		Vec3 bitangent = tangent ^ n;

		pTangents[idx] = SPipTangents(tangent, bitangent, pTangents[idx]);

		// clear in-place (to avoid memset the entire array)
		rec.n[0] = 0.0f;
		rec.n[1] = 0.0f;
		rec.n[2] = 0.0f;
		rec.n[3] = 0.0f;

		rec.t[0] = 0.0f;
		rec.t[1] = 0.0f;
		rec.t[2] = 0.0f;
		rec.t[3] = 0.0f;
	}
}

void VertexCommandAdd::Execute(VertexCommandAdd& command, CVertexData& vertexData)
{
	CRY_PROFILE_FUNCTION(PROFILE_ANIMATION);

	strided_pointer<Vec3> pPositions = vertexData.GetPositions();
	const float weight = command.weight;
	const uint count = command.count;
	for (uint i = 0; i < count; ++i)
	{
		const uint index = command.pIndices[i];

		Vec3* __restrict pPos = &pPositions[index];
		const Vec3 offset = command.pVectors[i] * weight;

		*pPos += offset;
	}
}

#endif

/*
   VertexCommandCopy
 */
void VertexCommandCopy::Execute(VertexCommandCopy& command, CVertexData& vertexData)
{
	CRY_PROFILE_FUNCTION(PROFILE_ANIMATION);

	const uint vertexCount = vertexData.GetVertexCount();
	strided_pointer<Vec3> pPositions = vertexData.GetPositions();
	strided_pointer<uint32> pColors = vertexData.GetColors();
	strided_pointer<Vec2> pCoords = vertexData.GetCoords();

	for (uint i = 0; i < vertexCount; ++i)
	{
		pPositions[i] = command.pVertexPositions[i];
		pColors[i] = command.pVertexColors[i];
		pCoords[i] = command.pVertexCoords[i];
	}

	const uint indexCount = vertexData.GetIndexCount();
	vtx_idx* pIndices = vertexData.GetIndices();

	memcpy(pIndices, command.pIndices, indexCount * sizeof(pIndices[0]));
}

void VertexCommandSkin::Execute(VertexCommandSkin& command, CVertexData& vertexData)
{
	assert(command.pTransformations);
	assert(command.pTransformationRemapTable);

	bool hasPreviousPosition = command.pVertexPositionsPrevious != 0;
	bool hasVelocity = vertexData.GetVelocities() != 0;

	if (hasPreviousPosition && hasVelocity)
	{
		ExecuteInternal<VELOCITY_VECTOR>(command, vertexData);
	}
	else if (hasVelocity)
	{
		ExecuteInternal<VELOCITY_ZERO>(command, vertexData);
	}
	else
	{
		ExecuteInternal<0>(command, vertexData);
	}
}

#ifndef USE_VERTEXCOMMAND_SSE

/*
   VertexCommandSkin
 */

template<uint32 TEMPLATE_FLAGS>
void VertexCommandSkin::ExecuteInternal(VertexCommandSkin& command, CVertexData& vertexData)
{
	CRY_PROFILE_FUNCTION(PROFILE_ANIMATION);
	PREFAST_SUPPRESS_WARNING(6255)
	DualQuat * pTransformations = (DualQuat*)alloca(command.transformationCount * sizeof(DualQuat));
	for (uint i = 0; i < command.transformationCount; ++i)
	{
		pTransformations[i] = command.pTransformations[command.pTransformationRemapTable[i]];
	}

	const uint vertexCount = vertexData.GetVertexCount();
	strided_pointer<Vec3> pPositions = vertexData.GetPositions();
	strided_pointer<Vec3> pVelocities = vertexData.GetVelocities();
	strided_pointer<SPipTangents> pTangents = vertexData.GetTangents();

	strided_pointer<const Vec3> pPositionsSource = command.pVertexPositions;
	strided_pointer<const Vec3> pPositionsPrevious = command.pVertexPositionsPrevious;
	if (!pPositionsSource.data)
		pPositionsSource = vertexData.pPositions;

	DualQuat dq;
	const DualQuat* dqTransform;
	float dqLengthInv;
	Quat qtangent;
	float flip;
	Vec3 newPos;

	const uint vertexTransformCount = command.vertexTransformCount;

	for (uint i = 0; i < vertexCount; ++i)
	{
		dq.SetZero();

		const SoftwareVertexBlendIndex* pBlendIndices = &command.pVertexTransformIndices[i];
		const SoftwareVertexBlendWeight* pBlendWeights = &command.pVertexTransformWeights[i];
		const uint64 weights8 = *(uint64*)pBlendWeights;
		for (uint j = 0; j < command.vertexTransformCount; ++j)
		{
			const uint transformIndex = pBlendIndices[j];
			const float transformWeight = ((weights8 >> (j * 8)) & 0xff) / 255.f;
			dqTransform = &pTransformations[transformIndex];
			dq.dq.v.x += dqTransform->dq.v.x * transformWeight;
			dq.dq.v.y += dqTransform->dq.v.y * transformWeight;
			dq.dq.v.z += dqTransform->dq.v.z * transformWeight;
			dq.dq.w += dqTransform->dq.w * transformWeight;
			dq.nq.v.x += dqTransform->nq.v.x * transformWeight;
			dq.nq.v.y += dqTransform->nq.v.y * transformWeight;
			dq.nq.v.z += dqTransform->nq.v.z * transformWeight;
			dq.nq.w += dqTransform->nq.w * transformWeight;
		}

		dqLengthInv = isqrt_fast_tpl(dq.nq.v.x * dq.nq.v.x + dq.nq.v.y * dq.nq.v.y + dq.nq.v.z * dq.nq.v.z + dq.nq.w * dq.nq.w);
		dq.nq *= dqLengthInv;
		dq.dq *= dqLengthInv;

		qtangent = dq.nq * command.pVertexQTangents[i];
		if (qtangent.w < 0.0f)
			qtangent = -qtangent;
		flip = command.pVertexQTangents[i].w < 0.0f ? -1.0f : 1.0f;
		qtangent *= flip;

		newPos = dq * pPositionsSource[i];

		pPositions[i] = newPos;
		pTangents[i] = SPipTangents(qtangent, (int16)flip);

		if ((TEMPLATE_FLAGS & VELOCITY_VECTOR) != 0)
		{
			pVelocities[i] = pPositionsPrevious[i] - newPos;
		}
		else if ((TEMPLATE_FLAGS & VELOCITY_ZERO) != 0)
		{
			memset(&pVelocities[i], 0, sizeof(Vec3));
		}
	}
}

#else

const int zero = 0;
const int flipSign = 0x80000000;
const __m128 quat_mask = _mm_setr_ps(*(float*)&zero,
                                     *(float*)&zero,
                                     *(float*)&zero,
                                     *(float*)&flipSign);

__m128 quat_mul_quat(__m128 a, __m128 b)
{
	__m128 swiz1 = vec4f_swizzle(b, 3, 3, 3, 3);
	__m128 swiz2 = vec4f_swizzle(a, 2, 0, 1, 0);
	__m128 swiz3 = vec4f_swizzle(b, 1, 2, 0, 0);
	__m128 swiz4 = vec4f_swizzle(a, 3, 3, 3, 1);
	__m128 swiz5 = vec4f_swizzle(b, 0, 1, 2, 1);
	__m128 swiz6 = vec4f_swizzle(a, 1, 2, 0, 2);
	__m128 swiz7 = vec4f_swizzle(b, 2, 0, 1, 2);
	__m128 mul4 = _mm_mul_ps(swiz6, swiz7);
	__m128 mul3 = _mm_mul_ps(swiz4, swiz5);
	__m128 mul2 = _mm_mul_ps(swiz2, swiz3);
	__m128 mul1 = _mm_mul_ps(a, swiz1);
	__m128 flip1 = _mm_xor_ps(mul4, quat_mask);
	__m128 flip2 = _mm_xor_ps(mul3, quat_mask);
	__m128 retVal = _mm_sub_ps(mul1, mul2);
	__m128 retVal2 = _mm_add_ps(flip1, flip2);
	return _mm_add_ps(retVal, retVal2);
}

ILINE __m128 GetColumn0(__m128 _quat)
{
	//x = 2*(v.x*v.x+w*w)-1;
	//y = 2*(v.y*v.x+v.z*w);
	//z = 2*(v.z*v.x-v.y*w);

	const __m128 _mask0 = _mm_setr_ps(1.f, 1.f, -1.f, 0.f);
	const __m128 _add0 = _mm_setr_ps(-1.f, 0.f, 0.f, 0.f);
	const __m128 _2 = _mm_setr_ps(2.f, 2.f, 2.f, 0.f);

	const __m128 _x = vec4f_swizzle(_quat, 0, 0, 0, 0);
	const __m128 _w = vec4f_swizzle(_quat, 3, 3, 3, 0);
	const __m128 _quatWZY = vec4f_swizzle(_quat, 3, 2, 1, 0);

	__m128 _mul1 = _mm_mul_ps(_quat, _x);
	__m128 _mul2 = _mm_mul_ps(_mm_mul_ps(_quatWZY, _w), _mask0);
	__m128 _add1 = _mm_add_ps(_mul1, _mul2);
	__m128 _mul3 = _mm_mul_ps(_add1, _2);
	return _mm_add_ps(_mul3, _add0);
}

ILINE __m128 GetColumn1(__m128 _quat)
{
	const __m128 _mask0 = _mm_setr_ps(-1.f, 1.f, 1.f, 0.f);
	const __m128 _add0 = _mm_setr_ps(0.f, -1.f, 0.f, 0.f);
	const __m128 _2 = _mm_setr_ps(2.f, 2.f, 2.f, 0.f);

	const __m128 _y = vec4f_swizzle(_quat, 1, 1, 1, 1);
	const __m128 _w = vec4f_swizzle(_quat, 3, 3, 3, 0);
	const __m128 _quatZWX = vec4f_swizzle(_quat, 2, 3, 0, 0);

	__m128 _mul1 = _mm_mul_ps(_quat, _y);
	__m128 _mul2 = _mm_mul_ps(_mm_mul_ps(_quatZWX, _w), _mask0);
	__m128 _add1 = _mm_add_ps(_mul1, _mul2);
	__m128 _mul3 = _mm_mul_ps(_add1, _2);
	return _mm_add_ps(_mul3, _add0);

	//x = 2*(v.x*v.y-v.z*w);
	//y = 2*(v.y*v.y+w*w)-1;
	//Z = 2*(v.z*v.y+v.x*w);
}

ILINE __m128 DualQuat_mul_Vec3(const __m128 _dq, const __m128 _nq, const __m128 _v)
{
	const __m128 _vZXY = vec4f_swizzle(_v, 2, 0, 1, 0);
	const __m128 _vYZX = vec4f_swizzle(_v, 1, 2, 0, 0);
	const __m128 _nqYZX = vec4f_swizzle(_nq, 1, 2, 0, 0);
	const __m128 _nqZXY = vec4f_swizzle(_nq, 2, 0, 1, 0);
	const __m128 _nqWWW = vec4f_swizzle(_nq, 3, 3, 3, 0);
	const __m128 _dqYZX = vec4f_swizzle(_dq, 1, 2, 0, 0);
	const __m128 _dqZXY = vec4f_swizzle(_dq, 2, 0, 1, 0);
	const __m128 _dqWWW = vec4f_swizzle(_dq, 3, 3, 3, 0);

	// register const F2 ax = (dq.nq.v.y*v.z) - (dq.nq.v.z*v.y) + (dq.nq.w*v.x);
	// register const F2 ay = (dq.nq.v.z*v.x) - (dq.nq.v.x*v.z) + (dq.nq.w*v.y);
	// register const F2 az = (dq.nq.v.x*v.y) - (dq.nq.v.y*v.x) + (dq.nq.w*v.z);
	__m128 _a = _mm_add_ps(_mm_sub_ps(_mm_mul_ps(_nqYZX, _vZXY), _mm_mul_ps(_nqZXY, _vYZX)), _mm_mul_ps(_nqWWW, _v));

	// register F2 x = (dq.dq.v.x*dq.nq.w) - (dq.nq.v.x*dq.dq.w) + (dq.nq.v.y*dq.dq.v.z) - (dq.nq.v.z*dq.dq.v.y);
	// register F2 y = (dq.dq.v.y*dq.nq.w) - (dq.nq.v.y*dq.dq.w) + (dq.nq.v.z*dq.dq.v.x) - (dq.nq.v.x*dq.dq.v.z);
	// register F2 z = (dq.dq.v.z*dq.nq.w) - (dq.nq.v.z*dq.dq.w) + (dq.nq.v.x*dq.dq.v.y) - (dq.nq.v.y*dq.dq.v.x);
	__m128 _b = _mm_add_ps(_mm_sub_ps(_mm_mul_ps(_dq, _nqWWW), _mm_mul_ps(_nq, _dqWWW)), _mm_sub_ps(_mm_mul_ps(_nqYZX, _dqZXY), _mm_mul_ps(_nqZXY, _dqYZX)));

	// x += x;
	// y += y;
	// z += z;
	_b = _mm_add_ps(_b, _b);

	// F2 tX = (az*dq.nq.v.y) - (ay*dq.nq.v.z);
	// F2 tY = (ax*dq.nq.v.z) - (az*dq.nq.v.x);
	// F2 tZ = (ay*dq.nq.v.x) - (ax*dq.nq.v.y);
	__m128 _t = _mm_sub_ps(_mm_mul_ps(vec4f_swizzle(_a, 2, 0, 1, 0), _nqYZX), _mm_mul_ps(vec4f_swizzle(_a, 1, 2, 0, 0), _nqZXY));

	// x += tX+tX+v.x;
	// y += tY+tY+v.y;
	// z += tZ+tZ+v.z;
	_b = _mm_add_ps(_mm_add_ps(_t, _t), _mm_add_ps(_b, _v));
	return _b;
}

struct Vec3A : Vec3
{
	float padding;
};

template<uint32 TEMPLATE_FLAGS>
PREFAST_SUPPRESS_WARNING(6262)
void VertexCommandSkin::ExecuteInternal(VertexCommandSkin& command, CVertexData& vertexData)
{
	CRY_PROFILE_FUNCTION(PROFILE_ANIMATION);
	PREFAST_SUPPRESS_WARNING(6255)
	__m128 pTransformations[MAX_JOINT_AMOUNT * 2];
	for (uint i = 0; i < command.transformationCount; ++i)
	{
		pTransformations[i * 2 + 0] = _mm_loadu_ps((float*)(&command.pTransformations[command.pTransformationRemapTable[i]].dq));
		pTransformations[i * 2 + 1] = _mm_loadu_ps((float*)(&command.pTransformations[command.pTransformationRemapTable[i]].nq));
	}

	const uint vertexCount = vertexData.GetVertexCount();
	strided_pointer<Vec3> pPositions = vertexData.GetPositions();
	strided_pointer<Vec3> pVelocities = vertexData.GetVelocities();
	strided_pointer<SPipTangents> pTangents = vertexData.GetTangents();

	__m128 dq_nq = _mm_setzero_ps();
	__m128 dq_dq;
	__m128 _nq, _dq;
	__m128i _weightsi;
	__m128i _weights03i;
	__m128i _weights47i;
	__m128 _weights03;
	__m128 _weights47;
	__m128 _weight;
	CRY_ALIGN(16) Vec4sf tangentBitangent[2];
	CRY_ALIGN(16) Vec3A newPos;

	strided_pointer<const Vec3> pPositionsPrevious = command.pVertexPositionsPrevious;
	strided_pointer<const SoftwareVertexBlendIndex> pIndices = command.pVertexTransformIndices;
	strided_pointer<const SoftwareVertexBlendWeight> pWeights = command.pVertexTransformWeights;

	strided_pointer<const Vec3> pPositionsSource = command.pVertexPositions;
	if (!pPositionsSource.data)
		pPositionsSource = vertexData.pPositions;

	const float Inv255 = 1.f / 255.f;
	const __m128 _Inv255 = _mm_set1_ps(Inv255);
	const __m128 _32767 = _mm_set1_ps(32767.0f);
	const __m128i _zeroi = _mm_setzero_si128();
	const __m128 _zero = _mm_setzero_ps();
	const __m128 _minus1 = _mm_setr_ps(0.f, 0.f, 0.f, -1.f);
	const __m128 _1 = _mm_setr_ps(0.f, 0.f, 0.f, 1.f);
	const __m128 _half = _mm_set_ps(0.5f, 0.5f, 0.5f, 0.5f);
	const __m128 _three = _mm_set_ps(3.f, 3.f, 3.f, 3.f);
	const __m128 _xyzMask = _mm_castsi128_ps(_mm_set_epi32(0, 0xffffffff, 0xffffffff, 0xffffffff));

	const int maxVertexCountMask = (1 << command.vertexTransformCount);

	for (unsigned i = 0; i < vertexCount; ++i)
	{
		_weightsi = _mm_loadl_epi64((__m128i*)&pWeights[i]);
		const SoftwareVertexBlendIndex* pVertexIndices = &pIndices[i];

		const int mask = _mm_movemask_epi8(_mm_cmpeq_epi8(_weightsi, _zeroi));
		unsigned long count;
	#if CRY_PLATFORM_ORBIS
		count = __builtin_ctz(mask | maxVertexCountMask);
	#else
		_BitScanForward(&count, mask | maxVertexCountMask);
	#endif // #if CRY_PLATFORM_ORBIS

		_weightsi = _mm_unpacklo_epi8(_weightsi, _zeroi);

		_weights03i = _mm_unpacklo_epi16(_weightsi, _zeroi);
		_weights03 = _mm_mul_ps(_mm_cvtepi32_ps(_weights03i), _Inv255);

		_weights47i = _mm_unpackhi_epi16(_weightsi, _zeroi);
		_weights47 = _mm_mul_ps(_mm_cvtepi32_ps(_weights47i), _Inv255);

		dq_nq = dq_dq = _mm_xor_ps(dq_nq, dq_nq);

		PREFAST_ASSUME(pVertexIndices[0] < command.transformationCount);
		_dq = pTransformations[pVertexIndices[0] * 2 + 0];
		_nq = pTransformations[pVertexIndices[0] * 2 + 1];
		_weight = _mm_shuffle_ps(_weights03, _weights03, _MM_SHUFFLE(0, 0, 0, 0));
		dq_nq = _mm_add_ps(dq_nq, _mm_mul_ps(_nq, _weight));
		dq_dq = _mm_add_ps(dq_dq, _mm_mul_ps(_dq, _weight));

		switch (count)
		{
		case 8:
			_dq = pTransformations[pVertexIndices[7] * 2 + 0];
			_nq = pTransformations[pVertexIndices[7] * 2 + 1];
			_weight = _mm_shuffle_ps(_weights47, _weights47, _MM_SHUFFLE(3, 3, 3, 3));
			dq_nq = _mm_add_ps(dq_nq, _mm_mul_ps(_nq, _weight));
			dq_dq = _mm_add_ps(dq_dq, _mm_mul_ps(_dq, _weight));
		case 7:
			_dq = pTransformations[pVertexIndices[6] * 2 + 0];
			_nq = pTransformations[pVertexIndices[6] * 2 + 1];
			_weight = _mm_shuffle_ps(_weights47, _weights47, _MM_SHUFFLE(2, 2, 2, 2));
			dq_nq = _mm_add_ps(dq_nq, _mm_mul_ps(_nq, _weight));
			dq_dq = _mm_add_ps(dq_dq, _mm_mul_ps(_dq, _weight));
		case 6:
			_dq = pTransformations[pVertexIndices[5] * 2 + 0];
			_nq = pTransformations[pVertexIndices[5] * 2 + 1];
			_weight = _mm_shuffle_ps(_weights47, _weights47, _MM_SHUFFLE(1, 1, 1, 1));
			dq_nq = _mm_add_ps(dq_nq, _mm_mul_ps(_nq, _weight));
			dq_dq = _mm_add_ps(dq_dq, _mm_mul_ps(_dq, _weight));
		case 5:
			_dq = pTransformations[pVertexIndices[4] * 2 + 0];
			_nq = pTransformations[pVertexIndices[4] * 2 + 1];
			_weight = _mm_shuffle_ps(_weights47, _weights47, _MM_SHUFFLE(0, 0, 0, 0));
			dq_nq = _mm_add_ps(dq_nq, _mm_mul_ps(_nq, _weight));
			dq_dq = _mm_add_ps(dq_dq, _mm_mul_ps(_dq, _weight));
		case 4:
			_dq = pTransformations[pVertexIndices[3] * 2 + 0];
			_nq = pTransformations[pVertexIndices[3] * 2 + 1];
			_weight = _mm_shuffle_ps(_weights03, _weights03, _MM_SHUFFLE(3, 3, 3, 3));
			dq_nq = _mm_add_ps(dq_nq, _mm_mul_ps(_nq, _weight));
			dq_dq = _mm_add_ps(dq_dq, _mm_mul_ps(_dq, _weight));
		case 3:
			_dq = pTransformations[pVertexIndices[2] * 2 + 0];
			_nq = pTransformations[pVertexIndices[2] * 2 + 1];
			_weight = _mm_shuffle_ps(_weights03, _weights03, _MM_SHUFFLE(2, 2, 2, 2));
			dq_nq = _mm_add_ps(dq_nq, _mm_mul_ps(_nq, _weight));
			dq_dq = _mm_add_ps(dq_dq, _mm_mul_ps(_dq, _weight));
		case 2:
			_dq = pTransformations[pVertexIndices[1] * 2 + 0];
			_nq = pTransformations[pVertexIndices[1] * 2 + 1];
			_weight = _mm_shuffle_ps(_weights03, _weights03, _MM_SHUFFLE(1, 1, 1, 1));
			dq_nq = _mm_add_ps(dq_nq, _mm_mul_ps(_nq, _weight));
			dq_dq = _mm_add_ps(dq_dq, _mm_mul_ps(_dq, _weight));
		default:
			break;
		}

		const __m128 _dp = _mm_dp_ps_emu(dq_nq, dq_nq);
		const __m128 nr = _mm_rsqrt_ps(_dp);
		const __m128 muls = _mm_mul_ps(_mm_mul_ps(_dp, nr), nr);
		const __m128 _invLength = _mm_mul_ps(_mm_mul_ps(_half, nr), _mm_sub_ps(_three, muls));
		dq_nq = _mm_mul_ps(dq_nq, _invLength);
		dq_dq = _mm_mul_ps(dq_dq, _invLength);

		const float* __restrict p1 = (const float*)&pPositionsSource[i];
		__m128 _pos = _mm_loadu_ps(p1);
		_pos = _mm_and_ps(_pos, _xyzMask);

		__m128 _tangent = (command.pVertexQTangents.data) ? _mm_load_ps((float*)&command.pVertexQTangents[i]) : _mm_setr_ps(0.f, 0.f, 1.f, 1.f);

		__m128 _flip = _mm_cmplt_ps(_tangent, _zero);
		const __m128 _flip2 = _mm_or_ps(_mm_and_ps(_flip, _minus1), _mm_andnot_ps(_flip, _1));

		_tangent = quat_mul_quat(dq_nq, _tangent);

		_flip = _mm_xor_ps(_flip, _mm_cmplt_ps(_tangent, _zero));
		_flip = _mm_or_ps(_mm_and_ps(_flip, _minus1), _mm_andnot_ps(_flip, _1));
		_tangent = _mm_mul_ps(_tangent, _mm_shuffle_ps(_flip, _flip, _MM_SHUFFLE(3, 3, 3, 3)));

		const __m128 _column0 = _mm_add_ps(GetColumn0(_tangent), _flip2);
		const __m128 _column1 = _mm_add_ps(GetColumn1(_tangent), _flip2);

		const __m128i _column0i = _mm_cvtps_epi32(_mm_mul_ps(_column0, _32767));
		const __m128i _column1i = _mm_cvtps_epi32(_mm_mul_ps(_column1, _32767));

		const __m128i _compressed = _mm_packs_epi32(_column0i, _column1i);

		_pos = DualQuat_mul_Vec3(dq_dq, dq_nq, _pos);

		_mm_store_si128((__m128i*)&tangentBitangent[0], _compressed);
		_mm_store_ps((float*)&newPos, _pos);

		PREFAST_SUPPRESS_WARNING(6313)
		if (TEMPLATE_FLAGS & VELOCITY_VECTOR)
		{
			pVelocities[i] = pPositionsPrevious[i] - newPos;
		}
		PREFAST_SUPPRESS_WARNING(6313)
		else if (TEMPLATE_FLAGS & VELOCITY_ZERO)
		{
			memset(&pVelocities[i], 0, sizeof(Vec3));
		}

		pPositions[i] = newPos;
		pTangents[i] = SPipTangents(
		  tangentBitangent[0],
		  tangentBitangent[1]
		  );
	}
}

/*
   VertexCommandAdd
 */

void VertexCommandAdd::Execute(VertexCommandAdd& command, CVertexData& vertexData)
{
	CRY_PROFILE_FUNCTION(PROFILE_ANIMATION);

	strided_pointer<Vec3> pPositions = vertexData.GetPositions();
	const float weight = command.weight;
	const __m128 _weight = _mm_setr_ps(weight, weight, weight, 0.f);
	__m128 _pos;
	__m128 _offset;

	const __m128 _xyzMask = _mm_castsi128_ps(_mm_set_epi32(0, 0xffffffff, 0xffffffff, 0xffffffff));
	const __m128 _wMask = _mm_castsi128_ps(_mm_set_epi32(0xffffffff, 0, 0, 0));

	const uint count = command.count;
	for (uint i = 0; i < count; ++i)
	{
		const uint index = command.pIndices[i];

		float* __restrict pPosition = (float*)&pPositions[index];

		__m128 _loadPos = _mm_loadu_ps(pPosition);
		_pos = _mm_and_ps(_loadPos, _xyzMask);

		const float* __restrict pVectors = (const float*)&command.pVectors[i];
		__m128 _loadOffset = _mm_loadu_ps(pVectors);
		_offset = _mm_and_ps(_loadOffset, _xyzMask);

		_pos = _mm_add_ps(_pos, _mm_mul_ps(_offset, _weight));

	#ifdef USE_VERTEXCOMMAND_SSE4
		_loadPos = _mm_blend_ps(_pos, _loadPos, 8);
	#else
		_loadPos = _mm_or_ps(_mm_and_ps(_pos, _xyzMask), _mm_and_ps(_loadPos, _wMask));
	#endif

		_mm_storeu_ps((float*)pPosition, _loadPos);
	}
}

#endif

#pragma warning(pop)
