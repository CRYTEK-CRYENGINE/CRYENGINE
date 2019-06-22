// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace ACE
{
namespace Impl
{
namespace Fmod
{
struct SAudioTableInfo final
{
	SAudioTableInfo() = delete;
	SAudioTableInfo& operator=(SAudioTableInfo const&) = delete;

	explicit SAudioTableInfo(
		string const& name_,
		bool const isLocalized_,
		bool const includeSubDirs_)
		: name(name_)
		, isLocalized(isLocalized_)
		, includeSubDirs(includeSubDirs_)
	{}

	bool operator==(SAudioTableInfo const& rhs) const
	{
		return (name == rhs.name) && (isLocalized == rhs.isLocalized) && (includeSubDirs == rhs.includeSubDirs);
	}

	bool operator<(SAudioTableInfo const& rhs) const { return (name < rhs.name); }

	string const name;
	bool const   isLocalized;
	bool const   includeSubDirs;
};

using AudioTableInfos = std::set<SAudioTableInfo>;
} // namespace Fmod
} // namespace Impl
} // namespace ACE
