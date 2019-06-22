// Copyright 2001-2018 Crytek GmbH. All rights reserved.
#include "StdAfx.h"
#include "FileOperationsExecutor.h"

#include "IFilesGroupController.h"
#include "PathUtils.h"
#include "QtUtil.h"
#include <QFileInfo>
#include <QDir>

namespace Private_FileOperationsExecutor
{
	std::vector<QString> GetFullPaths(const std::vector<string>& paths)
	{
		std::vector<QString> result;
		for (const auto& path : paths)
		{
			result.push_back(QtUtil::ToQString(PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), path)));
		}

		return result;
	}

	// Ensures that if there are directories within the list of paths, that the list doesn't also contain other
	// paths within that directory
	void ValidatePaths(std::vector<QString>& paths)
	{
		for (auto ite = paths.begin(); ite != paths.end(); ++ite)
		{
			const QString& path = *ite;
			QFileInfo info(path);
			if (!info.isDir())
			{
				continue;
			}

			for (auto otherIte = ite + 1; otherIte != paths.end(); ++otherIte)
			{
				const QString& otherPath = *otherIte;

				if (otherPath.startsWith(path))
				{
					otherIte = paths.erase(otherIte);
					continue;
				}
				else if (path.startsWith(otherPath))
				{
					ite = paths.erase(ite);
					break;
				}
			}
		}
	}

class CDefaultFileOperationsExecutor : public IFileOperationsExecutor
{
protected:
	virtual void DoDelete(std::vector<std::unique_ptr<IFilesGroupController>> fileGroups
		, std::function<void(void)> callback) override
	{
		for (const auto& pFileGroup : fileGroups)
		{
			DoDelete(pFileGroup->GetFiles(), pFileGroup == fileGroups.back() ? std::move(callback) : [] {});
		}
	}

	virtual void DoDelete(const std::vector<string>& files, std::function<void(void)> callback) override
	{
		std::vector<QString> paths = GetFullPaths(files);
		ValidatePaths(paths);

		for (const auto& path : paths)
		{
			QFileInfo info(path);
			if (info.isDir())
			{
				if (!QDir(path).removeRecursively())
				{
					GetISystem()->GetILog()->LogWarning("Can not remove folder %s", QtUtil::ToString(path).c_str());
				}
			} else if (!QFile::remove(path))
			{
				GetISystem()->GetILog()->LogWarning("Can not delete asset file %s", QtUtil::ToString(path).c_str());
			}
		}
		if (callback)
		{
			callback();
		}
	}
};

CDefaultFileOperationsExecutor g_defaultExecutor;

}

std::unique_ptr<IFileOperationsExecutor> CFileOperationExecutor::s_executor;

IFileOperationsExecutor* CFileOperationExecutor::GetExecutor()
{
	using namespace Private_FileOperationsExecutor;
	return s_executor ? s_executor.get() : &g_defaultExecutor;
}

IFileOperationsExecutor* CFileOperationExecutor::GetDefaultExecutor()
{
	using namespace Private_FileOperationsExecutor;
	return &g_defaultExecutor;
}
