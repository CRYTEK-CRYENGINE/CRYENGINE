// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// CryEngine Header File.
// Copyright (C), Crytek, 1999-2015.

#include "StdAfx.h"
#include <CryMovie/IMovieSystem.h>

#include <QFile>

struct CTrackViewIcons
{
	static void GetIconByParamType(string& path, EAnimParamType type)
	{
		switch (type)
		{
		case eAnimParamType_Position:
		case eAnimParamType_PositionX:
		case eAnimParamType_PositionY:
		case eAnimParamType_PositionZ:
			path = "icons:ObjectTypes/node_track_postion.ico";
			break;
		case eAnimParamType_Rotation:
		case eAnimParamType_RotationX:
		case eAnimParamType_RotationY:
		case eAnimParamType_RotationZ:
			path = "icons:ObjectTypes/node_track_rotation.ico";
			break;
		case eAnimParamType_Scale:
		case eAnimParamType_ScaleX:
		case eAnimParamType_ScaleY:
		case eAnimParamType_ScaleZ:
			path = "icons:ObjectTypes/node_track_scale.ico";
			break;
		case eAnimParamType_Event:
			path = "icons:ObjectTypes/node_event.ico";
			break;
		case eAnimParamType_Visibility:
			path = "icons:ObjectTypes/node_track_visibility.ico";
			break;
		case eAnimParamType_Camera:
		case eAnimParamType_Capture:
		case eAnimParamType_GameCameraInfluence:
		case eAnimParamType_ShutterSpeed:
		case eAnimParamType_NearZ:
			path = "icons:General/Camera.ico";
			break;
		case eAnimParamType_Animation:
			path = "icons:ObjectTypes/node_track_animation.ico";
			break;
		case eAnimParamType_AudioTrigger:
			path = "icons:ObjectTypes/node_track_audio-trigger.ico";
			break;
		case eAnimParamType_AudioFile:
			path = "icons:ObjectTypes/node_track_audio-file.ico";
			break;
		case eAnimParamType_AudioParameter:
			path = "icons:ObjectTypes/node_track_audio-parameter.ico";
			break;
		case eAnimParamType_AudioSwitch:
			path = "icons:ObjectTypes/node_track_audio-switch.ico";
			break;
		case eAnimParamType_Expression:
			path = "icons:ObjectTypes/node_track_expression.ico";
			break;
		case eAnimParamType_TrackEvent:
			path = "icons:ObjectTypes/node_track_event.ico";
			break;
		case eAnimParamType_FOV:
			path = "icons:ObjectTypes/node_track_fov.ico";
			break;
		case eAnimParamType_ShakeAmplitudeA:
		case eAnimParamType_ShakeAmplitudeB:
		case eAnimParamType_ShakeFrequencyA:
		case eAnimParamType_ShakeFrequencyB:
		case eAnimParamType_ShakeMultiplier:
		case eAnimParamType_ShakeNoise:
		case eAnimParamType_ShakeWorking:
		case eAnimParamType_ShakeAmpAMult:
		case eAnimParamType_ShakeAmpBMult:
		case eAnimParamType_ShakeFreqAMult:
		case eAnimParamType_ShakeFreqBMult:
		case eAnimParamType_TransformNoise:
			path = "icons:ObjectTypes/node_track_noise.ico";
			break;
		case eAnimParamType_DepthOfField:
		case eAnimParamType_FocusDistance:
		case eAnimParamType_FocusRange:
		case eAnimParamType_BlurAmount:
			path = "icons:ObjectTypes/node_depth-of-field.ico";
			break;
		case eAnimParamType_ScreenFader:
			path = "icons:ObjectTypes/node_screen-fader.ico";
			break;
		case eAnimParamType_CommentText:
			path = "icons:ObjectTypes/node_comment.ico";
			break;
		case eAnimParamType_MaterialDiffuse:
		case eAnimParamType_MaterialSpecular:
		case eAnimParamType_MaterialEmissive:
		case eAnimParamType_MaterialOpacity:
		case eAnimParamType_MaterialSmoothness:
			path = "icons:ObjectTypes/node_material.ico";
			break;
		case eAnimParamType_Physics:
		case eAnimParamType_PhysicsDriven:
			path = "icons:ObjectTypes/node_track_physics-driven.ico";
			break;
		case eAnimParamType_ProceduralEyes:
			path = "icons:ObjectTypes/node_track_procedural-eyes.ico";
			break;
		case eAnimParamType_Mannequin:
			path = "icons:ObjectTypes/node_track_mannequin.ico";
			break;
		case eAnimParamType_SunLongitude:
		case eAnimParamType_SunLatitude:
		case eAnimParamType_MoonLongitude:
		case eAnimParamType_MoonLatitude:
			path = "icons:ObjectTypes/Light_Sun_Moon.ico";
			break;
		case eAnimParamType_LightDiffuse:
		case eAnimParamType_LightRadius:
		case eAnimParamType_LightDiffuseMult:
		case eAnimParamType_LightHDRDynamic:
		case eAnimParamType_LightSpecularMult:
		case eAnimParamType_LightSpecPercentage:
			path = "icons:ObjectTypes/light.ico";
			break;
		case eAnimParamType_Sequence:
		case eAnimParamType_Goto: //goes to place in sequence
			path = "icons:ObjectTypes/sequence.ico";
			break;
		case eAnimParamType_Console:
			path = "icons:ObjectTypes/node_cvar.ico";
			break;
		case eAnimParamType_TimeWarp:
		case eAnimParamType_TimeRanges:
		case eAnimParamType_FixedTimeStep: //changes playback speed of movie system
			path = "icons:ObjectTypes/Time_Clock.ico";
			break;
		case eAnimParamType_GSMCache:
			path = "icons:ObjectTypes/shadow.ico";
			break;
		//Fallback to "curve icon"
		case eAnimParamType_Float:
		case eAnimParamType_ColorR:
		case eAnimParamType_ColorG:
		case eAnimParamType_ColorB:
			path = "icons:ObjectTypes/node_track.ico";
			break;
		case eAnimParamType_ByString:
			path = "icons:ObjectTypes/node_track_property.ico";
			break;
		default:
			if (type >= eAnimParamType_User)
			{
				path = "icons:ObjectTypes/node_track_property.ico";
				break;
			}
			path = "icons:General/Placeholder.ico";
			break;
			break;
		}
	}

