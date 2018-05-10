// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DesignerPanel.h"
#include "Core/SmoothingGroupManager.h"
#include "Tools/BaseTool.h"
#include "Material/MaterialManager.h"
#include "QtViewPane.h"
#include "QCollapsibleFrame.h"
#include "CryIcon.h"
#include <QTabWidget>
#include <QBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QToolButton>
#include <QFontMetrics>
#include "DesignerSession.h"

#define CREATE_BUTTON(tool, row, column, enforce) QObject::connect(AddButton(tool, wc, row, column), &QToolButton::clicked, this, [ = ] { OnClickedButton(tool, enforce); })

namespace Designer
{
DesignerPanel* s_pBrushDesignerPanel = NULL;

DesignerPanel::DesignerPanel(QWidget* parent)
	: QWidget(parent)
	, m_pAttributeFrame(new QCollapsibleFrame(""))
	, m_pSubMatComboBox(nullptr)
	, m_pToolPanel(nullptr)
{
	QWidget* pSelectionWidget = CreateSelectionWidget();
	QCollapsibleFrame* pCollapsibleSelectionWidget = new QCollapsibleFrame(tr("Selection"));
	pCollapsibleSelectionWidget->SetWidget(pSelectionWidget);
	m_pSelectionWidget = pCollapsibleSelectionWidget;

	QWidget* pShapeWidget = CreateShapeWidget();
	QCollapsibleFrame* pCollapsibleShapeWidget = new QCollapsibleFrame(tr("Shapes"));
	pCollapsibleShapeWidget->SetWidget(pShapeWidget);

	QTabWidget* pAdvancedTabWidget = new QTabWidget;
	OrganizeEditLayout(pAdvancedTabWidget);
	OrganizeModifyLayout(pAdvancedTabWidget);
	OrganizeSurfaceLayout(pAdvancedTabWidget);
	OrganizeMiscLayout(pAdvancedTabWidget);

	QCollapsibleFrame* pCollapsibleAdvancedWidget = new QCollapsibleFrame(tr("Advanced"));
	pCollapsibleAdvancedWidget->SetWidget(pAdvancedTabWidget);

	m_pAdvancedWidget = pCollapsibleAdvancedWidget;

	m_pAttributeFrame->hide();

	QVBoxLayout* pMainLayout = new QVBoxLayout();
	pMainLayout->setSpacing(0);
	// margins are stylable
	pMainLayout->setContentsMargins(0, 0, 0, 0);
	pMainLayout->addWidget(pCollapsibleShapeWidget);
	pMainLayout->addWidget(pCollapsibleSelectionWidget);
	pMainLayout->addWidget(pCollapsibleAdvancedWidget);
	pMainLayout->addWidget(m_pAttributeFrame);
	setLayout(pMainLayout);

	UpdateSubMaterialComboBox();

	// Disable, panels need to be explicitly enabled in Init
	m_pAdvancedWidget->setEnabled(false);
	m_pSelectionWidget->setEnabled(false);

	DesignerSession* pSession = DesignerSession::GetInstance();
	pSession->signalDesignerEvent.Connect(this, &DesignerPanel::OnDesignerNotifyHandler);

	// attempt to initialize the panel immediately if a session is active
	if (pSession->GetIsActive())
	{
		Init();
	}
}

DesignerPanel::~DesignerPanel()
{
	Done();
	DesignerSession::GetInstance()->signalDesignerEvent.DisconnectObject(this);
}

void DesignerPanel::Done()
{
	RemoveAttributeWidget();

	m_pAdvancedWidget->setEnabled(false);
	m_pSelectionWidget->setEnabled(false);

	UpdateCheckButtons(eDesigner_Invalid);
}

void DesignerPanel::OnDesignerNotifyHandler(EDesignerNotify notification, TDesignerNotifyParam param)
{
	switch (notification)
	{
	case eDesignerNotify_BeginDesignerSession:
		Init();
		break;
	case eDesignerNotify_EndDesignerSession:
		Done();
		break;
	case eDesignerNotify_RefreshSubMaterialList:
		UpdateSubMaterialComboBox();
		break;

	case eDesignerNotify_SubMaterialSelectionChanged:
		{
			MaterialChangeEvent* evt = static_cast<MaterialChangeEvent*>(param);
			uintptr_t nSelectedItem = evt->matID;
			if (!m_pSubMatComboBox->GetCurrentIndex() != nSelectedItem)
				m_pSubMatComboBox->SetCurrentIndex(nSelectedItem);
			break;
		}

	case eDesignerNotify_ObjectSelect:
		UpdateSubMaterialComboBox();
		UpdateCloneArrayButtons();
		break;

	case eDesignerNotify_SubToolChanged:
	case eDesignerNotify_SelectModeChanged:
		{
			ToolchangeEvent* evt = static_cast<ToolchangeEvent*>(param);
			UpdateCheckButtons(evt->current);
			UpdateCloneArrayButtons();

			if (notification == eDesignerNotify_SubToolChanged)
			{
				if (evt->current != eDesigner_Invalid)
				{
					BaseTool* pTool = GetDesigner()->GetCurrentTool();

					IBasePanel* pPanel = (IBasePanel*)UIPanelFactory::the().Create(pTool->Tool(), pTool);
					SetAttributeWidget(pTool->ToolName(), pPanel);
				}
				else
				{
					SetAttributeWidget("", nullptr);
				}
			}
			break;
		}
	case eDesignerNotify_DatabaseEvent:
		{
			DatabaseEvent* evt = static_cast<DatabaseEvent*>(param);
			OnDataBaseItemEvent(evt->pItem, evt->evt);
			break;
		}

	case eDesignerNotify_SubtoolOptionChanged:
		{
			if (m_pToolPanel)
			{
				m_pToolPanel->Update();
			}
			break;
		}
	case eDesignerNotify_EnterDesignerEditor:
		// update tool specific stuff here
		UpdateSubToolButtons();
		break;
	}
}

QToolButton* DesignerPanel::AddButton(EDesignerTool tool, const WidgetContext& wc, int row, int column, bool bColumnSpan)
{
	const char* szToolName = Serialization::getEnumDescription<EDesignerTool>().name(tool);

	QString icon = QString("icons:Designer/Designer_%1.ico").arg(szToolName);
	icon.replace(" ", "_");

	QToolButton* pButton = new QToolButton();
	pButton->setText(szToolName);
	pButton->setIcon(CryIcon(icon));
	pButton->setIconSize(QSize(24, 24));
	pButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	// buttons have fixed vertical size policies
	pButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	pButton->setCheckable(true);

	// font should always be readable
	const QFontMetrics fontMetrics(pButton->font());
	const QRect fontRect = fontMetrics.boundingRect(pButton->text());
	pButton->setMinimumHeight(fontRect.height());

	if (bColumnSpan)
		wc.pGridLayout->addWidget(pButton, row, column, 1, 2);
	else
		wc.pGridLayout->addWidget(pButton, row, column);
	m_Buttons[tool] = MenuButton(wc.pTab, wc.pPaper, pButton);
	pButton->setAutoFillBackground(true);
	return pButton;
}

void DesignerPanel::SetAttributeWidget(const char* name, IBasePanel* pPanel)
{
	RemoveAttributeWidget();

	m_pToolPanel = pPanel;

	if (pPanel)
	{
		m_pAttributeFrame->SetWidget(pPanel->GetWidget());
		m_pAttributeFrame->SetTitle(name);
		m_pAttributeFrame->show();

		pPanel->Update();
	}
}

void DesignerPanel::RemoveAttributeWidget()
{
	if (m_pToolPanel)
	{
		QWidget* pCurrentAttributeWidget = m_pToolPanel->GetWidget();
		m_pToolPanel->Done();
		// keep this -above- hiding of arribute frame!
		// hiding will cause flush of events on renderviewport
		// which may call this function recursively and we could end up with a crash.
		m_pToolPanel = nullptr;

		m_pAttributeFrame->SetWidget(nullptr);
		m_pAttributeFrame->hide();

		pCurrentAttributeWidget->deleteLater();

	}
}

int DesignerPanel::ArrangeButtons(WidgetContext& wc, EToolGroup toolGroup, int stride, int offset, bool enforce)
{
	std::vector<EDesignerTool> tools = ToolGroupMapper::the().GetToolList(toolGroup);
	int iEnd(tools.size() + offset);
	for (int i = offset; i < iEnd; ++i)
	{
		CREATE_BUTTON(tools[i - offset], i / stride, i % stride, enforce);
	}
	wc.pGridLayout->setRowStretch((iEnd - 1) / stride, 1);

	wc.pPaper->setLayout(wc.pGridLayout);

	return iEnd;
}

QWidget* DesignerPanel::CreateSelectionWidget()
{
	QWidget* pSelectionWidget = new QWidget;
	QGridLayout* pSelectionLayout = new QGridLayout;
	WidgetContext wc(nullptr, pSelectionWidget, pSelectionLayout);

	int offset = ArrangeButtons(wc, eToolGroup_BasicSelection, s_stride, 0);
	pSelectionLayout->addWidget(CreateHorizontalLine(), offset / s_stride + 1, 0, 1, s_stride, Qt::AlignTop);
	offset = ArrangeButtons(wc, eToolGroup_Selection, s_stride, ((offset + s_stride * 2) / s_stride) * s_stride);
	return pSelectionWidget;
}

QWidget* DesignerPanel::CreateShapeWidget()
{
	QWidget* pShapeWidget = new QWidget;
	QGridLayout* pShapeLayout = new QGridLayout;
	WidgetContext wc(nullptr, pShapeWidget, pShapeLayout);

	int offset = ArrangeButtons(wc, eToolGroup_Shape, s_stride, 0, true);
	pShapeLayout->addWidget(CreateHorizontalLine(), offset / s_stride + 1, 0, 1, s_stride, Qt::AlignTop);
	return pShapeWidget;
}

void DesignerPanel::OnClickedButton(EDesignerTool tool, bool ensureDesigner)
{
	// if enforcing is needed, we must create an object for the designer to work on
	if (!DesignerSession::GetInstance()->GetIsActive())
	{
		if (!ensureDesigner)
		{
			return;
		}
	}

	Designer::SwitchToDesignerToolForObject(DesignerSession::GetInstance()->GetBaseObject());
	CRY_ASSERT(GetDesigner());

	CEditTool* pEditTool = GetIEditor()->GetEditTool();
	if (pEditTool->IsKindOf(RUNTIME_CLASS(DesignerEditor)))
	{
		DesignerEditor* pDesignerEditor = static_cast<DesignerEditor*>(pEditTool);
		// Make sure that a designer object is set before we start editing
		// This might come from the current selection or a newly created object
		// Make sure as well that we don't have a current designer session, since
		// creating an object will destroy the current edit tool if a session is ongoing
		// resulting in a crash
		if (ensureDesigner && !pDesignerEditor->HasEditingObject() && !DesignerSession::GetInstance()->GetIsActive())
		{
			pDesignerEditor->Create("Designer");
		}
		else
		{
			pDesignerEditor->SetEditingObject(DesignerSession::GetInstance()->GetBaseObject());
		}

		pDesignerEditor->SwitchTool(tool);
	}
}

void DesignerPanel::OrganizeEditLayout(QTabWidget* pTab)
{
	int insertionIndex = pTab->insertTab(pTab->count(), new QWidget(pTab), "Extrude/Delete");
	QWidget* pPaper = pTab->widget(insertionIndex);
	QGridLayout* pEditGridLayout = new QGridLayout;
	ArrangeButtons(WidgetContext(pTab, pPaper, pEditGridLayout), eToolGroup_Edit, 3, 0);
}

void DesignerPanel::OrganizeModifyLayout(QTabWidget* pTab)
{
	int insertionIndex = pTab->insertTab(pTab->count(), new QWidget(pTab), "Tools");
	QWidget* pPaper = pTab->widget(insertionIndex);
	QGridLayout* pModifyGridLayout = new QGridLayout;
	ArrangeButtons(WidgetContext(pTab, pPaper, pModifyGridLayout), eToolGroup_Modify, 3, 0);
}

void DesignerPanel::OrganizeSurfaceLayout(QTabWidget* pTab)
{
	int insertionIndex = pTab->insertTab(pTab->count(), new QWidget(pTab), "Groups/UV");
	QWidget* pPaper = pTab->widget(insertionIndex);
	QGridLayout* pSurfaceGridLayout = new QGridLayout;
	int offset = ArrangeButtons(WidgetContext(pTab, pPaper, pSurfaceGridLayout), eToolGroup_Surface, 2, 0);

	int row = (offset - 1) / 2;
	pSurfaceGridLayout->setRowStretch(row, 0);
	pSurfaceGridLayout->addWidget(CreateHorizontalLine(), ++row, 0, 1, 2, Qt::AlignTop);

	m_pSubMatComboBox = new QMaterialComboBox;
	QLabel* pSubMatIDLabel = new QLabel("Sub Materials");
	pSubMatIDLabel->setAlignment(Qt::AlignHCenter);
	++row;
	pSurfaceGridLayout->addWidget(pSubMatIDLabel, row, 0, Qt::AlignTop);
	pSurfaceGridLayout->addWidget(m_pSubMatComboBox, row, 1, Qt::AlignTop);
	pSurfaceGridLayout->setRowStretch(row, 1);

	m_pSubMatComboBox->signalCurrentIndexChanged.Connect(this, &DesignerPanel::OnSubMatSelectionChanged);
}

void DesignerPanel::OrganizeMiscLayout(QTabWidget* pTab)
{
	int insertionIndex = pTab->insertTab(pTab->count(), new QWidget(pTab), "Transform");
	QWidget* pPaper = pTab->widget(insertionIndex);
	QGridLayout* pMiscGridLayout = new QGridLayout;
	ArrangeButtons(WidgetContext(pTab, pPaper, pMiscGridLayout), eToolGroup_Misc, 3, 0);
}

void DesignerPanel::UpdateSubToolButtons()
{
	DesignerEditor* pDesigner = GetDesigner();

	if (pDesigner)
	{
		// check the button of the current tool of the designer
		BaseTool* pTool = pDesigner->GetCurrentTool();
		if (pTool)
		{
			EDesignerTool current = pTool->Tool();
			UpdateCheckButtons(current);

			IBasePanel* pPanel = (IBasePanel*)UIPanelFactory::the().Create(pTool->Tool(), pTool);
			SetAttributeWidget(pTool->ToolName(), pPanel);
		}

	}
}

void DesignerPanel::Init()
{
	// first re-enable all buttons (in case they have been disabled by a previous tool)
	m_pAdvancedWidget->setEnabled(true);
	m_pSelectionWidget->setEnabled(true);

	EnableAllButtons();

	// get incompatible subtools for this designer session and disable their buttons
	std::vector<EDesignerTool> incompatibleTools = DesignerSession::GetInstance()->GetIncompatibleSubtools();

	for (EDesignerTool tool : incompatibleTools)
	{
		DisableButton(tool);
	}

	UpdateSubToolButtons();

	UpdateSubMaterialComboBox();
	UpdateCloneArrayButtons();
}

void DesignerPanel::UpdateCloneArrayButtons()
{
	CBaseObject* pObj = DesignerSession::GetInstance()->GetBaseObject();
	if (!pObj)
		return;

	if (pObj->GetParent())
	{
		GetButton(eDesigner_CircleClone)->setEnabled(false);
		GetButton(eDesigner_ArrayClone)->setEnabled(false);
	}
	else if (pObj->IsKindOf(RUNTIME_CLASS(DesignerObject)))
	{
		GetButton(eDesigner_CircleClone)->setEnabled(true);
		GetButton(eDesigner_ArrayClone)->setEnabled(true);
	}
}

QToolButton* DesignerPanel::GetButton(EDesignerTool tool)
{
	if (m_Buttons.find(tool) == m_Buttons.end())
	{
		return NULL;
	}
	return (QToolButton*)(m_Buttons[tool].pButton);
}

void DesignerPanel::EnableAllButtons()
{
	auto buttonIter = m_Buttons.cbegin();

	for (; buttonIter != m_Buttons.cend(); buttonIter++)
	{
		buttonIter->second.pButton->setEnabled(true);
	}
}

void DesignerPanel::DisableButton(EDesignerTool tool)
{
	QToolButton* pButton = GetButton(tool);
	if (pButton)
	{
		pButton->setEnabled(false);
	}
}

void DesignerPanel::SetButtonCheck(EDesignerTool tool, bool bChecked)
{
	QToolButton* pButton = GetButton(tool);
	if (!pButton)
	{
		return;
	}

	pButton->setChecked(bChecked);
}

void DesignerPanel::ShowTab(EDesignerTool tool)
{
	auto findIt = m_Buttons.find(tool);
	if (findIt == m_Buttons.end())
	{
		return;
	}
	const MenuButton& menuButton = findIt->second;
	// tool categories can have only one page widget
	// then no tab widget exists
	if (!menuButton.pTab)
	{
		return;
	}
	int nTabIndex = menuButton.pTab->indexOf(menuButton.pPaper);
	menuButton.pTab->setCurrentIndex(nTabIndex);
}

void DesignerPanel::UpdateSubMaterialComboBox()
{
	m_pSubMatComboBox->FillWithSubMaterials();
}

void DesignerPanel::OnDataBaseItemEvent(IDataBaseItem* pItem, EDataBaseItemEvent event)
{
	if (pItem == NULL)
	{
		return;
	}

	if (event == EDB_ITEM_EVENT_SELECTED)
	{
		CBaseObject* pObject = DesignerSession::GetInstance()->GetBaseObject();
		m_pSubMatComboBox->SelectMaterial(pObject, (CMaterial*)pItem);
	}
}

int DesignerPanel::GetSubMatID() const
{
	if (m_pSubMatComboBox->GetSubMatID() == -1)
	{
		return 0;
	}
	return m_pSubMatComboBox->GetSubMatID();
}

void DesignerPanel::SetSubMatID(int nID)
{
	CBaseObject* pObject = DesignerSession::GetInstance()->GetBaseObject();
	m_pSubMatComboBox->SetSubMatID(pObject, nID);
}

void DesignerPanel::OnSubMatSelectionChanged(int nSelectedItem)
{
	if (m_pSubMatComboBox->GetCurrentIndex() != nSelectedItem)
	{
		m_pSubMatComboBox->SetCurrentIndex(nSelectedItem);
	}

	DesignerSession::GetInstance()->SetCurrentSubMatID(nSelectedItem);
}

void DesignerPanel::UpdateCheckButtons(EDesignerTool tool)
{
	// unpress any other tools if no selection related (selection is handled below)
	for (auto buttonIter = m_Buttons.cbegin(); buttonIter != m_Buttons.cend(); buttonIter++)
	{
		if (!IsSelectElementMode(buttonIter->first))
		{
			SetButtonCheck(buttonIter->first, false);
		}
	}

	SetButtonCheck(eDesigner_Vertex, false);
	SetButtonCheck(eDesigner_Edge, false);
	SetButtonCheck(eDesigner_Polygon, false);

	if (IsSelectElementMode(tool))
	{
		int nDesignerMode = (int)tool;
		if (nDesignerMode & eDesigner_Vertex)
			SetButtonCheck(eDesigner_Vertex, true);
		if (nDesignerMode & eDesigner_Edge)
			SetButtonCheck(eDesigner_Edge, true);
		if (nDesignerMode & eDesigner_Polygon)
			SetButtonCheck(eDesigner_Polygon, true);
	}
	else
	{
		SetButtonCheck(tool, true);
	}
}
}

