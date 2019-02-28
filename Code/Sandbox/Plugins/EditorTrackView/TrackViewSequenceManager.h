// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// CryEngine HeaderFile.
// Copyright (C), Crytek, 1999-2014.

#pragma once

#include "Nodes/TrackViewSequence.h"
#include "IDataBaseManager.h"

struct ITrackViewSequenceManagerListener
{
	virtual void OnSequenceAdded(CTrackViewSequence* pSequence)   {}
	virtual void OnSequenceRemoved(CTrackViewSequence* pSequence) {}
};

class CTrackViewSequenceManager : public IEditorNotifyListener, public IDataBaseManagerListener
{
	friend class CAbstractUndoSequenceTransaction;
	friend class CSequenceObject;

	enum class EAttachmentChangeType
	{
		PreAttached,
		PreAttachedKeepTransform,
		Attached,
		PreDetached,
		PreDetachedKeepTransform,
		Detached
	};

public:
	CTrackViewSequenceManager();
	~CTrackViewSequenceManager();

	virtual void             OnEditorNotifyEvent(EEditorNotifyEvent event);

	unsigned int             GetCount() const { return m_sequences.size(); }

	CTrackViewSequence*      CreateSequence(const string& name);
	void                     DeleteSequence(CTrackViewSequence* pSequence);

	CTrackViewSequence*      GetSequenceByName(const string& name) const;
	CTrackViewSequence*      GetSequenceByIndex(unsigned int index) const;
	CTrackViewSequence*      GetSequenceByAnimSequence(IAnimSequence* pAnimSequence) const;
	CTrackViewSequence*      GetSequenceByGUID(CryGUID guid) const;

	CTrackViewAnimNodeBundle GetAllRelatedAnimNodes(const CEntityObject* pEntityObject) const;
	CTrackViewAnimNode*      GetActiveAnimNode(const CEntityObject* pEntityObject) const;

	void                     AddListener(ITrackViewSequenceManagerListener* pListener);
	void                     RemoveListener(ITrackViewSequenceManagerListener* pListener) { stl::find_and_erase(m_listeners, pListener); }

private:
	void         PostLoad();
	void         SortSequences();

	void         OnSequenceAdded(CTrackViewSequence* pSequence);
	void         OnSequenceRemoved(CTrackViewSequence* pSequence);

	virtual void OnDataBaseItemEvent(IDataBaseItem* pItem, EDataBaseItemEvent event);

	// Callback from SequenceObject
	IAnimSequence* OnCreateSequenceObject(const string& name);
	void           OnDeleteSequenceObject(const string& name);

	void           OnObjectEvent(CObjectEvent& event);
	void           OnBeforeObjectsAttached(CBaseObject* pParent, const std::vector<CBaseObject*>& objects, bool keepTransform);
	void           OnObjectsAttached(CBaseObject* pParent, const std::vector<CBaseObject*>& objects);
	void           OnBeforeObjectsDetached(CBaseObject* pParent, const std::vector<CBaseObject*>& objects, bool keepTransform);
	void           OnObjectsDetached(CBaseObject* pParent, const std::vector<CBaseObject*>& objects);
	void           HandleObjectRename(CBaseObject* pObject);
	void           HandleObjectDelete(CBaseObject* pObject);
	void           HandleAttachmentChange(CBaseObject* pObject, EAttachmentChangeType event);

	std::vector<ITrackViewSequenceManagerListener*>  m_listeners;
	std::vector<std::unique_ptr<CTrackViewSequence>> m_sequences;

	// Set to hold sequences that existed when undo transaction began
	std::set<CTrackViewSequence*> m_transactionSequences;

	uint32                        m_nextSequenceId;
	bool                          m_bUnloadingLevel;

	// Used to handle object attach/detach
	std::unordered_map<CTrackViewNode*, Matrix34> m_prevTransforms;
};

