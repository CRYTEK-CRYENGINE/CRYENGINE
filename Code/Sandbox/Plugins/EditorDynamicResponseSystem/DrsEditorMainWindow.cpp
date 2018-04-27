// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DrsEditorMainWindow.h"

#include <CryDynamicResponseSystem/IDynamicResponseSystem.h>
#include <IEditor.h>
#include <QDockWidget>
#include <QTabWidget>
#include <QPushButton>
#include <DockTitleBarWidget.h>
#include <QTimer>
#include <CrySerialization/IArchiveHost.h>
#include <Util/EditorUtils.h>
#include <EditorFramework/Events.h>

#include "Serialization/QPropertyTree/QPropertyTree.h"
#include "DrsResponseEditorWindow.h"
#include "DialogLinesEditorWidget.h"
#include "DrsSpokenLinesWindow.h"

//--------------------------------------------------------------------------------------------------
CDrsEditorMainWindow::CDrsEditorMainWindow()
{
	if (gEnv->pDynamicResponseSystem->GetResponseManager())
	{
		//Setup of the tab widgets
		m_pVariableInfosDockWidget = new DockedWidget<CDrsVariableInfos>(this, new CDrsVariableInfos, "Variables ", Qt::BottomDockWidgetArea);
		QObject::connect(m_pVariableInfosDockWidget->dock(), &QDockWidget::visibilityChanged, this, [=](bool bVisible)
		{
			m_pVariableInfosDockWidget->widget()->SetAutoUpdateActive(bVisible);
		});

		// Add Response Manager as a tabbed window
		m_pResponseEditorDockWidget = new DockedWidget<CDrsResponseEditorWindow>(this, new CDrsResponseEditorWindow, "Responses ", Qt::BottomDockWidgetArea);
		tabifyDockWidget(m_pVariableInfosDockWidget->dock(), m_pResponseEditorDockWidget->dock());

		QObject::connect(m_pResponseEditorDockWidget->dock(), &QDockWidget::visibilityChanged, this, [=](bool bVisible)
		{
			m_pResponseEditorDockWidget->widget()->OnVisibilityChanged(bVisible);
		});

		// Add dialog line editor as a tabbed window
		m_pSpokenLinesDockWidget = new DockedWidget<CSpokenLinesWidget>(this, new CSpokenLinesWidget, "Dialog Line History ", Qt::BottomDockWidgetArea);
		tabifyDockWidget(m_pVariableInfosDockWidget->dock(), m_pSpokenLinesDockWidget->dock());

		QObject::connect(m_pSpokenLinesDockWidget->dock(), &QDockWidget::visibilityChanged, this, [=](bool bVisible)
		{
			m_pSpokenLinesDockWidget->widget()->SetAutoUpdateActive(bVisible);
		});
	}

	// Add dialog line editor as a tabbed window
	if (gEnv->pDynamicResponseSystem->GetDialogLineDatabase())
	{
		m_pDialogLinesDockWidget = new DockedWidget<CDialogLinesEditorWidget>(this, new CDialogLinesEditorWidget(this), "Dialog Line Database", Qt::BottomDockWidgetArea);
		tabifyDockWidget(m_pVariableInfosDockWidget->dock(), m_pDialogLinesDockWidget->dock());
	}
}

void CDrsEditorMainWindow::customEvent(QEvent* event)
{
	// TODO: This handler should be removed whenever this editor is refactored to be a CDockableEditor
	if (event->type() == SandboxEvent::Command)
	{
		CommandEvent* commandEvent = static_cast<CommandEvent*>(event);

		const string& command = commandEvent->GetCommand();
		if (command == "general.help")
		{
			event->setAccepted(EditorUtils::OpenHelpPage(GetPaneTitle()));
		}
	}

	if (!event->isAccepted())
	{
		QWidget::customEvent(event);
	}
}

