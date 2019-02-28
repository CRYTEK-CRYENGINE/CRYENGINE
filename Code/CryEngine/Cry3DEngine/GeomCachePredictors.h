// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// ------------------------------------------------------------------------
//  File name:   GeomCachePredictors.h
//  Created:     17/1/2013 by Axel Gneiting
//  Description: Predictors for index frame compression
// -------------------------------------------------------------------------
////////////////////////////////////////////////////////////////////////////

#include <Cry3DEngine/GeomCacheFileFormat.h>

namespace GeomCachePredictors
{
//////////////////////////////////////////////////////////////////////////
// Index frame prediction
//////////////////////////////////////////////////////////////////////////

template<class T, bool kbEncode>
void ParallelogramPredictor(const uint numValues, T* pIn, T* pOut, const std::vector<uint16>& predictorData)
{
	T* pAbsoluteValues = kbEncode ? pIn : pOut;
	uint outPosition = 0;

	for (uint i = 0, predictorDataPos = 0; i < numValues; ++i)
	{
		const uint16 uDist = predictorData[predictorDataPos++];
		T predictedValue;

		if (uDist == 0xFFFF)
		{
			if (i == 0)
			{
				// There is no previous value, so we just pass through
				pOut[outPosition++] = pIn[i];
				continue;
			}

			// No neighbour triangle, just use previous value for prediction
			predictedValue = pAbsoluteValues[i - 1];
		}
		else
		{
			// Parallelogram prediction
			const uint16 vDist = predictorData[predictorDataPos++];
			const uint16 wDist = predictorData[predictorDataPos++];

			const T& u = pAbsoluteValues[i - uDist];
			const T& v = pAbsoluteValues[i - vDist];
			const T& w = pAbsoluteValues[i - wDist];

			predictedValue = u + v - w;
		}

		if (kbEncode)
		{
			const T realValue = pIn[i];
			const T delta = realValue - predictedValue;
			pOut[outPosition++] = delta;
		}
		else
		{
			const T delta = pIn[i];
			const T realValue = delta + predictedValue;
			pOut[outPosition++] = realValue;
		}
	}
}

template<bool kbEncode>
void QTangentPredictor(const uint numValues, const GeomCacheFile::QTangent* pIn, GeomCacheFile::QTangent* pOut, const std::vector<uint16>& predictorData)
{
	const GeomCacheFile::QTangent* pAbsoluteValues = kbEncode ? pIn : pOut;
	uint outPosition = 0;

	for (uint i = 0, predictorDataPos = 0; i < numValues; ++i)
	{
		const uint16 uDist = predictorData[predictorDataPos++];
		Vec4_tpl<int32> predictedValue;

		if (uDist == 0xFFFF)
		{
			if (i == 0)
			{
				// There is no previous value, so we just pass through
				pOut[outPosition++] = pIn[i];
				continue;
			}

			// No neighbour triangle, just use previous value for prediction
			predictedValue = pAbsoluteValues[i - 1];
		}
		else
		{
			// Average value of two nearest vertices of adjancent triangle
			const uint16 vDist = predictorData[predictorDataPos++];
			++predictorDataPos;

			const GeomCacheFile::QTangent& u = pAbsoluteValues[i - uDist];
			const GeomCacheFile::QTangent& v = pAbsoluteValues[i - vDist];
			predictedValue = Vec4_tpl<int32>(u) + Vec4_tpl<int32>(v);

			// Vec4_tpl defines division in a way that only works for floats
			predictedValue.x /= 2;
			predictedValue.y /= 2;
			predictedValue.z /= 2;
			predictedValue.w /= 2;
		}

		if (kbEncode)
		{
			const GeomCacheFile::QTangent& realValue = pIn[i];
			const GeomCacheFile::QTangent delta = realValue - predictedValue;
			pOut[outPosition++] = delta;
		}
		else
		{
			const GeomCacheFile::QTangent delta = pIn[i];
			const GeomCacheFile::QTangent realValue = delta + predictedValue;
			pOut[outPosition++] = realValue;
		}
	}
}

template<bool kbEncode>
inline void ColorPredictor(const uint numValues, const GeomCacheFile::Color* pIn, GeomCacheFile::Color* pOut, const std::vector<uint16>& predictorData)
{
	const GeomCacheFile::Color* pAbsoluteValues = kbEncode ? pIn : pOut;
	uint outPosition = 0;

	for (uint i = 0, predictorDataPos = 0; i < numValues; ++i)
	{
		const uint16 uDist = predictorData[predictorDataPos++];
		int32 predictedValue;

		if (uDist == 0xFFFF)
		{
			if (i == 0)
			{
				// There is no previous value, so we just pass through
				pOut[outPosition++] = pIn[i];
				continue;
			}

			// No neighbour triangle, just use previous value for prediction
			predictedValue = pAbsoluteValues[i - 1];
		}
		else
		{
			// Average value of two nearest vertices of adjancent triangle
			const uint16 vDist = predictorData[predictorDataPos++];
			++predictorDataPos;

			const GeomCacheFile::Color& u = pAbsoluteValues[i - uDist];
			const GeomCacheFile::Color& v = pAbsoluteValues[i - vDist];
			predictedValue = (int32(u) + int32(v)) / 2;
		}

		if (kbEncode)
		{
			const GeomCacheFile::Color& realValue = pIn[i];
			const GeomCacheFile::Color delta = realValue - predictedValue;
			pOut[outPosition++] = delta;
		}
		else
		{
			const GeomCacheFile::Color delta = pIn[i];
			const GeomCacheFile::Color realValue = delta + predictedValue;
			pOut[outPosition++] = realValue;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Temporal prediction
//////////////////////////////////////////////////////////////////////////

// Motion predictor input data
template<class T>
struct STemporalPredictorData
{
	uint     m_numElements;
	const T* m_pPrevFrames[2];
	const T* m_pFloorFrame;
	const T* m_pCeilFrame;
};

template<class T>
Vec2_tpl<T> operator>>(const Vec2_tpl<T>& v, uint shift)
{
	Vec2_tpl<T> result = v;
	result.x >>= shift;
	result.y >>= shift;
	return result;
}

template<class T>
Vec3_tpl<T> operator>>(const Vec3_tpl<T>& v, uint shift)
{
	Vec3_tpl<T> result = v;
	result.x >>= shift;
	result.y >>= shift;
	result.z >>= shift;
	return result;
}

template<class T>
Vec4_tpl<T> operator>>(const Vec4_tpl<T>& v, uint shift)
{
	Vec4_tpl<T> result = v;
	result.x >>= shift;
	result.y >>= shift;
	result.z >>= shift;
	result.w >>= shift;
	return result;
}

template<class I, class T>
void InterpolateDeltaEncode(const uint numValues, const uint8 lerpFactor, const T* pFloorFrame, const T* pCeilFrame, const T* pIn, T* pOut)
{
	for (uint i = 0; i < numValues; ++i)
	{
		const I floorValue = I(pFloorFrame[i]);
		const I ceilValue = I(pCeilFrame[i]);
		const T predictedValue = T(floorValue + (((ceilValue - floorValue) * lerpFactor) >> 8));

		const T& realValue = pIn[i];
		const T delta = realValue - predictedValue;
		pOut[i] = delta;
	}
}

template<class I, class T>
void MotionDeltaEncode(const uint numValues, const uint8 acceleration, const T* const pPrevFrames[2], const T* pIn, T* pOut)
{
	for (uint i = 0; i < numValues; ++i)
	{
		const I prevPrevFrameValue = I(pPrevFrames[0][i]);
		const I prevFrameValue = I(pPrevFrames[1][i]);
		const T predictedValue = T(prevFrameValue + (((prevFrameValue - prevPrevFrameValue) * acceleration) >> 7));

		const T& realValue = pIn[i];
		const T delta = realValue - predictedValue;
		pOut[i] = delta;
	}
}

template<class I, class T, bool kbEncode>
void InterpolateMotionDeltaPredictor(const GeomCacheFile::STemporalPredictorControl& controlIn, const STemporalPredictorData<T>& data, const T* pIn, T* pOut)
{
	const T* pFloorFrame = data.m_pFloorFrame;
	const T* const pCeilFrame = data.m_pCeilFrame;
	const T* const* pPrevFrames = data.m_pPrevFrames;

	const uint8& lerpFactor = controlIn.m_indexFrameLerpFactor;
	const uint8& acceleration = controlIn.m_acceleration;
	const uint8 combineFactor = controlIn.m_combineFactor;

	const uint numElements = data.m_numElements;
	for (uint i = 0; i < numElements; ++i)
	{
		const I prevPrevFrameValue = I(pPrevFrames[0][i]);
		const I prevFrameValue = I(pPrevFrames[1][i]);
		const I floorValue = I(pFloorFrame[i]);
		const I ceilValue = I(pCeilFrame[i]);

		const I interpolatePredictedValue = T(floorValue + (((ceilValue - floorValue) * lerpFactor) >> 8));
		const I motionPredictedValue = T(prevFrameValue + (((prevFrameValue - prevPrevFrameValue) * acceleration) >> 7));
		const T predictedValue = (interpolatePredictedValue + (((motionPredictedValue - interpolatePredictedValue) * combineFactor) >> 7));

		if (kbEncode)
		{
			const T realValue = pIn[i];
			const T delta = realValue - predictedValue;
			pOut[i] = delta;
		}
		else
		{
			const T delta = pIn[i];
			const T realValue = delta + predictedValue;
			pOut[i] = realValue;
		}
	}
}

#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_DURANGO || CRY_PLATFORM_ORBIS
ILINE __m128i _mm_mullo_epi32_emu(const __m128i& a, const __m128i& b)
{
	#if CRY_PLATFORM_DURANGO || CRY_PLATFORM_ORBIS
	return _mm_mullo_epi32(a, b);
	#else
	__m128i tmp1 = _mm_mul_epu32(a, b);
	__m128i tmp2 = _mm_mul_epu32(_mm_srli_si128(a, 4), _mm_srli_si128(b, 4));
	return _mm_unpacklo_epi32(_mm_shuffle_epi32(tmp1, _MM_SHUFFLE(0, 0, 2, 0)), _mm_shuffle_epi32(tmp2, _MM_SHUFFLE(0, 0, 2, 0)));
	#endif
}

ILINE __m128i _mm_packus_epi32_emu(__m128i& a, __m128i& b)
{
	#if CRY_PLATFORM_DURANGO || CRY_PLATFORM_ORBIS
	return _mm_packus_epi32(a, b);
	#else
	a = _mm_slli_epi32(a, 16);
	b = _mm_slli_epi32(b, 16);
	a = _mm_srai_epi32(a, 16);
	b = _mm_srai_epi32(b, 16);
	return _mm_packs_epi32(a, b);
	#endif
}

ILINE __m128i Interpolate(__m128i a, __m128i b, __m128i c, const uint32 factor, const int shiftFactor)
{
	const __m128i zero = _mm_setzero_si128();
	const __m128i truncate = _mm_set_epi16(0, -1, 0, -1, 0, -1, 0, -1);

	// Unpack to 2x4 32 bit integers
	__m128i factors = _mm_set1_epi32(factor);

	__m128i aLo = _mm_unpacklo_epi16(a, zero);
	__m128i aHi = _mm_unpackhi_epi16(a, zero);
	__m128i bLo = _mm_unpacklo_epi16(b, zero);
	__m128i bHi = _mm_unpackhi_epi16(b, zero);

	// Interpolate and pack again
	__m128i lerpLo = _mm_sub_epi32(bLo, aLo);
	lerpLo = _mm_mullo_epi32_emu(lerpLo, factors);
	lerpLo = _mm_srli_epi32(lerpLo, shiftFactor);
	lerpLo = _mm_and_si128(lerpLo, truncate);

	__m128i lerpHi = _mm_sub_epi32(bHi, aHi);
	lerpHi = _mm_mullo_epi32_emu(lerpHi, factors);
	lerpHi = _mm_srli_epi32(lerpHi, shiftFactor);
	lerpHi = _mm_and_si128(lerpHi, truncate);

	__m128i lerp = _mm_packus_epi32_emu(lerpLo, lerpHi);
	__m128i result = _mm_add_epi16(lerp, c);

	return result;
}

template<> void InterpolateMotionDeltaPredictor<uint32, uint16, false>
  (const GeomCacheFile::STemporalPredictorControl& controlIn, const STemporalPredictorData<uint16>& data, const uint16* pIn, uint16* pOut)
{
	__m128i* pRawIn = (__m128i*)pIn;
	__m128i* pRawOut = (__m128i*)pOut;
	__m128i* pFloorFrame = (__m128i*)data.m_pFloorFrame;
	__m128i* pCeilFrame = (__m128i*)data.m_pCeilFrame;
	__m128i* pPrevFrames[2] = { (__m128i*)data.m_pPrevFrames[0], (__m128i*)data.m_pPrevFrames[1] };

	const uint8 lerpFactor = controlIn.m_indexFrameLerpFactor;
	const uint8 acceleration = controlIn.m_acceleration;
	const uint8 combineFactor = controlIn.m_combineFactor;

	const uint numBlocks = data.m_numElements / 8;
	const uint tailSizeInBytes = (data.m_numElements - numBlocks * 8) * sizeof(uint16);

	for (uint i = 0; i < numBlocks; ++i)
	{
		// Load 8 floor & ceil values
		__m128i floorValues = _mm_load_si128(pFloorFrame + i);
		__m128i ceilValues = _mm_load_si128(pCeilFrame + i);

		// Load 8 prep prev & prev frame values
		__m128i prevPrevFrameValues = _mm_load_si128(pPrevFrames[0] + i);
		__m128i prevFrameValues = _mm_load_si128(pPrevFrames[1] + i);

		// Calculate prediction
		__m128i lerp = Interpolate(floorValues, ceilValues, floorValues, lerpFactor, 8);
		__m128i motion = Interpolate(prevPrevFrameValues, prevFrameValues, prevFrameValues, acceleration, 7);
		__m128i predictedValues = Interpolate(lerp, motion, lerp, combineFactor, 7);

		__m128i delta = _mm_load_si128(pRawIn + i);
		__m128i realValues = _mm_add_epi16(delta, predictedValues);
		_mm_store_si128(pRawOut + i, realValues);
	}

	if (tailSizeInBytes > 0)
	{
		// Load values
		__m128i floorValues, ceilValues, prevPrevFrameValues, prevFrameValues, delta;

		memset(&floorValues, 0, sizeof(floorValues));
		memset(&ceilValues, 0, sizeof(ceilValues));
		memset(&prevPrevFrameValues, 0, sizeof(prevPrevFrameValues));
		memset(&prevFrameValues, 0, sizeof(prevFrameValues));
		memset(&delta, 0, sizeof(delta));

		memcpy(&floorValues, pFloorFrame + numBlocks, tailSizeInBytes);
		memcpy(&ceilValues, pCeilFrame + numBlocks, tailSizeInBytes);
		memcpy(&prevPrevFrameValues, pPrevFrames[0] + numBlocks, tailSizeInBytes);
		memcpy(&prevFrameValues, pPrevFrames[1] + numBlocks, tailSizeInBytes);
		memcpy(&delta, pRawIn + numBlocks, tailSizeInBytes);

		// Calculate prediction
		__m128i lerp = Interpolate(floorValues, ceilValues, floorValues, lerpFactor, 8);
		__m128i motion = Interpolate(prevPrevFrameValues, prevFrameValues, prevFrameValues, acceleration, 7);
		__m128i predictedValues = Interpolate(lerp, motion, lerp, combineFactor, 7);

		__m128i realValues = _mm_add_epi16(delta, predictedValues);

		memcpy(pRawOut + numBlocks, &realValues, tailSizeInBytes);
	}
}

template<> void InterpolateMotionDeltaPredictor<Vec2_tpl<uint32>, Vec2_tpl<uint16>, false>
  (const GeomCacheFile::STemporalPredictorControl& controlIn, const STemporalPredictorData<Vec2_tpl<uint16>>& data,
  const Vec2_tpl<uint16>* pIn, Vec2_tpl<uint16>* pOut)
{
	STemporalPredictorData<uint16> uInt16Data;

	uInt16Data.m_pFloorFrame = (uint16*)data.m_pFloorFrame;
	uInt16Data.m_pCeilFrame = (uint16*)data.m_pCeilFrame;
	uInt16Data.m_pPrevFrames[0] = (uint16*)data.m_pPrevFrames[0];
	uInt16Data.m_pPrevFrames[1] = (uint16*)data.m_pPrevFrames[1];
	uInt16Data.m_numElements = data.m_numElements * 2;

	InterpolateMotionDeltaPredictor<uint32, uint16, false>(controlIn, uInt16Data, (uint16*)pIn, (uint16*)pOut);
}

template<> void InterpolateMotionDeltaPredictor<Vec3_tpl<uint32>, Vec3_tpl<uint16>, false>
  (const GeomCacheFile::STemporalPredictorControl& controlIn, const STemporalPredictorData<Vec3_tpl<uint16>>& data,
  const Vec3_tpl<uint16>* pIn, Vec3_tpl<uint16>* pOut)
{
	STemporalPredictorData<uint16> uInt16Data;

	uInt16Data.m_pFloorFrame = (uint16*)data.m_pFloorFrame;
	uInt16Data.m_pCeilFrame = (uint16*)data.m_pCeilFrame;
	uInt16Data.m_pPrevFrames[0] = (uint16*)data.m_pPrevFrames[0];
	uInt16Data.m_pPrevFrames[1] = (uint16*)data.m_pPrevFrames[1];
	uInt16Data.m_numElements = data.m_numElements * 3;

	InterpolateMotionDeltaPredictor<uint32, uint16, false>(controlIn, uInt16Data, (uint16*)pIn, (uint16*)pOut);
}

template<> void InterpolateMotionDeltaPredictor<Vec4_tpl<uint32>, Vec4_tpl<uint16>, false>
  (const GeomCacheFile::STemporalPredictorControl& controlIn, const STemporalPredictorData<Vec4_tpl<uint16>>& data,
  const Vec4_tpl<uint16>* pIn, Vec4_tpl<uint16>* pOut)
{
	STemporalPredictorData<uint16> uInt16Data;

	uInt16Data.m_pFloorFrame = (uint16*)data.m_pFloorFrame;
	uInt16Data.m_pCeilFrame = (uint16*)data.m_pCeilFrame;
	uInt16Data.m_pPrevFrames[0] = (uint16*)data.m_pPrevFrames[0];
	uInt16Data.m_pPrevFrames[1] = (uint16*)data.m_pPrevFrames[1];
	uInt16Data.m_numElements = data.m_numElements * 4;

	InterpolateMotionDeltaPredictor<uint32, uint16, false>(controlIn, uInt16Data, (uint16*)pIn, (uint16*)pOut);
}
#endif
}
