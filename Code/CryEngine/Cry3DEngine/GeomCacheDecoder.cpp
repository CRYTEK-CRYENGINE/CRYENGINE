// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// ------------------------------------------------------------------------
//  File name:   GeomCacheDecoder.cpp
//  Created:     23/8/2012 by Axel Gneiting
//  Description: Decodes geom cache data
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#if defined(USE_GEOM_CACHES)

	#include "GeomCacheDecoder.h"
	#include "GeomCache.h"
	#include "GeomCacheRenderNode.h"
	#include "GeomCachePredictors.h"
	#include <CrySystem/ZLib/IZlibDecompressor.h>
	#include <CrySystem/ZLib/ILZ4Decompressor.h>

namespace GeomCacheDecoder
{
// This namespace will provide the different vertex decode function permutations to avoid dynamic branching
const uint kNumPermutations = 2 * 2 * 2 * 3;

GeomCacheFile::Color FixedPointColorLerp(int32 a, int32 b, const int32 lerpFactor)
{
	return a + (((b - a) * lerpFactor) >> 16);
}

inline uint GetDecodeVerticesPerm(const bool bMotionBlur, const GeomCacheFile::EStreams constantStreamMask, const GeomCacheFile::EStreams animatedStreamMask)
{
	uint permutation = 0;

	permutation += bMotionBlur ? (2 * 2 * 3) : 0;
	permutation += (constantStreamMask& GeomCacheFile::eStream_Positions) ? (2 * 3) : 0;
	permutation += (constantStreamMask& GeomCacheFile::eStream_Texcoords) ? 3 : 0;
	permutation += (constantStreamMask& GeomCacheFile::eStream_Colors) ? 1 : 0;
	permutation += (animatedStreamMask& GeomCacheFile::eStream_Colors) ? 2 : 0;

	return permutation;
}

