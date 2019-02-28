// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "FaceChannelSmoothing.h"

#include "FaceAnimSequence.h"

namespace FaceChannel
{
void ApplyKernel(CFacialAnimChannelInterpolator* pSpline, CFacialAnimChannelInterpolator* pReference, int kernelSize, const float* kernel);
void ApplyKernelNoiseFilter(CFacialAnimChannelInterpolator* pSpline, CFacialAnimChannelInterpolator* pReference, int kernelSize, const float* kernel, float threshold);

class GaussianKernel
{
public:
	GaussianKernel(float sigma, bool ignoreSelf = false);
	const float* GetKernel() const;
	int          GetKernelSize() const;

private:
	float EvalGaussian(float x) const;

	enum {KERNEL_SIZE = 15};
	float kernel[KERNEL_SIZE << 1];
};
}

void FaceChannel::GaussianBlurKeys(CFacialAnimChannelInterpolator* pSpline, float sigma)
{
	if (pSpline->GetKeyCount() == 0)
		return;

	// Create a copy of the spline - we need this so that error calculations are made against the original
	// spline rather than the partially smoothed one.
	CFacialAnimChannelInterpolator backupSpline(*pSpline);

	GaussianKernel kernel(sigma);
	ApplyKernel(pSpline, &backupSpline, kernel.GetKernelSize(), kernel.GetKernel());
}

void FaceChannel::RemoveNoise(CFacialAnimChannelInterpolator* pSpline, float sigma, float threshold)
{
	if (pSpline->GetKeyCount() == 0)
		return;

	// Create a copy of the spline - we need this so that error calculations are made against the original
	// spline rather than the partially smoothed one.
	CFacialAnimChannelInterpolator backupSpline(*pSpline);

	bool ignoreSelf = true;
	GaussianKernel kernel(sigma, ignoreSelf);
	ApplyKernelNoiseFilter(pSpline, &backupSpline, kernel.GetKernelSize(), kernel.GetKernel(), threshold);
}

void FaceChannel::ApplyKernel(CFacialAnimChannelInterpolator* pSpline, CFacialAnimChannelInterpolator* pReference, int kernelSize, const float* kernel)
{
	float length = pReference->GetKeyTime(pReference->GetKeyCount() - 1);

	for (int keyIndex = 0, keyCount = pSpline->GetKeyCount(); keyIndex < keyCount; ++keyIndex)
	{
		float keyTime = pSpline->GetKeyTime(keyIndex);

		float kernelTotal = 0.0f;
		float kernelCoefficientTotal = 0.0f;
		for (int i = 0; i < kernelSize; ++i)
		{
			const float& sampleX = kernel[i << 1];
			const float& sampleY = kernel[(i << 1) + 1];

			float sampleTime = keyTime + sampleX;
			if (sampleTime >= 0.0f && sampleTime <= length)
			{
				kernelCoefficientTotal += sampleY;
				float sampleValue;
				pReference->InterpolateFloat(sampleTime, sampleValue);
				kernelTotal += sampleValue * sampleY;
			}
		}

		if (pSpline->IsKeySelectedAtAnyDimension(keyIndex))
		{
			pSpline->SetKeyValueFloat(keyIndex, kernelTotal / kernelCoefficientTotal);
		}
	}
}

void FaceChannel::ApplyKernelNoiseFilter(CFacialAnimChannelInterpolator* pSpline, CFacialAnimChannelInterpolator* pReference, int kernelSize, const float* kernel, float threshold)
{
	float length = pReference->GetKeyTime(pReference->GetKeyCount() - 1);

	for (int keyIndex = 0, keyCount = pSpline->GetKeyCount(); keyIndex < keyCount; ++keyIndex)
	{
		float keyTime = pSpline->GetKeyTime(keyIndex);

		float kernelTotal = 0.0f;
		float kernelCoefficientTotal = 0.0f;
		for (int i = 0; i < kernelSize; ++i)
		{
			const float& sampleX = kernel[i << 1];
			const float& sampleY = kernel[(i << 1) + 1];

			float sampleTime = keyTime + sampleX;
			if (sampleTime >= 0.0f && sampleTime <= length)
			{
				kernelCoefficientTotal += sampleY;
				float sampleValue;
				pReference->InterpolateFloat(sampleTime, sampleValue);
				kernelTotal += sampleValue * sampleY;
			}
		}

		const bool selected = pSpline->IsKeySelectedAtAnyDimension(keyIndex);

		float currentValue;
		pSpline->GetKeyValueFloat(keyIndex, currentValue);
		float expectedValue = kernelTotal / kernelCoefficientTotal;
		if (abs(currentValue - expectedValue) > threshold && selected)
			pSpline->SetKeyValueFloat(keyIndex, expectedValue);
	}
}

float FaceChannel::GaussianKernel::EvalGaussian(float x) const
{
	const float scale = 1.0f / sqrtf(2 * 3.14159f);
	return scale * expf(-x * x / 2);
}

// Suppress uninitMemberVar for kernel as the analyzer is unable to detect initialization through reference
// cppcheck-suppress uninitMemberVar
FaceChannel::GaussianKernel::GaussianKernel(float sigma, bool ignoreSelf)
{
	// Create the kernel - don't bother normalizing, since we need to handle edge cases anyway.
	static const float KERNEL_SAMPLE_SPACING = 1.0f / FACIAL_EDITOR_FPS;
	for (int i = 0; i < KERNEL_SIZE; ++i)
	{
		float& x = kernel[i << 1];
		float& y = kernel[(i << 1) + 1];

		x = float(i - (KERNEL_SIZE >> 1)) * KERNEL_SAMPLE_SPACING;
		y = EvalGaussian(x / sigma);
		if (ignoreSelf && x == 0.0f)
			y = 0.0f;
	}
}

const float* FaceChannel::GaussianKernel::GetKernel() const
{
	return this->kernel;
}

int FaceChannel::GaussianKernel::GetKernelSize() const
{
	return KERNEL_SIZE;
}
