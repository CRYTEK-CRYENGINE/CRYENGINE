// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// CryEngine Source File.
// Copyright (C), Crytek, 1999-2014.

#include "StdAfx.h"
#include "TrackViewTrack.h"
#include "TrackViewAnimNode.h"
#include "TrackViewSequence.h"
#include "TrackViewUndo.h"
#include "TrackViewNodeFactories.h"
#include "TrackViewPlugin.h"
#include "TrackViewSequenceManager.h"

namespace
{
float PreferShortestRotPath(float degree, float degree0)
{
	// Assumes the degree is in (-PI, PI).
	assert(-181.0f < degree && degree < 181.0f);
	float degree00 = degree0;
	degree0 = fmod_tpl(degree0, 360.0f);
	float n = (degree00 - degree0) / 360.0f;
	float degreeAlt;
	if (degree >= 0)
	{
		degreeAlt = degree - 360.0f;
	}
	else
	{
		degreeAlt = degree + 360.0f;
	}
	if (fabs(degreeAlt - degree0) < fabs(degree - degree0))
	{
		return degreeAlt + n * 360.0f;
	}
	else
	{
		return degree + n * 360.0f;
	}
}
}

void CTrackViewTrackBundle::AppendTrack(CTrackViewTrack* pTrack)
{
	// Check if newly added key has different type than existing ones
	if (m_bAllOfSameType && m_tracks.size() > 0)
	{
		const CTrackViewTrack* pLastTrack = m_tracks.back();

		if (pTrack->GetParameterType() != pLastTrack->GetParameterType()
		    || pTrack->GetValueType() != pLastTrack->GetValueType())
		{
			m_bAllOfSameType = false;
		}
	}

	stl::push_back_unique(m_tracks, pTrack);
}

void CTrackViewTrackBundle::AppendTrackBundle(const CTrackViewTrackBundle& bundle)
{
	for (auto iter = bundle.m_tracks.begin(); iter != bundle.m_tracks.end(); ++iter)
	{
		AppendTrack(*iter);
	}
}

CTrackViewTrack::CTrackViewTrack(IAnimTrack* pTrack, CTrackViewAnimNode* pTrackAnimNode,
                                 CTrackViewNode* pParentNode, bool bIsSubTrack, unsigned int subTrackIndex) : CTrackViewNode(pParentNode),
	m_pAnimTrack(pTrack), m_pTrackAnimNode(pTrackAnimNode), m_bIsSubTrack(bIsSubTrack), m_subTrackIndex(subTrackIndex)
{
	// Search for child tracks
	const unsigned int subTrackCount = m_pAnimTrack->GetSubTrackCount();
	for (unsigned int subTrackIndex = 0; subTrackIndex < subTrackCount; ++subTrackIndex)
	{
		IAnimTrack* pSubTrack = m_pAnimTrack->GetSubTrack(subTrackIndex);

		CTrackViewTrackFactory trackFactory;
		CTrackViewTrack* pNewTVTrack = trackFactory.BuildTrack(pSubTrack, pTrackAnimNode, this, true, subTrackIndex);
		AddNode(pNewTVTrack);
	}

	m_keySelectionStates.resize(pTrack->GetNumKeys(), false);

	m_bIsCompoundTrack = subTrackCount > 0;
}

CTrackViewAnimNode* CTrackViewTrack::GetAnimNode() const
{
	return m_pTrackAnimNode;
}

bool CTrackViewTrack::SnapTimeToPrevKey(SAnimTime& time) const
{
	CTrackViewKeyHandle prevKey = const_cast<CTrackViewTrack*>(this)->GetPrevKey(time);

	if (prevKey.IsValid())
	{
		time = prevKey.GetTime();
		return true;
	}

	return false;
}

bool CTrackViewTrack::SnapTimeToNextKey(SAnimTime& time) const
{
	CTrackViewKeyHandle prevKey = const_cast<CTrackViewTrack*>(this)->GetNextKey(time);

	if (prevKey.IsValid())
	{
		time = prevKey.GetTime();
		return true;
	}

	return false;
}

CTrackViewTrack* CTrackViewTrack::GetSubTrack(uint index)
{
	assert(index < GetChildCount());
	return static_cast<CTrackViewTrack*>(GetChild(index));
}

