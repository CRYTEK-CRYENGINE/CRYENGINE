// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "ConstantsWidget.h"
#include "EnvironmentEditor.h"

#include <Serialization/QPropertyTreeLegacy/QPropertyTreeLegacy.h>
#include <IUndoObject.h>

#include <CrySerialization/IArchiveHost.h>
#include <CrySystem/ISystem.h>

#include <QVBoxLayout>

class CConstantsWidget::CUndoConstPropTreeCommand : public IUndoObject
{
public:
	CUndoConstPropTreeCommand(const CEnvironmentEditor& envEditor, CConstantsWidget* pParentWnd)
		: m_envEditor(envEditor)
		, m_pParentWnd(pParentWnd)
	{
		ITimeOfDay::IPreset* const pPreset = m_envEditor.GetPreset();
		gEnv->pSystem->GetArchiveHost()->SaveBinaryBuffer(m_undoState, Serialization::SStruct(pPreset->GetConstants()));

		m_connection = QObject::connect(m_pParentWnd, &QWidget::destroyed, [this]()
		{
			m_pParentWnd = nullptr;
		});
	}

	~CUndoConstPropTreeCommand()
	{
		if (m_pParentWnd)
		{
			QObject::disconnect(m_connection);
		}
	}
private:
	virtual const char* GetDescription() override { return "Set Environment constant properties"; }

	virtual void        Undo(bool bUndo = true) override
	{
		if (!m_pParentWnd)
		{
			return;
		}

		if (bUndo)
		{
			ITimeOfDay::IPreset* const pPreset = m_envEditor.GetPreset();
			gEnv->pSystem->GetArchiveHost()->SaveBinaryBuffer(m_redoState, Serialization::SStruct(pPreset->GetConstants()));
		}

		ApplyState(m_undoState);
	}

	virtual void Redo() override
	{
		if (!m_pParentWnd)
		{
			return;
		}

		ApplyState(m_redoState);
	}

	void ApplyState(const DynArray<char>& state)
	{
		ITimeOfDay::IPreset* const pPreset = m_envEditor.GetPreset();
		gEnv->pSystem->GetArchiveHost()->LoadBinaryBuffer(Serialization::SStruct(pPreset->GetConstants()), state.data(), state.size());
		m_pParentWnd->OnChanged();
	}

	DynArray<char>            m_undoState;
	DynArray<char>            m_redoState;
	const CEnvironmentEditor& m_envEditor;
	CConstantsWidget*         m_pParentWnd;
	QMetaObject::Connection   m_connection;
};

CConstantsWidget::CConstantsWidget(CController& controller)
	: m_controller(controller)
{
	m_pPropertyTree = new QPropertyTreeLegacy(this);
	m_pPropertyTree->setHideSelection(false);
	m_pPropertyTree->setUndoEnabled(false, false);
	m_pPropertyTree->setMinimumWidth(300);

	QVBoxLayout* const pLayout = new QVBoxLayout;
	pLayout->setContentsMargins(QMargins(0, 0, 0, 0));
	pLayout->addWidget(m_pPropertyTree);
	setLayout(pLayout);

	connect(m_pPropertyTree, &QPropertyTreeLegacy::signalChanged, this, &CConstantsWidget::OnChanged);
	connect(m_pPropertyTree, &QPropertyTreeLegacy::signalBeginUndo, this, &CConstantsWidget::OnBeginUndo);
	connect(m_pPropertyTree, &QPropertyTreeLegacy::signalEndUndo, this, &CConstantsWidget::OnEndUndo);

	m_controller.signalAssetOpened.Connect(this, &CConstantsWidget::OnAssetOpened);
	m_controller.signalAssetClosed.Connect(this, &CConstantsWidget::OnAssetClosed);
	m_controller.signalPlaybackModeChanged.Connect(this, &CConstantsWidget::OnPlaybackModeChanged);
}

CConstantsWidget::~CConstantsWidget()
{
	m_controller.signalAssetOpened.DisconnectObject(this);
	m_controller.signalAssetClosed.DisconnectObject(this);
	m_controller.signalPlaybackModeChanged.DisconnectObject(this);
}

void CConstantsWidget::OnAssetOpened()
{
	auto& editor = m_controller.GetEditor();
	m_pPropertyTree->attach(Serialization::SStruct(editor.GetPreset()->GetConstants()));
	RestoreTreeState(editor, m_pPropertyTree, "ConstantsTree");
}

void CConstantsWidget::OnAssetClosed()
{
	SaveTreeState(m_controller.GetEditor(), m_pPropertyTree, "ConstantsTree");
	m_pPropertyTree->detach();
}

void CConstantsWidget::OnBeginUndo()
{
	GetIEditor()->GetIUndoManager()->Begin();
	GetIEditor()->GetIUndoManager()->RecordUndo(new CUndoConstPropTreeCommand(m_controller.GetEditor(), this));
}

void CConstantsWidget::OnEndUndo(bool acceptUndo)
{
	if (acceptUndo)
	{
		GetIEditor()->GetIUndoManager()->Accept("Update Environment Constants");
	}
	else
	{
		GetIEditor()->GetIUndoManager()->Cancel();
	}
}

void CConstantsWidget::OnChanged()
{
	auto& editor = m_controller.GetEditor();
	ITimeOfDay* const pTimeOfDay = editor.GetTimeOfDay();

	if (editor.GetPreset() == &pTimeOfDay->GetCurrentPreset())
	{
		m_pPropertyTree->revert();
		pTimeOfDay->ConstantsChanged();
		editor.GetAssetBeingEdited()->SetModified(true);
	}
}

void CConstantsWidget::OnPlaybackModeChanged(PlaybackMode newMode)
{
	m_pPropertyTree->setEnabled(newMode == PlaybackMode::Edit);
}
