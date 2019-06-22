// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Nodes/TrackViewAnimNode.h"
#include "TrackViewUndo.h"
#include "TrackViewSequenceManager.h"
#include "TrackViewPlugin.h"
#include "AnimationContext.h"
#include "TrackViewExporter.h"
#include "TrackViewEventsDialog.h"

#include <CryMovie/IMovieSystem.h>

#pragma warning(push)
#pragma warning(disable : 4005 4244 4800)

#include "EditorFramework/Events.h"
#include "Commands/CommandManager.h"
#include "Util/BoostPythonHelpers.h"

namespace
{
// Misc
void PyTrackViewSetRecording(bool bRecording)
{
	CAnimationContext* pAnimationContext = CTrackViewPlugin::GetAnimationContext();
	if (pAnimationContext)
	{
		pAnimationContext->SetRecording(bRecording);
	}
}

// Sequences
void PyTrackViewNewSequenceByName(const char* name)
{
	CTrackViewSequenceManager* pSequenceManager = CTrackViewPlugin::GetSequenceManager();

	CTrackViewSequence* pSequence = pSequenceManager->GetSequenceByName(name);
	if (pSequence)
	{
		throw std::exception("A sequence with this name already exists");
	}

	CUndo undo("Create TrackView sequence");
	pSequenceManager->CreateSequence(name);
}

void PyTrackViewDeleteSequenceByName(const char* name)
{
	CTrackViewSequenceManager* pSequenceManager = CTrackViewPlugin::GetSequenceManager();
	CTrackViewSequence* pSequence = pSequenceManager->GetSequenceByName(name);
	if (pSequence)
	{
		pSequenceManager->DeleteSequence(pSequence);
		return;
	}

	throw std::exception("Could not find sequence");
}

void PyTrackViewSetCurrentSequence(const char* name)
{
	const CTrackViewSequenceManager* pSequenceManager = CTrackViewPlugin::GetSequenceManager();
	CTrackViewSequence* pSequence = pSequenceManager->GetSequenceByName(name);
	CAnimationContext* pAnimationContext = CTrackViewPlugin::GetAnimationContext();
	pAnimationContext->SetSequence(pSequence, false, false);
}

int PyTrackViewGetNumSequences()
{
	const CTrackViewSequenceManager* pSequenceManager = CTrackViewPlugin::GetSequenceManager();
	return pSequenceManager->GetCount();
}

string PyTrackViewGetSequenceName(unsigned int index)
{
	if (index < PyTrackViewGetNumSequences())
	{
		const CTrackViewSequenceManager* pSequenceManager = CTrackViewPlugin::GetSequenceManager();
		return pSequenceManager->GetSequenceByIndex(index)->GetName();
	}

	throw std::exception("Could not find sequence");
}

boost::python::tuple PyTrackViewGetSequenceTimeRange(const char* name)
{
	const CTrackViewSequenceManager* pSequenceManager = CTrackViewPlugin::GetSequenceManager();
	CTrackViewSequence* pSequence = pSequenceManager->GetSequenceByName(name);
	if (!pSequence)
	{
		throw std::exception("A sequence with this name doesn't exists");
	}

	const Range timeRange = Range(pSequence->GetTimeRange().start.ToFloat(), pSequence->GetTimeRange().end.ToFloat());
	return boost::python::make_tuple(timeRange.start, timeRange.end);
}

void PyTrackViewSetSequenceTimeRange(const char* name, float start, float end)
{
	const CTrackViewSequenceManager* pSequenceManager = CTrackViewPlugin::GetSequenceManager();
	CTrackViewSequence* pSequence = pSequenceManager->GetSequenceByName(name);
	if (!pSequence)
	{
		throw std::exception("A sequence with this name doesn't exists");
	}

	CUndo undo("Set sequence time range");
	pSequence->SetTimeRange(TRange<SAnimTime>(SAnimTime(start), SAnimTime(end)));
}

void PyTrackViewSetSequenceTime(float time)
{
	CAnimationContext* pAnimationContext = CTrackViewPlugin::GetAnimationContext();
	pAnimationContext->SetTime(SAnimTime(time));
}

// Nodes
void PyTrackViewAddNode(const char* nodeTypeString, const char* nodeName)
{
	CTrackViewSequence* pSequence = CTrackViewPlugin::GetAnimationContext()->GetSequence();
	if (!pSequence)
	{
		return;
	}

	const EAnimNodeType nodeType = GetIEditor()->GetMovieSystem()->GetNodeTypeFromString(nodeTypeString);
	if (nodeType == eAnimNodeType_Invalid)
	{
		throw std::exception("Invalid node type");
	}

	CUndo undo("Create anim node");
	pSequence->CreateSubNode(nodeName, nodeType);
}

CTrackViewAnimNode* GetNodeFromName(const char* nodeName, const char* parentDirectorName)
{
	CAnimationContext* pAnimationContext = CTrackViewPlugin::GetAnimationContext();
	CTrackViewSequence* pSequence = pAnimationContext->GetSequence();
	if (!pSequence)
	{
		throw std::exception("No sequence is active");
	}

	CTrackViewAnimNode* pParentDirector = pSequence;
	if (strlen(parentDirectorName) > 0)
	{
		CTrackViewAnimNodeBundle foundNodes = pSequence->GetAnimNodesByName(parentDirectorName);
		if (foundNodes.GetCount() == 0 || foundNodes.GetNode(0)->GetType() != eAnimNodeType_Director)
		{
			throw std::exception("Director node not found");
		}

		pParentDirector = foundNodes.GetNode(0);
	}

	CTrackViewAnimNodeBundle foundNodes = pParentDirector->GetAnimNodesByName(nodeName);
	return (foundNodes.GetCount() > 0) ? foundNodes.GetNode(0) : nullptr;
}

void PyTrackViewDeleteNode(const char* nodeName, const char* parentDirectorName)
{
	CTrackViewAnimNode* pNode = GetNodeFromName(nodeName, parentDirectorName);
	if (pNode == nullptr)
	{
		throw std::exception("Couldn't find node");
	}

	CTrackViewAnimNode* pParentNode = static_cast<CTrackViewAnimNode*>(pNode->GetParentNode());

	CUndo undo("Delete TrackView Node");
	pParentNode->RemoveSubNode(pNode);
}

void PyTrackViewAddTrack(const char* paramName, const char* nodeName, const char* parentDirectorName)
{
	CTrackViewAnimNode* pNode = GetNodeFromName(nodeName, parentDirectorName);
	if (!pNode)
	{
		throw std::exception("Couldn't find node");
	}

	// Add tracks to menu, that can be added to animation node.
	const int paramCount = pNode->GetParamCount();
	for (int i = 0; i < paramCount; ++i)
	{
		CAnimParamType paramType = pNode->GetParamType(i);

		if (paramType == eAnimParamType_Invalid)
		{
			continue;
		}

		IAnimNode::ESupportedParamFlags paramFlags = pNode->GetParamFlags(paramType);

		CTrackViewTrack* pTrack = pNode->GetTrackForParameter(paramType);
		if (!pTrack || (paramFlags & IAnimNode::eSupportedParamFlags_MultipleTracks))
		{
			const char* name = pNode->GetParamName(paramType);
			if (stricmp(name, paramName) == 0)
			{
				CUndo undo("Create track");
				if (!pNode->CreateTrack(paramType))
				{
					undo.Cancel();
					throw std::exception("Could not create track");
				}

				pNode->SetSelected(true);
				return;
			}
		}
	}

	throw std::exception("Could not create track");
}

void PyTrackViewDeleteTrack(const char* paramName, uint32 index, const char* nodeName, const char* parentDirectorName)
{
	CTrackViewAnimNode* pNode = GetNodeFromName(nodeName, parentDirectorName);
	if (!pNode)
	{
		throw std::exception("Couldn't find node");
	}

	const CAnimParamType paramType = GetIEditor()->GetMovieSystem()->GetParamTypeFromString(paramName);
	CTrackViewTrack* pTrack = pNode->GetTrackForParameter(paramType, index);
	if (!pTrack)
	{
		throw std::exception("Could not find track");
	}

	CUndo undo("Delete TrackView track");
	pNode->RemoveTrack(pTrack);
}

int PyTrackViewGetNumNodes(const char* parentDirectorName)
{
	CAnimationContext* pAnimationContext = CTrackViewPlugin::GetAnimationContext();
	CTrackViewSequence* pSequence = pAnimationContext->GetSequence();
	if (!pSequence)
	{
		throw std::exception("No sequence is active");
	}

	CTrackViewAnimNode* pParentDirector = pSequence;
	if (strlen(parentDirectorName) > 0)
	{
		CTrackViewAnimNodeBundle foundNodes = pSequence->GetAnimNodesByName(parentDirectorName);
		if (foundNodes.GetCount() == 0 || foundNodes.GetNode(0)->GetType() != eAnimNodeType_Director)
		{
			throw std::exception("Director node not found");
		}

		pParentDirector = foundNodes.GetNode(0);
	}

	CTrackViewAnimNodeBundle foundNodes = pParentDirector->GetAllAnimNodes();
	return foundNodes.GetCount();
}

string PyTrackViewGetNodeName(int index, const char* parentDirectorName)
{
	CAnimationContext* pAnimationContext = CTrackViewPlugin::GetAnimationContext();
	CTrackViewSequence* pSequence = pAnimationContext->GetSequence();
	if (!pSequence)
	{
		throw std::exception("No sequence is active");
	}

	CTrackViewAnimNode* pParentDirector = pSequence;
	if (strlen(parentDirectorName) > 0)
	{
		CTrackViewAnimNodeBundle foundNodes = pSequence->GetAnimNodesByName(parentDirectorName);
		if (foundNodes.GetCount() == 0 || foundNodes.GetNode(0)->GetType() != eAnimNodeType_Director)
		{
			throw std::exception("Director node not found");
		}

		pParentDirector = foundNodes.GetNode(0);
	}

	CTrackViewAnimNodeBundle foundNodes = pParentDirector->GetAllAnimNodes();
	if (index < 0 || index >= foundNodes.GetCount())
	{
		throw std::exception("Invalid node index");
	}

	return foundNodes.GetNode(index)->GetName();
}

// Tracks
CTrackViewTrack* GetTrack(const char* paramName, uint32 index, const char* nodeName, const char* parentDirectorName)
{
	CTrackViewAnimNode* pNode = GetNodeFromName(nodeName, parentDirectorName);
	if (!pNode)
	{
		throw std::exception("Couldn't find node");
	}

	const CAnimParamType paramType = GetIEditor()->GetMovieSystem()->GetParamTypeFromString(paramName);
	CTrackViewTrack* pTrack = pNode->GetTrackForParameter(paramType, index);
	if (!pTrack)
	{
		throw std::exception("Track doesn't exist");
	}

	return pTrack;
}

std::set<float> GetKeyTimeSet(CTrackViewTrack* pTrack)
{
	std::set<float> keyTimeSet;
	for (uint i = 0; i < pTrack->GetKeyCount(); ++i)
	{
		CTrackViewKeyHandle keyHandle = pTrack->GetKey(i);
		keyTimeSet.insert(keyHandle.GetTime().ToFloat());
	}

	return keyTimeSet;
}

int PyTrackViewGetNumTrackKeys(const char* paramName, int trackIndex, const char* nodeName, const char* parentDirectorName)
{
	CTrackViewTrack* pTrack = GetTrack(paramName, trackIndex, nodeName, parentDirectorName);
	return (int)GetKeyTimeSet(pTrack).size();
}

pSPyWrappedProperty PyTrackViewGetInterpolatedValue(const char* paramName, int trackIndex, float time, const char* nodeName, const char* parentDirectorName)
{
	CTrackViewTrack* pTrack = GetTrack(paramName, trackIndex, nodeName, parentDirectorName);

	pSPyWrappedProperty prop(new SPyWrappedProperty);
	switch (pTrack->GetValueType())
	{
	case eAnimValue_Float:
	case eAnimValue_DiscreteFloat:
		{
			float value = stl::get<float>(pTrack->GetValue(SAnimTime(time)));
			prop->type = SPyWrappedProperty::eType_Float;
			prop->property.floatValue = value;
		}
		break;
	case eAnimValue_Bool:
		{
			bool value = stl::get<bool>(pTrack->GetValue(SAnimTime(time)));
			prop->type = SPyWrappedProperty::eType_Bool;
			prop->property.boolValue = value;
		}
		break;
	case eAnimValue_Quat:
		{
			Quat value = stl::get<Quat>(pTrack->GetValue(SAnimTime(time)));
			prop->type = SPyWrappedProperty::eType_Vec3;
			Ang3 rotation(value);
			prop->property.vecValue.x = rotation.x;
			prop->property.vecValue.y = rotation.y;
			prop->property.vecValue.z = rotation.z;
		}
	case eAnimValue_Vector:
		{
			Vec3 value = stl::get<Vec3>(pTrack->GetValue(SAnimTime(time)));
			prop->type = SPyWrappedProperty::eType_Vec3;
			prop->property.vecValue.x = value.x;
			prop->property.vecValue.y = value.y;
			prop->property.vecValue.z = value.z;
		}
		break;
	case eAnimValue_Vector4:
		{
			Vec4 value = stl::get<Vec4>(pTrack->GetValue(SAnimTime(time)));
			prop->type = SPyWrappedProperty::eType_Vec4;
			prop->property.vecValue.x = value.x;
			prop->property.vecValue.y = value.y;
			prop->property.vecValue.z = value.z;
			prop->property.vecValue.w = value.w;
		}
		break;
	case eAnimValue_RGB:
		{
			Vec3 value = stl::get<Vec3>(pTrack->GetValue(SAnimTime(time)));
			prop->type = SPyWrappedProperty::eType_Color;
			prop->property.colorValue.r = static_cast<int>(clamp_tpl(value.x, 0.0f, 1.0f) * 255.0f);
			prop->property.colorValue.g = static_cast<int>(clamp_tpl(value.y, 0.0f, 1.0f) * 255.0f);
			prop->property.colorValue.b = static_cast<int>(clamp_tpl(value.z, 0.0f, 1.0f) * 255.0f);
		}
		break;
	default:
		throw std::exception("Unsupported key type");
	}

	return prop;
}

pSPyWrappedProperty PyTrackViewGetKeyValue(const char* paramName, int trackIndex, int keyIndex, const char* nodeName, const char* parentDirectorName)
{
	CTrackViewTrack* pTrack = GetTrack(paramName, trackIndex, nodeName, parentDirectorName);

	std::set<float> keyTimeSet = GetKeyTimeSet(pTrack);
	if (keyIndex < 0 || keyIndex >= keyTimeSet.size())
	{
		throw std::exception("Invalid key index");
	}

	auto keyTimeIter = keyTimeSet.begin();
	std::advance(keyTimeIter, keyIndex);
	const float keyTime = *keyTimeIter;

	return PyTrackViewGetInterpolatedValue(paramName, trackIndex, keyTime, nodeName, parentDirectorName);
}

//Track view UI

//File
void PyTrackViewDeleteSequence()
{
	CommandEvent("trackview.delete_sequence").SendToKeyboardFocus();
}

void PyTrackViewImportFromFBX()
{
	CommandEvent("trackview.import_from_fbx").SendToKeyboardFocus();
}

void PyTrackViewExportToFBX()
{
	CommandEvent("trackview.export_to_fbx").SendToKeyboardFocus();
}

void PyTrackViewNewEvent()
{
	CommandEvent("trackview.new_event").SendToKeyboardFocus();
}

void PyTrackViewShowEvents()
{
	CommandEvent("trackview.show_events").SendToKeyboardFocus();
}

void PyTrackViewToggleShowDopesheet()
{
	CommandEvent("trackview.toggle_show_dopesheet").SendToKeyboardFocus();
}

void PyTrackViewToggleShowCurveEditor()
{
	CommandEvent("trackview.toggle_show_curve_editor").SendToKeyboardFocus();
}

void PyTrackViewShowSequenceProperties()
{
	CommandEvent("trackview.show_sequence_properties").SendToKeyboardFocus();
}

void PyTrackViewToggleLinkTimelines()
{
	CommandEvent("trackview.toggle_link_timelines").SendToKeyboardFocus();
}

void PyTrackViewSetUnitsTicks()
{
	CommandEvent("trackview.set_units_ticks").SendToKeyboardFocus();
}

void PyTrackViewSetUnitsTime()
{
	CommandEvent("trackview.set_units_time").SendToKeyboardFocus();
}

void PyTrackViewSetUnitsFramecode()
{
	CommandEvent("trackview.set_units_framecode").SendToKeyboardFocus();
}

void PyTrackViewSetUnitsFrames()
{
	CommandEvent("trackview.set_units_frames").SendToKeyboardFocus();
}

void PyTrackViewGoToStart()
{
	CommandEvent("trackview.go_to_start").SendToKeyboardFocus();
}

void PyTrackViewGoToEnd()
{
	CommandEvent("trackview.go_to_end").SendToKeyboardFocus();
}

void PyTrackViewPausePlay()
{
	CommandEvent("trackview.pause_play").SendToKeyboardFocus();
}

void PyTrackViewStop()
{
	CommandEvent("trackview.stop").SendToKeyboardFocus();
}

void PyTrackViewRecord()
{
	CommandEvent("trackview.record").SendToKeyboardFocus();
}

void PyTrackViewGoToNextKey()
{
	CommandEvent("trackview.go_to_next_key").SendToKeyboardFocus();
}

void PyTrackViewGoToPrevKey()
{
	CommandEvent("trackview.go_to_prev_key").SendToKeyboardFocus();
}

void PyTrackViewToggleLoop()
{
	CommandEvent("trackview.toogle_loop").SendToKeyboardFocus();
}

void PyTrackViewSetPlaybackStart()
{
	CommandEvent("trackview.set_playback_start").SendToKeyboardFocus();
}

void PyTrackViewSetPlaybackEnd()
{
	CommandEvent("trackview.set_playback_end").SendToKeyboardFocus();
}

void PyTrackViewResetPlaybackStartEnd()
{
	CommandEvent("trackview.reset_playback_start_end").SendToKeyboardFocus();
}

void PyTrackViewRenderSequence()
{
	CommandEvent("trackview.render_sequence").SendToKeyboardFocus();
}

void PyTrackViewCreateLightAnimationSet()
{
	CommandEvent("trackview.create_light_animation_set").SendToKeyboardFocus();
}

void PyTrackViewSyncSelectedTracksToBasePosition()
{
	CommandEvent("trackview.sync_selected_tracks_to_base_position").SendToKeyboardFocus();
}

void PyTrackViewSyncSelectedTracksFromBasePosition()
{
	CommandEvent("trackview.sync_selected_tracks_from_base_position").SendToKeyboardFocus();
}

void PyTrackViewNoSnap()
{
	CommandEvent("trackview.no_snap").SendToKeyboardFocus();
}

void PyTrackViewMagnetSnap()
{
	CommandEvent("trackview.magnet_snap").SendToKeyboardFocus();
}

void PyTrackViewFrameSnap()
{
	CommandEvent("trackview.frame_snap").SendToKeyboardFocus();
}

void PyTrackViewGridSnap()
{
	CommandEvent("trackview.grid_snap").SendToKeyboardFocus();
}

void PyTrackViewDeleteSelectedTracks()
{
	CommandEvent("trackview.delete_selected_tracks").SendToKeyboardFocus();
}

void PyTrackViewDisableSelectedTracks()
{
	CommandEvent("trackview.disable_selected_tracks").SendToKeyboardFocus();
}

void PyTrackViewMuteSelectedTracks()
{
	CommandEvent("trackview.mute_selected_tracks").SendToKeyboardFocus();
}

void PyTrackViewEnableSelectedTracks()
{
	CommandEvent("trackview.enable_selected_tracks").SendToKeyboardFocus();
}

void PyTrackViewSelectMoveKeysTool()
{
	CommandEvent("trackview.select_move_keys_tool").SendToKeyboardFocus();
}

void PyTrackViewSelectSlideKeysTool()
{
	CommandEvent("trackview.select_slide_keys_tool").SendToKeyboardFocus();
}

void PyTrackViewSelectScaleKeysTool()
{
	CommandEvent("trackview.select_scale_keys_tool").SendToKeyboardFocus();
}

void PyTrackViewSetTangentAuto()
{
	CommandEvent("trackview.set_tangent_auto").SendToKeyboardFocus();
}

void PyTrackViewSetTangentInZero()
{
	CommandEvent("trackview.set_trangent_in_zero").SendToKeyboardFocus();
}

void PyTrackViewSetTangentInStep()
{
	CommandEvent("trackview.set_tangent_in_step").SendToKeyboardFocus();
}

void PyTrackViewSetTangentInLinear()
{
	CommandEvent("trackview.set_tangent_in_linear").SendToKeyboardFocus();
}

void PyTrackViewSetTangentOutZero()
{
	CommandEvent("trackview.set_tangent_out_zero").SendToKeyboardFocus();
}

void PyTrackViewSetTangentOutStep()
{
	CommandEvent("trackview.set_tangent_out_step").SendToKeyboardFocus();
}

void PyTrackViewSetTangentOutLinear()
{
	CommandEvent("trackview.set_tangent_out_linear").SendToKeyboardFocus();
}

void PyTrackViewBreakTangents()
{
	CommandEvent("trackview.break_tangents").SendToKeyboardFocus();
}

void PyTrackViewUnifyTangents()
{
	CommandEvent("trackview.unify_tangents").SendToKeyboardFocus();
}

void PyTrackViewFitViewHorizontal()
{
	CommandEvent("trackview.fit_view_horizontal").SendToKeyboardFocus();
}

void PyTrackViewFitViewVertical()
{
	CommandEvent("trackview.fit_view_vertical").SendToKeyboardFocus();
}

void PyTrackViewAddTrackPosition()
{
	CommandEvent("trackview.add_track_position").SendToKeyboardFocus();
}

void PyTrackViewAddTrackRotation()
{
	CommandEvent("trackview.add_track_rotation").SendToKeyboardFocus();
}

void PyTrackViewAddTrackScale()
{
	CommandEvent("trackview.add_track_scale").SendToKeyboardFocus();
}

void PyTrackViewAddTrackVisibility()
{
	CommandEvent("trackview.add_track_visibility").SendToKeyboardFocus();
}

void PyTrackViewAddTrackAnimation()
{
	CommandEvent("trackview.add_track_animation").SendToKeyboardFocus();
}

void PyTrackViewAddTrackMannequin()
{
	CommandEvent("trackview.add_track_mannequin").SendToKeyboardFocus();
}

void PyTrackViewAddTrackNoise()
{
	CommandEvent("trackview.add_track_noise").SendToKeyboardFocus();
}

void PyTrackViewAddTrackAudioFile()
{
	CommandEvent("trackview.add_track_audio_file").SendToKeyboardFocus();
}

void PyTrackViewAddTrackAudioParameter()
{
	CommandEvent("trackview.add_track_audio_parameter").SendToKeyboardFocus();
}

void PyTrackViewAddTrackAudioSwitch()
{
	CommandEvent("trackview.add_track_audio_switch").SendToKeyboardFocus();
}

void PyTrackViewAddTrackAudioTrigger()
{
	CommandEvent("trackview.add_track_audio_trigger").SendToKeyboardFocus();
}

void PyTrackViewAddTrackDRSSignal()
{
	CommandEvent("trackview.add_track_drs_signal").SendToKeyboardFocus();
}

void PyTrackViewAddTrackEvent()
{
	CommandEvent("trackview.add_track_event").SendToKeyboardFocus();
}

void PyTrackViewAddTrackExpression()
{
	CommandEvent("trackview.add_track_expression").SendToKeyboardFocus();
}

void PyTrackViewAddTrackFacialSequence()
{
	CommandEvent("trackview.add_track_facial_sequence").SendToKeyboardFocus();
}

void PyTrackViewAddTrackLookAt()
{
	CommandEvent("trackview.add_track_look_at").SendToKeyboardFocus();
}

void PyTrackViewAddTrackPhysicalize()
{
	CommandEvent("trackview.add_track_physicalize").SendToKeyboardFocus();
}

void PyTrackViewAddTrackPhysicsDriven()
{
	CommandEvent("trackview.add_track_physics_driven").SendToKeyboardFocus();
}

void PyTrackViewAddTrackProceduralEyes()
{
	CommandEvent("trackview.add_track_procedural_eyes").SendToKeyboardFocus();
}

}

