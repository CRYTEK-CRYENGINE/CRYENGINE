// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "BehaviorTreeDocument.h"

#include <QPropertyTreeLegacy/ContextList.h>
#include <QDockWidget>

class QPropertyTreeLegacy;

class CMainWindow;

class TreePanel : public QDockWidget
{
	Q_OBJECT

public:
	TreePanel(CMainWindow* pMainWindow);

	void                Reset();

	void                OnWindowEvent_NewFile();
	void                OnWindowEvent_OpenFile();
	void                OnWindowEvent_OpenFile(const QString& absoluteFilePath);
	void                OnWindowEvent_Save();
	void                OnWindowEvent_SaveToFile();
	void 	            OnWindowEvent_Reload();

	bool                HandleCloseEvent();

	// Property tree control:
	void                ShowXmlLineNumbers(const bool showFlag);
	void                ShowComments(const bool showFlag);
	void                ShowCryEngineSignals(const bool enableFlag);
	void                ShowGameSDKSignals(const bool enableFlag);
	void                ShowDeprecatedSignals(const bool enableFlag);
		                
	void                OnWindowEvent_ShowXmlLineNumbers(const bool showFlag);
		                
	void                OnWindowEvent_ShowComments(const bool showFlag);
	void                OnWindowEvent_ShowCryEngineSignals(const bool enable);
	void                OnWindowEvent_ShowDeprecatedSignals(const bool enable);
	void                OnWindowEvent_ShowGameSDKSignals(const bool enable);

public slots:
	void                OnPropertyTreeDataChanged();

private:
	// Returns true if all data has been saved
	bool                CheckForUnsavedDataAndSave();
	bool                CheckForBehaviorTreeErrors();

	// File I/O support:
	void               TryOpenFileDirectly(const QString& absoluteFilePath);

	// Property tree control:
	void               AttachDocumentToPropertyTree();
	void               DetachDocumentFromPropertyTree();
	void               ForceAttachDocumentToPropertyTree();
	void               ForceApplyDocumentToPropertyTree();

	void               ReserializeDocumentIntoPropertyTree();

private:
	CMainWindow*                         m_pMainWindow;

	QPropertyTreeLegacy*                       m_propertyTree;

	BehaviorTreeDocument                 m_behaviorTreeDocument;
	bool                                 m_propertiesAttachedToDocument;
	Serialization::CContextList          m_contextList;
	BehaviorTree::NodeSerializationHints m_behaviorTreeNodeSerializationHints;
	QString                              m_lastOpenedFilePath;
};