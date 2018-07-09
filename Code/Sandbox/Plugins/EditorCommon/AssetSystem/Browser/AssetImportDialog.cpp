// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AssetImportDialog.h"
#include <AssetSystem/AssetImportContext.h>
#include <AssetSystem/AssetType.h>

#include <QtUtil.h>
#include <FileDialogs/EngineFileDialog.h>

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

namespace Private_AssetImportDialog
{

QString ShowSaveToDirectoryPrompt(QWidget* pParent)
{
	CEngineFileDialog::RunParams runParams;
	runParams.title = QObject::tr("Save to directory...");
	runParams.initialDir = "Objects";
	return CEngineFileDialog::RunGameSelectDirectory(runParams, pParent);
}

} // namespace Private_AssetImportDialog

CAssetImportDialog::CAssetImportDialog(
	CAssetImportContext& context,
	const std::vector<CAssetType*>& assetTypes,
	const std::vector<bool>& assetTypeSelection,
	const std::vector<string>& allFilePaths,
	QWidget* pParent)
	: CEditorDialog("CAssetImportDialog", pParent)
	, m_context(context)
	, m_assetTypeSelection(assetTypeSelection)
{
	using namespace Private_AssetImportDialog;

	QGridLayout* const pMainLayout = new QGridLayout();

	QDialogButtonBox* const pButtons = new QDialogButtonBox();
	QPushButton* const pImportButton = pButtons->addButton("Import", QDialogButtonBox::YesRole);

	if (allFilePaths.size() > 1)
	{
		QPushButton* const pImportAllButton = pButtons->addButton("Import All", QDialogButtonBox::YesRole);

		QString allFiles;
		for (auto& fp : allFilePaths)
		{
			allFiles += QtUtil::ToQString(fp) + "\n";
		}
		pImportAllButton->setToolTip(allFiles);

		connect(pButtons, &QDialogButtonBox::clicked, [this, pImportAllButton](QAbstractButton* pButton)
		{
			if (pButton == pImportAllButton)
			{
				m_context.SetImportAll(true);
			}
		});
	}

	connect(pButtons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(pButtons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	// Input file path.
	{
		QLabel* const pInputFilePath = new QLabel(QtUtil::ToQString(m_context.GetInputFilePath()));

		const int rc = pMainLayout->rowCount();
		pMainLayout->addWidget(new QLabel(tr("Importing")), rc, 0);
		pMainLayout->addWidget(pInputFilePath, rc, 1);
	}

	// Output directory path.
	{
		QLineEdit* const pOutputDirectory = new QLineEdit();
		connect(pOutputDirectory, &QLineEdit::textChanged, [this](const QString& text)
		{
			m_context.SetOutputDirectoryPath(QtUtil::ToString(text));
		});

		QPushButton* const pSelectDirectory = new QPushButton(tr("Select"));
		connect(pSelectDirectory, &QPushButton::clicked, [this, pOutputDirectory]()
		{
			const QString dir = ShowSaveToDirectoryPrompt(this);
			pOutputDirectory->setText(dir);
		});

		QHBoxLayout* const pLayout = new QHBoxLayout();
		pLayout->addWidget(pOutputDirectory);
		pLayout->addWidget(pSelectDirectory);

		const int rc = pMainLayout->rowCount();
		pMainLayout->addWidget(new QLabel(tr("Save to")), rc, 0);
		pMainLayout->addLayout(pLayout, rc, 1);

		pOutputDirectory->setText(QtUtil::ToQString(m_context.GetOutputDirectoryPath()));
	}

	QWidget* const pContent = CreateContentWidget(assetTypes);

	pMainLayout->addWidget(pContent, pMainLayout->rowCount(), 0, 1, 2, Qt::AlignCenter);
	pMainLayout->addWidget(pButtons, pMainLayout->rowCount(), 0, 1, -1, Qt::AlignRight);
	setLayout(pMainLayout);
}

std::vector<bool> CAssetImportDialog::GetAssetTypeSelection() const
{
	return m_assetTypeSelection;
}

QWidget* CAssetImportDialog::CreateContentWidget(const std::vector<CAssetType*>& assetTypes)
{
	QVBoxLayout* const pLayout = new QVBoxLayout();

	for (size_t i = 0; i < assetTypes.size(); ++i)
	{
		const CAssetType* const pAssetType = assetTypes[i];

		const QString label = pAssetType->GetTypeName();
		QCheckBox* const pCheckBox = new QCheckBox(label);
		connect(pCheckBox, &QCheckBox::stateChanged, [this, i](int state)
		{
			m_assetTypeSelection[i] = state == Qt::Checked;
		});
		pLayout->addWidget(pCheckBox);
		pCheckBox->setChecked(m_assetTypeSelection[i]);  // Initial selection.
	}

	QWidget* const pDummyWidget = new QWidget();
	pDummyWidget->setLayout(pLayout);
	return pDummyWidget;
}
