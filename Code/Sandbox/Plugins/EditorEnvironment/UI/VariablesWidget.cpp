// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "VariablesWidget.h"

#include "Controller.h"
#include "EnvironmentEditor.h"

#include <Serialization/QPropertyTreeLegacy/QPropertyTreeLegacy.h>

#include <QVBoxLayout>

CVariablesWidget::CVariablesWidget(CController& controller)
	: m_controller(controller)
{
	m_pPropertyTree = new QPropertyTreeLegacy(this);
	m_pPropertyTree->setHideSelection(false);
	m_pPropertyTree->setMultiSelection(false);

	m_pPropertyTree->setUndoEnabled(false, false);
	m_pPropertyTree->setMinimumWidth(300);

	QVBoxLayout* const pLayout = new QVBoxLayout;
	pLayout->setContentsMargins(QMargins(0, 0, 0, 0));
	pLayout->addWidget(m_pPropertyTree);
	setLayout(pLayout);

	m_treeContext.Update<CController>(&m_controller);
	m_pPropertyTree->setArchiveContext(m_treeContext.Tail());

	// Signals from this Tab to controller
	connect(m_pPropertyTree, &QPropertyTreeLegacy::signalSelected, this, &CVariablesWidget::OnTreeSelectionChanged);
	connect(m_pPropertyTree, &QPropertyTreeLegacy::signalBeginUndo, this, &CVariablesWidget::OnSelectedVariableStartChange);
	connect(m_pPropertyTree, &QPropertyTreeLegacy::signalChanged, this, &CVariablesWidget::OnSelectedVariableEndChange);

	// Signals from controller for UI updating
	m_controller.signalAssetOpened.Connect(this, &CVariablesWidget::OnAssetOpened);
	m_controller.signalAssetClosed.Connect(this, &CVariablesWidget::OnAssetClosed);
	m_controller.signalVariableTreeChanged.Connect(this, &CVariablesWidget::ResetTree);
	m_controller.signalPlaybackModeChanged.Connect(this, &CVariablesWidget::OnPlaybackModeChanged);
	m_controller.signalHandleKeyEventsInVarPropertyTree.Connect(this, &CVariablesWidget::ProcessUserEventsFromCurveEditor);
	m_controller.signalCurrentTimeChanged.Connect(this, &CVariablesWidget::ResetTree);
}

CVariablesWidget::~CVariablesWidget()
{
	m_controller.signalAssetOpened.DisconnectObject(this);
	m_controller.signalAssetClosed.DisconnectObject(this);
	m_controller.signalVariableTreeChanged.DisconnectObject(this);
	m_controller.signalPlaybackModeChanged.DisconnectObject(this);
	m_controller.signalHandleKeyEventsInVarPropertyTree.DisconnectObject(this);
	m_controller.signalCurrentTimeChanged.DisconnectObject(this);
}

// Handling user input
void CVariablesWidget::OnTreeSelectionChanged()
{
	int newSelection = -1;
	PropertyRow* pRow = m_pPropertyTree->selectedRow();
	if (pRow)
	{
		STodParameter* pParam = nullptr;
		while (pRow && pRow->parent())
		{
			if (STodGroup* pGroup = pRow->parent()->serializer().cast<STodGroup>())
			{
				int index = pRow->parent()->childIndex(pRow);
				if (index >= 0 && index < pGroup->params.size())
					pParam = &pGroup->params[index];
				break;
			}
			pRow = pRow->parent();
		}

		if (pParam)
		{
			newSelection = pParam->id;
		}
	}

	m_controller.OnVariableSelected(newSelection);
}

void CVariablesWidget::OnSelectedVariableStartChange()
{
	m_controller.OnSelectedVariableStartChange();
}

void CVariablesWidget::OnSelectedVariableEndChange()
{
	m_controller.OnVariableTreeEndChange();
}

// Updates from controller
void CVariablesWidget::OnAssetOpened()
{
	m_pPropertyTree->attach(Serialization::SStruct(m_controller.GetVariables()));
	RestoreTreeState(m_controller.GetEditor(), m_pPropertyTree, "VariablesTree");
}

void CVariablesWidget::OnAssetClosed()
{
	SaveTreeState(m_controller.GetEditor(), m_pPropertyTree, "VariablesTree");
	m_pPropertyTree->detach();
}

void CVariablesWidget::ResetTree()
{
	blockSignals(true);
	m_pPropertyTree->revert();
	blockSignals(false);
}

void CVariablesWidget::OnPlaybackModeChanged(PlaybackMode newMode)
{
	m_pPropertyTree->setEnabled(newMode == PlaybackMode::Edit);
}

void CVariablesWidget::ProcessUserEventsFromCurveEditor(QEvent* pEvent)
{
	QApplication::sendEvent(m_pPropertyTree, pEvent);
}
