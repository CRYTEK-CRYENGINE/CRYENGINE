// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include "QStringDialog.h"
#include <QLineEdit>
#include <QVBoxLayout>
#include <QDialogButtonBox>

QStringDialog::QStringDialog(const QString& title, QWidget* pParent /*= NULL*/, bool bFileNameLimitation /*= false*/, bool bFileNameAsciiOnly /*= false*/)
	: CEditorDialog(QStringLiteral("QStringDialog"), pParent ? pParent : QApplication::widgetAt(QCursor::pos()), false)
	, m_bFileNameLimitation(bFileNameLimitation)
	, m_bFileNameAsciiOnly(bFileNameAsciiOnly)
{
	setWindowTitle(title.isEmpty() ? tr("Enter a string") : title);

	m_pEdit = new QLineEdit(this);
	QDialogButtonBox* okCancelButton = new QDialogButtonBox(this);
	okCancelButton->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	connect(okCancelButton, &QDialogButtonBox::accepted, this, &QStringDialog::accept);
	connect(okCancelButton, &QDialogButtonBox::rejected, this, &QStringDialog::reject);

	QVBoxLayout* layout = new QVBoxLayout(this);
	layout->addWidget(m_pEdit);
	layout->addWidget(okCancelButton);

	setLayout(layout);
	SetResizable(false);
}

void QStringDialog::SetString(const char* str)
{
	m_pEdit->setText(str); 
}

const string QStringDialog::GetString()
{
	return m_pEdit->text().toUtf8().constData(); 
}

void QStringDialog::showEvent(QShowEvent* event)
{
	CEditorDialog::showEvent(event);
	m_pEdit->setFocus();
}

void QStringDialog::accept()
{
	string str = GetString();

	if (m_bFileNameLimitation)
	{
		const string reservedCharacters("<>:\"/\\|?*}");
		for (int i = 0, iCount(reservedCharacters.GetLength()); i < iCount; ++i)
		{
			if (-1 != str.Find(reservedCharacters[i]))
			{
				string msg;
				msg.Format("This string can't contain %s characters", reservedCharacters);
				CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, msg);
				return;
			}
		}
	}
	if (m_bFileNameAsciiOnly)
	{
		for (size_t i = 0; i < str.GetLength(); ++i)
		{
			if (static_cast<unsigned char>(str[i]) > 0x7F)
			{
				CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "This string can't contain non-ASCII characters");
				return;
			}
		}
	}

	if (m_check && !m_check(str))
		return;

	CEditorDialog::accept();
}


