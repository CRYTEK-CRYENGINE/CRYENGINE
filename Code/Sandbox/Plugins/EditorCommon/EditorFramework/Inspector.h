// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditorCommonAPI.h"
#include "QtViewPane.h"
#include <QSize>
#include <QPointer>
#include <vector>
#include <CryCore/CryVariant.h>

class QEvent;
class QLabel;
class QToolButton;
class QWidget;
class PopulateInspectorEvent;
class CBroadcastManager;
class CEditor;
class QScrollableBox;

//////////////////////////////////////////////////////////////////////////
//! Inspector class
//!
//! Inspector is provided as a means for editors to display context-sensitive information using client provided widgets
//!
//! To populate an inspector, you should dock it in your editor and broadcast a PopulateInspectorEvent
//! from the broadcast manager. The event includes a label for the inspector and a widget factory lambda that will
//! be called on population, and will create the client property widget type. Returning null from the factory lambda
//! will clear the inspector unless it is locked (see below)
//!
//! Example:
//!
//! CBroadcastManager* const pBroadcastManager = CBroadcastManager::Get(CMyEditor);
//! if (pBroadcastManager)
//! {
//!   CMyObservedObject* pObj;
//!   PopulateInspectorEvent popEvent([pObj](const PopulateInspectorEvent&)
//!   {
//!     return new CMyObservedObjectPropertyWidget(pObj);
//!   }, "Observed Object Properties");
//!
//!   pBroadcastManager->Broadcast(popEvent);
//! }
//!
//! The custom property widget would need to properly register with the observed object(s)
//! for notification and refresh itself when the object properties change, using signals or any other
//! mechanism. To avoid over-refresh or overpopulation, a lazy refresh/dirty flag scheme can be implemented,
//! by listening to Idle events from the editor
//!
//! The inspector provides a locking feature to the user, which allows her to keep the same
//! object properties displayed, ignoring further PopulateInspectorEvent broadcasts.
//!
//! This locking behavior can be disabled:
//! 1) Explicitly by the user, by pressing the lock icon next to the inspector title
//! 2) By external code that has access to the inspector object and calls the Lock/Unlock methods
//!    (Not recommended, since it overrides the user's preference)
//! 3) By destroying the properties widget explicitly. This would usually happen if the observed object(s)
//!    are somehow invalidated or destroyed.
class EDITOR_COMMON_API CInspector : public CDockableWidget
{
	Q_OBJECT
public:
	//!Parent must be specified in the constructor as we need to connect to a specific broadcast manager
	CInspector(QWidget* pParent);

	//!Inspector will use the context of the CEditor
	CInspector(CEditor* pParent);

	//!Directly assigning broadcast manager to the CInspector
	CInspector(CBroadcastManager* pBroadcastManager);

	virtual ~CInspector();

	//! Sets the inspector to be lockable, as it is not useful for unique panes.
	void SetLockable(bool bLockable);

	//!Locking API. If property widgets want to unlock the inspector they should delete themselves
	bool IsLocked() const { return m_bLocked; }
	void Lock();
	void Unlock();
	void ToggleLock() { IsLocked() ? Unlock() : Lock(); }

	//////////////////////////////////////////////////////////
	// CDockableWidget implementation
	virtual const char* GetPaneTitle() const override { return "Properties"; }
	virtual QRect       GetPaneRect() override        { return QRect(0, 0, 200, 200); }
	//////////////////////////////////////////////////////////

	virtual QSize sizeHint() const override { return QSize(240, 600); }
	void          closeEvent(QCloseEvent* event);

	void          AddWidget(QWidget* pWidget);

public slots:
	void OnWidgetDeleted(QObject* pObj);

private:
	void OnPopulate(PopulateInspectorEvent& event);

	// Remove previously created widgets
	void Clear();
	void ClearAndFillSpace();

	void Init();
	void Connect(CBroadcastManager* pBroadcastManager);
	void Disconnect();

	QPointer<CBroadcastManager> m_pBroadcastManager;
	QToolButton*                m_pLockButton;
	QLabel*                     m_pTitleLabel;
	bool                        m_bLocked;

	QScrollableBox*             m_pScrollableBox = nullptr;
};


//! This header widget only exists for styling purposes
class CInspectorHeaderWidget : public QWidget
{
	Q_OBJECT
public:
	using QWidget::QWidget;
};

