// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryString/CryPath.h>
#include "EditorCommonAPI.h"

namespace PathUtil
{

EDITOR_COMMON_API string GetAudioLocalizationFolder();

//! Returns the user folder. This is a temporary folder within the project folder used to store project specific temporary files
EDITOR_COMMON_API string GetUserSandboxFolder();

//! Split a path into a vector<string> where each item is the name of a drive, folder or file in the hierarchy.
EDITOR_COMMON_API std::vector<string> SplitIntoSegments(const string& path);

//! This had to be created because _splitpath is too not handling console drives properly
EDITOR_COMMON_API void SplitPath(const string& fullPathFilename, string& driveLetter, string& directory, string& filename, string& extension);

//! This function replaces the filename from a path, keeping extension and directory/drive path.
EDITOR_COMMON_API string ReplaceFilename(const string& filepath, const string& filename);

EDITOR_COMMON_API string GetDirectory(const string& filepath);

//! GetUniqueName returns a string that is different from all strings in \p otherNames.
//! The returned string is an indexed form of \p templateName.
//! This function is typically used to create unique filenames, such as 'Untitled2'.
//! Examples:
//! GetUniqueName("test", { "Test" }) => "test"
//! GetUniquename("test", { "test" }) => "test1"
//! GetUniquename("test2", { "test2", "test3" }) => "test4"
EDITOR_COMMON_API string GetUniqueName(const string& templateName, const std::vector<string>& otherNames);

//! GetUniqueName returns a filename that is unique in folder passed as second argument.
//! The returned string is an indexed form of \p templateName.
EDITOR_COMMON_API string GetUniqueName(const string& fileName, const string& folderPath);

//! Replaces extension of filename.
//! Argument ext may include a leading dot, but it is not required.
//! Only the shortest extension is replaced: ReplaceExtension("archive.tar.gz", "zip") = "archive.tar.zip"
EDITOR_COMMON_API QString ReplaceExtension(const QString& str, const char* ext);

//! Transforms an Absolute Path to a relative path provided the absolute path of the directory to be relative to
//! Example : AbsoluteToRelativePath("A:/dir/to/file.ext","A:/dir/") returns "to/file.ext"
//! Please note this has edge cases and will not always canonize the paths properly
EDITOR_COMMON_API string AbsoluteToRelativePath(const string& absolutePath, const char* dirPathRelativeTo);

//! DEPRECATED: Convert an absolute path to a patch that is relative to the project root directory
//! Prefer using ToGamePath which will always convert a path to a game path, which can be used in CryPak methods as well
//! Example: <project_root>/<sys_game_folder>/Objects/box.cgf -> <sys_game_folder>/Objects/box.cgf
EDITOR_COMMON_API string AbsolutePathToCryPakPath(const string& path);

//! Convert an absolute path to a path that is relative to the game root folder
//! Example: <project_root>/<sys_game_folder>/Objects/box.cgf -> Objects/box.cgf
EDITOR_COMMON_API string AbsolutePathToGamePath(const string& path);

//! DEPRECATED: Converts a path relative to the game's asset folder to a path that can be used in CryPak.
//! This method should not really be used considering CryPak itself will adjust assets-relative paths to be opened using the same implementation,
//! therefore all user code should only deal with paks relative to game assets folder and let crypak handle the internals
//! Example : "Objects/box.cgf" => "<sys_game_folder>/Objects/box.cgf". Notice separators were adjusted for platform
EDITOR_COMMON_API string GamePathToCryPakPath(const string& path, bool bForWriting = false);

//! Returns absolute path to game content directory of active project.
//! Path uses unix-style delimiters and contains no trailing delimiter.
//! Example if using "old" embedded projects in engine directory : A:/p4/GameSDK
//! Example if using "new" detached projects : A:/ProjectDir/Assets
EDITOR_COMMON_API string GetGameProjectAssetsPath();

//! Returns relative path to game content directory of active project.
//! Path uses unix-style delimiters and contains no trailing delimiter.
//! For example, for "path A:/ProjectDir/Assets" return value would be "Assets"
EDITOR_COMMON_API string GetGameProjectAssetsRelativePath();

//! Returns absolute path to the editor's resource cache folder of active project.
//! Path uses unix-style delimiters and contains no trailing delimiter.
//! Example: A:/ProjectDir/Editor/ResourceCache
EDITOR_COMMON_API string GetEditorCachePath();

//! Returns relative path to game the editor's resource cache folder of active project.
//! Path uses unix-style delimiters and contains no trailing delimiter.
//! Example: Editor/ResourceCache
EDITOR_COMMON_API string GetEditorCacheRelativePath();

//! Gets the absolute path to the root of the project directory, where the .cryproject resides.
//! \return Path without trailing separator.
EDITOR_COMMON_API string GetCurrentProjectDirectoryAbsolute();

//! Converts any path to a game path (relative to assets folder)
//! Strips project root path and game directory from 'path'.
//! A:/p4/GameSDK/Objects/bird.cgf -> Objects/bird.cgf
//! GameSDK/Objects/bird.cgf -> Objects/bird.cgf
//! Objects/bird.cgf -> Objects/bird.cgf
//! where project root is A:/p4, and game directory is GameSDK.
EDITOR_COMMON_API string  ToGamePath(const string& path);
EDITOR_COMMON_API QString ToGamePath(const QString& path);
EDITOR_COMMON_API string  ToGamePath(const char* path);

EDITOR_COMMON_API QString ToUnixPath(const QString& path);

//!Get the path of the Editor folder in the engine installation path
EDITOR_COMMON_API string GetEditorFolderPath();

//!Get the path of the Scripts folder inside the Editor folder in the engine installation path
EDITOR_COMMON_API string GetEditorScriptsFolderPath();

//! Returns true if the name is not empty and only contains letters, numbers or '-' '_' ' ', and starts with a significant character
//! This restrictive naming policy ensures filenames will work on any system
EDITOR_COMMON_API bool IsValidFileName(const QString& name);

//! Returns current platform specific folder name, used when the user wants to store platform specific data.
EDITOR_COMMON_API string GetCurrentPlatformFolder();

//! Adjust the path to follow CryPak casing mode, which may or may not be case-preserving.
EDITOR_COMMON_API string AdjustCasing(const string& path);
EDITOR_COMMON_API CryPathString AdjustCasing(const char* szPath);
EDITOR_COMMON_API CryPathString AdjustCasing(const CryPathString& path);

//! Returns the same path with correction that exactly match casing on files system.
//! If file is not present on the file system an empty string is returned.
EDITOR_COMMON_API string MatchGamePathToCaseOnFileSystem(const string& path);

//! Returns the same path with correction that exactly match casing on files system.
//! If file is not present on the file system an empty string is returned.
EDITOR_COMMON_API string MatchAbsolutePathToCaseOnFileSystem(const string& path);
}
