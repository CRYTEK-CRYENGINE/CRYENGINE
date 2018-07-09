// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "FileSystem_Internal_Mounts.h"

namespace FileSystem
{
namespace Internal
{

CMounts::CMounts(const QString& engineBasePath)
{
	auto rootEnginePath = SEnginePath(QString());
	AddMountPoint(rootEnginePath, SAbsolutePath(engineBasePath));
}

SAbsolutePath CMounts::GetAbsoluteEnginePath() const
{
	auto it = m_enginePathToMountPoint.find(QString());
	CRY_ASSERT(it != m_enginePathToMountPoint.end());
	SAbsolutePath result;
	result.key = it->absolute.key();
	result.full = it->absolute->fullAbsolutePath;
	return result;
}

SAbsolutePath CMounts::GetAbsolutePath(const SEnginePath& enginePath) const
{
	/*
	 * we search for the deepest mount point of enginePath
	 *
	 * a
	 * a/b -> up is a
	 * a/c -> up is b
	 * b
	 *
	 * upperbound finds the first entry > given
	 *
	 * upperbound("b/") -> it to end() // --it gives "b", direct match
	 * upperbound("a/b/c/") -> it to "a/c" // --it gives "a/b" - the closest mount
	 * upperbound("a/ba/") -> it to "a/c" // --it gives "a/b", up gives "a"
	 */
	auto it = m_enginePathToMountPoint.upperBound(enginePath.key + '/');
	CRY_ASSERT(it != m_enginePathToMountPoint.begin()); // as we always have root mounted we should match
	if (it != m_enginePathToMountPoint.begin())
	{
		--it;
		while (!IsPathEqualOrContained(enginePath.key, it.key()))
		{
			it = it.value().up;
		}
	}
	SAbsolutePath result;
	result.full = GetMountPath(enginePath.key, it.key(), enginePath.full, it->absolute->fullAbsolutePath);
	result.key = GetMountPath(enginePath.key, it.key(), enginePath.key, it->absolute.key());
	return result;
}

bool CMounts::AddMountPoint(const SEnginePath& enginePath, const SAbsolutePath& absolutePath)
{
	// check for nested mounts
	auto absoluteUpIt = m_absolutePathToEnginePathes.lowerBound(absolutePath.key);
	if (absoluteUpIt != m_absolutePathToEnginePathes.end() && IsPathContained(absoluteUpIt.key(), absolutePath.key))
	{
		return false; // nested mount - rejected
	}
	absoluteUpIt = m_absolutePathToEnginePathes.upperBound(absolutePath.key + '/');
	if (absoluteUpIt != m_absolutePathToEnginePathes.begin())
	{
		--absoluteUpIt;
		if (IsPathContained(absolutePath.key, absoluteUpIt.key()))
		{
			return false; // nested mount - rejected
		}
	}
	// create mount
	EngineMount engineMount = { absolutePath.full, enginePath };
	auto absoluteIt = m_absolutePathToEnginePathes.insertMulti(absolutePath.key, engineMount);

	auto engineUpIt = m_enginePathToMountPoint.upperBound(enginePath.key + '/');
	if (engineUpIt != m_enginePathToMountPoint.begin())
	{
		--engineUpIt;
		while (!IsPathEqualOrContained(enginePath.key, engineUpIt.key()))
		{
			engineUpIt = engineUpIt.value().up;
		}
	}
	AbsoluteMount mountPoint = { absoluteIt, engineUpIt };
	m_enginePathToMountPoint[enginePath.key] = mountPoint;
	return true;
}

void CMounts::RenameMount(const QString& engineKeyPath, const SAbsolutePath& toName)
{
	auto baseIt = m_enginePathToMountPoint.find(engineKeyPath);
	if (baseIt == m_enginePathToMountPoint.end())
	{
		return; // mount not found
	}
	AbsoluteMount baseMount = baseIt.value();
	m_enginePathToMountPoint.erase(baseIt);
	SEnginePath baseEnginePath;
	baseEnginePath.key = GetCombinedPath(GetDirectoryPath(engineKeyPath), toName.key);
	baseEnginePath.full = GetCombinedPath(GetDirectoryPath(baseMount.absolute->enginePath.full), toName.full);
	baseMount.absolute->enginePath = baseEnginePath;
	m_enginePathToMountPoint[baseEnginePath.key] = baseMount;

	ForEachMountBelowIt(engineKeyPath, [&](const EngineIterator& it) -> EngineIterator
			{
				auto mountKeyEnginePath = it.key();
				AbsoluteMount mount = it.value();
				auto nextIt = m_enginePathToMountPoint.erase(it);
				SEnginePath newMountEnginePath;
				newMountEnginePath.key = GetMountPath(mountKeyEnginePath, engineKeyPath, mountKeyEnginePath, baseEnginePath.key);
				newMountEnginePath.full = GetMountPath(mountKeyEnginePath, engineKeyPath, mount.absolute->enginePath.full, baseEnginePath.full);
				mount.absolute->enginePath = newMountEnginePath;
				m_enginePathToMountPoint[newMountEnginePath.key] = mount;
				return nextIt;
	    });
}

} // namespace Internal
} // namespace FileSystem

