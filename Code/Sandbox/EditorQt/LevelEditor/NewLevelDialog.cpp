// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "NewLevelDialog.h"

#include "LevelFileUtils.h"
#include "Terrain/Ui/GenerateHeightmapUi.h"
#include "Terrain/Ui/GenerateTerrainTextureUi.h"

#include "Controls/QMenuComboBox.h"

#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QSpacerItem>
#include <QVBoxLayout>
#include <QPushButton>
#include <QDir>

struct CNewLevelDialog::Implementation
{
	Implementation(CNewLevelDialog* pDialog)
		: m_pDialog(pDialog)
	{
		m_ui.Setup(pDialog);
		SetupView();
	}

	void SetupView()
	{
		connect(m_ui.m_pHeightmapDimensions, &CGenerateHeightmapUi::IsTerrainChanged, [this](bool isTerrain)
		{
			m_ui.m_pTerrainTextureDimensions->SetEnabled(isTerrain);
		});
		connect(m_ui.m_pDialogButtonBox, &QDialogButtonBox::accepted, m_pDialog, &QDialog::accept);
		connect(m_ui.m_pDialogButtonBox, &QDialogButtonBox::rejected, m_pDialog, &QDialog::reject);
	}

	CGenerateHeightmapUi::Result GetHeightmapResult() const
	{
		return m_ui.m_pHeightmapDimensions->GetResult();
	}

	CGenerateTerrainTextureUi::Result GetTextureResult() const
	{
		return m_ui.m_pTerrainTextureDimensions->GetResult();
	}

	struct Ui
	{
		void Setup(QDialog* pDialog)
		{
			pDialog->setWindowTitle(tr("New Level"));

			auto pFormLayout = new QFormLayout();

			CGenerateHeightmapUi::Config heightmapConfig;
			auto pHeightmapDimensions = new CGenerateHeightmapUi(heightmapConfig, pFormLayout, pDialog);

			CGenerateTerrainTextureUi::Result terrainTextureConfig;
			auto pTerrainTextureDimensions = new CGenerateTerrainTextureUi(terrainTextureConfig, pFormLayout, pDialog);

			auto verticalSpacer = new QSpacerItem(20, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);

			auto pDialogButtonBox = new QDialogButtonBox(pDialog);
			pDialogButtonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

			auto dialogLayout = new QVBoxLayout(pDialog);
			dialogLayout->addLayout(pFormLayout);
			dialogLayout->addItem(verticalSpacer);
			dialogLayout->addWidget(pDialogButtonBox);

			m_pHeightmapDimensions = pHeightmapDimensions;
			m_pTerrainTextureDimensions = pTerrainTextureDimensions;
			m_pDialogButtonBox = pDialogButtonBox;
		}

		CGenerateHeightmapUi*      m_pHeightmapDimensions;
		CGenerateTerrainTextureUi* m_pTerrainTextureDimensions;
		QDialogButtonBox*          m_pDialogButtonBox;
	};

	CNewLevelDialog* m_pDialog;
	Ui               m_ui;
};

CNewLevelDialog::CNewLevelDialog()
	: CEditorDialog(QStringLiteral("NewLevelDialog"))
	, p(new Implementation(this))
{}

CNewLevelDialog::~CNewLevelDialog()
{}

CLevelType::SCreateParams CNewLevelDialog::GetResult() const
{
	auto heightmap = p->GetHeightmapResult();
	auto texture = p->GetTextureResult();

	CLevelType::SCreateParams params;
	params.resolution = heightmap.resolution;
	params.unitSize = heightmap.unitSize;
	params.bUseTerrain = heightmap.isTerrain;
	params.texture.resolution = texture.resolution;
	params.texture.colorMultiplier = texture.colorMultiplier;
	params.texture.bHighQualityCompression = texture.isHighQualityCompression;

	return params;
}

