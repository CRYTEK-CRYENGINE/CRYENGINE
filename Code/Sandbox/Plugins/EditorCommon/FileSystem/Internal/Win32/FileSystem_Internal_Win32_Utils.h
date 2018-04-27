// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <QString>

namespace FileSystem
{
namespace Internal
{
namespace Win32
{

/// \return absolute path where the link points to (empty if not a link)
/// \note assure that linkPath is a path
QString GetLinkTargetPath(const QString& linkPath);

} // namespace Win32
} // namespace Internal
} // namespace FileSystem