CTrackViewKeyHandle CTrackViewTrack::GetPrevKey(const SAnimTime time)
{
	CTrackViewKeyHandle keyHandle;

	const SAnimTime startTime = time;
	SAnimTime closestTime = SAnimTime::Min();

	const int numKeys = m_pAnimTrack->GetNumKeys();
	for (int i = 0; i < numKeys; ++i)
	{
		const SAnimTime keyTime = m_pAnimTrack->GetKeyTime(i);
		if (keyTime < startTime && keyTime > closestTime)
		{
			keyHandle = CTrackViewKeyHandle(this, i);
			closestTime = keyTime;
		}
	}

	return keyHandle;
}

CTrackViewKeyHandle CTrackViewTrack::GetNextKey(const SAnimTime time)
{
	CTrackViewKeyHandle keyHandle;

	const SAnimTime startTime = time;
	SAnimTime closestTime = SAnimTime::Max();
	bool bFoundKey = false;

	const int numKeys = m_pAnimTrack->GetNumKeys();
	for (int i = 0; i < numKeys; ++i)
	{
		const SAnimTime keyTime = m_pAnimTrack->GetKeyTime(i);
		if (keyTime > startTime && keyTime < closestTime)
		{
			keyHandle = CTrackViewKeyHandle(this, i);
			closestTime = keyTime;
		}
	}

	return keyHandle;
}

CTrackViewKeyBundle CTrackViewTrack::GetSelectedKeys()
{
	CTrackViewKeyBundle bundle;

	if (m_bIsCompoundTrack)
	{
		for (auto iter = m_childNodes.begin(); iter != m_childNodes.end(); ++iter)
		{
			bundle.AppendKeyBundle((*iter)->GetSelectedKeys());
		}
	}
	else
	{
		bundle = GetKeys(true, SAnimTime::Min(), SAnimTime::Max());
	}

	return bundle;
}

CTrackViewKeyBundle CTrackViewTrack::GetAllKeys()
{
	CTrackViewKeyBundle bundle;

	if (m_bIsCompoundTrack)
	{
		for (auto iter = m_childNodes.begin(); iter != m_childNodes.end(); ++iter)
		{
			bundle.AppendKeyBundle((*iter)->GetAllKeys());
		}
	}
	else
	{
		bundle = GetKeys(false, SAnimTime::Min(), SAnimTime::Max());
	}

	return bundle;
}

CTrackViewKeyBundle CTrackViewTrack::GetKeysInTimeRange(const SAnimTime t0, const SAnimTime t1)
{
	CTrackViewKeyBundle bundle;

	if (m_bIsCompoundTrack)
	{
		for (auto iter = m_childNodes.begin(); iter != m_childNodes.end(); ++iter)
		{
			bundle.AppendKeyBundle((*iter)->GetKeysInTimeRange(t0, t1));
		}
	}
	else
	{
		bundle = GetKeys(false, t0, t1);
	}

	return bundle;
}

CTrackViewKeyBundle CTrackViewTrack::GetKeys(const bool bOnlySelected, const SAnimTime t0, const SAnimTime t1)
{
	CTrackViewKeyBundle bundle;

	const int keyCount = m_pAnimTrack->GetNumKeys();
	for (int keyIndex = 0; keyIndex < keyCount; ++keyIndex)
	{
		const SAnimTime keyTime = m_pAnimTrack->GetKeyTime(keyIndex);
		const bool timeRangeOk = (t0 <= keyTime && keyTime <= t1);

		if ((!bOnlySelected || IsKeySelected(keyIndex)) && timeRangeOk)
		{
			CTrackViewKeyHandle keyHandle(this, keyIndex);
			bundle.AppendKey(keyHandle);
		}
	}

	return bundle;
}

CTrackViewKeyHandle CTrackViewTrack::CreateKey(const SAnimTime time)
{
	const int keyIndex = m_pAnimTrack->CreateKey(time);
	m_keySelectionStates.insert(m_keySelectionStates.begin() + keyIndex, false);
	GetSequence()->OnNodeChanged(this, ITrackViewSequenceListener::eNodeChangeType_KeysChanged);
	return CTrackViewKeyHandle(this, keyIndex);
}

void CTrackViewTrack::ClearKeys()
{
	assert(CUndo::IsRecording());
	m_pAnimTrack->ClearKeys();
	GetSequence()->OnNodeChanged(this, ITrackViewSequenceListener::eNodeChangeType_KeysChanged);
}

