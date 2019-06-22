// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TrackViewPlugin.h"
#include "TrackViewSequenceManager.h"
#include "TrackViewUndo.h"
#include "Nodes/TrackViewEntityNode.h"
#include "Material/MaterialManager.h"
#include "AnimationContext.h"
#include "Objects/EntityObject.h"
#include "Objects/ObjectManager.h"
#include "IUndoManager.h"
#include "GameEngine.h"

CTrackViewSequenceManager::CTrackViewSequenceManager() : m_nextSequenceId(0)
{
	GetIEditor()->RegisterNotifyListener(this);
	GetIEditor()->GetMaterialManager()->AddListener(this);
	auto pManager = GetIEditor()->GetObjectManager();
	pManager->signalObjectsChanged.Connect(this, &CTrackViewSequenceManager::OnObjectsChanged);
}

CTrackViewSequenceManager::~CTrackViewSequenceManager()
{
	auto pManager = GetIEditor()->GetObjectManager();
	pManager->signalObjectsChanged.DisconnectObject(this);
	GetIEditor()->GetMaterialManager()->RemoveListener(this);
	GetIEditor()->UnregisterNotifyListener(this);
}

void CTrackViewSequenceManager::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	switch (event)
	{
	case eNotify_OnClearLevelContents:
	// Fall through
	case eNotify_OnBeginLoad:
		m_bUnloadingLevel = true;
		break;
	case eNotify_OnEndLoad:
		PostLoad();
	// Fall through
	case eNotify_OnEndNewScene:
	// Fall through
	case eNotify_OnEndSceneOpen:
		m_bUnloadingLevel = false;
		SortSequences();
		break;
	}
}

CTrackViewSequence* CTrackViewSequenceManager::GetSequenceByName(const string& name) const
{
	for (auto iter = m_sequences.begin(); iter != m_sequences.end(); ++iter)
	{
		CTrackViewSequence* pSequence = (*iter).get();

		if (pSequence->GetName() == name)
		{
			return pSequence;
		}
	}

	return nullptr;
}

CTrackViewSequence* CTrackViewSequenceManager::GetSequenceByAnimSequence(IAnimSequence* pAnimSequence) const
{
	for (auto iter = m_sequences.begin(); iter != m_sequences.end(); ++iter)
	{
		CTrackViewSequence* pSequence = (*iter).get();

		if (pSequence->m_pAnimSequence == pAnimSequence)
		{
			return pSequence;
		}
	}

	return nullptr;
}

CTrackViewSequence* CTrackViewSequenceManager::GetSequenceByGUID(CryGUID guid) const
{
	for (auto iter = m_sequences.begin(); iter != m_sequences.end(); ++iter)
	{
		CTrackViewSequence* pSequence = (*iter).get();
		if (pSequence->GetGUID() == guid)
		{
			return pSequence;
		}
	}

	return nullptr;
}

CTrackViewSequence* CTrackViewSequenceManager::GetSequenceByIndex(unsigned int index) const
{
	if (index >= m_sequences.size())
	{
		return nullptr;
	}

	return m_sequences[index].get();
}

CTrackViewSequence* CTrackViewSequenceManager::CreateSequence(const string& name)
{
	CGameEngine* pGameEngine = GetIEditor()->GetGameEngine();
	CTrackViewSequence* pExistingSequence = GetSequenceByName(name);

	if (pGameEngine && pGameEngine->IsLevelLoaded() && !pExistingSequence)
	{
		if (GetIEditor()->GetObjectManager()->CanCreateObject())
		{
			CUndo undo("Create TrackView Sequence");
			GetIEditor()->GetObjectManager()->NewObject("SequenceObject", 0, name.c_str());

			CTrackViewSequence* pSequence = GetSequenceByName(name);
			pSequence->Load();

			CAnimationContext* pAnimationContext = CTrackViewPlugin::GetAnimationContext();

			if (pAnimationContext)
			{
				TRange<SAnimTime> playbackRange;
				playbackRange.start = pSequence->GetTimeRange().start;
				playbackRange.end = pSequence->GetTimeRange().end;
				pSequence->SetPlaybackRange(pSequence->GetTimeRange());
			}

			return pSequence;
		}
		else
		{
			CryLog("$4Can't create new sequence object, do you have a valid layer selected?");
		}
	}

	return nullptr;
}

