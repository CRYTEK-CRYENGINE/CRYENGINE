// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <QString>

namespace FileSystem
{
namespace Internal
{
// these are very fast internal methods (no error checking)
// please use with case - do not use as general purpose replacements

/// \return the combined path of the @containerPath@ and the relative @subPath@
/// \note subPath should not start with a slash
/// \example "a/b" + "c" => "a/b/c"
/// \example "" + "a" => "/a"
inline QString GetCombinedPath(const QString& containerPath, const QString& subPath)
{
	return containerPath + '/' + subPath;
}

/// \return file name of the given @path@
/// \example "a/b/c.txt" => "c.txt"
/// \example "b.txt" => "b.txt"
inline QString GetFilenameFromPath(const QString& path)
{
	auto index = path.lastIndexOf('/'); // -1 if not found
	return path.mid(index + 1);         // all if not found, text after slash if found
}

/// \return containing directory path for given @path@
/// \example "a/b/c" => "a/b"
/// \example "/a" => ""
/// \example "a" => ""
/// \example "" => ""
inline QString GetDirectoryPath(const QString& path)
{
	auto index = path.lastIndexOf('/'); // -1 if not found
	if (index <= 0)
	{
		return QString(); // first character or not found => empty path
	}
	return path.left(index); // path is before the last slash
}

/// \return last file extension for a given @filename@
/// \example "c.tar.gz" => "gz"
/// \example "a.txt" => "txt"
/// \example ".git" => "git"
/// \example "abc" => ""
inline QString GetFileExtensionFromFilename(const QString& filename)
{
	auto index = filename.lastIndexOf('.');
	if (index < 0)
	{
		return QString(); // no extension
	}
	return filename.mid(index + 1);
}

/// \return base name without the last file extension for a given @filename@
/// \example "c.tar.gz" => "c.tar"
/// \example "a.txt" => "a"
/// \example ".git" => ""
/// \example "abc" => "abc"
inline QString GetBasenameFromFilename(const QString& filename)
{
	auto index = filename.lastIndexOf('.');
	if (index < 0)
	{
		return filename; // no extension
	}
	return filename.left(index);
}

/// \return true if @path@ is contained in @containerPath@
/// \note false if @path@ == @containerPath@
///
/// containerPath should not end with a slash!
inline bool IsPathContained(const QString& keyPath, const QString& containerKeyPath)
{
	if (keyPath.length() > containerKeyPath.length())
	{
		return keyPath[containerKeyPath.length()] == '/' && keyPath.startsWith(containerKeyPath);
	}
	return false;
}

/// \return true if @path@ is equal or is contained in @containerPath@
///
/// containerPath should not end with a slash!
inline bool IsPathEqualOrContained(const QString& keyPath, const QString& containerKeyPath)
{
	if (keyPath.length() > containerKeyPath.length())
	{
		return keyPath[containerKeyPath.length()] == '/' && keyPath.startsWith(containerKeyPath);
	}
	return keyPath == containerKeyPath;
}

/// \returns the remounted @keyPath@ that is equal or is contained in @containerKeyPath@ as @path@ to the @containerMountPath@
/// \note if @keyPath@ is not equal or contained in @containerKeyPath@ an empty string is returned
///
/// \example
///  @keyPath@=@path@="/a/b/c"
///  @containerKeyPath@="/a"
///  @containerMountPath@="/mnt"
///  return "/mnt/b/c"
///
inline QString GetMountPath(const QString& keyPath, const QString& containerKeyPath, const QString& path, const QString& containerMountPath)
{
	CRY_ASSERT(keyPath.length() == path.length());
	if (keyPath.length() > containerKeyPath.length())
	{
		if (keyPath[containerKeyPath.length()] == '/' && keyPath.startsWith(containerKeyPath))
		{
			return containerMountPath + path.mid(containerKeyPath.length());
		}
	}
	else if (keyPath == containerKeyPath)
	{
		return containerMountPath;
	}
	return QString();
}

} // namespace Internal
} // namespace FileSystem

