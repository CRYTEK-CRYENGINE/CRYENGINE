// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// CryEngine Source File.
// Copyright (C), Crytek, 1999-2014.

#include "StdAfx.h"

#include "SequenceObject.h"
#include "Objects/EntityObject.h"
#include "Objects/ObjectLoader.h"

#include <CryEntitySystem/IEntitySystem.h>

#include "TrackViewSequenceManager.h"
#include "TrackViewPlugin.h"
#include "IUndoManager.h"

REGISTER_CLASS_DESC(CSequenceObjectClassDesc)

IMPLEMENT_DYNCREATE(CSequenceObject, CBaseObject)

CSequenceObject::CSequenceObject()
	: m_pSequence(nullptr)
	, m_sequenceId(0)
{
}

bool CSequenceObject::Init(CBaseObject* pPrev, const string& file)
{
	// A SequenceObject cannot be cloned.
	if (pPrev)
	{
		return false;
	}

	SetColor(RGB(127, 127, 255));

	if (!file.IsEmpty())
	{
		SetName(file);
	}

	SetTextureIcon(GetClassDesc()->GetTextureIconId());

	// Must be after SetSequence call.
	return CBaseObject::Init(pPrev, file);
}

bool CSequenceObject::CreateGameObject()
{
	if (!m_pSequence)
	{
		CTrackViewSequenceManager* pSequenceManager = CTrackViewPlugin::GetSequenceManager();
		m_pSequence = pSequenceManager->OnCreateSequenceObject(GetName().GetString());
	}

	if (m_pSequence)
	{
		m_pSequence->SetOwner(this);
	}

	return true;
}

void CSequenceObject::Done()
{
	LOADING_TIME_PROFILE_SECTION_ARGS(GetName().c_str());
	assert(m_pSequence);

	CTrackViewPlugin::GetSequenceManager()->OnDeleteSequenceObject(GetName().GetString());

	if (m_pSequence)
	{
		m_pSequence->SetOwner(nullptr);
	}

	m_pSequence = nullptr;

	CBaseObject::Done();
}

void CSequenceObject::SetName(const string& name)
{
	if (!m_pSequence)
	{
		CBaseObject::SetName(name);
		return;
	}

	// If it has a valid sequence, the name cannot be edited through this.
}

void CSequenceObject::OnModified()
{
	assert(m_pSequence);

	if (m_pSequence)
	{
		string fullname = m_pSequence->GetName();
		CBaseObject::SetName(fullname.c_str());
		SetLayerModified();
	}
}

void CSequenceObject::GetBoundBox(AABB& box)
{
	box.SetTransformedAABB(GetWorldTM(), AABB(Vec3(-1, -1, -1), Vec3(1, 1, 1)));
}

void CSequenceObject::GetLocalBounds(AABB& box)
{
	box.min = Vec3(-1, -1, -1);
	box.max = Vec3(1, 1, 1);
}

void CSequenceObject::Display(CObjectRenderHelper& objRenderHelper)
{
	DisplayContext& dc = objRenderHelper.GetDisplayContextRef();
	DrawDefault(dc);
}

void CSequenceObject::Serialize(CObjectArchive& ar)
{
	CBaseObject::Serialize(ar);

	// Sequence Undo/Redo is not handled by serialization. See TrackViewUndo.cpp
	const IUndoManager* pUndoManager = GetIEditor()->GetIUndoManager();
	if (pUndoManager->IsUndoTransaction() || pUndoManager->IsUndoRecording())
	{
		return;
	}

	if (ar.bLoading)
	{
		XmlNodeRef idNode = ar.node->findChild("ID");

		if (idNode)
		{
			idNode->getAttr("value", m_sequenceId);

			if (GetIEditor()->GetMovieSystem()->FindSequenceById(m_sequenceId)
			    || ar.IsAmongPendingIds(m_sequenceId))  // A collision found!
			{
				uint32 newId = GetIEditor()->GetMovieSystem()->GrabNextSequenceId();
				ar.AddSequenceIdMapping(m_sequenceId, newId);
				m_sequenceId = newId;
			}
		}
	}
	else
	{
		assert(m_pSequence);

		if (m_pSequence)
		{
			XmlNodeRef sequenceNode = ar.node->newChild("Sequence");
			m_pSequence->Serialize(sequenceNode, false);

			// The id should be saved here again so that it can be restored
			// early enough. This is required for a proper saving of FG nodes like
			// the 'TrackEvent' node with the new id-based sequence identification.
			XmlNodeRef idNode = ar.node->newChild("ID");
			idNode->setAttr("value", m_pSequence->GetId());
		}
	}
}

void CSequenceObject::PostLoad(CObjectArchive& ar)
{
	XmlNodeRef sequenceNode = ar.node->findChild("Sequence");

	if (m_pSequence != NULL && sequenceNode != NULL)
	{
		m_pSequence->Serialize(sequenceNode, true, true, m_sequenceId, ar.bUndo);
		CTrackViewSequence* pTrackViewSequence = CTrackViewPlugin::GetSequenceManager()->GetSequenceByName(m_pSequence->GetName());

		if (pTrackViewSequence)
		{
			pTrackViewSequence->Load();
		}
	}
}