CTrackViewKeyHandle CTrackViewTrack::GetKey(unsigned int index)
{
	if (index < GetKeyCount())
	{
		return CTrackViewKeyHandle(this, index);
	}

	return CTrackViewKeyHandle();
}

CTrackViewKeyConstHandle CTrackViewTrack::GetKey(unsigned int index) const
{
	if (index < GetKeyCount())
	{
		return CTrackViewKeyConstHandle(this, index);
	}

	return CTrackViewKeyConstHandle();
}

CTrackViewKeyHandle CTrackViewTrack::GetKeyByTime(const SAnimTime time)
{
	if (m_bIsCompoundTrack)
	{
		// Search key in sub tracks
		unsigned int currentIndex = 0;

		unsigned int childCount = GetChildCount();
		for (unsigned int i = 0; i < childCount; ++i)
		{
			CTrackViewTrack* pChildTrack = static_cast<CTrackViewTrack*>(GetChild(i));

			int keyIndex = pChildTrack->m_pAnimTrack->FindKey(time);
			if (keyIndex >= 0)
			{
				return CTrackViewKeyHandle(this, currentIndex + keyIndex);
			}

			currentIndex += pChildTrack->GetKeyCount();
		}
	}

	int keyIndex = m_pAnimTrack->FindKey(time);

	if (keyIndex < 0)
	{
		return CTrackViewKeyHandle();
	}

	return CTrackViewKeyHandle(this, keyIndex);
}

void CTrackViewTrack::GetKeyValueRange(float& min, float& max) const
{
	m_pAnimTrack->GetKeyValueRange(min, max);
}

IAnimTrack::EAnimTrackFlags CTrackViewTrack::GetFlags() const
{
	return (IAnimTrack::EAnimTrackFlags)m_pAnimTrack->GetFlags();
}

CTrackViewTrackMemento CTrackViewTrack::GetMemento() const
{
	CTrackViewTrackMemento memento;
	memento.m_serializedTrackState = XmlHelpers::CreateXmlNode("TrackState");
	m_pAnimTrack->Serialize(memento.m_serializedTrackState, false);
	return memento;
}

void CTrackViewTrack::RestoreFromMemento(const CTrackViewTrackMemento& memento)
{
	// We're going to de-serialize, so this is const safe
	XmlNodeRef& xmlNode = const_cast<XmlNodeRef&>(memento.m_serializedTrackState);
	m_pAnimTrack->Serialize(xmlNode, true);
}

const char* CTrackViewTrack::GetName() const
{
	CTrackViewNode* pParentNode = GetParentNode();

	if (pParentNode->GetNodeType() == eTVNT_Track)
	{
		CTrackViewTrack* pParentTrack = static_cast<CTrackViewTrack*>(pParentNode);
		return pParentTrack->m_pAnimTrack->GetSubTrackName(m_subTrackIndex);
	}

	return GetAnimNode()->GetParamName(GetParameterType());
}

void CTrackViewTrack::SetDisabled(bool bDisabled)
{
	bool oldState = IsDisabled();
	if (bDisabled)
	{
		m_pAnimTrack->SetFlags(m_pAnimTrack->GetFlags() | IAnimTrack::eAnimTrackFlags_Disabled);
		GetSequence()->OnNodeChanged(this, ITrackViewSequenceListener::eNodeChangeType_Disabled);
	}
	else
	{
		m_pAnimTrack->SetFlags(m_pAnimTrack->GetFlags() & ~IAnimTrack::eAnimTrackFlags_Disabled);
		GetSequence()->OnNodeChanged(this, ITrackViewSequenceListener::eNodeChangeType_Enabled);
	}

	if (CUndo::IsRecording())
	{
		CUndo::Record(new CUndoTrackNodeDisable(this, oldState));
	}
}

bool CTrackViewTrack::IsDisabled() const
{
	return (m_pAnimTrack->GetFlags() & IAnimTrack::eAnimTrackFlags_Disabled) != 0;
}

