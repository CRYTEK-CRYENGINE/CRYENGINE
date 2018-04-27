// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "FileSystem_Internal_PathUtils.h"
#include "FileSystem_Internal_Data.h"

#include "FileSystem/FileSystem_EnginePath.h"

#include <QMultiHash>
#include <QMap>
#include <QVector>
#include <QString>

namespace FileSystem
{
namespace Internal
{

/// \brief manage mounts from filesystem to the engine path system
class CMounts
{
public:
	CMounts(const QString& engineBasePath);

	/// \brief invokes callback for each enginePath where @absolutePath@ is mounted to
	template<typename Callback>
	void ForEachMountPoint(const SAbsolutePath& absolutePath, Callback callback) const
	{
		/*
		 * we search all mounts that contain absolutePath
		 *
		 * this does allows to have multiple mounts (two engine pathes that refer to the same absolute path)
		 * NO Nested mounts are allowed (two engine pathes refer to nested folders)
		 */
		auto it = m_absolutePathToEnginePathes.upperBound(absolutePath.key);
		if (it == m_absolutePathToEnginePathes.begin())
		{
			return; // out of range
		}
		it--; // we got upperbound so we go back at least once
		if (!IsPathEqualOrContained(absolutePath.key, it.key()))
		{
			return; // absolutePath not in path
		}
		const auto& absoluteTargetKeyPath = it.key();
		do
		{
			SEnginePath enginePath;
			enginePath.key = GetMountPath(absolutePath.key, absoluteTargetKeyPath, absolutePath.key, it->enginePath.key);
			enginePath.full = GetMountPath(absolutePath.key, absoluteTargetKeyPath, absolutePath.full, it->enginePath.full);
			if (it == m_absolutePathToEnginePathes.begin())
			{
				callback(enginePath);
				break; // finished
			}
			auto nextIt = it - 1; // get next iterator now, so callback can rename it
			callback(enginePath);
			it = nextIt;
		}
		while (it.key() == absoluteTargetKeyPath);
	}

	/// \brief invokes callback for each enginePath where @absolutePath@ is mounted to
	template<typename Callback>
	void ForEachKeyMountPoint(const QString& keyAbsolutePath, Callback callback) const
	{
		/*
		 * we search all mounts that contain absolutePath
		 *
		 * this does allows to have multiple mounts (two engine pathes that refer to the same absolute path)
		 * NO Nested mounts are allowed (two engine pathes refer to nested folders)
		 */
		auto it = m_absolutePathToEnginePathes.upperBound(keyAbsolutePath);
		if (it == m_absolutePathToEnginePathes.begin())
		{
			return; // out of range
		}
		it--; // we got upperbound so we go back at least once
		if (!IsPathEqualOrContained(keyAbsolutePath, it.key()))
		{
			return; // absolutePath not in path
		}
		const auto& absoluteTargetKeyPath = it.key();
		do
		{
			QString keyEnginePath;
			keyEnginePath = GetMountPath(keyAbsolutePath, absoluteTargetKeyPath, keyAbsolutePath, it->enginePath.key);
			if (it == m_absolutePathToEnginePathes.begin())
			{
				callback(keyEnginePath);
				break; // finished
			}
			auto nextIt = it - 1; // get next iterator now, so callback can rename it
			callback(keyEnginePath);
			it = nextIt;
		}
		while (it.key() == absoluteTargetKeyPath);
	}

	template<typename Callback>
	void ForEachMountBelow(const QString& engineKeyPath, Callback callback) const
	{
		ForEachMountBelowIt(engineKeyPath, [&](const EnginePathToAbsoluteMount::const_iterator& it)
				{
					SAbsolutePath absolutePath;
					absolutePath.key = it->absolute.key();
					absolutePath.full = it->absolute->fullAbsolutePath;
					callback(it->absolute->enginePath, absolutePath);
		    });
	}

	/// \return one of the engine pathes where @absolutePath@ is mounted to (empty if non exists or mounted to root)
	inline SEnginePath GetEnginePath(const QString& absoluteKeyPath) const
	{
		return m_absolutePathToEnginePathes.value(absoluteKeyPath).enginePath;
	}

	/// \return the absolute path of the root mount
	SAbsolutePath GetAbsoluteEnginePath() const;

