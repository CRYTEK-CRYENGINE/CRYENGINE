// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "../Models/EffectAsset.h"
#include "../Models/EffectAssetModel.h"

#include "EffectViewWidget.h"

#include "Undo.h"

#include "Explorer/Explorer.h"

#include <CryIcon.h>

namespace CryParticleEditor {

CEffectAssetWidget::CEffectAssetWidget(std::shared_ptr<CEffectAssetModel>& pEffectAssetModel, QWidget* pParent)
	: QWidget(pParent)
	, m_pEffectAssetModel(pEffectAssetModel)
	, m_pEffectAsset(nullptr)
{
	QBoxLayout* pMainLayout = new QVBoxLayout();
	pMainLayout->setSpacing(0.0f);
	pMainLayout->setContentsMargins(0.0f, 0.0f, 0.0f, 0.0f);
	setContentsMargins(0.0f, 0.0f, 0.0f, 0.0f);

	m_pGraphView = new CGraphView();
	pMainLayout->addWidget(m_pGraphView);

	setLayout(pMainLayout);

	CRY_ASSERT(pEffectAssetModel);
	m_pEffectAssetModel->signalBeginEffectAssetChange.Connect(this, &CEffectAssetWidget::OnBeginEffectAssetChange);
	m_pEffectAssetModel->signalEndEffectAssetChange.Connect(this, &CEffectAssetWidget::OnEndEffectAssetChange);
	OnEndEffectAssetChange(); // Set current effect asset.

  // One-time scene fitting in the view.
	std::shared_ptr<QMetaObject::Connection> pConnection{ new QMetaObject::Connection };
	*pConnection = QObject::connect(m_pGraphView, &CGraphView::SignalItemsReloaded, [this, pConnection](auto& view)
	{
		if (!view.GetModel()->GetNodeItemCount())
		{
			return;
		}

		view.FitSceneInView();
		QObject::disconnect(*pConnection);
	});
}

CEffectAssetWidget::~CEffectAssetWidget()
{
	if (m_pEffectAssetModel)
	{
		m_pEffectAssetModel->signalBeginEffectAssetChange.DisconnectObject(this);
		m_pEffectAssetModel->signalEndEffectAssetChange.DisconnectObject(this);
	}
}

const pfx2::IParticleEffectPfx2* CEffectAssetWidget::GetEffect() const
{
	return m_pEffectAsset ? m_pEffectAsset->GetEffect() : nullptr;
}

pfx2::IParticleEffectPfx2* CEffectAssetWidget::GetEffect()
{
	return m_pEffectAsset ? m_pEffectAsset->GetEffect() : nullptr;
}

const char* CEffectAssetWidget::GetName() const
{
	CRY_ASSERT(m_pEffectAsset);
	return m_pEffectAsset->GetName();
}

void CEffectAssetWidget::SetModified()
{
	CRY_ASSERT(m_pEffectAsset);
	m_pEffectAsset->SetModified(true);
}

void CEffectAssetWidget::OnDeleteSelected()
{
	if (m_pGraphView)
	{
		m_pGraphView->DeleteSelectedItems();
	}
}

void CEffectAssetWidget::CopyComponents()
{
	if (m_pGraphView)
	{
		m_pGraphView->OnCopyEvent();
	}
}

void CEffectAssetWidget::OnPasteComponent()
{
	if (m_pGraphView)
	{
		m_pGraphView->OnPasteEvent();
	}
}

void CEffectAssetWidget::OnNewComponent()
{
	CRY_ASSERT(m_pEffectAsset);

	QAction* pAction = qobject_cast<QAction*>(sender());
	if (!pAction)
		return;

	QString templateFile = pAction->data().toString();
	if (templateFile.isEmpty())
		return;

	m_pEffectAsset->MakeNewComponent(templateFile.toStdString().c_str());
}

void CEffectAssetWidget::OnOptionsChanged()
{
	CRY_ASSERT(m_pEffectAsset);
	SetModified();
	m_pEffectAsset->GetEffect()->SetChanged();
}

bool CEffectAssetWidget::MakeNewComponent(const char* szTemplateName)
{
	CRY_ASSERT(m_pEffectAsset);
	return m_pEffectAsset->MakeNewComponent(szTemplateName);
}

void CEffectAssetWidget::customEvent(QEvent* pEvent)
{
	if (pEvent->type() != SandboxEvent::Command)
	{
		return QWidget::customEvent(pEvent);
	}

	CommandEvent* const pCommandEvent = (CommandEvent*)pEvent;
	const string& command = pCommandEvent->GetCommand();
	if (command == "general.copy")
	{
		CopyComponents();
		pEvent->setAccepted(true);
	}
	else if (command == "general.paste")
	{
		OnPasteComponent();
		pEvent->setAccepted(true);
	}
	else if (command == "general.delete")
	{
		OnDeleteSelected();
		pEvent->setAccepted(true);
	}
	else
	{
		return QWidget::customEvent(pEvent);
	}
}

void CEffectAssetWidget::OnBeginEffectAssetChange()
{
	m_pGraphView->SetModel(nullptr);
}

void CEffectAssetWidget::OnEndEffectAssetChange()
{
	CRY_ASSERT(m_pEffectAssetModel);
	CEffectAsset* const pEffectAsset = m_pEffectAssetModel->GetEffectAsset();
	m_pEffectAsset = pEffectAsset;
	if (m_pEffectAsset)
	{
		CParticleGraphModel* const pModel = m_pEffectAsset->GetModel();
		CRY_ASSERT(pModel);
		m_pGraphView->SetModel(pModel);
	}
}

}

