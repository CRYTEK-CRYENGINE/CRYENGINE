// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include <QWidget>

struct SProjectDescription;

class CProjectsModel;
class CProjectSortProxyModel;
class CSelectProjectDialog;
class QAdvancedTreeView;
class QButtonGroup;
class QItemSelection;
class QModelIndex;
class QPushButton;
class QThumbnailsView;
class QVBoxLayout;

class COpenProjectPanel : public QWidget
{
public:
	COpenProjectPanel(CSelectProjectDialog* pParent, bool runOnSandboxInit);

protected:
	void paintEvent(QPaintEvent*) override;

private:
	void CreateSearchPanel();
	void CreateViews();
	void CreateListViewButton(QButtonGroup* pGroup);
	void CreateThumbnailButton(QButtonGroup* pGroup);
	void CreateDialogButtons(bool runOnSandboxInit);

	void SetViewMode(bool thumbnailMode);
	void SelectProject(const SProjectDescription* pProject);

	void OnContextMenu(const QPoint& pos);

	void OnDeleteProject(const SProjectDescription* pDescription);
	void OnAddProject();
	void OpenProject(const QModelIndex& index);
	void OnSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected);

	void OnLoadProjectPressed();

	virtual void keyPressEvent(QKeyEvent* pEvent) override;

	CSelectProjectDialog*   m_pParent;

	CProjectsModel*         m_pModel;
	CProjectSortProxyModel* m_pSortedModel;

	QVBoxLayout*            m_pMainLayout;
	QAdvancedTreeView*      m_pTreeView;
	QThumbnailsView*        m_pThumbnailView;
	QPushButton*            m_pAddProjectBtn;
	QPushButton*            m_pOpenProjectBtn;
};
