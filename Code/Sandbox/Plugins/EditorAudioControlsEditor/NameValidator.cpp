// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "NameValidator.h"
#include "QtUtil.h"

#include <QToolTip>

namespace ACE
{
//////////////////////////////////////////////////////////////////////////
CNameValidator::CNameValidator()
	: QRegularExpressionValidator(nullptr)
	, m_pParent(nullptr)
{
}

//////////////////////////////////////////////////////////////////////////
CNameValidator::CNameValidator(QWidget* pParent)
	: QRegularExpressionValidator(pParent)
	, m_pParent(pParent)
{
}

//////////////////////////////////////////////////////////////////////////
void CNameValidator::Initialize(QRegularExpression const& regex)
{
	setRegularExpression(regex);

	if (regex == s_regexInvalidFilePath)
	{
		m_toolTipText = "A path can't contain any of the following characters:\n: ; , * ? \" < > |";
	}
	else
	{
		m_toolTipText = "A name can't contain any of the following characters:\n\\ / : ; , * ? \" < > |";
	}
}

//////////////////////////////////////////////////////////////////////////
QValidator::State CNameValidator::validate(QString& string, int& pos) const
{
	QValidator::State const state = QRegularExpressionValidator::validate(string, pos);

	if ((m_pParent != nullptr) && (state != QValidator::State::Acceptable))
	{
		QToolTip::showText(m_pParent->mapToGlobal(QPoint(0, -55)), m_toolTipText);
	}

	return state;
}

//////////////////////////////////////////////////////////////////////////
void CNameValidator::fixup(QString& input) const
{
	while (input.startsWith(".") || input.startsWith(" "))
	{
		input.remove(0, 1);
	}

	while (input.endsWith(".") || input.endsWith(" "))
	{
		input.remove(-1, 1);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CNameValidator::IsValid(string const& input) const
{
	QString qString = QtUtil::ToQString(input);
	int pos = 0;

	return validate(qString, pos) == QValidator::State::Acceptable;
}

//////////////////////////////////////////////////////////////////////////
void CNameValidator::FixupString(string& input) const
{
	QString qString = QtUtil::ToQString(input);
	fixup(qString);
	input = QtUtil::ToString(qString);
}
} // namespace ACE
