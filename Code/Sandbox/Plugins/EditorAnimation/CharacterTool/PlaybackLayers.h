// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <vector>
#include <CryMath/Cry_Math.h>
#include <CryMath/Cry_Geo.h>
#include <CryAnimation/CryCharAnimationParams.h>
#include <CrySerialization/Forward.h>

namespace CharacterTool
{
using Serialization::IArchive;
using std::vector;

struct MotionParameters
{
	// indexed with eMotionParamID_*
	float values[eMotionParamID_COUNT];
	bool  enabled[eMotionParamID_COUNT];
	float rangeMin[eMotionParamID_COUNT];
	float rangeMax[eMotionParamID_COUNT];

	MotionParameters();
	void Serialize(IArchive& ar);
};

struct AimParameters
{
	enum AimDirection
	{
		AIM_CAMERA,
		AIM_FORWARD,
		AIM_TARGET
	};

	AimDirection direction;
	float        offsetX;
	float        offsetY;
	float        smoothTime;
	Vec3         targetPosition;

	AimParameters()
		: offsetX(0.0f)
		, offsetY(0.0f)
		, smoothTime(0.1f)
		, direction(AIM_CAMERA)
		, targetPosition(0.0, 2.0f, 0.0f)
	{
	}

	void Serialize(IArchive& ar);
};

struct PlaybackLayer
{
	enum { SERIALIZE_LAYERS_ONLY = 1 << 31 };

	bool             enabled;
	int              layerId;
	string           animation;
	string           path;
	float            weight;
	int              type;

	AimParameters    aim;
	MotionParameters motionParameters;

	PlaybackLayer();
	void Serialize(IArchive& ar);
};

struct PlaybackLayers
{
	bool                  bindPose;
	bool                  allowRedirect;
	vector<PlaybackLayer> layers;
	int                   activeLayer;

	bool                  ContainsSameAnimations(const PlaybackLayers& rhs) const
	{
		if (layers.size() != rhs.layers.size())
			return false;

		for (size_t i = 0; i < rhs.layers.size(); ++i)
			if (layers[i].animation != rhs.layers[i].animation ||
			    layers[i].path != rhs.layers[i].path)
				return false;

		return true;
	}

	PlaybackLayers();
	void Serialize(bool* activeLayerSetToEmpty, IArchive& ar);
	void Serialize(IArchive& ar) { bool dummy; Serialize(&dummy, ar); }
};

}