//--------------------------------------------------------------------------------------------------
CDrsVariableChangesSerializer::CDrsVariableChangesSerializer()
{
	m_SerializationFilter = 0;
	m_pPropertyTree->setExpandLevels(16);
	m_pPropertyTree->attach(Serialization::SStruct(*this));

	m_pClearButton = new QPushButton("Clear", this);
	QObject::connect(m_pClearButton, &QPushButton::clicked, this, [ = ]
	{
		m_SerializationFilter = 1;
		TriggerUpdate();
		m_SerializationFilter = 0;
	});

	m_pHLayout = new QHBoxLayout(this);
	m_pHLayout->addWidget(m_pClearButton);
	m_pHLayout->addWidget(m_pUpdateButton);
	m_pVLayout->addItem(m_pHLayout);
}

//--------------------------------------------------------------------------------------------------
CDrsVariableSerializer::CDrsVariableSerializer()
{
	m_pPropertyTree->setExpandLevels(16);
	m_pPropertyTree->attach(Serialization::SStruct(*this));
}

//--------------------------------------------------------------------------------------------------
void CDrsVariableSerializer::Serialize(Serialization::IArchive& ar)
{
	if (!m_pUpdateButton->isChecked())
		ar.openBlock("VariableChange", "+Variables");
	else
		ar.openBlock("VariableChange", "+!Variables (read only because of auto-update)");

	gEnv->pDynamicResponseSystem->Serialize(ar);

	ar.closeBlock();
}

//--------------------------------------------------------------------------------------------------
void CDrsVariableChangesSerializer::Serialize(Serialization::IArchive& ar)
{
	ar.setFilter(m_SerializationFilter);
	gEnv->pDynamicResponseSystem->GetResponseManager()->SerializeVariableChanges(ar);
}

//--------------------------------------------------------------------------------------------------
CDrsSerializerBase::CDrsSerializerBase()
{
	m_pPropertyTree = new QPropertyTree();
	PropertyTreeStyle treeStyle(QPropertyTree::defaultTreeStyle());
	treeStyle.propertySplitter = false;
	m_pPropertyTree->setTreeStyle(treeStyle);
	m_pPropertyTree->setUndoEnabled(true);

	m_pVLayout = new QVBoxLayout(this);
	m_pVLayout->addWidget(m_pPropertyTree);

	m_pAutoUpdateTimer = new QTimer(this);
	m_pAutoUpdateTimer->setInterval(333);
	QObject::connect(m_pAutoUpdateTimer, &QTimer::timeout, this, [ = ] { TriggerUpdate();
	                                                                     m_pAutoUpdateTimer->start();
	                 });

	m_pUpdateButton = new QPushButton("Auto Update", this);
	m_pUpdateButton->setCheckable(true);
	QObject::connect(m_pUpdateButton, &QPushButton::toggled, this, [=](bool checked)
	{
		if (checked)
			m_pAutoUpdateTimer->start();
		else
			m_pAutoUpdateTimer->stop();
		TriggerUpdate();
	});

	m_pVLayout->addWidget(m_pUpdateButton);
}

//--------------------------------------------------------------------------------------------------
void CDrsSerializerBase::SetAutoUpdateActive(bool bValue)
{
	m_pUpdateButton->setChecked(bValue);
}

//--------------------------------------------------------------------------------------------------
void CDrsSerializerBase::TriggerUpdate()
{
	m_pPropertyTree->revert();
}

//--------------------------------------------------------------------------------------------------
CDrsVariableInfos::CDrsVariableInfos()
{
	m_pVariablesSerializerDockWidget = new DockedWidget<CDrsVariableSerializer>(this, new CDrsVariableSerializer, "Variable Collections", Qt::TopDockWidgetArea);
	m_pVariableChangesSerializerDockWidget = new DockedWidget<CDrsVariableChangesSerializer>(this, new CDrsVariableChangesSerializer, "Variable Changes", Qt::TopDockWidgetArea);
}

//--------------------------------------------------------------------------------------------------
void CDrsVariableInfos::SetAutoUpdateActive(bool bValue)
{
	m_pVariableChangesSerializerDockWidget->widget()->SetAutoUpdateActive(bValue);
	m_pVariablesSerializerDockWidget->widget()->SetAutoUpdateActive(bValue);
}