void CTrackViewTrack::SetMuted(bool bMuted)
{
	if (bMuted)
	{
		m_pAnimTrack->SetFlags(m_pAnimTrack->GetFlags() | IAnimTrack::eAnimTrackFlags_Muted);
		GetSequence()->OnNodeChanged(this, ITrackViewSequenceListener::eNodeChangeType_Muted);
	}
	else
	{
		m_pAnimTrack->SetFlags(m_pAnimTrack->GetFlags() & ~IAnimTrack::eAnimTrackFlags_Muted);
		GetSequence()->OnNodeChanged(this, ITrackViewSequenceListener::eNodeChangeType_Unmuted);
	}
}

bool CTrackViewTrack::IsMuted() const
{
	return (m_pAnimTrack->GetFlags() & IAnimTrack::eAnimTrackFlags_Muted) != 0;
}

void CTrackViewTrack::SetKey(unsigned int keyIndex, const STrackKey* pKey)
{
	m_pAnimTrack->SetKey(keyIndex, pKey);
	GetSequence()->OnNodeChanged(this, ITrackViewSequenceListener::eNodeChangeType_KeysChanged);
}

void CTrackViewTrack::GetKey(unsigned int keyIndex, STrackKey* pKey) const
{
	m_pAnimTrack->GetKey(keyIndex, pKey);
}

void CTrackViewTrack::SelectKey(unsigned int keyIndex, bool bSelect)
{
	if (keyIndex < m_keySelectionStates.size())
	{
		if (m_keySelectionStates[keyIndex] != bSelect)
		{
			m_keySelectionStates[keyIndex] = bSelect;
			GetSequence()->OnNodeChanged(this, ITrackViewSequenceListener::eNodeChangeType_KeySelectionChanged);
		}
	}
}

SAnimTime CTrackViewTrack::GetKeyTime(const uint index) const
{
	return m_pAnimTrack->GetKeyTime(index);
}

string CTrackViewTrack::GetKeyDescription(const uint index) const
{
	_smart_ptr<IAnimKeyWrapper> pWrappedKey = m_pAnimTrack->GetWrappedKey(index);
	return pWrappedKey->GetDescription();
}

void CTrackViewTrack::RemoveKey(const int index)
{
	m_pAnimTrack->RemoveKey(index);
	GetSequence()->OnNodeChanged(this, ITrackViewSequenceListener::eNodeChangeType_KeysChanged);
}

void CTrackViewTrack::GetSelectedKeysTimes(std::vector<SAnimTime>& selectedKeysTimes)
{
	const int keyCount = m_pAnimTrack->GetNumKeys();
	for (int keyIndex = 0; keyIndex < keyCount; ++keyIndex)
	{
		const SAnimTime keyTime = m_pAnimTrack->GetKeyTime(keyIndex);
		if (IsKeySelected(keyIndex))
		{
			selectedKeysTimes.push_back(keyTime);
		}
	}
}

void CTrackViewTrack::SelectKeysByAnimTimes(const std::vector<SAnimTime>& selectedKeys)
{
	CTrackViewKeyBundle allKeys = GetAllKeys();
	allKeys.SelectKeys(false);

	for (size_t i = 0; i < allKeys.GetKeyCount(); i++)
	{
		CTrackViewKeyHandle keyHandle = allKeys.GetKey(i);
		bool found = (std::find(selectedKeys.begin(), selectedKeys.end(), keyHandle.GetTime()) != selectedKeys.end());
		if (found)
		{
			keyHandle.Select(true);
		}
	}
}

void CTrackViewTrack::SerializeSelectedKeys(XmlNodeRef& xmlNode, bool bLoading, SerializationCallback callback, const SAnimTime time, size_t index, SAnimTimeVector keysTimes)
{
	if (m_bIsCompoundTrack)
	{
		for (auto& node : m_childNodes)
		{
			index++;
			static_cast<CTrackViewTrack*>(node.get())->SerializeSelectedKeys(xmlNode, bLoading, callback, time, index, keysTimes);
		}
		
		callback(keysTimes);
	}
	else
	{
		XmlNodeRef subTrackNode;
		SAnimTimeVector selectedKeys;

		if (bLoading)
		{
			selectedKeys.reserve(xmlNode->getChildCount());
		}
		else
		{
			GetSelectedKeysTimes(selectedKeys);
		}

		if (bLoading)
		{
			if (index)
			{
				subTrackNode = xmlNode->getChild(index-1);
			}
			else
			{
				subTrackNode = xmlNode;
			}
		}
		else
		{
			if (index)
			{
				subTrackNode = xmlNode->newChild("NewSubTrack");
			}
			else
			{
				subTrackNode = xmlNode;
			}
		}

		if (subTrackNode.isValid())
		{
			m_pAnimTrack->SerializeKeys(subTrackNode, bLoading, selectedKeys, time);
		}		
		
		keysTimes.insert(keysTimes.end(), selectedKeys.begin(), selectedKeys.end());
	}
}

