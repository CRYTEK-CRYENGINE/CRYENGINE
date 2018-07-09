// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "FileNameLineEdit.h"

namespace FileNameLineEdit_Private
{
namespace
{
static const QChar QUOTE_CHAR = '"';
static const QLatin1String DOUBLE_QUOTE("\"\"");

QStringList SplitAndUnquote(const QString& text)
{
	QStringList result;
	if (text.isEmpty())
	{
		return result; // no text => empty list
	}
	if (!text.contains(QUOTE_CHAR))
	{
		result << text;
		return result; // no quotes => one entry
	}

	// perform the unquote and split
	bool inQuote = false;
	QString filename;
	auto length = text.length();
	auto currentChar = text[0];
	for (int i = 1; i < length; ++i)
	{
		QChar nextChar = text[i];
		if (currentChar == QUOTE_CHAR)
		{
			if (nextChar == QUOTE_CHAR)
			{
				++i; // "" will result in "
				filename += nextChar;
			}
			else
			{
				inQuote = !inQuote;
			}
		}
		else if (inQuote || !currentChar.isSpace())
		{
			filename += currentChar;
		}
		else if (!filename.isEmpty())
		{
			result << filename;
			filename.clear();
		}
		currentChar = nextChar;
	}
	if (currentChar != QUOTE_CHAR && (inQuote || !currentChar.isSpace()))
	{
		filename += currentChar;
	}
	if (!filename.isEmpty())
	{
		result << filename;
	}
	return result;
}

QString QuoteAndJoin(const QStringList& stringList)
{
	if (stringList.isEmpty())
	{
		return QString(); // no entries => no text
	}
	if (1 == stringList.count())
	{
		auto first = stringList.first();
		if (!first.contains(QUOTE_CHAR))
		{
			return first; // one entry + no quotation => plain text
		}
	}

	// quote each string and join them with spaces
	QStringList parts;
	for (QString fileName : stringList)
	{
		parts << QUOTE_CHAR + fileName.replace(QUOTE_CHAR, DOUBLE_QUOTE) + QUOTE_CHAR;
	}
	return parts.join(QChar::Space);
}

} // namespace
} // namespace FileNameLineEdit_Private

CFileNameLineEdit::CFileNameLineEdit(QWidget* parent)
	: QLineEdit(parent)
{}

QStringList CFileNameLineEdit::GetFileNames() const
{
	auto lineText = text();
	return FileNameLineEdit_Private::SplitAndUnquote(lineText);
}

void CFileNameLineEdit::SetFileNames(const QStringList& fileNames)
{
	auto lineText = FileNameLineEdit_Private::QuoteAndJoin(fileNames);
	setText(lineText);
}

void CFileNameLineEdit::SetExtensionForFiles(const QString& extension)
{
	auto filenames = GetFileNames();
	if(!filenames.empty())
	{
		for (auto& filename : filenames)
		{
			//extension is always only considered from the last dot, consistent with behavior from windows
			const int dotIndex = filename.lastIndexOf(".");
			if (dotIndex > 0)
				filename = filename.left(dotIndex);

			filename.append("." + extension);
		}

		SetFileNames(filenames);
	}
}

void CFileNameLineEdit::Clear()
{
	setText("");
}

