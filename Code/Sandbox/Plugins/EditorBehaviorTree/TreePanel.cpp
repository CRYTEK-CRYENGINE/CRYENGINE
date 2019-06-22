// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TreePanel.h"
#include "MainWindow.h"

#include <Serialization/QPropertyTreeLegacy/QPropertyTreeLegacy.h>
#include <Serialization/PropertyTreeLegacy/PropertyTreeStyle.h>
#include <QDir>
#include <QFileInfo>
#include <QFileDialog>
#include <QMessageBox>

TreePanel::TreePanel(CMainWindow* pMainWindow)
	: m_pMainWindow(pMainWindow)
	, m_propertiesAttachedToDocument(false)
	, m_contextList()
	, m_behaviorTreeNodeSerializationHints()
	, m_lastOpenedFilePath()
{
	m_propertyTree = new QPropertyTreeLegacy(this);
	PropertyTreeStyle treeStyle(QPropertyTreeLegacy::defaultTreeStyle());
	treeStyle.propertySplitter = false;
	treeStyle.levelIndent = 1.0f;
	treeStyle.firstLevelIndent = 1.0f;

	m_propertyTree->setTreeStyle(treeStyle);
	m_propertyTree->setUndoEnabled(true);

	setWidget(m_propertyTree);
	setFeatures((QDockWidget::DockWidgetFeatures)(QDockWidget::AllDockWidgetFeatures & ~(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetMovable)));
	connect(m_propertyTree, SIGNAL(signalChanged()), this, SLOT(OnPropertyTreeDataChanged()));
}

void TreePanel::Reset()
{
	DetachDocumentFromPropertyTree();
}

void TreePanel::OnWindowEvent_NewFile()
{
	if (!CheckForUnsavedDataAndSave())
		return;

	QString gameFolder = QString::fromLocal8Bit(GetIEditor()->GetSystem()->GetIPak()->GetGameFolder());
	QDir behaviorFolder(QDir::fromNativeSeparators(gameFolder));

	QString absoluteFilePath = QFileDialog::getSaveFileName(this, "Behavior Tree", behaviorFolder.absolutePath(), "XML files (*.xml)");

	if (absoluteFilePath.isEmpty())
		return;

	Reset();

	QFileInfo fileInfo(absoluteFilePath);

	m_behaviorTreeDocument.NewFile(fileInfo.baseName().toStdString().c_str(), absoluteFilePath.toStdString().c_str());

	AttachDocumentToPropertyTree();
}

void TreePanel::OnWindowEvent_OpenFile()
{
	if (!CheckForUnsavedDataAndSave())
		return;

	QString gameFolder = QString::fromLocal8Bit(GetIEditor()->GetSystem()->GetIPak()->GetGameFolder());
	QDir behaviorFolder(QDir::fromNativeSeparators(gameFolder));

	QString absoluteFilePath = QFileDialog::getOpenFileName(this, "Behavior Tree", behaviorFolder.absolutePath(), "XML files (*.xml)");

	TryOpenFileDirectly(absoluteFilePath);
}

void TreePanel::OnWindowEvent_OpenFile(const QString& absoluteFilePath)
{
	if (!CheckForUnsavedDataAndSave())
		return;

	TryOpenFileDirectly(absoluteFilePath);
}

void TreePanel::OnWindowEvent_Save()
{
	if (!CheckForBehaviorTreeErrors())
		return;

	if (m_behaviorTreeDocument.Loaded() && m_behaviorTreeDocument.Changed())
		m_behaviorTreeDocument.Save();

	if (m_behaviorTreeNodeSerializationHints.showXmlLineNumbers)
	{
		ReserializeDocumentIntoPropertyTree();
	}
}

void TreePanel::OnWindowEvent_SaveToFile()
{
	if (!CheckForBehaviorTreeErrors())
		return;

	if (!m_behaviorTreeDocument.Loaded() && !m_behaviorTreeDocument.Changed())
		return;

	QString gameFolder = QString::fromStdString(GetIEditor()->GetSystem()->GetIPak()->GetGameFolder());
	QDir behaviorFolder(QDir::fromNativeSeparators(gameFolder));

	QString absoluteFilePath = QFileDialog::getSaveFileName(this, "Behavior Tree", behaviorFolder.absolutePath(), "XML files (*.xml)");

	if (absoluteFilePath.isEmpty())
		return;

	m_pMainWindow->AddRecentFile(absoluteFilePath);

	QFileInfo fileInfo(absoluteFilePath);

	m_behaviorTreeDocument.SaveToFile(fileInfo.baseName().toStdString().c_str(), absoluteFilePath.toStdString().c_str());

	m_propertyTree->revert();

	if (m_behaviorTreeNodeSerializationHints.showXmlLineNumbers)
	{
		ReserializeDocumentIntoPropertyTree();
	}
}

void TreePanel::OnWindowEvent_Reload()
{
	if (!CheckForUnsavedDataAndSave())
		return;

	ReserializeDocumentIntoPropertyTree();
}

void TreePanel::OnWindowEvent_ShowXmlLineNumbers(const bool showFlag)
{
	m_behaviorTreeNodeSerializationHints.showXmlLineNumbers = showFlag;

	if (m_behaviorTreeDocument.Loaded() && CheckForUnsavedDataAndSave())
	{
		ReserializeDocumentIntoPropertyTree();
	}
	else
	{
		ForceAttachDocumentToPropertyTree();
	}
}

void TreePanel::OnWindowEvent_ShowComments(const bool showFlag)
{
	m_behaviorTreeNodeSerializationHints.showComments = showFlag;
	ForceAttachDocumentToPropertyTree();
}

