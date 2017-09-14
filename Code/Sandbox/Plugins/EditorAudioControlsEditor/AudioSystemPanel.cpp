// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "QtUtil.h"
#include "AudioSystemPanel.h"
#include "IAudioSystemEditor.h"
#include "AudioControlsEditorPlugin.h"
#include "AudioSystemModel.h"
#include "QAudioSystemSettingsDialog.h"
#include "ImplementationManager.h"
#include "AdvancedTreeView.h"

#include <QSearchBox.h>

// Qt
#include <QtUtil.h>
#include <QCheckBox>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolButton>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QFontMetrics>

class CElidedLabel final : public QLabel
{
public:

	CElidedLabel::CElidedLabel(QString const& text)
	{
		setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		SetLabelText(text);
	}

	void SetLabelText(QString const& text)
	{
		m_originalText = text;
		ElideText();
	}

private:

	// QWidget
	virtual void resizeEvent(QResizeEvent *) override
	{
		ElideText();
	}
	// ~QWidget

	void ElideText()
	{
		QFontMetrics const metrics(font());
		QString const elidedText = metrics.elidedText(m_originalText, Qt::ElideRight, size().width());
		setText(elidedText);
	}

	QString m_originalText;
};

namespace ACE
{
//////////////////////////////////////////////////////////////////////////
CAudioSystemPanel::CAudioSystemPanel()
	: m_pModelProxy(new QAudioSystemModelProxyFilter(this))
	, m_pModel(new QAudioSystemModel())
	, m_pImplNameLabel(nullptr)
{
	resize(299, 674);

	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

	QHBoxLayout* pHorizontalLayout = new QHBoxLayout();

	QSearchBox* pSearchBox = new QSearchBox(this);
	pSearchBox->setToolTip(tr("Show only middleware controls with this name"));
	pHorizontalLayout->addWidget(pSearchBox);

	QCheckBox* pHideAssignedCheckbox = new QCheckBox(this);
	pHideAssignedCheckbox->setEnabled(true);
	pHorizontalLayout->addWidget(pHideAssignedCheckbox);

	QToolButton* pSettingsButton = new QToolButton();
	pSettingsButton->setIcon(QIcon("://Icons/Options.ico"));
	pSettingsButton->setToolTip(tr("Location settings of audio middleware project"));
	connect(pSettingsButton, &QToolButton::clicked, [&]()
		{
			QAudioSystemSettingsDialog* dialog = new QAudioSystemSettingsDialog(this);
			connect(dialog, &QAudioSystemSettingsDialog::ImplementationSettingsAboutToChange, this, &CAudioSystemPanel::ImplementationSettingsAboutToChange);
			connect(dialog, &QAudioSystemSettingsDialog::ImplementationSettingsChanged, this, &CAudioSystemPanel::ImplementationSettingsChanged);
			dialog->exec();
		});

	pHorizontalLayout->addWidget(pSettingsButton);

	QVBoxLayout* pVerticalLayout = new QVBoxLayout(this);
	pVerticalLayout->setContentsMargins(0, 0, 0, 0);

	m_pImplNameLabel = new CElidedLabel("");
	IAudioSystemEditor* pAudioImpl = CAudioControlsEditorPlugin::GetAudioSystemEditorImpl();

	if (pAudioImpl)
	{
		m_pImplNameLabel->SetLabelText(QtUtil::ToQString(pAudioImpl->GetName()));
	}

	m_pImplNameLabel->setObjectName("ImplementationTitle");
	pVerticalLayout->addWidget(m_pImplNameLabel);

	pVerticalLayout->addLayout(pHorizontalLayout);

	pHideAssignedCheckbox->setText(tr("Hide Assigned"));
	pHideAssignedCheckbox->setToolTip(tr("Hide or show assigned middleware controls"));

	m_pModelProxy->setDynamicSortFilter(true);

	connect(pSearchBox, &QSearchBox::textChanged, [&](QString const& filter)
		{
			if (m_filter != filter)
			{
				if (m_filter.isEmpty() && !filter.isEmpty())
				{
					BackupTreeViewStates();
					m_pTreeView->expandAll();
				}
				else if (!m_filter.isEmpty() && filter.isEmpty())
				{
					m_pModelProxy->setFilterFixedString(filter);
					m_pTreeView->collapseAll();
					RestoreTreeViewStates();
				}
				else if (!m_filter.isEmpty() && !filter.isEmpty())
				{
					m_pModelProxy->setFilterFixedString(filter);
					m_pTreeView->expandAll();
				}

				m_filter = filter;
			}

			m_pModelProxy->setFilterFixedString(filter);
		});

	connect(pHideAssignedCheckbox, &QCheckBox::clicked, [&](bool bHide)
		{
			if (bHide)
			{
				m_pTreeView->BackupExpanded();
				m_pModelProxy->SetHideConnected(bHide);
			}
			else
			{
				m_pModelProxy->SetHideConnected(bHide);
				m_pTreeView->RestoreExpanded();
			}
		});

	m_pTreeView = new CAdvancedTreeView();
	m_pTreeView->header()->setVisible(false);
	m_pTreeView->setDragEnabled(true);
	m_pTreeView->setDragDropMode(QAbstractItemView::DragOnly);
	m_pTreeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pTreeView->setSortingEnabled(true);
	m_pTreeView->sortByColumn(0, Qt::AscendingOrder);

	m_pModelProxy->setSourceModel(m_pModel);
	m_pTreeView->setModel(m_pModelProxy);
	pVerticalLayout->addWidget(m_pTreeView);

	m_pTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pTreeView, &CAdvancedTreeView::customContextMenuRequested, this, &CAudioSystemPanel::ShowControlsContextMenu);
	connect(m_pTreeView->selectionModel(), &QItemSelectionModel::selectionChanged, m_pTreeView, &CAdvancedTreeView::OnSelectionChanged);

