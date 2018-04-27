// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

class QString;

// Used by the file dialogs
namespace LevelFileUtils
{

typedef QString AbsolutePath; // native filesystem global
typedef QString EnginePath;   // path relative to the current path
typedef QString GamePath;     // path relative to the game folder
typedef QString UserPath;     // user sees path beneath game/Levels

// Append the usual level file & file extension
AbsolutePath GetSaveLevelFile(const AbsolutePath&);

// Find the proper level file in a path
AbsolutePath FindLevelFile(const AbsolutePath&);

// Fallback takes any level file in the path
AbsolutePath FindAnyLevelFile(const AbsolutePath&);

// Returns true if a path contains a level
bool IsPathToLevel(const AbsolutePath&);

// Path conversion between absolute and CryEngine local pathes
AbsolutePath GetEngineBasePath();
EnginePath   ConvertAbsoluteToEnginePath(const AbsolutePath&);
AbsolutePath ConvertEngineToAbsolutePath(const EnginePath&);

// Path conversion between absolute and Game local pathes
AbsolutePath GetGameBasePath();
GamePath     ConvertAbsoluteToGamePath(const AbsolutePath&);
AbsolutePath ConvertGameToAbsolutePath(const GamePath&);

// Path conversion between absolute and user displayed level pathes
AbsolutePath GetUserBasePath();
UserPath     ConvertAbsoluteToUserPath(const AbsolutePath&);
AbsolutePath ConvertUserToAbsolutePath(const UserPath&);

// Returns true if the path contains only valid characters
bool IsValidLevelPath(const UserPath& userPath);

// Returns true if any parent folder contains a level
bool IsAnyParentPathLevel(const UserPath& userPath);

// Returns true if any subfolder contains a level
bool IsAnySubFolderLevel(const AbsolutePath& path);

// Make sure the level path exists
bool EnsureLevelPathsValid();

} // namespace LevelFileUtils