IAnimSequence* CTrackViewSequenceManager::OnCreateSequenceObject(const string& name)
{
	CTrackViewSequence* pExistingSequence = GetSequenceByName(name);
	if (pExistingSequence)
	{
		return pExistingSequence->m_pAnimSequence;
	}

	IAnimSequence* pNewCryMovieSequence = GetIEditor()->GetMovieSystem()->CreateSequence(name, false, ++m_nextSequenceId);
	CTrackViewSequence* pNewSequence = new CTrackViewSequence(pNewCryMovieSequence);

	m_sequences.push_back(std::unique_ptr<CTrackViewSequence>(pNewSequence));
	const bool bUndoWasSuspended = GetIEditor()->GetIUndoManager()->IsUndoSuspended();
	if (bUndoWasSuspended)
	{
		GetIEditor()->GetIUndoManager()->Resume();
	}

	CUndo::Record(new CUndoSequenceAdd(pNewSequence));

	if (bUndoWasSuspended)
	{
		GetIEditor()->GetIUndoManager()->Suspend();
	}

	SortSequences();

	// OnSequenceAdded will be called in PostLoad after loading
	if (!GetIEditor()->GetObjectManager()->IsLoading())
	{
		OnSequenceAdded(pNewSequence);
	}

	return pNewCryMovieSequence;
}

void CTrackViewSequenceManager::DeleteSequence(CTrackViewSequence* pSequence)
{
	CUndo undo("Delete TrackView Sequence");
	CSequenceObject* pSequenceObject = static_cast<CSequenceObject*>(pSequence->m_pAnimSequence->GetOwner());
	GetIEditor()->GetObjectManager()->DeleteObject(pSequenceObject);
}

void CTrackViewSequenceManager::OnDeleteSequenceObject(const string& name)
{
	CTrackViewSequence* pSequence = GetSequenceByName(name);
	assert(pSequence);

	if (pSequence)
	{
		const bool bUndoWasSuspended = GetIEditor()->GetIUndoManager()->IsUndoSuspended();

		if (bUndoWasSuspended)
		{
			GetIEditor()->GetIUndoManager()->Resume();
		}

		if (m_bUnloadingLevel)
		{
			// While unloading, there is no recording so
			// only make the undo object destroy the sequence
			std::unique_ptr<CUndoSequenceRemove> sequenceRemove(new CUndoSequenceRemove(pSequence));
		}
		else if (CUndo::IsRecording())
		{
			CUndo::Record(new CUndoSequenceRemove(pSequence));
		}

		if (bUndoWasSuspended)
		{
			GetIEditor()->GetIUndoManager()->Suspend();
		}
	}
}

void CTrackViewSequenceManager::AddListener(ITrackViewSequenceManagerListener* pListener)
{
	stl::push_back_unique(m_listeners, pListener);
	for (std::unique_ptr<CTrackViewSequence>& pSequence : m_sequences)
	{
		pListener->OnSequenceAdded(pSequence.get());
	}
}

void CTrackViewSequenceManager::PostLoad()
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);
	for (std::unique_ptr<CTrackViewSequence>& pSequence : m_sequences)
	{
		OnSequenceAdded(pSequence.get());
	}
}

void CTrackViewSequenceManager::SortSequences()
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);
	std::stable_sort(m_sequences.begin(), m_sequences.end(),
	                 [](const std::unique_ptr<CTrackViewSequence>& a, const std::unique_ptr<CTrackViewSequence>& b) -> bool
	{
		string aName = a.get()->GetName();
		string bName = b.get()->GetName();
		return aName.compareNoCase(bName) < 0;
	});
}