	#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_DURANGO || CRY_PLATFORM_ORBIS
		#define vec4f_swizzle(v, p, q, r, s) (_mm_shuffle_ps((v), (v), ((s) << 6 | (r) << 4 | (q) << 2 | (p))))

void ConvertToTangentAndBitangentVec4f(const __m128 interpolated, const __m128 floor, __m128& tangent, __m128& bitangent)
{
	const __m128 comparedAgainstW = _mm_setr_ps(FLT_MIN, FLT_MIN, FLT_MIN, 0.0f);
	const __m128 flipSignMask = _mm_castsi128_ps(_mm_set1_epi32(0x80000000));
	const __m128 twos = _mm_setr_ps(2.0f, 2.0f, 2.0f, 0.0f);

	// (interpolated.w < 0.0f) != (floor.w < 0.0f) => flip sign of quaternions in registers
	const __m128 cmp = _mm_xor_ps(_mm_cmplt_ps(interpolated, comparedAgainstW), _mm_cmplt_ps(floor, comparedAgainstW));
	const __m128 signCmp = vec4f_swizzle(cmp, 3, 3, 3, 3);
	const __m128 xyzw = _mm_xor_ps(interpolated, _mm_and_ps(signCmp, flipSignMask));
	const __m128 wSignBit = _mm_and_ps(_mm_castsi128_ps(_mm_setr_epi32(0, 0, 0, 0x80000000)), xyzw);

	// Calculate tangent & bitangent
	const __m128 xxxx = vec4f_swizzle(xyzw, 0, 0, 0, 0);
	const __m128 yyyy = vec4f_swizzle(xyzw, 1, 1, 1, 1);
	const __m128 wwww = vec4f_swizzle(xyzw, 3, 3, 3, 3);
	const __m128 wzyx = vec4f_swizzle(xyzw, 3, 2, 1, 0);
	const __m128 zwxy = vec4f_swizzle(xyzw, 2, 3, 0, 1);

	// tangent = (2 * (x * x + w * w) - 1, 2 * (y * x + z * w), 2 * (z * x - y * w), sign(w))
	__m128 wwnw = _mm_xor_ps(wwww, _mm_castsi128_ps(_mm_setr_epi32(0, 0, 0x80000000, 0)));   // -> (w, w, -w, w)
	tangent = _mm_add_ps(_mm_mul_ps(_mm_add_ps(_mm_mul_ps(xyzw, xxxx), _mm_mul_ps(wzyx, wwnw)), twos), _mm_setr_ps(-1.0f, 0.0f, 0.0f, 1.0f));
	tangent = _mm_or_ps(wSignBit, tangent);

	// bitangent = (2 * (x * y - z * w), 2 * (y * y + w * w) - 1, 2 * (z * y + x * w), sign(w))
	__m128 nwww = _mm_xor_ps(wwww, _mm_castsi128_ps(_mm_setr_epi32(0x80000000, 0, 0, 0)));   // -> (-w, w, w, w)
	bitangent = _mm_add_ps(_mm_mul_ps(_mm_add_ps(_mm_mul_ps(xyzw, yyyy), _mm_mul_ps(zwxy, nwww)), twos), _mm_setr_ps(0.0f, -1.0f, 0.0f, 1.0f));
	bitangent = _mm_or_ps(wSignBit, bitangent);
}

// Even on Durango and Orbis we don't use _mm_dp_ps because it's slower than the _mm_hadd_ps way (_mm_dp_ps is a microcoded instruction).
ILINE __m128 _mm_dp_ps_emu(const __m128& a, const __m128& b)
{
	__m128 tmp1 = _mm_mul_ps(a, b);
	__m128 tmp2 = _mm_hadd_ps(tmp1, tmp1);
	return _mm_hadd_ps(tmp2, tmp2);
}

__m128i _mm_cvtepi16_epi32_emu(const __m128i& a)
{
		#if CRY_PLATFORM_DURANGO || CRY_PLATFORM_ORBIS
	return _mm_cvtepi16_epi32(a);
		#else
	// 5 instructions (unpack, and, cmp, and, or). Idea is to fill 0xFFFF in the hi-word if the sign bit of the lo-word 1.
	const __m128i signBitsMask = _mm_set1_epi32(0x00008000);
	const __m128i hiWordBitMask = _mm_set1_epi32(0xFFFF0000);

	// Unsigned conversion, upper word will be 0x0000 even if sign bit is set
	const __m128i unpacked = _mm_unpacklo_epi16(a, _mm_set1_epi16(0));

	// Mask out sign bits
	const __m128i signBitsMasked = _mm_castps_si128(_mm_and_ps(_mm_castsi128_ps(unpacked), _mm_castsi128_ps(signBitsMask)));

	// Sets dwords to 0xFFFFFFFF if sign bit is 1
	const __m128i cmpBits = _mm_cmpeq_epi32(signBitsMasked, signBitsMask);

	// Mask dwords to 0xFFFF0000 if sign bit was set
	const __m128i signExtendBits = _mm_and_si128(hiWordBitMask, cmpBits);

	// Finally sign extend with 0xFFFF0000 if sign bit was set
	return _mm_or_si128(unpacked, signExtendBits);
		#endif
}
	#endif

void DecodeAndInterpolateTangents(const uint numVertices, const float lerpFactor, const GeomCacheFile::QTangent* __restrict pFloorQTangents,
                                  const GeomCacheFile::QTangent* __restrict pCeilQTangents, strided_pointer<SPipTangents> pTangents)
{
	#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_DURANGO || CRY_PLATFORM_ORBIS
	const uint numVerticesPerIteration = 2;
	const uint numSIMDIterations = numVertices / numVerticesPerIteration;

	const float kMultiplier = float((2 << (GeomCacheFile::kTangentQuatPrecision - 1)) - 1);
	const __m128 convertFromUint16FactorPacked = _mm_set1_ps(1.0f / kMultiplier);
	const __m128 lerpFactorPacked = _mm_set1_ps(lerpFactor);
	const __m128i zero = _mm_setzero_si128();
	const __m128 flipSignMask = _mm_castsi128_ps(_mm_set1_epi32(0x80000000));
	const __m128 scaleToInt16Factor = _mm_setr_ps(32767.0f, 32767.0f, 32767.0f, 32767.0f);

	__m128i* pFloorQTangents128 = (__m128i*)&pFloorQTangents[0];
	__m128i* pCeilQTangents128 = (__m128i*)&pCeilQTangents[0];
	__m128i* pTangents128 = (__m128i*)pTangents.data;

	for (unsigned int i = 0, j = 0; i < numSIMDIterations; ++i, j += 2)
	{
		const __m128i floorQTangents = _mm_load_si128(pFloorQTangents128 + i);
		const __m128i ceilQTangents = _mm_load_si128(pCeilQTangents128 + i);

		// Unpack to lo/hi qTangents and convert to float [-1, 1]
		__m128 floorLo = _mm_mul_ps(_mm_cvtepi32_ps(_mm_cvtepi16_epi32_emu(floorQTangents)), convertFromUint16FactorPacked);
		__m128 floorHi = _mm_mul_ps(_mm_cvtepi32_ps(_mm_cvtepi16_epi32_emu(_mm_shuffle_epi32(floorQTangents, _MM_SHUFFLE(1, 0, 3, 2)))), convertFromUint16FactorPacked);
		__m128 ceilLo = _mm_mul_ps(_mm_cvtepi32_ps(_mm_cvtepi16_epi32_emu(ceilQTangents)), convertFromUint16FactorPacked);
		__m128 ceilHi = _mm_mul_ps(_mm_cvtepi32_ps(_mm_cvtepi16_epi32_emu(_mm_shuffle_epi32(ceilQTangents, _MM_SHUFFLE(1, 0, 3, 2)))), convertFromUint16FactorPacked);

		// Need to flip sign of the ceil quaternion if the dot product of floor and ceil < 0
		__m128 dotLo = _mm_dp_ps_emu(floorLo, ceilLo);
		__m128 dotCmpLo = _mm_cmplt_ps(dotLo, _mm_castsi128_ps(zero));
		__m128 flipSignLo = _mm_and_ps(dotCmpLo, flipSignMask);
		ceilLo = _mm_xor_ps(ceilLo, flipSignLo);

		__m128 dotHi = _mm_dp_ps_emu(floorHi, ceilHi);
		__m128 dotCmpHi = _mm_cmplt_ps(dotHi, _mm_castsi128_ps(zero));
		__m128 flipSignHi = _mm_and_ps(dotCmpHi, flipSignMask);
		ceilHi = _mm_xor_ps(ceilHi, flipSignHi);

		// Interpolate the quaternions
		__m128 interpolatedLo = _mm_add_ps(_mm_mul_ps(_mm_sub_ps(ceilLo, floorLo), lerpFactorPacked), floorLo);
		__m128 interpolatedHi = _mm_add_ps(_mm_mul_ps(_mm_sub_ps(ceilHi, floorHi), lerpFactorPacked), floorHi);

		// Normalize
		interpolatedLo = _mm_mul_ps(_mm_rsqrt_ps(_mm_dp_ps_emu(interpolatedLo, interpolatedLo)), interpolatedLo);
		interpolatedHi = _mm_mul_ps(_mm_rsqrt_ps(_mm_dp_ps_emu(interpolatedHi, interpolatedHi)), interpolatedHi);

		// Convert to tangent/bitangent pairs
		__m128 tangentLo, bitangentLo, tangentHi, bitangentHi;
		ConvertToTangentAndBitangentVec4f(interpolatedLo, floorLo, tangentLo, bitangentLo);
		ConvertToTangentAndBitangentVec4f(interpolatedHi, floorHi, tangentHi, bitangentHi);

		// Scale and convert to int
		__m128i tangentIntLo = _mm_cvtps_epi32(_mm_mul_ps(tangentLo, scaleToInt16Factor));
		__m128i bitangentIntLo = _mm_cvtps_epi32(_mm_mul_ps(bitangentLo, scaleToInt16Factor));
		__m128i tangentIntHi = _mm_cvtps_epi32(_mm_mul_ps(tangentHi, scaleToInt16Factor));
		__m128i bitangentIntHi = _mm_cvtps_epi32(_mm_mul_ps(bitangentHi, scaleToInt16Factor));

		// Pack
		__m128i tangentBitangentLo = _mm_packs_epi32(tangentIntLo, bitangentIntLo);
		__m128i tangentBitangentHi = _mm_packs_epi32(tangentIntHi, bitangentIntHi);

		// And finally store
		_mm_store_si128(pTangents128 + j, tangentBitangentLo);
		_mm_store_si128(pTangents128 + j + 1, tangentBitangentHi);
	}

	const uint scalarStart = numSIMDIterations * numVerticesPerIteration;
	#else
	const uint scalarStart = 0;
	#endif
	for (unsigned int i = scalarStart; i < numVertices; ++i)
	{
		const Quat decodedFloorQTangent = DecodeQTangent(pFloorQTangents[i]);
		const Quat decodedCeilQTangent = DecodeQTangent(pCeilQTangents[i]);

		Quat interpolatedQTangent = Quat::CreateNlerp(decodedFloorQTangent, decodedCeilQTangent, lerpFactor);

		if ((interpolatedQTangent.w < 0.0f) != (decodedFloorQTangent.w < 0.0f))
		{
			interpolatedQTangent = -interpolatedQTangent;
		}

		ConvertToTangentAndBitangent(interpolatedQTangent, pTangents[i]);
	}
}

template<bool bConstantColors, bool bAnimatedColors, bool bConstantTexcoords>
void DecodeAndInterpolateColorAndTexcoords(SGeomCacheRenderMeshUpdateContext& updateContext, const uint index,
                                           const SGeomCacheStaticMeshData& staticMeshData, uint32 fpLerpFactor, const float lerpFactor,
                                           const GeomCacheFile::Color* __restrict pFloorReds, const GeomCacheFile::Color* __restrict pCeilReds,
                                           const GeomCacheFile::Color* __restrict pFloorGreens, const GeomCacheFile::Color* __restrict pCeilGreens,
                                           const GeomCacheFile::Color* __restrict pFloorBlues, const GeomCacheFile::Color* __restrict pCeilBlues,
                                           const GeomCacheFile::Color* __restrict pFloorAlphas, const GeomCacheFile::Color* __restrict pCeilAlphas,
                                           const GeomCacheFile::Texcoords* __restrict pFloorTexcoords, const GeomCacheFile::Texcoords* __restrict pCeilTexcoords)
{
	if (!bConstantColors && !bAnimatedColors)
	{
		updateContext.m_pColors[index].dcolor = 0xFFFFFFFF;
	}
	else if (bConstantColors)
	{
		updateContext.m_pColors[index].bcolor[0] = staticMeshData.m_colors[index].bcolor[0];
		updateContext.m_pColors[index].bcolor[1] = staticMeshData.m_colors[index].bcolor[1];
		updateContext.m_pColors[index].bcolor[2] = staticMeshData.m_colors[index].bcolor[2];
		updateContext.m_pColors[index].bcolor[3] = staticMeshData.m_colors[index].bcolor[3];
	}
	else if (bAnimatedColors)
	{
		updateContext.m_pColors[index].bcolor[0] = FixedPointColorLerp(pFloorBlues[index], pCeilBlues[index], fpLerpFactor);
		updateContext.m_pColors[index].bcolor[1] = FixedPointColorLerp(pFloorGreens[index], pCeilGreens[index], fpLerpFactor);
		updateContext.m_pColors[index].bcolor[2] = FixedPointColorLerp(pFloorReds[index], pCeilReds[index], fpLerpFactor);
		updateContext.m_pColors[index].bcolor[3] = FixedPointColorLerp(pFloorAlphas[index], pCeilAlphas[index], fpLerpFactor);
	}

	if (bConstantTexcoords)
	{
		updateContext.m_pTexcoords[index] = staticMeshData.m_texcoords[index];
	}
	else
	{
		updateContext.m_pTexcoords[index] = Vec2::CreateLerp(DecodeTexcoord(pFloorTexcoords[index]), DecodeTexcoord(pCeilTexcoords[index]), lerpFactor);
	}
}

template<uint Permutation>
void DecodeMeshVerticesBranchless(SGeomCacheRenderMeshUpdateContext& updateContext,
                                  const SGeomCacheStaticMeshData& staticMeshData, const char* pFloorFrameDataPtr,
                                  const char* pCeilFrameDataPtr, const float lerpFactor)
{
	const unsigned int numVertices = staticMeshData.m_numVertices;
	const int32 fpLerpFactor = int32(lerpFactor * 65535.0f);

	const bool bMotionBlur = Permutation % (2 * 2 * 2 * 3) >= (2 * 2 * 3);
	const bool bConstantPositions = Permutation % (2 * 2 * 3) >= (2 * 3);
	const bool bConstantTexcoords = (Permutation % (2 * 3)) >= 3;
	const bool bConstantColors = (Permutation % 3) == 1;
	const bool bAnimatedColors = (Permutation % 3) == 2;

	const Vec3& aabbMin = staticMeshData.m_aabb.min;
	const Vec3 aabbSize = staticMeshData.m_aabb.GetSize();

	const GeomCacheFile::Position* __restrict pFloorPositions = bConstantPositions ? NULL :
	                                                            reinterpret_cast<const GeomCacheFile::Position*>(pFloorFrameDataPtr);
	const GeomCacheFile::Position* __restrict pCeilPositions = bConstantPositions ? NULL :
	                                                           reinterpret_cast<const GeomCacheFile::Position*>(pCeilFrameDataPtr);
	pFloorFrameDataPtr += bConstantPositions ? 0 : ((numVertices * sizeof(GeomCacheFile::Position) + 15) & ~15);
	pCeilFrameDataPtr += bConstantPositions ? 0 : ((numVertices * sizeof(GeomCacheFile::Position) + 15) & ~15);

	const GeomCacheFile::Texcoords* __restrict pFloorTexcoords = bConstantTexcoords ? NULL :
	                                                             reinterpret_cast<const GeomCacheFile::Texcoords*>(pFloorFrameDataPtr);
	const GeomCacheFile::Texcoords* __restrict pCeilTexcoords = bConstantTexcoords ? NULL :
	                                                            reinterpret_cast<const GeomCacheFile::Texcoords*>(pCeilFrameDataPtr);
	pFloorFrameDataPtr += bConstantTexcoords ? 0 : ((numVertices * sizeof(GeomCacheFile::Texcoords) + 15) & ~15);
	pCeilFrameDataPtr += bConstantTexcoords ? 0 : ((numVertices * sizeof(GeomCacheFile::Texcoords) + 15) & ~15);

	const GeomCacheFile::QTangent* __restrict pFloorQTangents = (bConstantPositions && bConstantTexcoords) ? NULL :
	                                                            reinterpret_cast<const GeomCacheFile::QTangent*>(pFloorFrameDataPtr);
	const GeomCacheFile::QTangent* __restrict pCeilQTangents = (bConstantPositions && bConstantTexcoords) ? NULL :
	                                                           reinterpret_cast<const GeomCacheFile::QTangent*>(pCeilFrameDataPtr);
	pFloorFrameDataPtr += (bConstantPositions && bConstantTexcoords) ? 0 : ((numVertices * sizeof(GeomCacheFile::QTangent) + 15) & ~15);
	pCeilFrameDataPtr += (bConstantPositions && bConstantTexcoords) ? 0 : ((numVertices * sizeof(GeomCacheFile::QTangent) + 15) & ~15);

	const GeomCacheFile::Color* __restrict pFloorReds = !bAnimatedColors ? NULL :
	                                                    reinterpret_cast<const GeomCacheFile::Color*>(pFloorFrameDataPtr);
	const GeomCacheFile::Color* __restrict pCeilReds = !bAnimatedColors ? NULL :
	                                                   reinterpret_cast<const GeomCacheFile::Color*>(pCeilFrameDataPtr);
	pFloorFrameDataPtr += !bAnimatedColors ? 0 : ((numVertices * sizeof(GeomCacheFile::Color) + 15) & ~15);
	pCeilFrameDataPtr += !bAnimatedColors ? 0 : ((numVertices * sizeof(GeomCacheFile::Color) + 15) & ~15);

	const GeomCacheFile::Color* __restrict pFloorGreens = !bAnimatedColors ? NULL :
	                                                      reinterpret_cast<const GeomCacheFile::Color*>(pFloorFrameDataPtr);
	const GeomCacheFile::Color* __restrict pCeilGreens = !bAnimatedColors ? NULL :
	                                                     reinterpret_cast<const GeomCacheFile::Color*>(pCeilFrameDataPtr);
	pFloorFrameDataPtr += !bAnimatedColors ? 0 : ((numVertices * sizeof(GeomCacheFile::Color) + 15) & ~15);
	pCeilFrameDataPtr += !bAnimatedColors ? 0 : ((numVertices * sizeof(GeomCacheFile::Color) + 15) & ~15);

	const GeomCacheFile::Color* __restrict pFloorBlues = !bAnimatedColors ? NULL :
	                                                     reinterpret_cast<const GeomCacheFile::Color*>(pFloorFrameDataPtr);
	const GeomCacheFile::Color* __restrict pCeilBlues = !bAnimatedColors ? NULL :
	                                                    reinterpret_cast<const GeomCacheFile::Color*>(pCeilFrameDataPtr);
	pFloorFrameDataPtr += !bAnimatedColors ? 0 : ((numVertices * sizeof(GeomCacheFile::Color) + 15) & ~15);
	pCeilFrameDataPtr += !bAnimatedColors ? 0 : ((numVertices * sizeof(GeomCacheFile::Color) + 15) & ~15);

	const GeomCacheFile::Color* __restrict pFloorAlphas = !bAnimatedColors ? NULL :
	                                                      reinterpret_cast<const GeomCacheFile::Color*>(pFloorFrameDataPtr);
	const GeomCacheFile::Color* __restrict pCeilAlphas = !bAnimatedColors ? NULL :
	                                                     reinterpret_cast<const GeomCacheFile::Color*>(pCeilFrameDataPtr);
	pFloorFrameDataPtr += !bAnimatedColors ? 0 : ((numVertices * sizeof(GeomCacheFile::Color) + 15) & ~15);
	pCeilFrameDataPtr += !bAnimatedColors ? 0 : ((numVertices * sizeof(GeomCacheFile::Color) + 15) & ~15);

	const Vec3 posConvertFactor = Vec3(1.0f / float((2 << (staticMeshData.m_positionPrecision[0] - 1)) - 1),
	                                   1.0f / float((2 << (staticMeshData.m_positionPrecision[1] - 1)) - 1),
	                                   1.0f / float((2 << (staticMeshData.m_positionPrecision[2] - 1)) - 1));

	#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_DURANGO || CRY_PLATFORM_ORBIS
	const uint numVerticesPerIteration = 8;
	const uint numPackedFloatsPerIteration = (numVerticesPerIteration * sizeof(Vec3)) / 16;
	const uint numPackedUInt16PerIteration = (numVerticesPerIteration * sizeof(Vec3_tpl<uint16> )) / 16;
	const uint numFloatsPerIteration = numVerticesPerIteration * (sizeof(Vec3) / sizeof(float));
	const uint numFloatsPerPack = 4;
	const uint numSIMDIterations = numVertices / numVerticesPerIteration;

	float* pPrevPositionsF = updateContext.m_prevPositions.size() > 0 ? (float*)&updateContext.m_prevPositions[0] : NULL;
	float* pStaticPositionsF = staticMeshData.m_positions.size() > 0 ? (float*)&staticMeshData.m_positions[0] : NULL;
	float* pVelocitiesF = (float*)&updateContext.m_pVelocities[0];
	__m128i* pFloorPositions128 = (__m128i*)&pFloorPositions[0];
	__m128i* pCeilPositions128 = (__m128i*)&pCeilPositions[0];

	const __m128 lerpFactorPacked = _mm_set1_ps(lerpFactor);

	__m128 convertFromUint16FactorPacked[numPackedFloatsPerIteration];
	convertFromUint16FactorPacked[0] = _mm_setr_ps(posConvertFactor.x, posConvertFactor.y, posConvertFactor.z, posConvertFactor.x);
	convertFromUint16FactorPacked[1] = _mm_setr_ps(posConvertFactor.y, posConvertFactor.z, posConvertFactor.x, posConvertFactor.y);
	convertFromUint16FactorPacked[2] = _mm_setr_ps(posConvertFactor.z, posConvertFactor.x, posConvertFactor.y, posConvertFactor.z);
	convertFromUint16FactorPacked[3] = _mm_setr_ps(posConvertFactor.x, posConvertFactor.y, posConvertFactor.z, posConvertFactor.x);
	convertFromUint16FactorPacked[4] = _mm_setr_ps(posConvertFactor.y, posConvertFactor.z, posConvertFactor.x, posConvertFactor.y);
	convertFromUint16FactorPacked[5] = _mm_setr_ps(posConvertFactor.z, posConvertFactor.x, posConvertFactor.y, posConvertFactor.z);

	__m128 aabbMinPacked[numPackedFloatsPerIteration];
	aabbMinPacked[0] = _mm_setr_ps(aabbMin.x, aabbMin.y, aabbMin.z, aabbMin.x);
	aabbMinPacked[1] = _mm_setr_ps(aabbMin.y, aabbMin.z, aabbMin.x, aabbMin.y);
	aabbMinPacked[2] = _mm_setr_ps(aabbMin.z, aabbMin.x, aabbMin.y, aabbMin.z);
	aabbMinPacked[3] = _mm_setr_ps(aabbMin.x, aabbMin.y, aabbMin.z, aabbMin.x);
	aabbMinPacked[4] = _mm_setr_ps(aabbMin.y, aabbMin.z, aabbMin.x, aabbMin.y);
	aabbMinPacked[5] = _mm_setr_ps(aabbMin.z, aabbMin.x, aabbMin.y, aabbMin.z);

	__m128 aabbSizePacked[numPackedFloatsPerIteration];
	aabbSizePacked[0] = _mm_setr_ps(aabbSize.x, aabbSize.y, aabbSize.z, aabbSize.x);
	aabbSizePacked[1] = _mm_setr_ps(aabbSize.y, aabbSize.z, aabbSize.x, aabbSize.y);
	aabbSizePacked[2] = _mm_setr_ps(aabbSize.z, aabbSize.x, aabbSize.y, aabbSize.z);
	aabbSizePacked[3] = _mm_setr_ps(aabbSize.x, aabbSize.y, aabbSize.z, aabbSize.x);
	aabbSizePacked[4] = _mm_setr_ps(aabbSize.y, aabbSize.z, aabbSize.x, aabbSize.y);
	aabbSizePacked[5] = _mm_setr_ps(aabbSize.z, aabbSize.x, aabbSize.y, aabbSize.z);

	__m128 newPositions[numPackedFloatsPerIteration];
	__m128 oldPositions[numPackedFloatsPerIteration];

	for (unsigned int i = 0; i < numSIMDIterations; ++i)
	{
		const uint floatOffset = i * numFloatsPerIteration;

		if (bMotionBlur && !bConstantPositions)
		{
			for (uint j = 0; j < numPackedFloatsPerIteration; ++j)
			{
				oldPositions[j] = _mm_load_ps(&pPrevPositionsF[floatOffset + j * numFloatsPerPack]);
			}
		}

		if (bConstantPositions)
		{
			for (uint j = 0; j < numFloatsPerIteration; ++j)
			{
				const uint index = (i * numVerticesPerIteration) + j;
				updateContext.m_pPositions[index] = staticMeshData.m_positions[index];
			}
		}
		else if (!bConstantPositions)
		{
			const __m128i zero = _mm_setzero_si128();

			for (uint j = 0, k = 0; j < numPackedUInt16PerIteration; ++j, k += 2)
			{
				const uint indexLo = k;
				const uint indexHi = k + 1;

				__m128i floorPositions = _mm_load_si128(pFloorPositions128 + (i * numPackedUInt16PerIteration) + j);
				__m128i ceilPositions = _mm_load_si128(pCeilPositions128 + (i * numPackedUInt16PerIteration) + j);

				// Unpack and convert to float [0, 1]
				__m128 floorLo = _mm_mul_ps(_mm_cvtepi32_ps(_mm_unpacklo_epi16(floorPositions, zero)), convertFromUint16FactorPacked[indexLo]);
				__m128 floorHi = _mm_mul_ps(_mm_cvtepi32_ps(_mm_unpackhi_epi16(floorPositions, zero)), convertFromUint16FactorPacked[indexHi]);
				__m128 ceilLo = _mm_mul_ps(_mm_cvtepi32_ps(_mm_unpacklo_epi16(ceilPositions, zero)), convertFromUint16FactorPacked[indexLo]);
				__m128 ceilHi = _mm_mul_ps(_mm_cvtepi32_ps(_mm_unpackhi_epi16(ceilPositions, zero)), convertFromUint16FactorPacked[indexHi]);

				// Convert to [aabbMin, aabbMax] range
				floorLo = _mm_add_ps(_mm_mul_ps(floorLo, aabbSizePacked[indexLo]), aabbMinPacked[indexLo]);
				floorHi = _mm_add_ps(_mm_mul_ps(floorHi, aabbSizePacked[indexHi]), aabbMinPacked[indexHi]);
				ceilLo = _mm_add_ps(_mm_mul_ps(ceilLo, aabbSizePacked[indexLo]), aabbMinPacked[indexLo]);
				ceilHi = _mm_add_ps(_mm_mul_ps(ceilHi, aabbSizePacked[indexHi]), aabbMinPacked[indexHi]);

				// Interpolate
				newPositions[indexLo] = _mm_add_ps(_mm_mul_ps(_mm_sub_ps(ceilLo, floorLo), lerpFactorPacked), floorLo);
				newPositions[indexHi] = _mm_add_ps(_mm_mul_ps(_mm_sub_ps(ceilHi, floorHi), lerpFactorPacked), floorHi);
			}

			// Store to scratch & prev position array
			CRY_ALIGN(16) Vec3 positionScratch[numVerticesPerIteration];
			float* __restrict pPositionScratch128 = (float*)&positionScratch[0];
			for (uint j = 0; j < numPackedFloatsPerIteration; ++j)
			{
				_mm_store_ps(pPositionScratch128 + j * numFloatsPerPack, newPositions[j]);
				_mm_store_ps(&pPrevPositionsF[floatOffset + j * numFloatsPerPack], newPositions[j]);
			}

			// Scatter to position vertex stream
			for (uint j = 0; j < numVerticesPerIteration; ++j)
			{
				const uint index = (i * numVerticesPerIteration) + j;
				updateContext.m_pPositions[index] = positionScratch[j];
			}
		}

		for (uint j = 0; j < numVerticesPerIteration; ++j)
		{
			const uint index = (i * numVerticesPerIteration) + j;
			DecodeAndInterpolateColorAndTexcoords<bConstantColors, bAnimatedColors, bConstantTexcoords>(updateContext, index, staticMeshData, fpLerpFactor, lerpFactor,
			                                                                                            pFloorReds, pCeilReds, pFloorGreens, pCeilGreens, pFloorBlues, pCeilBlues, pFloorAlphas, pCeilAlphas, pFloorTexcoords, pCeilTexcoords);
		}

		if (!bMotionBlur)
		{
			__m128 zero = _mm_setzero_ps();
			for (uint j = 0; j < numPackedFloatsPerIteration; ++j)
			{
				_mm_store_ps(&pVelocitiesF[floatOffset + j * numFloatsPerPack], zero);
			}
		}
		else if (!bConstantPositions)
		{
			for (uint j = 0; j < numPackedFloatsPerIteration; ++j)
			{
				__m128 motionVectors = _mm_sub_ps(oldPositions[j], newPositions[j]);
				_mm_store_ps(&pVelocitiesF[floatOffset + j * numFloatsPerPack], motionVectors);
			}
		}
	}

	const uint scalarStart = numSIMDIterations * numVerticesPerIteration;
	#else
	const uint scalarStart = 0;
	#endif

	for (unsigned int i = scalarStart; i < numVertices; ++i)
	{
		Vec3 newPosition;
		Vec3 oldPosition;

		if (bMotionBlur && !bConstantPositions)
		{
			oldPosition = updateContext.m_prevPositions[i];
		}

		if (bConstantPositions)
		{
			newPosition = staticMeshData.m_positions[i];
		}
		else if (!bConstantPositions)
		{
			newPosition = Vec3::CreateLerp(DecodePosition(aabbMin, aabbSize, pFloorPositions[i], posConvertFactor),
			                               DecodePosition(aabbMin, aabbSize, pCeilPositions[i], posConvertFactor), lerpFactor);
		}

		Vec3 oldPos = updateContext.m_pPositions[i];
		updateContext.m_pPositions[i] = newPosition;
		updateContext.m_prevPositions[i] = newPosition;

		DecodeAndInterpolateColorAndTexcoords<bConstantColors, bAnimatedColors, bConstantTexcoords>(updateContext, i, staticMeshData, fpLerpFactor, lerpFactor,
		                                                                                            pFloorReds, pCeilReds, pFloorGreens, pCeilGreens, pFloorBlues, pCeilBlues, pFloorAlphas, pCeilAlphas, pFloorTexcoords, pCeilTexcoords);

		if (!bMotionBlur)
		{
			updateContext.m_pVelocities[i] = Vec3(0.0f, 0.0f, 0.0f);
		}
		else if (!bConstantPositions)
		{
			updateContext.m_pVelocities[i] = oldPosition - newPosition;
		}
	}

	if (bConstantPositions && bConstantTexcoords)
	{
		for (unsigned int i = 0; i < numVertices; ++i)
		{
			updateContext.m_pTangents[i] = staticMeshData.m_tangents[i];
		}
	}
	else
	{
		DecodeAndInterpolateTangents(numVertices, lerpFactor, pFloorQTangents, pCeilQTangents, updateContext.m_pTangents);
	}
}

uint32 GetMeshDataSize(const SGeomCacheStaticMeshData& staticMeshData)
{
	const unsigned int numVertices = staticMeshData.m_numVertices;

	const GeomCacheFile::EStreams constantStreamMask = staticMeshData.m_constantStreams;
	const GeomCacheFile::EStreams animatedStreamMask = staticMeshData.m_animatedStreams;

	const bool bConstantPositions = (constantStreamMask& GeomCacheFile::eStream_Positions) != 0;
	const bool bConstantTexcoords = (constantStreamMask& GeomCacheFile::eStream_Texcoords) != 0;
	const bool bAnimatedColors = (animatedStreamMask& GeomCacheFile::eStream_Colors) != 0;

	uint32 offset = 0;
	offset += bConstantPositions ? 0 : ((numVertices * sizeof(GeomCacheFile::Position) + 15) & ~15);
	offset += bConstantTexcoords ? 0 : ((numVertices * sizeof(GeomCacheFile::Texcoords) + 15) & ~15);
	offset += (bConstantPositions && bConstantTexcoords) ? 0 : ((numVertices * sizeof(GeomCacheFile::QTangent) + 15) & ~15);
	offset += !bAnimatedColors ? 0 : (4 * ((numVertices * sizeof(GeomCacheFile::Color) + 15) & ~15));

	return offset;
}

typedef void (* TDecodeVerticesBranchlessPtr)(SGeomCacheRenderMeshUpdateContext& updateContext,
                                              const SGeomCacheStaticMeshData& staticMeshData, const char* pFloorFrameDataPtr,
                                              const char* pCeilFrameDataPtr, const float lerpFactor);

TDecodeVerticesBranchlessPtr pDecodeFunctions[kNumPermutations];

template<unsigned int Permutation>
struct PermutationInit
{
	// Need two variables, because GCC otherwise complains about too many recursions
	static TDecodeVerticesBranchlessPtr m_pFunction1;
	static TDecodeVerticesBranchlessPtr m_pFunction2;
};

template<unsigned int Permutation>
TDecodeVerticesBranchlessPtr PermutationInit<Permutation>::m_pFunction1 = pDecodeFunctions[Permutation - 1]
                                                                            = PermutationInit<Permutation - 1>::m_pFunction1 = &DecodeMeshVerticesBranchless<Permutation - 1>;
template<> TDecodeVerticesBranchlessPtr PermutationInit<0>::m_pFunction1 = &DecodeMeshVerticesBranchless<0>;

template<unsigned int Permutation>
TDecodeVerticesBranchlessPtr PermutationInit<Permutation>::m_pFunction2 = pDecodeFunctions[Permutation - 1 + (kNumPermutations / 2)]
                                                                            = PermutationInit<Permutation - 1>::m_pFunction2 = &DecodeMeshVerticesBranchless<Permutation - 1 + (kNumPermutations / 2)>;
template<> TDecodeVerticesBranchlessPtr PermutationInit<0>::m_pFunction2 = &DecodeMeshVerticesBranchless<(kNumPermutations / 2)>;

// This forces the instantiation of PermutationInit<kNumPermutations / 2>::m_pFunction
// and therefore recursively initializes pDecodeFunctions with the DecodeVertices<N> permutations
template struct PermutationInit<kNumPermutations / 2>;

void DecodeIFrame(const CGeomCache* pGeomCache, char* pData)
{
	// Skip header
	pData += sizeof(GeomCacheFile::SFrameHeader);
	const std::vector<SGeomCacheStaticMeshData>& staticMeshData = pGeomCache->GetStaticMeshData();

	const uint numMeshes = staticMeshData.size();
	for (uint i = 0; i < numMeshes; ++i)
	{
		const SGeomCacheStaticMeshData& currentStaticMeshData = staticMeshData[i];

		if (currentStaticMeshData.m_animatedStreams == 0)
		{
			continue;
		}

		const GeomCacheFile::SMeshFrameHeader* pFrameHeader = reinterpret_cast<GeomCacheFile::SMeshFrameHeader*>(pData);
		pData += sizeof(GeomCacheFile::SMeshFrameHeader);

		const GeomCacheFile::EStreams streamMask = currentStaticMeshData.m_animatedStreams;
		const bool bUsePrediction = currentStaticMeshData.m_bUsePredictor;
		const uint numVertices = currentStaticMeshData.m_numVertices;

		if (streamMask & GeomCacheFile::eStream_Positions)
		{
			if (bUsePrediction)
			{
				GeomCacheFile::Position* pPositions = reinterpret_cast<GeomCacheFile::Position*>(pData);
				GeomCachePredictors::ParallelogramPredictor<GeomCacheFile::Position, false>(numVertices, pPositions, pPositions, currentStaticMeshData.m_predictorData);
			}

			pData += ((sizeof(GeomCacheFile::Position) * numVertices) + 15) & ~15;
		}

		if (streamMask & GeomCacheFile::eStream_Texcoords)
		{
			if (bUsePrediction)
			{
				GeomCacheFile::Texcoords* pTexcoords = reinterpret_cast<GeomCacheFile::Texcoords*>(pData);
				GeomCachePredictors::ParallelogramPredictor<GeomCacheFile::Texcoords, false>(numVertices, pTexcoords, pTexcoords, currentStaticMeshData.m_predictorData);
			}

			pData += ((sizeof(GeomCacheFile::Texcoords) * numVertices) + 15) & ~15;
		}

		if (streamMask & GeomCacheFile::eStream_QTangents)
		{
			if (bUsePrediction)
			{
				GeomCacheFile::QTangent* pQTangents = reinterpret_cast<GeomCacheFile::QTangent*>(pData);
				GeomCachePredictors::QTangentPredictor<false>(numVertices, pQTangents, pQTangents, currentStaticMeshData.m_predictorData);
			}

			pData += ((sizeof(GeomCacheFile::QTangent) * numVertices) + 15) & ~15;
		}

		if (streamMask & GeomCacheFile::eStream_Colors)
		{
			if (bUsePrediction)
			{
				GeomCacheFile::Color* pReds = reinterpret_cast<GeomCacheFile::Color*>(pData);
				pData += ((sizeof(GeomCacheFile::Color) * numVertices) + 15) & ~15;
				GeomCacheFile::Color* pGreens = reinterpret_cast<GeomCacheFile::Color*>(pData);
				pData += ((sizeof(GeomCacheFile::Color) * numVertices) + 15) & ~15;
				GeomCacheFile::Color* pBlues = reinterpret_cast<GeomCacheFile::Color*>(pData);
				pData += ((sizeof(GeomCacheFile::Color) * numVertices) + 15) & ~15;
				GeomCacheFile::Color* pAlphas = reinterpret_cast<GeomCacheFile::Color*>(pData);
				pData += ((sizeof(GeomCacheFile::Color) * numVertices) + 15) & ~15;

				GeomCachePredictors::ColorPredictor<false>(numVertices, pReds, pReds, currentStaticMeshData.m_predictorData);
				GeomCachePredictors::ColorPredictor<false>(numVertices, pGreens, pGreens, currentStaticMeshData.m_predictorData);
				GeomCachePredictors::ColorPredictor<false>(numVertices, pBlues, pBlues, currentStaticMeshData.m_predictorData);
				GeomCachePredictors::ColorPredictor<false>(numVertices, pAlphas, pAlphas, currentStaticMeshData.m_predictorData);
			}
			else
			{
				pData += 4 * ((sizeof(GeomCacheFile::Color) * numVertices) + 15) & ~15;
			}
		}
	}
}

void DecodeBFrame(const CGeomCache* pGeomCache, char* pData, char* pPrevFramesData[2], char* pFloorIndexFrameData, char* pCeilIndexFrameData)
{
	// Skip header
	size_t offset = sizeof(GeomCacheFile::SFrameHeader);
	const std::vector<SGeomCacheStaticMeshData>& staticMeshData = pGeomCache->GetStaticMeshData();

	const uint numMeshes = staticMeshData.size();
	for (uint i = 0; i < numMeshes; ++i)
	{
		const SGeomCacheStaticMeshData& currentStaticMeshData = staticMeshData[i];

		if (currentStaticMeshData.m_animatedStreams == 0)
		{
			continue;
		}

		const GeomCacheFile::SMeshFrameHeader* pFrameHeader = reinterpret_cast<GeomCacheFile::SMeshFrameHeader*>(pData + offset);
		offset += sizeof(GeomCacheFile::SMeshFrameHeader);

		if ((pFrameHeader->m_flags & GeomCacheFile::eFrameFlags_Hidden) != 0)
		{
			offset += GetMeshDataSize(currentStaticMeshData);
			continue;
		}

		const GeomCacheFile::EStreams streamMask = currentStaticMeshData.m_animatedStreams;
		const uint numVertices = currentStaticMeshData.m_numVertices;

		if (streamMask & GeomCacheFile::eStream_Positions)
		{
			GeomCacheFile::Position* pPositions = reinterpret_cast<GeomCacheFile::Position*>(pData + offset);
			GeomCachePredictors::STemporalPredictorData<GeomCacheFile::Position> predictorData;
			predictorData.m_numElements = numVertices;
			predictorData.m_pPrevFrames[0] = reinterpret_cast<GeomCacheFile::Position*>(pPrevFramesData[0] + offset);
			predictorData.m_pPrevFrames[1] = reinterpret_cast<GeomCacheFile::Position*>(pPrevFramesData[1] + offset);
			predictorData.m_pFloorFrame = reinterpret_cast<GeomCacheFile::Position*>(pFloorIndexFrameData + offset);
			predictorData.m_pCeilFrame = reinterpret_cast<GeomCacheFile::Position*>(pCeilIndexFrameData + offset);

			typedef Vec3_tpl<uint32> I;
			GeomCachePredictors::InterpolateMotionDeltaPredictor<I, GeomCacheFile::Position, false>
			  (pFrameHeader->m_positionStreamPredictorControl, predictorData, pPositions, pPositions);

			offset += ((sizeof(GeomCacheFile::Position) * numVertices) + 15) & ~15;
		}

		if (streamMask & GeomCacheFile::eStream_Texcoords)
		{
			GeomCacheFile::Texcoords* pTexcoords = reinterpret_cast<GeomCacheFile::Texcoords*>(pData + offset);
			GeomCachePredictors::STemporalPredictorData<GeomCacheFile::Texcoords> predictorData;
			predictorData.m_numElements = numVertices;
			predictorData.m_pPrevFrames[0] = reinterpret_cast<GeomCacheFile::Texcoords*>(pPrevFramesData[0] + offset);
			predictorData.m_pPrevFrames[1] = reinterpret_cast<GeomCacheFile::Texcoords*>(pPrevFramesData[1] + offset);
			predictorData.m_pFloorFrame = reinterpret_cast<GeomCacheFile::Texcoords*>(pFloorIndexFrameData + offset);
			predictorData.m_pCeilFrame = reinterpret_cast<GeomCacheFile::Texcoords*>(pCeilIndexFrameData + offset);

			typedef Vec2_tpl<uint32> I;
			GeomCachePredictors::InterpolateMotionDeltaPredictor<I, GeomCacheFile::Texcoords, false>
			  (pFrameHeader->m_texcoordStreamPredictorControl, predictorData, pTexcoords, pTexcoords);

			offset += ((sizeof(GeomCacheFile::Texcoords) * numVertices) + 15) & ~15;
		}

		if (streamMask & GeomCacheFile::eStream_QTangents)
		{
			GeomCacheFile::QTangent* pQTangents = reinterpret_cast<GeomCacheFile::QTangent*>(pData + offset);
			GeomCachePredictors::STemporalPredictorData<GeomCacheFile::QTangent> predictorData;
			predictorData.m_numElements = numVertices;
			predictorData.m_pPrevFrames[0] = reinterpret_cast<GeomCacheFile::QTangent*>(pPrevFramesData[0] + offset);
			predictorData.m_pPrevFrames[1] = reinterpret_cast<GeomCacheFile::QTangent*>(pPrevFramesData[1] + offset);
			predictorData.m_pFloorFrame = reinterpret_cast<GeomCacheFile::QTangent*>(pFloorIndexFrameData + offset);
			predictorData.m_pCeilFrame = reinterpret_cast<GeomCacheFile::QTangent*>(pCeilIndexFrameData + offset);

			typedef Vec4_tpl<uint32> I;
			GeomCachePredictors::InterpolateMotionDeltaPredictor<I, GeomCacheFile::QTangent, false>
			  (pFrameHeader->m_qTangentStreamPredictorControl, predictorData, pQTangents, pQTangents);

			offset += ((sizeof(GeomCacheFile::QTangent) * numVertices) + 15) & ~15;
		}

		if (streamMask & GeomCacheFile::eStream_Colors)
		{
			GeomCachePredictors::STemporalPredictorData<GeomCacheFile::Color> predictorData;
			typedef uint16 I;

			GeomCacheFile::Color* pReds = reinterpret_cast<GeomCacheFile::Color*>(pData + offset);
			predictorData.m_numElements = numVertices;
			predictorData.m_pPrevFrames[0] = reinterpret_cast<GeomCacheFile::Color*>(pPrevFramesData[0] + offset);
			predictorData.m_pPrevFrames[1] = reinterpret_cast<GeomCacheFile::Color*>(pPrevFramesData[1] + offset);
			predictorData.m_pFloorFrame = reinterpret_cast<GeomCacheFile::Color*>(pFloorIndexFrameData + offset);
			predictorData.m_pCeilFrame = reinterpret_cast<GeomCacheFile::Color*>(pCeilIndexFrameData + offset);

			GeomCachePredictors::InterpolateMotionDeltaPredictor<I, GeomCacheFile::Color, false>
			  (pFrameHeader->m_colorStreamPredictorControl[0], predictorData, pReds, pReds);

			offset += ((sizeof(GeomCacheFile::Color) * numVertices) + 15) & ~15;

			GeomCacheFile::Color* pGreens = reinterpret_cast<GeomCacheFile::Color*>(pData + offset);
			predictorData.m_numElements = numVertices;
			predictorData.m_pPrevFrames[0] = reinterpret_cast<GeomCacheFile::Color*>(pPrevFramesData[0] + offset);
			predictorData.m_pPrevFrames[1] = reinterpret_cast<GeomCacheFile::Color*>(pPrevFramesData[1] + offset);
			predictorData.m_pFloorFrame = reinterpret_cast<GeomCacheFile::Color*>(pFloorIndexFrameData + offset);
			predictorData.m_pCeilFrame = reinterpret_cast<GeomCacheFile::Color*>(pCeilIndexFrameData + offset);

			GeomCachePredictors::InterpolateMotionDeltaPredictor<I, GeomCacheFile::Color, false>
			  (pFrameHeader->m_colorStreamPredictorControl[1], predictorData, pGreens, pGreens);

			offset += ((sizeof(GeomCacheFile::Color) * numVertices) + 15) & ~15;

			GeomCacheFile::Color* pBlues = reinterpret_cast<GeomCacheFile::Color*>(pData + offset);
			predictorData.m_numElements = numVertices;
			predictorData.m_pPrevFrames[0] = reinterpret_cast<GeomCacheFile::Color*>(pPrevFramesData[0] + offset);
			predictorData.m_pPrevFrames[1] = reinterpret_cast<GeomCacheFile::Color*>(pPrevFramesData[1] + offset);
			predictorData.m_pFloorFrame = reinterpret_cast<GeomCacheFile::Color*>(pFloorIndexFrameData + offset);
			predictorData.m_pCeilFrame = reinterpret_cast<GeomCacheFile::Color*>(pCeilIndexFrameData + offset);

			GeomCachePredictors::InterpolateMotionDeltaPredictor<I, GeomCacheFile::Color, false>
			  (pFrameHeader->m_colorStreamPredictorControl[2], predictorData, pBlues, pBlues);

			offset += ((sizeof(GeomCacheFile::Color) * numVertices) + 15) & ~15;

			GeomCacheFile::Color* pAlphas = reinterpret_cast<GeomCacheFile::Color*>(pData + offset);
			predictorData.m_numElements = numVertices;
			predictorData.m_pPrevFrames[0] = reinterpret_cast<GeomCacheFile::Color*>(pPrevFramesData[0] + offset);
			predictorData.m_pPrevFrames[1] = reinterpret_cast<GeomCacheFile::Color*>(pPrevFramesData[1] + offset);
			predictorData.m_pFloorFrame = reinterpret_cast<GeomCacheFile::Color*>(pFloorIndexFrameData + offset);
			predictorData.m_pCeilFrame = reinterpret_cast<GeomCacheFile::Color*>(pCeilIndexFrameData + offset);

			GeomCachePredictors::InterpolateMotionDeltaPredictor<I, GeomCacheFile::Color, false>
			  (pFrameHeader->m_colorStreamPredictorControl[3], predictorData, pAlphas, pAlphas);

			offset += ((sizeof(GeomCacheFile::Color) * numVertices) + 15) & ~15;
		}
	}
}

bool PrepareFillMeshData(SGeomCacheRenderMeshUpdateContext& updateContext, const SGeomCacheStaticMeshData& staticMeshData,
                         const char*& pFloorFrameMeshData, const char*& pCeilFrameMeshData, size_t& offsetToNextMesh, float& lerpFactor)
{
	const GeomCacheFile::SMeshFrameHeader* pFloorHeader = reinterpret_cast<const GeomCacheFile::SMeshFrameHeader* const>(pFloorFrameMeshData);
	const GeomCacheFile::SMeshFrameHeader* pCeilHeader = reinterpret_cast<const GeomCacheFile::SMeshFrameHeader* const>(pCeilFrameMeshData);

	pFloorFrameMeshData += sizeof(GeomCacheFile::SMeshFrameHeader);
	pCeilFrameMeshData += sizeof(GeomCacheFile::SMeshFrameHeader);

	const bool bFloorFrameHidden = (pFloorHeader->m_flags & GeomCacheFile::eFrameFlags_Hidden) != 0;
	const bool bCeilFrameHidden = (pCeilHeader->m_flags & GeomCacheFile::eFrameFlags_Hidden) != 0;

	offsetToNextMesh = GetMeshDataSize(staticMeshData);

	if (bFloorFrameHidden && bCeilFrameHidden)
	{
		return false;
	}
	else if (bFloorFrameHidden)
	{
		lerpFactor = 1.0f;
	}
	else if (bCeilFrameHidden)
	{
		lerpFactor = 0.0f;
	}

	if (Cry3DEngineBase::m_pCVars->e_GeomCacheLerpBetweenFrames == 0)
	{
		pCeilFrameMeshData = pFloorFrameMeshData;
		lerpFactor = 0.0f;
	}

	return true;
}

void FillMeshDataFromDecodedFrame(const bool bMotionBlur, SGeomCacheRenderMeshUpdateContext& updateContext,
                                  const SGeomCacheStaticMeshData& staticMeshData, const char* pFloorFrameMeshData, const char* pCeilFrameMeshData, float lerpFactor)
{
	// Fetch indices from static data
	const uint numIndices = staticMeshData.m_indices.size();
	for (uint i = 0; i < numIndices; ++i)
	{
		updateContext.m_pIndices[i] = staticMeshData.m_indices[i];
	}

	const uint permutation = GetDecodeVerticesPerm(bMotionBlur, staticMeshData.m_constantStreams, staticMeshData.m_animatedStreams);
	TDecodeVerticesBranchlessPtr pDecodeFunction = pDecodeFunctions[permutation];
	(*pDecodeFunction)(updateContext, staticMeshData, pFloorFrameMeshData, pCeilFrameMeshData, lerpFactor);
}

Vec3 DecodePosition(const Vec3& aabbMin, const Vec3& aabbSize, const GeomCacheFile::Position& inPosition, const Vec3& convertFactor)
{
	return Vec3(aabbMin.x + ((float)inPosition.x * convertFactor.x) * aabbSize.x,
	            aabbMin.y + ((float)inPosition.y * convertFactor.y) * aabbSize.y,
	            aabbMin.z + ((float)inPosition.z * convertFactor.z) * aabbSize.z);
}

Vec2 DecodeTexcoord(const GeomCacheFile::Texcoords& inTexcoords)
{
	const float convertFromInt16Factor = 1.0f / 32767.0f;

	return Vec2((float)inTexcoords.x * convertFromInt16Factor * GeomCacheFile::kTexcoordRange,
	            (float)inTexcoords.y * convertFromInt16Factor * GeomCacheFile::kTexcoordRange);
}

Quat DecodeQTangent(const GeomCacheFile::QTangent& inQTangent)
{
	const float kMultiplier = float((2 << (GeomCacheFile::kTangentQuatPrecision - 1)) - 1);
	const float convertFromInt16Factor = 1.0f / kMultiplier;

	return Quat((float)inQTangent.w * convertFromInt16Factor, (float)inQTangent.x * convertFromInt16Factor,
	            (float)inQTangent.y * convertFromInt16Factor, (float)inQTangent.z * convertFromInt16Factor);
}

void TransformAndConvertToTangentAndBitangent(const Quat& rotation, const Quat& inQTangent, SPipTangents& outTangents)
{
	int16 reflection = alias_cast<uint32>(inQTangent.w) & 0x80000000 ? -1 : +1;
	Quat transformedQTangent = rotation * inQTangent;

	outTangents = SPipTangents(transformedQTangent, reflection);
}

void ConvertToTangentAndBitangent(const Quat& inQTangent, SPipTangents& outTangents)
{
	int16 reflection = alias_cast<uint32>(inQTangent.w) & 0x80000000 ? -1 : +1;

	outTangents = SPipTangents(inQTangent, reflection);
}

uint32 GetDecompressBufferSize(const char* const pStartBlock, const unsigned int numFrames)
{
	FUNCTION_PROFILER_3DENGINE;

	uint32 totalUncompressedSize = 0;
	const char* pCurrentBlock = pStartBlock;

	const uint32 headersSize = ((sizeof(SGeomCacheFrameHeader) * numFrames) + 15) & ~15;

	for (unsigned int i = 0; i < numFrames; ++i)
	{
		const GeomCacheFile::SCompressedBlockHeader* pBlockHeader = reinterpret_cast<const GeomCacheFile::SCompressedBlockHeader*>(pCurrentBlock);
		pCurrentBlock += sizeof(GeomCacheFile::SCompressedBlockHeader) + pBlockHeader->m_compressedSize;
		totalUncompressedSize += pBlockHeader->m_uncompressedSize;
	}

	uint32 totalSize = headersSize + totalUncompressedSize;
	if (totalSize % 16 != 0)
	{
		CryFatalError("GetDecompressBufferSize mod 16 != 0");
	}

	return totalSize;
}

bool DecompressBlock(const GeomCacheFile::EBlockCompressionFormat compressionFormat, char* const pDest, const char* const pSource)
{
	FUNCTION_PROFILER_3DENGINE;

	const GeomCacheFile::SCompressedBlockHeader* const pBlockHeader = reinterpret_cast<const GeomCacheFile::SCompressedBlockHeader* const>(pSource);
	const char* const pBlockData = reinterpret_cast<const char* const>(pSource + sizeof(GeomCacheFile::SCompressedBlockHeader));

	if (compressionFormat == GeomCacheFile::eBlockCompressionFormat_None)
	{
		assert(pBlockHeader->m_compressedSize == pBlockHeader->m_uncompressedSize);
		memcpy(const_cast<char*>(pDest), pSource + sizeof(GeomCacheFile::SCompressedBlockHeader), pBlockHeader->m_uncompressedSize);
	}
	else if (compressionFormat == GeomCacheFile::eBlockCompressionFormat_Deflate)
	{
		IZLibInflateStream* pInflateStream = GetISystem()->GetIZLibDecompressor()->CreateInflateStream();

		pInflateStream->SetOutputBuffer(const_cast<char*>(pDest), pBlockHeader->m_uncompressedSize);
		pInflateStream->Input(pBlockData, pBlockHeader->m_compressedSize);
		pInflateStream->EndInput();
		EZInflateState state = pInflateStream->GetState();
		assert(state == eZInfState_Finished);
		pInflateStream->Release();

		if (state == eZInfState_Error)
		{
			return false;
		}
	}
	else if (compressionFormat == GeomCacheFile::eBlockCompressionFormat_LZ4HC)
	{
		ILZ4Decompressor* pDecompressor = GetISystem()->GetLZ4Decompressor();
		return pDecompressor->DecompressData(pBlockData, const_cast<char*>(pDest), pBlockHeader->m_uncompressedSize);
	}
	else
	{
		return false;
	}

	return true;
}

bool DecompressBlocks(const GeomCacheFile::EBlockCompressionFormat compressionFormat, char* const pDest,
                      const char* const pSource, const uint blockOffset, const uint numBlocks, const uint numHandleFrames)
{
	FUNCTION_PROFILER_3DENGINE;

	const char* pCurrentSource = pSource;
	const uint32 headersSize = ((sizeof(SGeomCacheFrameHeader) * numHandleFrames) + 15) & ~15;
	char* pCurrentDest = pDest + headersSize;

	for (uint i = 0; i < blockOffset; ++i)
	{
		const GeomCacheFile::SCompressedBlockHeader* const pBlockHeader = reinterpret_cast<const GeomCacheFile::SCompressedBlockHeader* const>(pCurrentSource);
		pCurrentSource += sizeof(GeomCacheFile::SCompressedBlockHeader) + pBlockHeader->m_compressedSize;
		pCurrentDest += pBlockHeader->m_uncompressedSize;
	}

	for (uint i = blockOffset; i < blockOffset + numBlocks; ++i)
	{
		const GeomCacheFile::SCompressedBlockHeader* const pBlockHeader = reinterpret_cast<const GeomCacheFile::SCompressedBlockHeader* const>(pCurrentSource);

		if (!DecompressBlock(compressionFormat, pCurrentDest, pCurrentSource))
		{
			return false;
		}

		SGeomCacheFrameHeader* pHeader = reinterpret_cast<SGeomCacheFrameHeader*>(pDest + i * sizeof(SGeomCacheFrameHeader));
		pHeader->m_offset = static_cast<uint32>(pCurrentDest - pDest);
		pHeader->m_state = SGeomCacheFrameHeader::eFHS_Undecoded;

		pCurrentSource += sizeof(GeomCacheFile::SCompressedBlockHeader) + pBlockHeader->m_compressedSize;
		pCurrentDest += pBlockHeader->m_uncompressedSize;
	}

	return true;
}
}

#endif
