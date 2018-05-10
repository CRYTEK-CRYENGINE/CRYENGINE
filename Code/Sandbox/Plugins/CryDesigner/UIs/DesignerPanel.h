// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "UICommon.h"
#include "DesignerEditor.h"

class QTabWidget;
class QGridLayout;
class QToolButton;
class QPropertyTree;
class QTabWidget;

class QCollapsibleFrame;

namespace Designer
{

struct ToolchangeEvent;
class QMaterialComboBox;
class IBasePanel;

struct MenuButton
{
	MenuButton() : pTab(NULL), pPaper(NULL), pButton(NULL)
	{
	}

	MenuButton(QTabWidget* _pTab, QWidget* _pPaper, QWidget* _pButton) : pTab(_pTab), pPaper(_pPaper), pButton(_pButton)
	{
	}

	QTabWidget* pTab;
	QWidget*    pPaper;
	QWidget*    pButton;
};

struct WidgetContext
{
	WidgetContext() :
		pTab(NULL), pPaper(NULL), pGridLayout(NULL)
	{
	}
	WidgetContext(QTabWidget* _pTab, QWidget* _pPaper, QGridLayout* _pLayout) :
		pTab(_pTab), pPaper(_pPaper), pGridLayout(_pLayout)
	{
	}
	QTabWidget*  pTab;
	QWidget*     pPaper;
	QGridLayout* pGridLayout;
};

class DesignerPanel : public QWidget, public IDesignerPanel
{
	Q_OBJECT

public:
	DesignerPanel(QWidget* parent);
	~DesignerPanel();

	void     Done() override;

	void     Init() override;
	void     DisableButton(EDesignerTool tool) override;
	void     SetButtonCheck(EDesignerTool tool, bool bChecked) override;
	void     UpdateSubMaterialComboBox() override;
	void     UpdateSubToolButtons();
	int      GetSubMatID() const override;
	void     SetSubMatID(int nID) override;
	void     UpdateCheckButtons(EDesignerTool tool);
	void     OnDataBaseItemEvent(IDataBaseItem* pItem, EDataBaseItemEvent event) override;
	void     UpdateCloneArrayButtons() override;

	void     OnDesignerNotifyHandler(EDesignerNotify notification, TDesignerNotifyParam param);
	QWidget* GetWidget() override { return this; }

public slots:
	void OnSubMatSelectionChanged(int nSelectedItem);

private:

	std::map<EDesignerTool, MenuButton> m_Buttons;
	QCollapsibleFrame*        m_pAttributeFrame;
	QMaterialComboBox*        m_pSubMatComboBox;

	QWidget*                  m_pSelectionWidget;
	QWidget*                  m_pAdvancedWidget;

	QWidget* CreateSelectionWidget();
	QWidget* CreateShapeWidget();

	void         OrganizeEditLayout(QTabWidget* pTab);
	void         OrganizeModifyLayout(QTabWidget* pTab);
	void         OrganizeSurfaceLayout(QTabWidget* pTab);
	void         OrganizeMiscLayout(QTabWidget* pTab);

	void         SetAttributeWidget(const char* name, IBasePanel* pPanel);
	void         RemoveAttributeWidget();

	QToolButton* AddButton(EDesignerTool tool, const WidgetContext& wc, int row, int column, bool bColumnSpan = false);
	QToolButton* GetButton(EDesignerTool tool);
	void         EnableAllButtons();
	void         OnClickedButton(EDesignerTool tool, bool ensureDesigner = false);
	void         ShowTab(EDesignerTool tool);

	void         UpdateBackFaceFlag(MainContext& mc);
	void         UpdateBackFaceCheckBoxFromContext();

	int          ArrangeButtons(Designer::WidgetContext& wc, Designer::EToolGroup toolGroup, int stride, int offset, bool enforce = false);

	IBasePanel*      m_pToolPanel;

	static const int s_stride = 3;
};
}

