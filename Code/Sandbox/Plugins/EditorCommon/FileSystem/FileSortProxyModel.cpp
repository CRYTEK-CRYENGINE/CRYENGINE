// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "FileSortProxyModel.h"

#include "FileTreeModel.h"

namespace Private_FileSortProxyModel
{

static inline QChar getNextChar(const QString& s, int index)
{
	return (index < s.length()) ? s.at(index) : QChar();
}

int naturalCompare(const QString& left, const QString& right)
{
	auto leftCount = left.count();
	auto rightCount = right.count();
	for (int iLeft = 0, iRight = 0; iLeft <= leftCount && iRight <= rightCount; ++iLeft, ++iRight)
	{
		// skip spaces, tabs and 0's
		QChar leftChar = getNextChar(left, iLeft);
		while (leftChar.isSpace())
		{
			leftChar = getNextChar(left, ++iLeft);
		}
		QChar rightChar = getNextChar(right, iRight);
		while (rightChar.isSpace())
		{
			rightChar = getNextChar(right, ++iRight);
		}
		if (leftChar.isDigit() && rightChar.isDigit())
		{
			while (leftChar.digitValue() == 0)
			{
				leftChar = getNextChar(left, ++iLeft);
			}
			while (rightChar.digitValue() == 0)
			{
				rightChar = getNextChar(right, ++iRight);
			}
			int lookAheadLocationLeft = iLeft;
			int lookAheadLocationRight = iRight;
			int currentReturnValue = 0;
			// find the last digit, setting currentReturnValue as we go if it isn't equal
			for (
			  QChar lookAheadLeft = leftChar, lookAheadRight = rightChar;
			  (lookAheadLocationLeft <= leftCount && lookAheadLocationRight <= rightCount);
			  lookAheadLeft = getNextChar(left, ++lookAheadLocationLeft),
			  lookAheadRight = getNextChar(right, ++lookAheadLocationRight)
			  )
			{
				bool is1ADigit = !lookAheadLeft.isNull() && lookAheadLeft.isDigit();
				bool is2ADigit = !lookAheadRight.isNull() && lookAheadRight.isDigit();
				if (!is1ADigit && !is2ADigit)
				{
					break;
				}
				if (!is1ADigit)
				{
					return -1;
				}
				if (!is2ADigit)
				{
					return 1;
				}
				if (currentReturnValue == 0)
				{
					if (lookAheadLeft < lookAheadRight)
					{
						currentReturnValue = -1;
					}
					else if (lookAheadLeft > lookAheadRight)
					{
						currentReturnValue = 1;
					}
				}
			}
			if (currentReturnValue != 0)
			{
				return currentReturnValue;
			}
		}

		if (!leftChar.isLower())
		{
			leftChar = leftChar.toLower();
		}
		if (!rightChar.isLower())
		{
			rightChar = rightChar.toLower();
		}
		int r = QString::localeAwareCompare(leftChar, rightChar);
		if (r < 0)
		{
			return -1;
		}
		if (r > 0)
		{
			return 1;
		}
	}
	// The two strings are the same (02 == 2) so fall back to the normal sort
	return QString::compare(left, right, Qt::CaseInsensitive);
}

bool GetFileNameLessThan(const QModelIndex& sourceLeft, const QModelIndex& sourceRight)
{
	auto leftFileName = sourceLeft.data().toString();
	auto rightFileName = sourceRight.data().toString();
	return naturalCompare(leftFileName, rightFileName) < 0;
}

} // namespace Private_FileSortProxyModel

CFileSortProxyModel::CFileSortProxyModel(QObject* parent)
	: QSortFilterProxyModel(parent)
{}

bool CFileSortProxyModel::lessThan(const QModelIndex& sourceLeft, const QModelIndex& sourceRight) const
{
	using namespace Private_FileSortProxyModel;

	switch (sortColumn())
	{
	case CFileTreeModel::eColumn_Name:
		{
			// place directories before files
			auto isLeftDir = sourceModel()->hasChildren(sourceLeft);
			auto isRightDir = sourceModel()->hasChildren(sourceRight);
			if (isLeftDir ^ isRightDir)
			{
				return isLeftDir;
			}
			return GetFileNameLessThan(sourceLeft, sourceRight);
		}
	case CFileTreeModel::eColumn_Size:
		{
			// Directories go first
			auto isLeftDir = sourceModel()->hasChildren(sourceLeft);
			auto isRightDir = sourceModel()->hasChildren(sourceRight);
			if (isLeftDir ^ isRightDir)
			{
				return isLeftDir;
			}
			auto leftSize = sourceLeft.data(CFileTreeModel::eRole_FileSize).toLongLong();
			auto rightSize = sourceRight.data(CFileTreeModel::eRole_FileSize).toLongLong();
			auto sizeDifference = leftSize - rightSize;
			if (sizeDifference != 0)
			{
				return sizeDifference < 0;
			}
			return GetFileNameLessThan(sourceLeft, sourceRight);
		}
	case CFileTreeModel::eColumn_Type:
		{
			auto leftType = sourceLeft.sibling(sourceLeft.row(), CFileTreeModel::eColumn_Type).data().toString();
			auto rightType = sourceRight.sibling(sourceRight.row(), CFileTreeModel::eColumn_Type).data().toString();
			int typeDifference = QString::localeAwareCompare(leftType, rightType);
			if (typeDifference != 0)
			{
				return typeDifference < 0;
			}
			return GetFileNameLessThan(sourceLeft, sourceRight);
		}
	case CFileTreeModel::eColumn_LastModified:
		{
			auto leftModified = sourceLeft.data(CFileTreeModel::eRole_LastModified).toDateTime();
			auto rightModified = sourceRight.data(CFileTreeModel::eRole_LastModified).toDateTime();
			if (leftModified != rightModified)
			{
				return leftModified < rightModified;
			}
			return GetFileNameLessThan(sourceLeft, sourceRight);
		}
	} // case
	CRY_ASSERT(false);
	return false;
}