DECLARE_PYTHON_MODULE(trackview);


// General
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewSetRecording, trackview, set_recording,
                                     "Activates/deactivates TrackView recording mode",
                                     "trackview.set_time(bool bRecording)")

// Sequences
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewNewSequenceByName, trackview, new_sequence_by_name,
                                     "Creates a new sequence with the given name.",
                                     "trackview.new_sequence_by_name(str sequenceName)")
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewDeleteSequenceByName, trackview, delete_sequence_by_name,
                                     "Deletes the specified sequence.",
                                     "trackview.delete_sequence_by_name(str sequenceName)")
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewSetCurrentSequence, trackview, set_current_sequence,
                                     "Sets the specified sequence as a current one in TrackView.",
                                     "trackview.set_current_sequence(str sequenceName)")
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewGetSequenceName, trackview, get_sequence_name,
                                     "Gets the name of a sequence by its index.",
                                     "trackview.get_sequence_name(int sequenceIndex)")
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewGetSequenceTimeRange, trackview, get_sequence_time_range,
                                     "Gets the time range of a sequence as a pair",
                                     "trackview.get_sequence_time_range(string sequenceName)")
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewSetSequenceTimeRange, trackview, set_sequence_time_range,
                                     "Sets the time range of a sequence",
                                     "trackview.set_sequence_time_range(string sequenceName, float start, float end)")
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewSetSequenceTime, trackview, set_time,
                                     "Sets the time of the sequence currently playing in TrackView.",
                                     "trackview.set_time(float time)")

