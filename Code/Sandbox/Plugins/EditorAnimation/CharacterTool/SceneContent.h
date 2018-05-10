// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <vector>
#include <QObject>
#include "FeatureTest.h"
#include "PlaybackLayers.h"

#include <CryAnimation/IAnimEventPlayer.h>

namespace CharacterTool {
using std::vector;

struct IFeatureTest;

struct BlendShapeParameter
{
	string name;
	float  weight;
	bool operator<(const BlendShapeParameter& rhs) const { return name < rhs.name; }

	BlendShapeParameter()
		: weight(1.0f)
	{
	}
};
typedef vector<BlendShapeParameter> BlendShapeParameters;

struct BlendShapeSkin
{
	string               name;
	bool                 blendShapesSupported;
	BlendShapeParameters params;

	BlendShapeSkin()
		: blendShapesSupported(false)
	{
	}

	void Serialize(IArchive& ar);
	bool operator<(const BlendShapeSkin& rhs) const { return name < rhs.name; }
};
typedef vector<BlendShapeSkin> BlendShapeSkins;

struct BlendShapeOptions
{
	bool            overrideWeights;
	BlendShapeSkins skins;

	BlendShapeOptions()
		: overrideWeights(false)
	{
	}

	void Serialize(IArchive& ar);
};

struct SceneContent : public QObject
{
	Q_OBJECT
public:

	string characterPath;
	PlaybackLayers           layers;
	int                      aimLayer;
	int                      lookLayer;
	BlendShapeOptions        blendShapeOptions;
	IAnimEventPlayerPtr      animEventPlayer;

	bool                     runFeatureTest;
	_smart_ptr<IFeatureTest> featureTest;

	std::vector<char>        lastLayersContent;
	std::vector<char>        lastContent;

	SceneContent();
	void              Serialize(Serialization::IArchive& ar);
	bool              CheckIfPlaybackLayersChanged(bool continuous);
	void              CharacterChanged();
	void              PlaybackLayersChanged(bool continuous);
	void              LayerActivated();
	AimParameters&    GetAimParameters();
	AimParameters&    GetLookParameters();
	MotionParameters& GetMotionParameters();
signals:
	void              SignalAnimEventPlayerTypeChanged();
	void              SignalCharacterChanged();
	void              SignalPlaybackLayersChanged(bool continuous);
	void              SignalLayerActivated();
	void              SignalBlendShapeOptionsChanged();
	void              SignalNewLayerActivated();

	void              SignalChanged(bool continuous);
};

}

