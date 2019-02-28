// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ICryGraphEditor.h"

#include <CrySandbox/CrySignal.h>
#include <CrySerialization/Forward.h>

class QPropertyTree;

namespace CryGraphEditor {

class CNodeGraphViewModel;

class EDITOR_COMMON_API CAbstractNodeGraphViewModelItem
{
public:
	CAbstractNodeGraphViewModelItem(CNodeGraphViewModel& viewModel);
	virtual ~CAbstractNodeGraphViewModelItem();

	CNodeGraphViewModel& GetViewModel() const                                           { return m_viewModel; }
	bool                 IsSame(const CAbstractNodeGraphViewModelItem& otherItem) const { return (this == &otherItem); }

	virtual QWidget*     CreatePropertiesWidget()                                       { return nullptr; }
	virtual void         Serialize(Serialization::IArchive& archive)                    {}

	virtual int32        GetType() const                                                { return eItemType_Unset; }
	virtual QString      GetToolTipText() const                                         { return QString(); }

	virtual QPointF      GetPosition() const                                            { return QPointF(); }
	virtual void         SetPosition(QPointF position)                                  {}

	virtual bool         IsDeactivated() const                                          { return false; }
	virtual void         SetDeactivated(bool isDeactivated)                             {}

	virtual bool         HasWarnings() const                                            { return false; }
	virtual void         SetWarnings(bool hasWarnings)                                  {}

	virtual bool         HasErrors() const                                              { return false; }
	virtual void         SetErrors(bool hasErrors)                                      {}

	void                 MoveBy(float dx, float dy);

	bool                 GetAcceptsMoves() const                { return m_acceptsMoves; }
	void                 SetAccpetsMoves(bool accept)           { m_acceptsMoves = accept; }
	bool                 GetAcceptsSelection() const            { return m_acceptsSelection; }
	void                 SetAcceptsSelection(bool accept)       { m_acceptsSelection = accept; }
	bool                 GetAcceptsHighlightning() const        { return m_acceptsHighlightning; }
	void                 SetAcceptsHighlightning(bool accept)   { m_acceptsHighlightning = accept; }
	bool                 GetAcceptsDeactivation() const         { return m_acceptsDeactivation; }
	void                 SetAcceptsDeactivation(bool accept)    { m_acceptsDeactivation = accept; }
	bool                 GetAcceptsDeletion() const             { return m_acceptsDeletion; }
	void                 SetAcceptsDeletion(bool accept)        { m_acceptsDeletion = accept; }
	bool                 GetAcceptsCopy() const                 { return m_acceptsCopy; }
	void                 SetAcceptsCopy(bool accept)            { m_acceptsCopy = accept; }
	bool                 GetAcceptsPaste() const                { return m_acceptsPaste; }
	void                 SetAcceptsPaste(bool accept)           { m_acceptsPaste = accept; }

	uint16               GetPropertiesPriority() const          { return m_propertiesPriority; }
	void                 SetPropertiesPriority(uint16 priority) { m_propertiesPriority = priority; }

	template<typename T>
	static T* Cast(CAbstractNodeGraphViewModelItem* pViewItem);

	template<typename T>
	T* Cast();

public:
	CCrySignal<void()> SignalInvalidated;
	CCrySignal<void(CAbstractNodeGraphViewModelItem*)> SignalDeletion;
	CCrySignal<void(CAbstractNodeGraphViewModelItem&)> SignalValidated;

private:
	CNodeGraphViewModel& m_viewModel;

	uint16               m_propertiesPriority;

	bool                 m_acceptsMoves              : 1;
	bool                 m_acceptsSelection          : 1;
	bool                 m_acceptsHighlightning      : 1;
	bool                 m_acceptsDeactivation       : 1;
	bool                 m_acceptsDeletion           : 1;
	bool                 m_acceptsCopy               : 1;
	bool                 m_acceptsPaste              : 1;
	bool                 m_allowsMultiItemProperties : 1;
};

inline void CAbstractNodeGraphViewModelItem::MoveBy(float dx, float dy)
{
	if (m_acceptsMoves && (dx != .0f || dy != .0f))
	{
		const QPointF position = GetPosition();
		SetPosition(QPointF(position.x() + dx, position.y() + dy));
	}
}

template<typename T>
inline static T* CAbstractNodeGraphViewModelItem::Cast(CAbstractNodeGraphViewModelItem* pViewItem)
{
	if (pViewItem && T::Type == pViewItem->GetType())
	{
		return static_cast<T*>(pViewItem);
	}
	return nullptr;
}

template<typename T>
inline T* CAbstractNodeGraphViewModelItem::Cast()
{
	return Cast<T>(this);
}

}