void CTrackViewTrack::SelectKeys(const bool bSelected)
{
	m_pTrackAnimNode->GetSequence()->QueueNotifications();

	if (!m_bIsCompoundTrack)
	{
		unsigned int keyCount = GetKeyCount();
		for (unsigned int i = 0; i < keyCount; ++i)
		{
			SelectKey(i, bSelected);
			m_pTrackAnimNode->GetSequence()->OnNodeChanged(this, ITrackViewSequenceListener::eNodeChangeType_KeySelectionChanged);
		}
	}
	else
	{
		// Affect sub tracks
		unsigned int childCount = GetChildCount();
		for (unsigned int childIndex = 0; childIndex < childCount; ++childCount)
		{
			CTrackViewTrack* pChildTrack = static_cast<CTrackViewTrack*>(GetChild(childIndex));
			pChildTrack->SelectKeys(bSelected);
			m_pTrackAnimNode->GetSequence()->OnNodeChanged(this, ITrackViewSequenceListener::eNodeChangeType_KeySelectionChanged);
		}
	}

	m_pTrackAnimNode->GetSequence()->SubmitPendingNotifcations();
}

bool CTrackViewTrack::IsKeySelected(unsigned int keyIndex) const
{
	return (keyIndex < m_keySelectionStates.size()) ? m_keySelectionStates[keyIndex] : false;
}

CTrackViewKeyHandle CTrackViewTrack::GetSubTrackKeyHandle(unsigned int index) const
{
	// Return handle to sub track key
	unsigned int childCount = GetChildCount();
	for (unsigned int childIndex = 0; childIndex < childCount; ++childIndex)
	{
		CTrackViewTrack* pChildTrack = static_cast<CTrackViewTrack*>(GetChild(childIndex));

		const unsigned int childKeyCount = pChildTrack->GetKeyCount();
		if (index < childKeyCount)
		{
			return pChildTrack->GetKey(index);
		}

		index -= childKeyCount;
	}

	return CTrackViewKeyHandle();
}

void CTrackViewTrack::SetAnimationLayerIndex(const int index)
{
	if (m_pAnimTrack)
	{
		m_pAnimTrack->SetAnimationLayerIndex(index);
	}
}

int CTrackViewTrack::GetAnimationLayerIndex() const
{
	return m_pAnimTrack->GetAnimationLayerIndex();
}

void CTrackViewTrack::CopyKeysToClipboard(XmlNodeRef& xmlNode, const bool bOnlySelectedKeys, const bool bOnlyFromSelectedTracks)
{
	if (bOnlyFromSelectedTracks && !IsSelected())
	{
		return;
	}

	if (GetKeyCount() == 0)
	{
		return;
	}

	XmlNodeRef trackXML = GetISystem()->CreateXmlNode("Track");
	trackXML->setAttr("name", GetName());
	GetParameterType().Serialize(trackXML, false);
	trackXML->setAttr("valueType", GetValueType());

	if (bOnlySelectedKeys)
	{
		if (GetSelectedKeys().GetKeyCount() == 0)
		{
			return;
		}

		SerializeSelectedKeys(trackXML, false, [this](SAnimTimeVector&) { GetAllKeys().SelectKeys(false); });
	}
	else
	{
		m_pAnimTrack->Serialize(trackXML, false, false);
	}

	xmlNode->addChild(trackXML);
}

void CTrackViewTrack::PasteKeys(XmlNodeRef xmlNode, const SAnimTime time)
{
	assert(CUndo::IsRecording());

	CTrackViewSequence* pSequence = GetSequence();

	CUndo::Record(new CUndoTrackObject(this, pSequence != nullptr));
	SerializeSelectedKeys(xmlNode, true, [this](SAnimTimeVector& selectedKeysTimes) { SelectKeysByAnimTimes(selectedKeysTimes); }, time);
	CUndo::Record(new CUndoAnimKeySelection(pSequence));

	GetSequence()->OnNodeChanged(this, ITrackViewSequenceListener::eNodeChangeType_KeysChanged);
}

