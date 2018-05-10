// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryRenderer/VertexFormats.h>

namespace stable_rand
{
void  setSeed(uint32 seed);
float randUnit();
float randPositive();
float randBias(float noise);
}

struct SpritePoint
{
	Vec2   pos;
	float  size;
	float  brightness;
	float  rotation;
	ColorF color;

	SpritePoint(const Vec2& _pos, float _brightness) :
		pos(_pos),
		brightness(_brightness),
		size(0.1f),
		rotation(0)
	{
		color.set(1, 1, 1, 1);
	}
	SpritePoint() :
		brightness(1),
		size(0.1f),
		rotation(0)
	{
		pos.set(0, 0);
		color.set(1, 1, 1, 1);
	}
};

class MeshUtil
{
public:
	static void GenDisk(float radius, int polySides, int ringCount, bool capInnerHole, const ColorF& clr, float* ringPosArray, std::vector<SVF_P3F_C4B_T2F>& vertOut, std::vector<uint16>& idxOut);

	static void GenHoop(float radius, int polySides, float thickness, int ringCount, const ColorF& clr, float noiseStrength, int noiseSeed, float startAngle, float endAngle, float fadeAngle, std::vector<SVF_P3F_C4B_T2F>& vertOut, std::vector<uint16>& idxOut);
	static void GenTrapezoidFan(int numSideVert, float radius, float startAngleDegree, float endAngleDegree, float centerThickness, const ColorF& clr, std::vector<SVF_P3F_C4B_T2F>& vertOut, std::vector<uint16>& idxOut);
	static void GenFan(int numSideVert, float radius, float startAngleDegree, float endAngleDegree, const ColorF& clr, std::vector<SVF_P3F_C4B_T2F>& vertOut, std::vector<uint16>& idxOut);

	//A falloff fan is a simple coarse fan-shaped mesh with odd number of side-vertices.
	//It's UV mapping spans a strict rectanglular shape (x:[0,1,0]  y:[0,1]).
	//On top of the fan shape, there's a concentric beam which simulates the spike effect.
	static void GenShaft(float radius, float centerThickness, int complexity, float startAngleDegree, float endAngleDegree, const ColorF& clr, std::vector<SVF_P3F_C4B_T2F>& vertOut, std::vector<uint16>& idxOut);
	static void GenStreak(float dir, float radius, float thickness, const ColorF& clr, std::vector<SVF_P3F_C4B_T2F>& vertOut, std::vector<uint16>& idxOut);
	static void GenSprites(std::vector<SpritePoint>& spriteList, float aspectRatio, bool packPivotPos, std::vector<SVF_P3F_C4B_T2F>& vertOut, std::vector<uint16>& idxOut);
	static void TrianglizeQuadIndices(int quadCount, std::vector<uint16>& idxOut);
	static void GenScreenTile(float x0, float y0, float x1, float y1, ColorF clr, int rowCount, int columnCount, stl::aligned_vector<SVF_P3F_C4B_T2F, CRY_PLATFORM_ALIGNMENT>& vertOut, stl::aligned_vector<uint16, CRY_PLATFORM_ALIGNMENT>& idxOut);
};