void CTrackViewSequenceManager::OnSequenceAdded(CTrackViewSequence* pSequence)
{
	for (auto iter = m_listeners.begin(); iter != m_listeners.end(); ++iter)
	{
		(*iter)->OnSequenceAdded(pSequence);
	}

	GetIEditor()->GetIUndoManager()->AddListener(pSequence);
	SortSequences();
}

void CTrackViewSequenceManager::OnSequenceRemoved(CTrackViewSequence* pSequence)
{
	GetIEditor()->GetIUndoManager()->RemoveListener(pSequence);

	for (auto iter = m_listeners.begin(); iter != m_listeners.end(); ++iter)
	{
		(*iter)->OnSequenceRemoved(pSequence);
	}
}

void CTrackViewSequenceManager::OnDataBaseItemEvent(IDataBaseItem* pItem, EDataBaseItemEvent event)
{
	const uint numSequences = m_sequences.size();

	for (uint i = 0; i < numSequences; ++i)
	{
		m_sequences[i]->UpdateDynamicParams();
	}
}

CTrackViewAnimNodeBundle CTrackViewSequenceManager::GetAllRelatedAnimNodes(const CEntityObject* pEntityObject) const
{
	CTrackViewAnimNodeBundle nodeBundle;

	const uint sequenceCount = GetCount();

	for (uint sequenceIndex = 0; sequenceIndex < sequenceCount; ++sequenceIndex)
	{
		CTrackViewSequence* pSequence = GetSequenceByIndex(sequenceIndex);
		nodeBundle.AppendAnimNodeBundle(pSequence->GetAllOwnedNodes(pEntityObject));
	}

	return nodeBundle;
}

CTrackViewAnimNode* CTrackViewSequenceManager::GetActiveAnimNode(const CEntityObject* pEntityObject) const
{
	CTrackViewAnimNodeBundle nodeBundle = GetAllRelatedAnimNodes(pEntityObject);

	const uint nodeCount = nodeBundle.GetCount();
	for (uint nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex)
	{
		CTrackViewAnimNode* pAnimNode = nodeBundle.GetNode(nodeIndex);
		if (pAnimNode->IsActive())
		{
			return pAnimNode;
		}
	}

	return nullptr;
}

void CTrackViewSequenceManager::OnObjectsChanged(const std::vector<CBaseObject*>& objects, const CObjectEvent& event)
{
	for (const CBaseObject* pObject : objects)
	{
		switch (event.m_type)
		{
		case OBJECT_ON_RENAME:
		{
			HandleObjectRename(pObject);
		}
		break;
		case OBJECT_ON_PREDELETE:
		{
			HandleObjectDelete(pObject);
		}
		break;
		case OBJECT_ON_PREATTACHED:
		{
			HandleAttachmentChange(pObject, EAttachmentChangeType::PreAttached);
		}
		case OBJECT_ON_ATTACHED:
		{
			HandleAttachmentChange(pObject, EAttachmentChangeType::Attached);
		}
		break;
		case OBJECT_ON_PREDETACHED:
		{
			const CObjectPreDetachedEvent& evt = static_cast<const CObjectPreDetachedEvent&>(event);
			HandleAttachmentChange(pObject, evt.m_shouldKeepPos ? EAttachmentChangeType::PreDetachedKeepTransform : EAttachmentChangeType::PreDetached);
		}
		break;
		case OBJECT_ON_DETACHED:
		{
			HandleAttachmentChange(pObject, EAttachmentChangeType::Detached);
		}
		break;
		}
	}
}

