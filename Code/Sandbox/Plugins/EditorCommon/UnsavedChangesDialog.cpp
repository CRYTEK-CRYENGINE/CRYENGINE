// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "UnsavedChangesDialog.h"

#include <QAbstractButton>
#include <QBoxLayout>
#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>

CUnsavedChangedDialog::CUnsavedChangedDialog(QWidget* parent)
	: CEditorDialog("CUnsavedChangedDialog")
{
	setWindowTitle("Unsaved Changes");
	setModal(true);

	auto layout = new QBoxLayout(QBoxLayout::TopToBottom);
	auto label = new QLabel("The following files were modified.\n\nWould you like to save them before closing?");
	layout->addWidget(label, 0);

	m_list = new QListWidget();
	layout->addWidget(m_list, 1);

	auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Yes | QDialogButtonBox::No | QDialogButtonBox::Cancel, Qt::Horizontal);
	layout->addWidget(buttonBox, 0);

	connect(buttonBox, &QDialogButtonBox::clicked, this, [this, buttonBox](QAbstractButton* button)
	{
		done(buttonBox->buttonRole(button));
	});

	setLayout(layout);
	resize(500, 350);

	if (parent)
	{
		QPoint center = parent->mapToGlobal(parent->geometry().center());
		move(center - QPoint(width() / 2, height() / 2));
	}
}

bool CUnsavedChangedDialog::Exec(DynArray<string>* selectedFiles, const DynArray<string>& files)
{
	m_list->clear();

	std::vector<QListWidgetItem*> items(files.size(), nullptr);

	for (size_t i = 0; i < files.size(); ++i)
	{
		auto item = new QListWidgetItem(files[i].c_str(), m_list);
		item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
		item->setCheckState(Qt::Checked);
		items[i] = item;
	}

	selectedFiles->clear();

	int result = exec();
	if (result == QDialogButtonBox::YesRole)
	{
		for (size_t i = 0; i < items.size(); ++i)
		{
			if (items[i]->checkState() == Qt::Checked)
				selectedFiles->push_back(files[i].c_str());
		}
		return true;
	}
	else if (result == QDialogButtonBox::NoRole)
		return true;
	else
		return false;
}

bool EDITOR_COMMON_API UnsavedChangesDialog(QWidget* parent, DynArray<string>* selectedFiles, const DynArray<string>& files)
{
	CUnsavedChangedDialog dialog(parent);
	return dialog.Exec(selectedFiles, files);
}

