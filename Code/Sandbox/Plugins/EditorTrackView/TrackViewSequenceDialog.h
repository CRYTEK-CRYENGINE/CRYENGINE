// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CryExtension/CryGUID.h"
#include "Controls/EditorDialog.h"

class QLineEdit;
class QListWidget;
class QPushButton;
class QListWidgetItem;

class CTrackViewSequenceDialog
	: public CEditorDialog
{
public:
	typedef std::vector<CryGUID> SequenceGuids;

	enum EMode
	{
		OpenSequences,
		DeleteSequences
	};

	CTrackViewSequenceDialog(EMode mode);
	~CTrackViewSequenceDialog();

	const SequenceGuids& GetSequenceGuids() const { return m_sequenceGuids; }

private:
	void Initialize();
	void UpdateSequenceList();

	void OnAccepted();
	void OnSequenceListItemPressed(QListWidgetItem* pItem);
	void OnSequencesFilterChanged();

	void keyPressEvent(QKeyEvent* pEvent);

	SequenceGuids m_sequenceGuids;
	QLineEdit*    m_pFilter;
	QListWidget*  m_pSequences;
	QPushButton*  m_pActionButton;
	EMode         m_mode;
};

