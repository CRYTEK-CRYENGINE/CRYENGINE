// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef  __SUFFIXUTIL_H__
#define __SUFFIXUTIL_H__

#pragma once

// convenience class to work with suffixes in filenames, like in like dirt_ddn.dds
class SuffixUtil
{
public:

	// filename allowed to have many suffixes (e.g. "test_ddn_bump.dds" has "bump" and "ddn"
	// as suffixes (assuming that suffixSeparator is '_').
	// suffixes in file extension are also considered (e.g. "test_abc.my_data" has "abc" and "data" as suffixes)
	// suffixes in path part are also considered. if it's not what you want - remove path before calling this function.
	// comparison is case insensitive
	static bool HasSuffix(const char* const filename, const char suffixSeparator, const char* const suffix)
	{	
		assert(filename);
		assert(suffix && suffix[0]);

		const size_t suffixLen = strlen(suffix);

		for (const char* p = filename; *p; ++p)
		{
			if (p[0] != suffixSeparator)
			{
				continue;
			}

			if (memicmp(&p[1], suffix, suffixLen) != 0)
			{
				continue;
			}

			const char c = p[1 + suffixLen];
			if ((c == 0) || (c == suffixSeparator) || (c == '.'))
			{
				return true;
			}
		}

		return false;
	}
};

#endif // __SUFFIXUTIL_H__
