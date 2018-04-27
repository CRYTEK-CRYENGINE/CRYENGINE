// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CryExtension/CryGUID.h"
#include "Controls/EditorDialog.h"

class QPushButton;
class QAdvancedTreeView;
class QKeyEvent;
class QItemSelection;

class CTrackViewSequence;
class CEventNameLineEdit;
class CTrackViewSequenceEventsModel;

class CTrackViewNewEventDialog : public CEditorDialog
{
public:
	CTrackViewNewEventDialog(CTrackViewSequence& sequence);

	string GetEventName() const;

protected:
	void OnTextChanged(const QString& text);

	CTrackViewSequence& m_sequence;
	CEventNameLineEdit* m_pEventName;
	QLabel*             m_pWarning;
	QPushButton*        m_pOkButton;
};

class CTrackViewEventsDialog : public CEditorDialog
{
public:
	CTrackViewEventsDialog(CTrackViewSequence& sequence);
	~CTrackViewEventsDialog();

private:
	void Initialize();

	void OnAddEventButtonPressed();
	void OnRemoveEventButtonPressed();

	void OnSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected);

	void keyPressEvent(QKeyEvent* pEvent);
	void customEvent(QEvent* pEvent);

private:
	CTrackViewSequence&            m_sequence;
	CTrackViewSequenceEventsModel* m_pModel;

	QPushButton*                   m_pAddButton;
	QPushButton*                   m_pRemoveButton;

	QAdvancedTreeView*             m_pEventsView;
};

