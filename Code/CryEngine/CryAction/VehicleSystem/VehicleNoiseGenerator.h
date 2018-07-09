// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _VEH_NOISE_GEN__H__
#define _VEH_NOISE_GEN__H__

class CVehicleNoiseValue
{
public:
	CVehicleNoiseValue();
	void  SetAmpFreq(float amplitude, float frequency);
	void  Setup(float amplitude, float frequency, int32 offset = 0);
	float Update(float dt);

private:
	float PseudoNoise(int x);
	float Interp(float a, float b, float t);
	float SoftNoise(int32 x);

private:
	float m_amplitude;
	float m_frequency;
	int32 m_position;
	int32 m_offset;
};

class CVehicleNoiseValue3D
{
public:
	CVehicleNoiseValue x;
	CVehicleNoiseValue y;
	CVehicleNoiseValue z;

	Vec3 Update(float tickTime)
	{
		return Vec3(x.Update(tickTime), y.Update(tickTime), z.Update(tickTime));
	}
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// CVehicleNoiseValue
////////////////////////////////////////////////////////////////////////////////////////////////////

NO_INLINE_WEAK CVehicleNoiseValue::CVehicleNoiseValue()
{
	m_offset = 0;
	m_position = 0;
	m_amplitude = 0.f;
	m_frequency = 0.f;
}

NO_INLINE_WEAK float CVehicleNoiseValue::PseudoNoise(int32 x)
{
	// Predictable random number generator for input 'x' (output is -1 to +1)
	x = (x << 13) ^ x;
	x = x * (x * x * 19379 + 819233) + 1266122899;
	return ((x & 0x7fffffff) * (1.f / 1073741823.5f)) - 1.f;
}

NO_INLINE_WEAK float CVehicleNoiseValue::Interp(float a, float b, float t)
{
	// Interpolate between a and b where t is the amount (0 to 1)
	t = (1.f - cosf(t * 3.14159265358979323f)) * 0.5f; // Change t to a curve
	return a * (1.f - t) + b * t;
}

NO_INLINE_WEAK float CVehicleNoiseValue::SoftNoise(int32 x)
{
	// Soft Perlin noise
	float a = PseudoNoise(x >> 16);
	float b = PseudoNoise((x >> 16) + 1);
	return Interp(a, b, (x & 0xffff) / 65536.f);
}

NO_INLINE_WEAK void CVehicleNoiseValue::SetAmpFreq(float amplitude, float frequency)
{
	m_amplitude = amplitude;
	m_frequency = frequency;
}

NO_INLINE_WEAK void CVehicleNoiseValue::Setup(float amplitude, float frequency, int32 offset)
{
	SetAmpFreq(amplitude, frequency);
	m_offset = offset;
	m_position = 0;
}

NO_INLINE_WEAK float CVehicleNoiseValue::Update(float tickTime)
{
	float r = SoftNoise(m_position + m_offset) * m_amplitude;
	m_position += (int32)(tickTime * 65536.f * m_frequency);
	return r;
}

#endif
