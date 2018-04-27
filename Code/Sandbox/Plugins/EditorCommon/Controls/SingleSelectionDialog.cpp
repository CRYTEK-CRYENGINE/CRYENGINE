// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SingleSelectionDialog.h"
#include "QtUtil.h"

#include <QPushButton>
#include <QVBoxLayout>

CSingleSelectionDialog::CSingleSelectionDialog(QWidget* pParent)
	: CEditorDialog("SingleSelectionDialog", pParent)
	, m_selectedOptionIndex(0)
{
	SetResizable(false);
}

void CSingleSelectionDialog::SetOptions(const std::vector<string>& options)
{
	m_options = options;
	Rebuild();
}

int CSingleSelectionDialog::GetSelectedIndex() const
{
	return m_selectedOptionIndex;
}

void CSingleSelectionDialog::Rebuild()
{
	// #TODO If the number of options exceeds a certain threshold, a combo-box should be used instead.
	QVBoxLayout* const pMainLayout = new QVBoxLayout();

	for (size_t i = 0, N = m_options.size(); i < N; ++i)
	{
		QPushButton* const pButton = new QPushButton(QtUtil::ToQString(m_options[i]));
		connect(pButton, &QPushButton::clicked, [this, i]()
		{
			m_selectedOptionIndex = i;
			accept();
		});
		pMainLayout->addWidget(pButton);
	}

	QDialogButtonBox* const pButtons = new QDialogButtonBox();
	pButtons->setStandardButtons(QDialogButtonBox::Cancel);
	connect(pButtons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	pMainLayout->addWidget(pButtons);

	setLayout(pMainLayout);
}