	/// \return the absolute path that corresponds to the given @enginePath@
	SAbsolutePath GetAbsolutePath(const SEnginePath& enginePath) const;

	/// \brief add a new mount point
	/// \param enginePath where the mount point occurs
	/// \param absolutePath where the mount point leads to
	bool AddMountPoint(const SEnginePath& enginePath, const SAbsolutePath& absolutePath);

	/// \brief rename all mounts below the given @fromEnginePath@ which is now known by @toEnginePath@
	void RenameMount(const QString& fromKeyEnginePath, const SAbsolutePath& toName);

	/// \brief removes all mount points inside the engine path
	template<typename Callback>
	void RemoveMountsIn(const QString& keyEnginePath, Callback callback)
	{
		ForEachMountInIt(keyEnginePath, [&](EngineIterator it) -> EngineIterator
				{
					auto absoluteIt = it->absolute;
					auto absoluteKeyPath = absoluteIt.key();
					auto absoluteFullPath = absoluteIt->fullAbsolutePath;
					absoluteIt = m_absolutePathToEnginePathes.erase(absoluteIt);
					if (
					  (absoluteIt == m_absolutePathToEnginePathes.end() || absoluteIt.key() != absoluteKeyPath) &&
					  (absoluteIt == m_absolutePathToEnginePathes.begin() || (absoluteIt - 1).key() != absoluteKeyPath))
					{
					  SAbsolutePath absolutePath;
					  absolutePath.key = absoluteKeyPath;
					  absolutePath.full = absoluteFullPath;
					  callback(absolutePath); // absolutePath is no longer mounted
					}
					return m_enginePathToMountPoint.erase(it);
		    });
	}

	/// \brief removes the mount points that target absolutePath
	template<typename Callback>
	void RemoveMountTarget(const QString& absoluteKeyPath, Callback callback)
	{
		// remove for all pathes == absolutePath
		auto it = m_absolutePathToEnginePathes.find(absoluteKeyPath);
		while (it != m_absolutePathToEnginePathes.end())
		{
			auto engineKeyPath = it->enginePath.key; // make a copy will get invalid
			RemoveMountsIn(engineKeyPath, callback);
			it = m_absolutePathToEnginePathes.find(absoluteKeyPath);
		}
	}

private:
	template<typename Callback>
	void ForEachMountBelowIt(const QString& engineKeyPath, Callback callback) const
	{
		auto it = m_enginePathToMountPoint.upperBound(engineKeyPath + '/');
		while (it != m_enginePathToMountPoint.end() && IsPathEqualOrContained(it.key(), engineKeyPath))
		{
			callback(it);
			++it;
		}
	}

	template<typename Callback>
	void ForEachMountBelowIt(const QString& engineKeyPath, Callback callback)
	{
		auto it = m_enginePathToMountPoint.upperBound(engineKeyPath + '/');
		while (it != m_enginePathToMountPoint.end() && IsPathEqualOrContained(it.key(), engineKeyPath))
		{
			it = callback(it);
		}
	}

	template<typename Callback>
	void ForEachMountInIt(const QString& engineKeyPath, Callback callback)
	{
		auto it = m_enginePathToMountPoint.find(engineKeyPath);
		if (it != m_enginePathToMountPoint.end())
		{
			callback(it);
		}
		ForEachMountBelowIt(engineKeyPath, callback);
	}

private:
	struct EngineMount
	{
		QString     fullAbsolutePath;
		SEnginePath enginePath;
	};
	typedef QMultiMap<QString, EngineMount>    AbsolutePathToEnginePath; ///< key absolute path => engine mount
	typedef AbsolutePathToEnginePath::iterator AbsoluteIterator;

	struct AbsoluteMount;
	typedef QMap<QString, AbsoluteMount>        EnginePathToAbsoluteMount; ///< key engine path => absolute mount
	typedef EnginePathToAbsoluteMount::iterator EngineIterator;
	struct AbsoluteMount
	{
		AbsoluteIterator absolute;
		EngineIterator   up; ///< one directory level up
	};

	AbsolutePathToEnginePath  m_absolutePathToEnginePathes;
	EnginePathToAbsoluteMount m_enginePathToMountPoint;
};

} // namespace Internal
} // namespace FileSystem