void CTrackViewTrack::OffsetKeys(const TMovieSystemValue& offset)
{
	if (IsCompoundTrack())
	{
		const uint numSubTracks = GetChildCount();
		const size_t valueType = offset.index();
		assert(((numSubTracks == 3) && (valueType == eTDT_Vec3))
		       || ((numSubTracks == 3) && (valueType == eTDT_Quat))
		       || ((numSubTracks == 4) && (valueType == eTDT_Vec4)));

		Vec4 vecOffset;
		if ((valueType == eTDT_Vec3))
		{
			vecOffset = Vec4(stl::get<Vec3>(offset), 0.0f);
		}
		else if (valueType == eTDT_Quat)
		{
			Quat quatOffset = stl::get<Quat>(offset);
			Vec3 angles(RAD2DEG(Ang3::GetAnglesXYZ(Matrix33(quatOffset))));

			for (uint i = 0; i < numSubTracks; ++i)
			{
				CTrackViewTrack* pSubTrack = GetSubTrack(i);
				pSubTrack->OffsetKeys(TMovieSystemValue(angles[i]));
			}
			return;
		}
		else
		{
			vecOffset = stl::get<Vec4>(offset);
		}

		for (uint i = 0; i < numSubTracks; ++i)
		{
			CTrackViewTrack* pSubTrack = GetSubTrack(i);
			pSubTrack->OffsetKeys(TMovieSystemValue(vecOffset[i]));
		}
	}
}

void CTrackViewTrack::SetValue(const SAnimTime time, const TMovieSystemValue& value)
{
	if (IsCompoundTrack())
	{
		const uint numSubTracks = GetChildCount();
		const size_t valueType = value.index();
		assert(((numSubTracks == 3) && (valueType == eTDT_Vec3))
		       || ((numSubTracks == 3) && (valueType == eTDT_Quat))
		       || ((numSubTracks == 4) && (valueType == eTDT_Vec4)));

		switch (value.index())
		{
		case eTDT_Vec3:
			if (numSubTracks == 3)
			{
				const Vec3 vecValue = stl::get<Vec3>(value);
				for (int i = 0; i < numSubTracks; ++i)
				{
					GetSubTrack(i)->SetValue(time, TMovieSystemValue(vecValue[i]));
				}
			}
		case eTDT_Vec4:
			if (numSubTracks == 4)
			{
				const Vec4 vecValue = stl::get<Vec4>(value);
				for (int i = 0; i < numSubTracks; ++i)
				{
					GetSubTrack(i)->SetValue(time, TMovieSystemValue(vecValue[i]));
				}
			}
			break;
		case eTDT_Quat:
			if (numSubTracks == 3)
			{
				const Quat quatValue = stl::get<Quat>(value);

				// Assume Euler Angles XYZ
				Ang3 angles = Ang3::GetAnglesXYZ(quatValue);
				for (int i = 0; i < 3; i++)
				{
					float degree = RAD2DEG(angles[i]);
					// Try to prefer the shortest path of rotation.
					const TMovieSystemValue degree0 = GetSubTrack(i)->GetValue(time);
					const float degree0Float = stl::get<float>(degree0);
					degree = PreferShortestRotPath(degree, degree0Float);
					GetSubTrack(i)->SetValue(time, TMovieSystemValue(degree));
				}
			}
		}
	}
	else
	{
		m_pAnimTrack->SetValue(time, value);
	}
}

bool CTrackViewTrack::KeysHaveDuration() const
{
	return m_pAnimTrack->KeysDeriveTrackDurationKey();
}

SAnimTime CTrackViewTrack::GetKeyDuration(const uint index) const
{
	if (m_pAnimTrack->KeysDeriveTrackDurationKey())
	{
		_smart_ptr<IAnimKeyWrapper> pWrappedKey = m_pAnimTrack->GetWrappedKey(index);
		const STrackDurationKey* pDurationKey = static_cast<const STrackDurationKey*>(pWrappedKey->Key());
		return pDurationKey->m_duration;
	}

	return SAnimTime(0);
}

void CTrackViewTrack::Serialize(Serialization::IArchive& ar)
{
	CTrackViewNode::Serialize(ar);
	m_pAnimTrack->Serialize(ar);
}
