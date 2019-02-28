// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// CryEngine Source File.
// Copyright (C), Crytek, 1999-2014.

#include "StdAfx.h"
#include "Nodes/TrackViewAnimNode.h"
#include "TrackViewUndo.h"
#include "TrackViewSequenceManager.h"
#include "TrackViewPlugin.h"
#include "AnimationContext.h"

#include <CryMovie/IMovieSystem.h>

#pragma warning(push)
#pragma warning(disable : 4005 4244 4800)
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
void PyTrackViewNewSequence(const char* name)
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

void PyTrackViewDeleteSequence(const char* name)
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

void PyTrackViewPlaySequence()
{
	CAnimationContext* pAnimationContext = CTrackViewPlugin::GetAnimationContext();
	if (pAnimationContext->IsPlaying())
	{
		return;
	}

	pAnimationContext->PlayPause(true);
}

void PyTrackViewStopSequence()
{
	CAnimationContext* pAnimationContext = CTrackViewPlugin::GetAnimationContext();
	if (!pAnimationContext->IsPlaying())
	{
		return;
	}

	pAnimationContext->Stop();
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

void PyTrackViewAddSelectedEntities()
{
	CAnimationContext* pAnimationContext = CTrackViewPlugin::GetAnimationContext();
	CTrackViewSequence* pSequence = pAnimationContext->GetSequence();
	if (!pSequence)
	{
		return;
	}

	CUndo undo("Add entities to TrackView");
	pSequence->AddSelectedEntities();
	pSequence->BindToEditorObjects();
}

void PyTrackViewAddLayerNode()
{
	CAnimationContext* pAnimationContext = CTrackViewPlugin::GetAnimationContext();
	CTrackViewSequence* pSequence = pAnimationContext->GetSequence();
	if (!pSequence)
	{
		return;
	}

	CUndo undo("Add current layer to TrackView");
	pSequence->AddCurrentLayer();
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
}

DECLARE_PYTHON_MODULE(trackview);

// General
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewSetRecording, trackview, set_recording,
                                     "Activates/deactivates TrackView recording mode",
                                     "trackview.set_time(bool bRecording)");

// Sequences
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewNewSequence, trackview, new_sequence,
                                     "Creates a new sequence with the given name.",
                                     "trackview.new_sequence(str sequenceName)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewDeleteSequence, trackview, delete_sequence,
                                     "Deletes the specified sequence.",
                                     "trackview.delete_sequence(str sequenceName)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewSetCurrentSequence, trackview, set_current_sequence,
                                     "Sets the specified sequence as a current one in TrackView.",
                                     "trackview.set_current_sequence(str sequenceName)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewGetNumSequences, trackview, get_num_sequences,
                                     "Gets the number of sequences.",
                                     "trackview.get_num_sequences()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewGetSequenceName, trackview, get_sequence_name,
                                     "Gets the name of a sequence by its index.",
                                     "trackview.get_sequence_name(int sequenceIndex)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewGetSequenceTimeRange, trackview, get_sequence_time_range,
                                     "Gets the time range of a sequence as a pair",
                                     "trackview.get_sequence_time_range(string sequenceName)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewSetSequenceTimeRange, trackview, set_sequence_time_range,
                                     "Sets the time range of a sequence",
                                     "trackview.set_sequence_time_range(string sequenceName, float start, float end)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewPlaySequence, trackview, play_sequence,
                                     "Plays the current sequence in TrackView.",
                                     "trackview.play_sequence()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewStopSequence, trackview, stop_sequence,
                                     "Stops any sequence currently playing in TrackView.",
                                     "trackview.stop_sequence()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewSetSequenceTime, trackview, set_time,
                                     "Sets the time of the sequence currently playing in TrackView.",
                                     "trackview.set_time(float time)");

// Nodes
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewAddNode, trackview, add_node,
                                     "Adds a new node with the given type & name to the current sequence.",
                                     "trackview.add_node(str nodeTypeName, str nodeName)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewAddSelectedEntities, trackview, add_selected_entities,
                                     "Adds an entity node(s) from viewport selection to the current sequence.",
                                     "trackview.add_entity_node()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewAddLayerNode, trackview, add_layer_node,
                                     "Adds a layer node from the current layer to the current sequence.",
                                     "trackview.add_layer_node()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewDeleteNode, trackview, delete_node,
                                     "Deletes the specified node from the current sequence.",
                                     "trackview.delete_node(str nodeName, str parentDirectorName)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewAddTrack, trackview, add_track,
                                     "Adds a track of the given parameter ID to the node.",
                                     "trackview.add_track(str paramType, str nodeName, str parentDirectorName)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewDeleteTrack, trackview, delete_track,
                                     "Deletes a track of the given parameter ID (in the given index in case of a multi-track) from the node.",
                                     "trackview.delete_track(str paramType, int index, str nodeName, str parentDirectorName)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewGetNumNodes, trackview, get_num_nodes,
                                     "Gets the number of sequences.",
                                     "trackview.get_num_nodes(str parentDirectorName)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewGetNodeName, trackview, get_node_name,
                                     "Gets the name of a sequence by its index.",
                                     "trackview.get_node_name(int nodeIndex, str parentDirectorName)");

// Tracks
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewGetNumTrackKeys, trackview, get_num_track_keys,
                                     "Gets number of keys of the specified track",
                                     "trackview.get_num_track_keys(str paramName, int trackIndex, str nodeName, str parentDirectorName)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewGetKeyValue, trackview, get_key_value,
                                     "Gets the value of the specified key",
                                     "trackview.get_num_track_keys(str paramName, int trackIndex, int keyIndex, str nodeName, str parentDirectorName)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyTrackViewGetInterpolatedValue, trackview, get_interpolated_value,
                                     "Gets the interpolated value of a track at the specified time",
                                     "trackview.get_num_track_keys(str paramName, int trackIndex, float time, str nodeName, str parentDirectorName)");

#pragma warning(pop)

