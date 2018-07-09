// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include "TrackViewSequenceDialog.h"
#include "TrackViewSequenceManager.h"
#include "TrackViewPlugin.h"

#include "QtUtil.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QListWidgetItem>
#include <QLineEdit>
#include <QKeyEvent>

CTrackViewSequenceDialog::CTrackViewSequenceDialog(EMode mode)
	: CEditorDialog("SequencesDialog")
	, m_mode(mode)
{
	Initialize();
}

CTrackViewSequenceDialog::~CTrackViewSequenceDialog()
{

}

void CTrackViewSequenceDialog::Initialize()
{
	QVBoxLayout* pDialogLayout = new QVBoxLayout(this);

	m_pFilter = new QLineEdit();
	m_pFilter->setMaximumSize(16777215, 20);
	m_pFilter->setPlaceholderText("Filter");

	QVBoxLayout* pVBoxLayout = new QVBoxLayout();
	pVBoxLayout->setSpacing(1);
	pVBoxLayout->setContentsMargins(3, 3, 3, 3);

	QHBoxLayout* pHBoxLayout = new QHBoxLayout();
	pHBoxLayout->setSpacing(0);
	pHBoxLayout->setMargin(0);
	pHBoxLayout->addWidget(m_pFilter);
	pVBoxLayout->addLayout(pHBoxLayout);

	m_pSequences = new QListWidget();
	m_pSequences->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pSequences->setSelectionMode(QAbstractItemView::SelectionMode::MultiSelection);
	m_pSequences->setObjectName("SequencesField");
	m_pSequences->setAccessibleName("SequencesField");
	connect(m_pSequences, &QListWidget::doubleClicked, [&]() { OnAccepted(); });
	pVBoxLayout->addWidget(m_pSequences);

	pDialogLayout->addLayout(pHBoxLayout);
	pDialogLayout->addLayout(pVBoxLayout);

	QDialogButtonBox* pButtonBox = new QDialogButtonBox(this);
	pButtonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	pDialogLayout->addWidget(pButtonBox);

	m_pActionButton = pButtonBox->button(QDialogButtonBox::Ok);
	m_pActionButton->setEnabled(false);

	switch (m_mode)
	{
	case OpenSequences:
		m_pActionButton->setText("Open");
		this->setWindowTitle("Open Sequences");
		break;
	case DeleteSequences:
		m_pActionButton->setText("Delete");
		this->setWindowTitle("Delete Sequences");
		break;
	}

	setMinimumSize(200, 500);
	setLayout(pDialogLayout);

	QObject::connect(m_pSequences, &QListWidget::itemPressed, this, &CTrackViewSequenceDialog::OnSequenceListItemPressed);
	QObject::connect(m_pFilter, &QLineEdit::textChanged, this, &CTrackViewSequenceDialog::OnSequencesFilterChanged);
	QObject::connect(pButtonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
	QObject::connect(pButtonBox, &QDialogButtonBox::accepted, this, &CTrackViewSequenceDialog::OnAccepted);

	UpdateSequenceList();
}

void CTrackViewSequenceDialog::UpdateSequenceList()
{
	m_pSequences->clear();

	// TODO: Create a model for this.
	const string filterString = QtUtil::ToString(m_pFilter->text());

	CTrackViewSequenceManager* pSequenceManager = CTrackViewPlugin::GetSequenceManager();
	const int32 numSequences = pSequenceManager->GetCount();
	for (int i = 0; i < numSequences; ++i)
	{
		CTrackViewSequence* pSequence = pSequenceManager->GetSequenceByIndex(i);
		if (!filterString.empty() && CryStringUtils::stristr(pSequence->GetName(), filterString) == nullptr)
		{
			continue;
		}

		const CryGUID guid = pSequence->GetGUID();
		const QByteArray guidByteArray(reinterpret_cast<const char*>(&guid), sizeof(CryGUID));

		QListWidgetItem* pNewItem = new QListWidgetItem(pSequence->GetName());
		pNewItem->setData(Qt::UserRole, guidByteArray);
		m_pSequences->addItem(pNewItem);
	}
}

void CTrackViewSequenceDialog::OnAccepted()
{
	switch (m_mode)
	{
	case OpenSequences:
		for (const QListWidgetItem* pItem : m_pSequences->selectedItems())
		{
			const CryGUID guid = *reinterpret_cast<const CryGUID*>(pItem->data(Qt::UserRole).toByteArray().data());
			m_sequenceGuids.emplace_back(guid);
		}
		break;
	case DeleteSequences:
		for (const QListWidgetItem* pItem : m_pSequences->selectedItems())
		{
			const CryGUID guid = *reinterpret_cast<const CryGUID*>(pItem->data(Qt::UserRole).toByteArray().data());
			if (CTrackViewSequence* pSequence = CTrackViewPlugin::GetSequenceManager()->GetSequenceByGUID(guid))
			{
				CTrackViewPlugin::GetSequenceManager()->DeleteSequence(pSequence);
				m_sequenceGuids.emplace_back(guid);
			}
		}
		break;
	}

	QDialog::accept();
}

void CTrackViewSequenceDialog::OnSequenceListItemPressed(QListWidgetItem* pItem)
{
	m_pActionButton->setEnabled(m_pSequences->selectedItems().length() > 0);
}

void CTrackViewSequenceDialog::OnSequencesFilterChanged()
{
	UpdateSequenceList();
}

void CTrackViewSequenceDialog::keyPressEvent(QKeyEvent* pEvent)
{
	switch (pEvent->key())
	{
	case Qt::Key_Return:
	case Qt::Key_Enter:
		break;

	default:
		QDialog::keyPressEvent(pEvent);
	}
}