void CTrackViewSequenceManager::HandleAttachmentChange(const CBaseObject* pObject, EAttachmentChangeType event)
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);

	// If an object gets attached/detached from its parent we need to update all related anim nodes, otherwise
	// they will end up very near the origin or very far away from the attached object when animated

	if (!pObject->IsKindOf(RUNTIME_CLASS(CEntityObject)) || pObject->CheckFlags(OBJFLAG_DELETED))
	{
		return;
	}

	const CEntityObject* pEntityObject = static_cast<const CEntityObject*>(pObject);
	CTrackViewAnimNodeBundle bundle = GetAllRelatedAnimNodes(pEntityObject);

	const uint numAffectedAnimNodes = bundle.GetCount();
	if (numAffectedAnimNodes == 0)
	{
		return;
	}

	std::unordered_set<CTrackViewSequence*> affectedSequences;
	for (uint i = 0; i < numAffectedAnimNodes; ++i)
	{
		CTrackViewAnimNode* pAnimNode = bundle.GetNode(i);
		affectedSequences.insert(pAnimNode->GetSequence());
	}

	CAnimationContext* pAnimationContext = CTrackViewPlugin::GetAnimationContext();
	CTrackViewSequence* pActiveSequence = pAnimationContext->GetSequence();
	const SAnimTime time = pAnimationContext->GetTime();

	for (auto iter = affectedSequences.begin(); iter != affectedSequences.end(); ++iter)
	{
		CTrackViewSequence* pSequence = *iter;
		pAnimationContext->SetSequence(pSequence, true, true);

		if (pSequence == pActiveSequence)
		{
			pAnimationContext->SetTime(time);
		}

		for (uint i = 0; i < numAffectedAnimNodes; ++i)
		{
			CTrackViewAnimNode* pNode = bundle.GetNode(i);
			if (pNode->GetSequence() == pSequence)
			{
				if (event == EAttachmentChangeType::PreAttachedKeepTransform || event == EAttachmentChangeType::PreDetachedKeepTransform)
				{
					const Matrix34 transform = pNode->GetNodeEntity()->GetWorldTM();
					m_prevTransforms.emplace(pNode, transform);
				}
				else if (event == EAttachmentChangeType::Attached || event == EAttachmentChangeType::Detached)
				{
					auto findIter = m_prevTransforms.find(pNode);
					if (findIter != m_prevTransforms.end())
					{
						pNode->GetNodeEntity()->SetWorldTM(findIter->second);
					}
				}
			}
		}
	}

	if (event == EAttachmentChangeType::Attached || event == EAttachmentChangeType::Detached)
	{
		m_prevTransforms.clear();
	}

	pAnimationContext->SetSequence(pActiveSequence, true, true);
	pAnimationContext->SetTime(time);
}

void CTrackViewSequenceManager::HandleObjectRename(const CBaseObject* pObject)
{
	if (!pObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
	{
		return;
	}

	const CEntityObject* pEntityObject = static_cast<const CEntityObject*>(pObject);
	CTrackViewAnimNodeBundle bundle = GetAllRelatedAnimNodes(pEntityObject);

	const uint numAffectedAnimNodes = bundle.GetCount();
	for (uint i = 0; i < numAffectedAnimNodes; ++i)
	{
		CTrackViewAnimNode* pAnimNode = bundle.GetNode(i);
		pAnimNode->SetName(pObject->GetName());
	}
}

void CTrackViewSequenceManager::HandleObjectDelete(const CBaseObject* pObject)
{
	if (!pObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
	{
		return;
	}

	const CEntityObject* pEntityObject = static_cast<const CEntityObject*>(pObject);
	CTrackViewAnimNodeBundle bundle = GetAllRelatedAnimNodes(pEntityObject);

	const uint numAffectedAnimNodes = bundle.GetCount();
	for (uint i = 0; i < numAffectedAnimNodes; ++i)
	{
		CTrackViewAnimNode* pAnimNode = bundle.GetNode(i);
		CTrackViewSequence* pSequence = pAnimNode->GetSequence();
		if (pSequence)
		{
			CTrackViewEntityNode* pEntityNode = static_cast<CTrackViewEntityNode*>(pAnimNode);
			CUndo::Record(new CUndoDetachAnimEntity(pEntityNode));
		}
	}
}
