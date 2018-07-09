// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QValidator>

namespace ACE
{
static QRegularExpression const s_regexInvalidFileName(QRegularExpression(R"([^<>:;,?"*|\\/]*)"));
static QRegularExpression const s_regexInvalidFilePath(QRegularExpression(R"([^<>:;,?"*|]*)"));

class CNameValidator final : public QRegularExpressionValidator
{
public:

	explicit CNameValidator(QRegularExpression const& regex, QWidget* const pParent);

	CNameValidator() = delete;

	// QRegularExpressionValidator
	virtual QValidator::State validate(QString& string, int& pos) const override;
	virtual void fixup(QString& input) const override;
	// ~QRegularExpressionValidator

private:

	QWidget* const m_pParent;
	QString        m_toolTipText;
};
} // namespace ACE
