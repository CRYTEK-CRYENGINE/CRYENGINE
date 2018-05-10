// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PhysProxiesControlsWidget.h"
#include "PhysicsProxies.h"

// EditorCommon.
#include <Controls/QMenuComboBox.h>

#include <QGroupBox>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

CPhysProxiesControlsWidget::CPhysProxiesControlsWidget(QWidget* pParent)
	: QWidget(pParent)
{
	SetupUI();

	connect(m_pCloseHoles, &QPushButton::pressed, [this]() { SigCloseHoles(); });
	connect(m_pReopenHoles, &QPushButton::pressed, [this]() { SigReopenHoles(); });
	connect(m_pSelectAll, &QPushButton::pressed, [this]() { SigSelectAll(); });
	connect(m_pSelectNone, &QPushButton::pressed, [this]() { SigSelectNone(); });
	connect(m_pGenerateProxies, &QPushButton::pressed, [this]() { SigGenerateProxies(); });
	connect(m_pResetProxies, &QPushButton::pressed, [this]() { SigResetProxies(); });
}

void CPhysProxiesControlsWidget::OnProxyIslandsChanged(const SPhysProxies* pProx)
{
	m_pCloseHoles->setEnabled(!pProx->ready && pProx->params.islandMap & (~pProx->pSrc->closedIsles | pProx->pSrc->restoredIsles));
	m_pReopenHoles->setEnabled(!pProx->ready && pProx->params.islandMap & pProx->pSrc->closedIsles & ~pProx->pSrc->restoredIsles);
	m_pSelectAll->setEnabled(!pProx->ready && (pProx->params.islandMap & (1ull << pProx->pSrc->isleTriCount.size()) - 1) != (1ull << pProx->pSrc->isleTriCount.size()) - 1);
	m_pSelectNone->setEnabled(!pProx->ready && pProx->params.islandMap);
	m_pGenerateProxies->setEnabled(!pProx->ready && pProx->params.islandMap);
	m_pResetProxies->setEnabled(pProx->ready);
	m_pShowSrc->setEnabled(pProx->ready);
	m_pShowMeshes->setEnabled(pProx->ready);
	m_pShowPrims->setEnabled(pProx->ready);
	m_pShowVoxels->setEnabled(pProx->ready);
}

inline CProxyGenerator::EVisibility GetVisibility(QMenuComboBox* cb)
{
	CRY_ASSERT(cb->GetCurrentData().canConvert<int>());
	return (CProxyGenerator::EVisibility)cb->GetCurrentData().toInt();
};

CProxyGenerator::EVisibility CPhysProxiesControlsWidget::GetSourceVisibility() const
{
	return GetVisibility(m_pShowSrc);
}

CProxyGenerator::EVisibility CPhysProxiesControlsWidget::GetMeshesVisibility() const
{
	return GetVisibility(m_pShowMeshes);
}

CProxyGenerator::EVisibility CPhysProxiesControlsWidget::GetPrimitivesVisibility() const
{
	return GetVisibility(m_pShowPrims);
}

bool CPhysProxiesControlsWidget::IsVoxelsVisible() const
{
	return GetVisibility(m_pShowVoxels) == CProxyGenerator::EVisibility::Solid;
}