	// Update the middleware name label.
	// Note the 'this' ptr being passed as a context variable so that Qt can disconnect this lambda when the object is destroyed (ie. the ACE is closed).
	CAudioControlsEditorPlugin::GetImplementationManger()->signalImplementationChanged.Connect([&]()
		{
			IAudioSystemEditor* pAudioImpl = CAudioControlsEditorPlugin::GetAudioSystemEditorImpl();

			if (pAudioImpl)
			{
			  m_pImplNameLabel->SetLabelText(QtUtil::ToQString(pAudioImpl->GetName()));
			}
		}, reinterpret_cast<uintptr_t>(this));
}

//////////////////////////////////////////////////////////////////////////
CAudioSystemPanel::~CAudioSystemPanel()
{
	CAudioControlsEditorPlugin::GetImplementationManger()->signalImplementationChanged.DisconnectById(reinterpret_cast<uintptr_t>(this));
}

//////////////////////////////////////////////////////////////////////////
void CAudioSystemPanel::SetAllowedControls(EItemType type, bool bAllowed)
{
	const ACE::IAudioSystemEditor* pAudioSystemEditorImpl = CAudioControlsEditorPlugin::GetAudioSystemEditorImpl();

	if (pAudioSystemEditorImpl)
	{
		m_allowedATLTypes[type] = bAllowed;
		uint mask = 0;

		for (int i = 0; i < EItemType::eItemType_NumTypes; ++i)
		{
			if (m_allowedATLTypes[i])
			{
				mask |= pAudioSystemEditorImpl->GetCompatibleTypes((EItemType)i);
			}
		}

		m_pModelProxy->SetAllowedControlsMask(mask);
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioSystemPanel::ShowControlsContextMenu(QPoint const& pos)
{
	QMenu contextMenu(tr("Context menu"), this);
	QMenu addMenu(tr("Add"));
	auto selection = m_pTreeView->selectionModel()->selectedRows();

	if (!selection.isEmpty())
	{
		contextMenu.addAction(tr("Expand Selection"), [&]() { m_pTreeView->ExpandSelection(m_pTreeView->GetSelectedIndexes()); });
		contextMenu.addAction(tr("Collapse Selection"), [&]() { m_pTreeView->CollapseSelection(m_pTreeView->GetSelectedIndexes()); });
		contextMenu.addSeparator();
	}

	contextMenu.addAction(tr("Expand All"), [&]() { m_pTreeView->expandAll(); });
	contextMenu.addAction(tr("Collapse All"), [&]() { m_pTreeView->collapseAll(); });

	contextMenu.exec(m_pTreeView->mapToGlobal(pos));
}

//////////////////////////////////////////////////////////////////////////
void CAudioSystemPanel::Reset()
{
	m_pModel->Reset();
	m_pModelProxy->invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CAudioSystemPanel::BackupTreeViewStates()
{
	m_pTreeView->BackupExpanded();
	m_pTreeView->BackupSelection();
}

//////////////////////////////////////////////////////////////////////////
void CAudioSystemPanel::RestoreTreeViewStates()
{
	m_pTreeView->RestoreExpanded();
	m_pTreeView->RestoreSelection();
}
} // namespace ACE