void TreePanel::OnWindowEvent_ShowCryEngineSignals(const bool enable)
{
	ShowCryEngineSignals(enable);
	ForceAttachDocumentToPropertyTree();
}

void TreePanel::OnWindowEvent_ShowDeprecatedSignals(const bool enable)
{
	ShowGameSDKSignals(enable);
	ForceAttachDocumentToPropertyTree();
}

void TreePanel::OnWindowEvent_ShowGameSDKSignals(const bool enable)
{
	ShowDeprecatedSignals(enable);
	ForceAttachDocumentToPropertyTree();
}

void TreePanel::OnPropertyTreeDataChanged()
{
	m_behaviorTreeDocument.SetChanged();
}

bool TreePanel::HandleCloseEvent()
{
	return CheckForUnsavedDataAndSave();
}

void TreePanel::ShowXmlLineNumbers(const bool showFlag)
{
	m_behaviorTreeNodeSerializationHints.showXmlLineNumbers = showFlag;
}

void TreePanel::ShowComments(const bool showFlag)
{
	m_behaviorTreeNodeSerializationHints.showComments = showFlag;
}

void TreePanel::ShowCryEngineSignals(const bool enableFlag)
{
	if (enableFlag)
	{
		m_behaviorTreeNodeSerializationHints.eventsFlags.set(BehaviorTree::NodeSerializationHints::EEventsFlags_CryEngine);
	}
	else
	{
		m_behaviorTreeNodeSerializationHints.eventsFlags.reset(BehaviorTree::NodeSerializationHints::EEventsFlags_CryEngine);
	}
}

void TreePanel::ShowGameSDKSignals(const bool enableFlag)
{
	if (enableFlag)
	{
		m_behaviorTreeNodeSerializationHints.eventsFlags.set(BehaviorTree::NodeSerializationHints::EEventsFlags_GameSDK);
	}
	else
	{
		m_behaviorTreeNodeSerializationHints.eventsFlags.reset(BehaviorTree::NodeSerializationHints::EEventsFlags_GameSDK);
	}
}

void TreePanel::ShowDeprecatedSignals(const bool enableFlag)
{
	if (enableFlag)
	{
		m_behaviorTreeNodeSerializationHints.eventsFlags.set(BehaviorTree::NodeSerializationHints::EEventsFlags_Deprecated);
	}
	else
	{
		m_behaviorTreeNodeSerializationHints.eventsFlags.reset(BehaviorTree::NodeSerializationHints::EEventsFlags_Deprecated);
	}
}

void TreePanel::ReserializeDocumentIntoPropertyTree()
{
	m_behaviorTreeDocument.Reset();

	QFileInfo fileInfo(m_lastOpenedFilePath);

	if (!m_behaviorTreeDocument.OpenFile(fileInfo.baseName().toStdString().c_str(), m_lastOpenedFilePath.toStdString().c_str(), m_behaviorTreeNodeSerializationHints.eventsFlags))
	{
		return;
	}

	ForceAttachDocumentToPropertyTree();
}

bool TreePanel::CheckForUnsavedDataAndSave()
{
	if (!m_behaviorTreeDocument.Changed())
		return true;

	UnsavedChangesDialog saveDialog;
	saveDialog.exec();
	if (saveDialog.result == UnsavedChangesDialog::Yes)
	{
		if (!CheckForBehaviorTreeErrors())
			return false;

		return m_behaviorTreeDocument.Save();
	}
	else if (saveDialog.result == UnsavedChangesDialog::Cancel)
	{
		return false;
	}

	return true;
}

bool TreePanel::CheckForBehaviorTreeErrors()
{
	if (m_propertyTree->containsErrors())
	{
		m_propertyTree->focusFirstError();
		QMessageBox::critical(0, "Behavior Tree Editor", "This behavior tree contains an error and could not be saved.");
		return false;
	}

	return true;
}

void TreePanel::TryOpenFileDirectly(const QString& absoluteFilePath)
{
	if (absoluteFilePath.isEmpty())
		return;

	m_pMainWindow->AddRecentFile(absoluteFilePath);

	Reset();

	QFileInfo fileInfo(absoluteFilePath);

	if (!m_behaviorTreeDocument.OpenFile(fileInfo.baseName().toStdString().c_str(), absoluteFilePath.toStdString().c_str(), m_behaviorTreeNodeSerializationHints.eventsFlags))
		return;

	m_lastOpenedFilePath = absoluteFilePath;

	AttachDocumentToPropertyTree();
}

void TreePanel::AttachDocumentToPropertyTree()
{
	if (!m_propertiesAttachedToDocument)
	{
		ForceAttachDocumentToPropertyTree();
	}
}

void TreePanel::DetachDocumentFromPropertyTree()
{
	if (m_propertiesAttachedToDocument)
	{
		m_propertyTree->detach();
		m_propertiesAttachedToDocument = false;
	}
}

void TreePanel::ForceAttachDocumentToPropertyTree()
{
	m_contextList.Update<BehaviorTree::NodeSerializationHints>(&m_behaviorTreeNodeSerializationHints);
	m_propertyTree->setArchiveContext(m_contextList.Tail());

	if (m_behaviorTreeDocument.Loaded())
	{
		m_propertyTree->attach(Serialization::SStruct(m_behaviorTreeDocument));
		m_propertiesAttachedToDocument = true;
	}
}

void TreePanel::ForceApplyDocumentToPropertyTree()
{
	m_contextList.Update<BehaviorTree::NodeSerializationHints>(&m_behaviorTreeNodeSerializationHints);
	m_propertyTree->setArchiveContext(m_contextList.Tail());

	m_propertyTree->apply();
}

