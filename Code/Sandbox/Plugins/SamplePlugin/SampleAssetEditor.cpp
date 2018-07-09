// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SampleAssetEditor.h"

#include <AssetSystem/Asset.h>
#include <AssetSystem/EditableAsset.h>
#include <FilePathUtil.h>

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>

REGISTER_VIEWPANE_FACTORY(CSampleAssetEditor, "Sample Asset Editor", "Tools", false);

namespace Private_SampleAssetEditor
{

// For the sake of simplicity, we directly use the C file API in this sample.
// In a real-world scenario, however, you should prefer the ICryPak interface.

bool ReadFile(const char* szFilePath, string& out)
{
	FILE* pFile;
	if (fopen_s(&pFile, szFilePath, "r"))
	{
		return false;
	}

	fseek(pFile, 0, SEEK_END);
	const size_t size = ftell(pFile);
	fseek(pFile, 0, SEEK_SET);

	std::vector<char> buffer;
	buffer.resize(size + 1, '\0');
	size_t elementsRead = fread(buffer.data(), sizeof(char), buffer.size(), pFile);
	buffer.resize(elementsRead);
	out = &buffer[0];

	fclose(pFile);

	return true;
}

bool CreateDataFile(const char* szFilePath, const string& data)
{
	FILE* pFile;
	if (fopen_s(&pFile, szFilePath, "w"))
	{
		return false;
	}
	fprintf(pFile, data.c_str());
	fclose(pFile);
	return true;
}

} // namespace Private_SampleAssetEditor

CSampleAssetEditor::CSampleAssetEditor()
	: CAssetEditor("SampleAssetType")
{
	m_pLineEdit = new QLineEdit();
	m_pLineEdit->setEnabled(false);

	connect(m_pLineEdit, &QLineEdit::textEdited, [this]()
	{
		if (GetAssetBeingEdited())
		{
			GetAssetBeingEdited()->SetModified(true);
		}
	});

	QHBoxLayout* const pMainLayout = new QHBoxLayout();
	pMainLayout->addWidget(new QLabel(tr("Data:")));
	pMainLayout->addWidget(m_pLineEdit);
	SetContent(pMainLayout);
}

bool CSampleAssetEditor::OnOpenAsset(CAsset* pAsset)
{
	using namespace Private_SampleAssetEditor;

	string data;
	const string absoluteFilePath = PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), pAsset->GetFile(0));
	if (!ReadFile(absoluteFilePath.c_str(), data))
	{
		return false;
	}

	m_pLineEdit->setText(data.c_str());
	m_pLineEdit->setEnabled(true);

	return true;
}

bool CSampleAssetEditor::OnSaveAsset(CEditableAsset& editAsset)
{
	using namespace Private_SampleAssetEditor;

	const string basePath = PathUtil::RemoveExtension(PathUtil::RemoveExtension(editAsset.GetAsset().GetMetadataFile()));

	const string dataFilePath = basePath + ".txt";
	const string absoluteDataFilePath = PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), dataFilePath);

	if (!CreateDataFile(absoluteDataFilePath.c_str(), m_pLineEdit->text().toUtf8().data()))
	{
		return false;
	}

	editAsset.SetFiles("", { dataFilePath });

	return true;
}

void CSampleAssetEditor::OnCloseAsset()
{
	m_pLineEdit->clear();
	m_pLineEdit->setEnabled(false);
}
