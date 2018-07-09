// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "SelectExtensionDialog.h"

#include "Controls/QMenuComboBox.h"

#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QDialogButtonBox>

struct CSelectExtensionDialog::Implementation
{
	CSelectExtensionDialog* m_dialog;
	QLabel*                 m_label;
	QMenuComboBox*          m_combobox;
	QString					m_preferredExtension;
	QString                 m_result;

	Implementation(CSelectExtensionDialog* dialog)
		: m_dialog(dialog)
	{
		UiSetup(dialog);
	}

	void UiSetup(QDialog* parent)
	{
		parent->setWindowTitle(tr("Select extension"));

		auto textLabel = new QLabel(tr("Please select a valid file extension"), parent);

		m_label = new QLabel(parent);
		m_label->setTextFormat(Qt::PlainText);

		m_combobox = new QMenuComboBox(parent);

		auto innerLayout = new QHBoxLayout();
		innerLayout->setSpacing(5);
		innerLayout->addStretch(1);
		innerLayout->addWidget(m_label);
		innerLayout->addWidget(m_combobox);

		auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, parent);

		connect(buttonBox, &QDialogButtonBox::accepted, [&]()
		{
			UpdateResult();
			m_dialog->accept();
		});
		connect(buttonBox, &QDialogButtonBox::rejected, parent, &QDialog::reject);

		auto layout = new QVBoxLayout();
		layout->addWidget(textLabel);
		layout->addStretch(1);
		layout->addLayout(innerLayout);
		layout->addStretch(1);
		layout->addWidget(buttonBox);

		parent->setLayout(layout);
	}

	void SetFilename(const QString& filename)
	{
		m_result.clear();
		m_label->setText(filename + L'.');
	}

	void SetExtensions(const QStringList& extensions)
	{
		QStringList exts = extensions;
		exts.removeAll("*");

		CRY_ASSERT(exts.count() > 0); //If extensions contained only wildcards, it's a programmer error

		m_combobox->Clear();
		m_combobox->AddItems(exts);
		m_result.clear();
	}

	void SetPreferred(const QString& extension)
	{
		if(!extension.isEmpty())
		{
			m_preferredExtension = extension;
			m_combobox->SetChecked(extension);
		}
	}

	void UpdateResult()
	{
		m_result = m_label->text() + m_combobox->GetText();
	}
};

CSelectExtensionDialog::CSelectExtensionDialog(QWidget* parent)
	: CEditorDialog("SelectExtensionDialog", parent)
	, p(new Implementation(this))
{}

CSelectExtensionDialog::~CSelectExtensionDialog()
{
	// unique_ptr cleans it up
}

const QString& CSelectExtensionDialog::GetFilenameWithExtension() const
{
	return p->m_result;
}

void CSelectExtensionDialog::SetFilename(const QString& filename)
{
	p->SetFilename(filename);
}

void CSelectExtensionDialog::SetExtensions(const QStringList& extensions)
{
	p->SetExtensions(extensions);
}

void CSelectExtensionDialog::SetPreferred(const QString& extension)
{
	p->SetPreferred(extension);
}

bool CSelectExtensionDialog::IsValid() const
{
	return p->m_combobox->GetItemCount() > 1;
}

int CSelectExtensionDialog::exec()
{
	if (0 == p->m_combobox->GetItemCount())
	{
		p->m_result = p->m_label->text();
		return true; // filename seems ok
	}
	if (1 == p->m_combobox->GetItemCount())
	{
		p->UpdateResult();
		return true; // user can only select one option
	}
	return QDialog::exec();
}