// Nodes
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewAddNode, trackview, add_node,
                                     "Adds a new node with the given type & name to the current sequence.",
                                     "trackview.add_node(str nodeTypeName, str nodeName)")
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewDeleteNode, trackview, delete_node,
                                     "Deletes the specified node from the current sequence.",
                                     "trackview.delete_node(str nodeName, str parentDirectorName)")
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewAddTrack, trackview, add_track,
                                     "Adds a track of the given parameter ID to the node.",
                                     "trackview.add_track(str paramType, str nodeName, str parentDirectorName)")
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewDeleteTrack, trackview, delete_track,
                                     "Deletes a track of the given parameter ID (in the given index in case of a multi-track) from the node.",
                                     "trackview.delete_track(str paramType, int index, str nodeName, str parentDirectorName)")
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewGetNumNodes, trackview, get_num_nodes,
                                     "Gets the number of sequences.",
                                     "trackview.get_num_nodes(str parentDirectorName)")
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewGetNodeName, trackview, get_node_name,
                                     "Gets the name of a sequence by its index.",
                                     "trackview.get_node_name(int nodeIndex, str parentDirectorName)")

// Tracks
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewGetNumTrackKeys, trackview, get_num_track_keys,
                                     "Gets number of keys of the specified track",
                                     "trackview.get_num_track_keys(str paramName, int trackIndex, str nodeName, str parentDirectorName)")
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewGetKeyValue, trackview, get_key_value,
                                     "Gets the value of the specified key",
                                     "trackview.get_num_track_keys(str paramName, int trackIndex, int keyIndex, str nodeName, str parentDirectorName)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewGetInterpolatedValue, trackview, get_interpolated_value,
                                     "Gets the interpolated value of a track at the specified time",
                                     "trackview.get_num_track_keys(str paramName, int trackIndex, float time, str nodeName, str parentDirectorName)");