	static void GetIconByNodeType(string& path, EAnimNodeType type)
	{
		switch (type)
		{
		case eAnimNodeType_Entity:
			path = "icons:CreateEditor/Add_To_Scene_Legacy_Entities.ico";
			break;
		case eAnimNodeType_Director:
			path = "icons:ObjectTypes/node_director.ico";
			break;
		case eAnimNodeType_Camera:
			path = "icons:General/Camera.ico";
			break;
		case eAnimNodeType_CVar:
			path = "icons:ObjectTypes/node_cvar.ico";
			break;
		case eAnimNodeType_ScriptVar:
			path = "icons:ObjectTypes/node_script-variable.ico";
			break;
		case eAnimNodeType_Material:
			path = "icons:ObjectTypes/node_material.ico";
			break;
		case eAnimNodeType_Event:
			path = "icons:ObjectTypes/node_event.ico";
			break;
		case eAnimNodeType_Group:
			path = "icons:ObjectTypes/node_group.ico";
			break;
		case eAnimNodeType_Layer:
			path = "icons:ObjectTypes/node_layer.ico";
			break;
		case eAnimNodeType_Comment:
			path = "icons:ObjectTypes/node_comment.ico";
			break;
		case eAnimNodeType_RadialBlur:
			path = "icons:ObjectTypes/node_radial-blur.ico";
			break;
		case eAnimNodeType_ColorCorrection:
			path = "icons:ObjectTypes/node_color-correction.ico";
			break;
		case eAnimNodeType_DepthOfField:
			path = "icons:ObjectTypes/node_depth-of-field.ico";
			break;
		case eAnimNodeType_ScreenFader:
			path = "icons:ObjectTypes/node_screen-fader.ico";
			break;
		case eAnimNodeType_HDRSetup:
			path = "icons:ObjectTypes/node_hdr-setup.ico";
			break;
		case eAnimNodeType_Environment:
			path = "icons:ObjectTypes/node_environment.ico";
			break;
		case eAnimNodeType_Light:
			path = "icons:ObjectTypes/light.ico";
			break;
		case eAnimNodeType_ShadowSetup:
			path = "icons:ObjectTypes/shadow.ico";
			break;
		case eAnimNodeType_Alembic:
			path = "icons:ObjectTypes/dcc_asset.ico";
			break;
		case eAnimNodeType_GeomCache:
			path = "icons:ObjectTypes/object.ico";
			break;
		case eAnimNodeType_Audio:
			path = "icons:ObjectTypes/audio.ico";
			break;
		case eAnimNodeType_Invalid:
		default:
			path = "icons:General/Placeholder.ico";
			break;
		}
	}
};

