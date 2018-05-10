// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QtViewPane.h>
#include <QLabel>
#include <QAbstractItemModel>
#include <QTextEdit>
#include <QComboBox>
#include <QPushButton>

#include <IEditor.h>
#include <DockedWidget.h>
#include <Controls/EditorDialog.h>
#include <IPostRenderer.h>

#include <CryUQS/Shared/SharedIncludes.h>
#include <Serialization/QPropertyTree/QPropertyTree.h>

struct SQuery;
class CHistoricQueryTreeModel;
class CHistoricQueryTreeView;

class CMainEditorWindow : public CDockableWindow, public UQS::Core::IQueryHistoryListener, public UQS::Core::IQueryHistoryConsumer
{
	Q_OBJECT

private:

	class CUQSHistoryPostRenderer : public IPostRenderer
	{
	public:

		explicit CUQSHistoryPostRenderer(CHistoricQueryTreeView& historicQueryTreeView);

		// IPostRenderer
		virtual void OnPostRender() const override;
		// ~IPostRenderer

	private:

		CHistoricQueryTreeView& m_historicQueryTreeView;
	};

public:

	CMainEditorWindow();
	~CMainEditorWindow();

	// CDockableWindow
	virtual const char*                       GetPaneTitle() const override;
	virtual IViewPaneClass::EDockingDirection GetDockingDirection() const override { return IViewPaneClass::DOCK_FLOAT; }
	// ~CDockableWindow

	// ~IQueryHistoryListener
	virtual void OnQueryHistoryEvent(const UQS::Core::IQueryHistoryListener::SEvent& ev) override;
	// ~IQueryHistoryListener

	// IQueryHistoryConsumer
	virtual void AddOrUpdateHistoricQuery(const SHistoricQueryOverview& overview) override;
	virtual void AddTextLineToCurrentHistoricQuery(const ColorF& color, const char* szFormat, ...) override;
	virtual void AddTextLineToFocusedItem(const ColorF& color, const char* szFormat, ...) override;
	virtual void AddInstantEvaluatorName(const char* szInstantEvaluatorName) override;
	virtual void AddDeferredEvaluatorName(const char* szDeferredEvaluatorName) override;
	// ~IQueryHistoryConsumer

protected:
	void customEvent(QEvent* event) override;

private:
	void OnHistoryOriginComboBoxSelectionChanged(int index);
	void OnClearHistoryButtonClicked(bool checked);
	void OnSaveLiveHistoryToFile();
	void OnLoadHistoryFromFile();
	void OnTreeViewCurrentChanged(const QModelIndex &current, const QModelIndex &previous);

private:
	string                           m_windowTitle;
	UQS::Core::IQueryHistoryManager* m_pQueryHistoryManager;
	SQuery*                          m_pFreshlyAddedOrUpdatedQuery;
	CHistoricQueryTreeView*          m_pTreeView;
	CHistoricQueryTreeModel*         m_pTreeModel;
	QTextEdit*                       m_pTextQueryDetails;
	QTextEdit*                       m_pTextItemDetails;
	QComboBox*                       m_pComboBoxHistoryOrigin;
	QPushButton*                     m_pButtonClearCurrentHistory;
	QPropertyTree*                   m_pPropertyTree;
	CUQSHistoryPostRenderer*         m_pHistoryPostRenderer;
};