// Track view UI Shortcuts
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewDeleteSequence, trackview, delete_sequence,
                                     "Deletes current sequence",
                                     "trackview.delete_sequence()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewImportFromFBX, trackview, import_from_fbx,
                                     "Imports from FBX",
                                     "trackview.import_from_fbx()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewExportToFBX, trackview, export_to_fbx,
                                     "Exports from FBX",
                                     "trackview.export_to_fbx()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewNewEvent, trackview, new_event,
                                     "New event",
                                     "trackview.new_event()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewShowEvents, trackview, show_events,
                                     "Show events",
                                     "trackview.show_events()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewToggleShowDopesheet, trackview, toggle_show_dopesheet,
                                     "Toggle show dopesheet",
                                     "trackview.toggle_show_dopesheet()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewToggleShowCurveEditor, trackview, toggle_show_curve_editor,
                                     "Toggle show curve editor",
                                     "trackview.toggle_show_curve_editor()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewShowSequenceProperties, trackview, show_sequence_properties,
                                     "Show sequence properties",
                                     "trackview.show_sequence_properties()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewToggleLinkTimelines, trackview, toggle_link_timelines,
                                     "Toggle link timelines",
                                     "trackview.toggle_link_timelines()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewSetUnitsTicks, trackview, set_units_ticks,
                                     "Set units ticks",
                                     "trackview.set_units_ticks()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewSetUnitsTime, trackview, set_units_time,
                                     "Set units time",
                                     "trackview.set_units_time()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewSetUnitsFramecode, trackview, set_units_framecode,
                                     "Set units framecode",
                                     "trackview.set_units_framecode()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewSetUnitsFrames, trackview, set_units_frames,
                                     "Set units frames",
                                     "trackview.set_units_frames()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewGoToStart, trackview, go_to_start,
                                     "Go to start",
                                     "trackview.go_to_start()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewGoToEnd, trackview, go_to_end,
                                     "Go to end",
                                     "trackview.go_to_end()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewPausePlay, trackview, pause_play,
                                     "Pause/Play",
                                     "trackview.pause_play()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewStop, trackview, stop,
                                     "Stop",
                                     "trackview.stop()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewRecord, trackview, record,
                                     "Record",
                                     "trackview.record()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewGoToNextKey, trackview, go_to_next_key,
                                     "Go to next key",
                                     "trackview.go_to_next_key()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewGoToPrevKey, trackview, go_to_prev_key,
                                     "Go to previous key",
                                     "trackview.go_to_prev_key()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewToggleLoop, trackview, toogle_loop,
                                     "Toggle loop",
                                     "trackview.toogle_loop()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewSetPlaybackStart, trackview, set_playback_start,
                                     "Set playback start",
                                     "trackview.set_playback_start()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewSetPlaybackEnd, trackview, set_playback_end,
                                     "Set playback end",
                                     "trackview.set_playback_end()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewResetPlaybackStartEnd, trackview, reset_playback_start_end,
                                     "Reset playback start end",
                                     "trackview.reset_playback_start_end()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewRenderSequence, trackview, render_sequence,
                                     "Render sequence",
                                     "trackview.render_sequence()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewCreateLightAnimationSet, trackview, create_light_animation_set,
                                     "Create light animation set",
                                     "trackview.create_light_animation_set()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewSyncSelectedTracksToBasePosition, trackview, sync_selected_tracks_to_base_position,
                                     "Sync selected track to base position",
                                     "trackview.sync_selected_tracks_to_base_position()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewSyncSelectedTracksFromBasePosition, trackview, sync_selected_tracks_from_base_position,
                                     "Sync selected tracks from base position",
                                     "trackview.sync_selected_tracks_from_base_position()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewNoSnap, trackview, no_snap,
                                     "No snap",
                                     "trackview.no_snap");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewMagnetSnap, trackview, magnet_snap,
                                     "Magnet snap",
                                     "trackview.magnet_snap()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewFrameSnap, trackview, frame_snap,
                                     "Frame snap",
                                     "trackview.frame_snap()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewGridSnap, trackview, grid_snap,
                                     "Grid snap",
                                     "trackview.grid_snap()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewDeleteSelectedTracks, trackview, delete_selected_tracks,
                                     "Delete selected tracks",
                                     "trackview.delete_selected_tracks()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewDisableSelectedTracks, trackview, disable_selected_tracks,
                                     "Disable selected tracks",
                                     "trackview.disable_selected_tracks()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewMuteSelectedTracks, trackview, mute_selected_tracks,
                                     "Mute selected tracks",
                                     "trackview.mute_selected_tracks()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewEnableSelectedTracks, trackview, enable_selected_tracks,
                                     "Enable selected tracks",
                                     "trackview.enable_selected_tracks()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewSelectMoveKeysTool, trackview, select_move_keys_tool,
                                     "Select move keys tool",
                                     "trackview.select_move_keys_tool()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewSelectSlideKeysTool, trackview, select_slide_keys_tool,
                                     "Select slide keys tool",
                                     "trackview.select_slide_keys_tool()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewSelectScaleKeysTool, trackview, select_scale_keys_tools,
                                     "Select scale keys tool",
                                     "trackview.select_scale_keys_tools()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewSetTangentAuto, trackview, set_tangent_auto,
                                     "Set tangent auto",
                                     "trackview.set_tangent_auto()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewSetTangentInZero, trackview, set_tangent_in_zero,
                                     "Set tangent in zero",
                                     "trackview.set_trangent_in_zero()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewSetTangentInStep, trackview, set_tangent_in_step,
                                     "Set tangent in step",
                                     "trackview.set_tangent_in_step()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewSetTangentInLinear, trackview, set_tangent_in_linear,
                                     "Set tangent in linear",
                                     "trackview.set_tangent_in_linear()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewSetTangentOutZero, trackview, set_tangent_out_zero,
                                     "Set tangent out zero",
                                     "trackview.set_tangent_out_zero()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewSetTangentOutStep, trackview, set_tangent_out_step,
                                     "Set tangent out step",
                                     "trackview.set_tangent_out_step()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewSetTangentOutLinear, trackview, set_tangent_out_linear,
                                     "Set tangent out linear",
                                     "trackview.set_tangent_out_linear()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewBreakTangents, trackview, break_tangents,
                                     "Break tangents",
                                     "trackview.break_tangents()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewUnifyTangents, trackview, unify_tangents,
                                     "Unify tangents",
                                     "trackview.unify_tangents()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewFitViewHorizontal, trackview, fit_view_horizontal,
                                     "Fit view horizontal",
                                     "trackview.fit_view_horizontal()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewFitViewVertical, trackview, fit_view_vertical,
                                     "Fit view vertical",
                                     "trackview.fit_view_vertical()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewAddTrackPosition, trackview, add_track_position,
                                     "Add track: Position",
                                     "trackview.add_track_position()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewAddTrackRotation, trackview, add_track_rotation,
                                     "Add track: Rotation",
                                     "trackview.add_track_rotation()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewAddTrackScale, trackview, add_track_scale,
                                     "Add track: Scale",
                                     "trackview.add_track_scale()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewAddTrackVisibility, trackview, add_track_visibility,
                                     "Add track: Visibility",
                                     "trackview.add_track_visibility()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewAddTrackAnimation, trackview, add_track_animation,
                                     "Add track: Animation",
                                     "trackview.add_track_animation()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewAddTrackMannequin, trackview, add_track_mannequin,
                                     "Add track: Mannequin",
                                     "trackview.add_track_mannequin()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewAddTrackNoise, trackview, add_track_noise,
                                     "Add track: Noise",
                                     "trackview.add_track_noise()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewAddTrackAudioFile, trackview, add_track_audio_file,
                                     "Add track: Audio.File",
                                     "trackview.add_track_audio_file()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewAddTrackAudioParameter, trackview, add_track_audio_parameter,
                                     "Add track: Audio.Parameter",
                                     "trackview.add_track_audio_parameter()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewAddTrackAudioSwitch, trackview, add_track_audio_switch,
                                     "Add track: Audio.Switch",
                                     "trackview.add_track_audio_switch()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewAddTrackAudioTrigger, trackview, add_track_audio_trigger,
                                     "Add track: Audio.Trigger",
                                     "trackview.add_track_audio_trigger()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewAddTrackDRSSignal, trackview, add_track_drs_signal,
                                     "Add track: DRS Signal",
                                     "trackview.add_track_drs_signal()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewAddTrackEvent, trackview, add_track_event,
                                     "Add track: Event",
                                     "trackview.add_track_event()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewAddTrackExpression, trackview, add_track_expression,
                                     "Add track: Expression",
                                     "trackview.add_track_expression()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewAddTrackFacialSequence, trackview, add_track_facial_sequence,
                                     "Add track: Facial sequence",
                                     "trackview.add_track_facial_sequence()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewAddTrackLookAt, trackview, add_track_look_at,
                                     "Add track: Look At",
                                     "trackview.add_track_look_at()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewAddTrackPhysicalize, trackview, add_track_physicalize,
                                     "Add track: Physicalize",
                                     "trackview.add_track_physicalize()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewAddTrackPhysicsDriven, trackview, add_track_physics_driven,
                                     "Add track: Physics driven",
                                     "trackview.add_track_physics_driven()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewAddTrackProceduralEyes, trackview, add_track_procedural_eyes,
                                     "Add track: Procedural Eyes",
                                     "trackview.add_track_procedural_eyes()");
REGISTER_COMMAND_REMAPPING(trackview, toggle_sync_selection, general, toggle_sync_selection)
#pragma warning(pop)