void CPhysProxiesControlsWidget::SetupUI()
{
	auto createVisibilityWidget = [](QWidget* pParent, bool canBeTransparent = true)
	{
		QMenuComboBox* const cb = new QMenuComboBox(pParent);
		cb->AddItem("Solid", (int)CProxyGenerator::EVisibility::Solid);
		if (canBeTransparent)
		{
			cb->AddItem("Transparent", (int)CProxyGenerator::EVisibility::Wireframe);
		}
		cb->AddItem("Hidden", (int)CProxyGenerator::EVisibility::Hidden);
		return cb;
	};

	m_pPhysProxies = new QGroupBox();
	m_pPhysProxies->setObjectName(QStringLiteral("m_pPhysProxies"));
	QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	sizePolicy.setHorizontalStretch(0);
	sizePolicy.setVerticalStretch(0);
	sizePolicy.setHeightForWidth(m_pPhysProxies->sizePolicy().hasHeightForWidth());
	m_pPhysProxies->setSizePolicy(sizePolicy);
	m_pPhysProxies->setMinimumSize(QSize(250, 110));
	m_pCloseHoles = new QPushButton(m_pPhysProxies);
	m_pCloseHoles->setObjectName(QStringLiteral("m_pCloseHoles"));
	m_pCloseHoles->setGeometry(QRect(10, 20, 81, 23));
	m_pReopenHoles = new QPushButton(m_pPhysProxies);
	m_pReopenHoles->setObjectName(QStringLiteral("m_pReopenHoles"));
	m_pReopenHoles->setGeometry(QRect(10, 43, 81, 23));
	m_pGenerateProxies = new QPushButton(m_pPhysProxies);
	m_pGenerateProxies->setObjectName(QStringLiteral("m_pGenerateProxies"));
	m_pGenerateProxies->setGeometry(QRect(10, 72, 71, 31));
	m_pLabel = new QLabel(m_pPhysProxies);
	m_pLabel->setObjectName(QStringLiteral("label"));
	m_pLabel->setGeometry(QRect(150, 20, 47, 13));
	m_pShowSrc = createVisibilityWidget(m_pPhysProxies);
	m_pShowSrc->setObjectName(QStringLiteral("m_pShowSrc"));
	m_pShowSrc->setGeometry(QRect(150, 32, 91, 20));
	m_pShowMeshes = createVisibilityWidget(m_pPhysProxies);
	m_pShowMeshes->setObjectName(QStringLiteral("m_pShowMeshes"));
	m_pShowMeshes->setGeometry(QRect(150, 50, 91, 17));
	m_pShowPrims = createVisibilityWidget(m_pPhysProxies);
	m_pShowPrims->setObjectName(QStringLiteral("m_pShowPrims"));
	m_pShowPrims->setGeometry(QRect(150, 68, 70, 17));
	m_pShowVoxels = createVisibilityWidget(m_pPhysProxies, false /* Cannot be transparent */);
	m_pShowVoxels->setObjectName(QStringLiteral("m_pShowVoxels"));
	m_pShowVoxels->setGeometry(QRect(150, 86, 70, 17));
	m_pSelectAll = new QPushButton(m_pPhysProxies);
	m_pSelectAll->setObjectName(QStringLiteral("m_pSelectAll"));
	m_pSelectAll->setGeometry(QRect(100, 20, 41, 23));
	m_pSelectNone = new QPushButton(m_pPhysProxies);
	m_pSelectNone->setObjectName(QStringLiteral("m_pSelectNone"));
	m_pSelectNone->setGeometry(QRect(100, 43, 41, 23));
	m_pResetProxies = new QPushButton(m_pPhysProxies);
	m_pResetProxies->setObjectName(QStringLiteral("m_pResetProxies"));
	m_pResetProxies->setGeometry(QRect(80, 72, 61, 31));

	//m_pPhysProxies->setTitle(QString::fromLatin1("Physics Proxies"));
	m_pCloseHoles->setText(QString::fromLatin1("Close Holes"));
	m_pReopenHoles->setText(QString::fromLatin1("Reopen Holes"));
	m_pGenerateProxies->setText(QString::fromLatin1("Generate"));
	m_pLabel->setText(QString::fromLatin1("Show:"));
	m_pSelectAll->setText(QString::fromLatin1("All"));
	m_pSelectNone->setText(QString::fromLatin1("None"));
	m_pResetProxies->setText(QString::fromLatin1("Reset"));

	m_pShowSrc->SetChecked(1); // Set wireframe.

	QVBoxLayout* const pMainLayout = new QVBoxLayout();
	{
		QVBoxLayout* const pButtonLayout = new QVBoxLayout();
		pButtonLayout->setContentsMargins(0, 0, 0, 0);
		{
			QGridLayout* const pLayout = new QGridLayout();
			pLayout->setContentsMargins(0, 0, 0, 0);
			pLayout->setHorizontalSpacing(10);
			pLayout->setVerticalSpacing(1);

			pLayout->addWidget(m_pCloseHoles, 0, 0);
			pLayout->addWidget(m_pReopenHoles, 1, 0);
			pLayout->addWidget(m_pSelectAll, 0, 1);
			pLayout->addWidget(m_pSelectNone, 1, 1);

			pButtonLayout->addLayout(pLayout);
		}
		{
			QGridLayout* const pLayout = new QGridLayout();
			pLayout->setContentsMargins(0, 0, 0, 0);
			pLayout->setHorizontalSpacing(10);

			pLayout->addWidget(m_pGenerateProxies, 0, 0);
			pLayout->addWidget(m_pResetProxies, 0, 1);

			pButtonLayout->addLayout(pLayout);
		}
		pMainLayout->addLayout(pButtonLayout);
	}
	pMainLayout->addSpacing(10);
	{
		QGridLayout* const pLayout = new QGridLayout();
		pLayout->setContentsMargins(0, 0, 0, 0);

		pLayout->addWidget(new QLabel(tr("Source mesh")), 0, 0);
		pLayout->addWidget(new QLabel(tr("Proxy meshes")), 1, 0);
		pLayout->addWidget(new QLabel(tr("Primitives")), 2, 0);
		pLayout->addWidget(new QLabel(tr("Voxels")), 3, 0);
		pLayout->addWidget(m_pShowSrc, 0, 1);
		pLayout->addWidget(m_pShowMeshes, 1, 1);
		pLayout->addWidget(m_pShowPrims, 2, 1);
		pLayout->addWidget(m_pShowVoxels, 3, 1);

		pMainLayout->addLayout(pLayout);
	}
	pMainLayout->addStretch();

	setLayout(pMainLayout);
}

