// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MiddlewareDataWidget.h"

#include "AssetsManager.h"
#include "ImplManager.h"
#include "SystemControlsWidget.h"
#include "AssetIcons.h"
#include "FileImporterUtils.h"
#include "Common/IImpl.h"

#include <QDir>
#include <QVBoxLayout>

namespace ACE
{
//////////////////////////////////////////////////////////////////////////
CMiddlewareDataWidget::CMiddlewareDataWidget(QWidget* const pParent)
	: QWidget(pParent)
	, m_pLayout(new QVBoxLayout(this))
	, m_pImplDataPanel(nullptr)
{
	m_pLayout->setContentsMargins(0, 0, 0, 0);
	InitImplDataWidget();

	g_implManager.SignalOnBeforeImplChange.Connect([this]()
		{
			ClearImplDataWidget();
		}, reinterpret_cast<uintptr_t>(this));

	g_implManager.SignalOnAfterImplChange.Connect([this]()
		{
			InitImplDataWidget();
		}, reinterpret_cast<uintptr_t>(this));
}

//////////////////////////////////////////////////////////////////////////
CMiddlewareDataWidget::~CMiddlewareDataWidget()
{
	g_implManager.SignalOnBeforeImplChange.DisconnectById(reinterpret_cast<uintptr_t>(this));
	g_implManager.SignalOnAfterImplChange.DisconnectById(reinterpret_cast<uintptr_t>(this));

	ClearImplDataWidget();
}

//////////////////////////////////////////////////////////////////////////
void CMiddlewareDataWidget::InitImplDataWidget()
{
	if (g_pIImpl != nullptr)
	{
		m_pImplDataPanel = g_pIImpl->CreateDataPanel();
		m_pLayout->addWidget(m_pImplDataPanel);

		g_pIImpl->SignalGetConnectedSystemControls.Connect([&](ControlId const id, SControlInfos& controlInfos)
			{
				GetConnectedControls(id, controlInfos);
			}, reinterpret_cast<uintptr_t>(this));

		g_pIImpl->SignalSelectConnectedSystemControl.Connect([&](ControlId const systemControlId, ControlId const implItemId)
			{
				if (g_pSystemControlsWidget != nullptr)
				{
				  g_pSystemControlsWidget->SelectConnectedSystemControl(systemControlId, implItemId);
				}

			}, reinterpret_cast<uintptr_t>(this));

		g_pIImpl->SignalImportFiles.Connect([&](QString const& targetFolderName, bool const isLocalized)
			{
				OpenFileSelectorFromImpl(targetFolderName, isLocalized);
			}, reinterpret_cast<uintptr_t>(this));

		g_pIImpl->SignalFilesDropped.Connect([&](FileImportInfos const& fileImportInfos, QString const& targetFolderName, bool const isLocalized)
			{
				OpenFileImporter(fileImportInfos, targetFolderName, isLocalized, EImportTargetType::Middleware, nullptr);
			}, reinterpret_cast<uintptr_t>(this));
	}
}

//////////////////////////////////////////////////////////////////////////
void CMiddlewareDataWidget::ClearImplDataWidget()
{
	if (g_pIImpl != nullptr)
	{
		g_pIImpl->SignalGetConnectedSystemControls.DisconnectById(reinterpret_cast<uintptr_t>(this));
		g_pIImpl->SignalSelectConnectedSystemControl.DisconnectById(reinterpret_cast<uintptr_t>(this));
		g_pIImpl->SignalImportFiles.DisconnectById(reinterpret_cast<uintptr_t>(this));
		g_pIImpl->SignalFilesDropped.DisconnectById(reinterpret_cast<uintptr_t>(this));
		m_pLayout->removeWidget(m_pImplDataPanel);
		m_pImplDataPanel = nullptr;
		g_pIImpl->DestroyDataPanel();
	}
}

//////////////////////////////////////////////////////////////////////////
void CMiddlewareDataWidget::GetConnectedControls(ControlId const implItemId, SControlInfos& controlInfos)
{
	auto const& controls = g_assetsManager.GetControls();

	for (auto const pControl : controls)
	{
		if (pControl->GetConnection(implItemId) != nullptr)
		{
			SControlInfo info(pControl->GetName(), pControl->GetId(), GetAssetIcon(pControl->GetType()));
			controlInfos.emplace_back(info);
		}
	}
}
} // namespace ACE
