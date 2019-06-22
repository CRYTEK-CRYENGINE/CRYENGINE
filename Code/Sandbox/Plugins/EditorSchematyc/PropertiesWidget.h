// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "DetailWidget.h"

#include "PreviewWidget.h"

#include <NodeGraph/ICryGraphEditor.h>
#include <CrySerialization/IArchive.h>

#include <QWidget>

class QAdvancedPropertyTreeLegacy;

namespace CrySchematycEditor {

class CComponentItem;
class CAbstractObjectStructureModelItem;
class CAbstractVariablesModelItem;
class CMainWindow;

class CPropertiesWidget : public QWidget
{
	Q_OBJECT

public:
	CPropertiesWidget(CComponentItem& item, CMainWindow* pEditor);
	CPropertiesWidget(CAbstractObjectStructureModelItem& item, CMainWindow* pEditor);
	CPropertiesWidget(CAbstractVariablesModelItem& item, CMainWindow* pEditor);
	CPropertiesWidget(CryGraphEditor::GraphItemSet& items, CMainWindow* pEditor);

	// TEMP
	CPropertiesWidget(IDetailItem& item, CMainWindow* pEditor, Schematyc::CPreviewWidget* pPreview = nullptr);
	// ~TEMP

	virtual ~CPropertiesWidget() override;

	virtual void showEvent(QShowEvent* pEvent) override;

	void         OnContentDeleted(CryGraphEditor::CAbstractNodeGraphViewModelItem* deletedItem);

Q_SIGNALS:
	void SignalPropertyChanged();

protected:
	void SetupTree();
	void OnPropertiesChanged();
	void OnPreviewChanged();
	void OnBeginUndo();
	void OnEndUndo(bool acceptUndo);

protected:
	QAdvancedPropertyTreeLegacy*       m_pPropertyTree;
	Serialization::SStructs      m_structs;
	Serialization::CContextList* m_pContextList;

	// TEMP
	CMainWindow*                 m_pEditor;
	Schematyc::CPreviewWidget*   m_pPreview;
	std::unique_ptr<IDetailItem> m_pDetailItem;
	//~TEMP

	//The description to use when accepting the latest undo action
	string m_latestUndoActionDescription;
};

}
