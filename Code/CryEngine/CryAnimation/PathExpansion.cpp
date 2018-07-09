// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "PathExpansion.h"

#include <CryMath/Random.h>

// Expand patterns into paths. An example of a pattern before expansion:
// animations/facial/idle_{0,1,2,3}.fsq
// {} is used to specify options for parts of the string.
// output must point to buffer that is at least as large as the pattern.
void PathExpansion::SelectRandomPathExpansion(const char* pattern, char* output)
{
	// Loop through all the characters.
	int output_pos = 0;
	for (int pos = 0; pattern[pos]; )
	{
		// Check whether this character is the start of an options list.
		if (pattern[pos] == '{')
		{
			struct Option
			{
				int start;
				int length;
			};
			static const int MAX_OPTIONS = 100;
			Option options[MAX_OPTIONS];
			int option_count = 0;

			// Skip the '{'.
			++pos;

			// Create the first option.
			options[option_count].start = pos;
			options[option_count].length = 0;
			++option_count;

			// Loop until we find the matching }.
			while (pattern[pos] && pattern[pos] != '}')
			{
				if (pattern[pos] == ',')
				{
					// Skip the ','.
					++pos;

					// Add a new option to the list.
					if (option_count < MAX_OPTIONS)
					{
						options[option_count].start = pos;
						options[option_count].length = 0;
					}
					++option_count;
				}
				else
				{
					// Extend the length of the current option.
					if (option_count <= MAX_OPTIONS)
						++options[option_count - 1].length;

					++pos;
				}
			}

			// Skip the '}'.
			++pos;

			// Pick a random option from the list.
			int option_index = cry_random(0, std::min(MAX_OPTIONS, option_count) - 1);
			for (int i = 0; i < options[option_index].length; ++i)
				output[output_pos++] = pattern[options[option_index].start + i];
		}
		else
		{
			// The character is a normal one - simply copy it to the output.
			output[output_pos++] = pattern[pos++];
		}
	}

	// Write the null terminator.
	output[output_pos] = 0;
}

namespace PathExpansion
{
struct Option
{
	int start;
	int length;
};
struct Segment
{
	static const int MAX_OPTIONS = 30;
	Option           options[MAX_OPTIONS];
	int              optionCount;
};
}

void PathExpansion::EnumeratePathExpansions(const char* pattern, void (* enumCallback)(void* userData, const char* expansion), void* userData) PREFAST_SUPPRESS_WARNING(6262)
{
	// Allocate some temporary working buffers.
	int patternLength = strlen(pattern);
	std::vector<char> buffer(patternLength + 1);
	char* expansion = &buffer[0];

	// Divide the pattern into a list of segments, which contain a list of option text (the case where there is only one
	// option is used to represent simple text copying).
	static const int MAX_SEGMENTS = 100;
	Segment segments[MAX_SEGMENTS] = { Segment() };
	int segmentCount = 0;

	// Loop through all the characters.
	for (int pos = 0; pattern[pos]; )
	{
		// Check whether this character is the start of an options list.
		if (pattern[pos] == '{')
		{
			Segment dummySegment;
			dummySegment.optionCount = 0;
			Segment& segment = (segmentCount < MAX_SEGMENTS ? segments[segmentCount++] : dummySegment);
			segment.optionCount = 0;

			// Skip the '{'.
			++pos;

			// Create the first option.
			segment.options[segment.optionCount].start = pos;
			segment.options[segment.optionCount].length = 0;
			++segment.optionCount;

			// Loop until we find the matching }.
			while (pattern[pos] && pattern[pos] != '}')
			{
				if (pattern[pos] == ',')
				{
					// Skip the ','.
					++pos;

					// Add a new option to the list.
					if (segment.optionCount < Segment::MAX_OPTIONS)
					{
						segment.options[segment.optionCount].start = pos;
						segment.options[segment.optionCount].length = 0;
					}
					++segment.optionCount;
				}
				else
				{
					// Extend the length of the current option.
					if (segment.optionCount <= Segment::MAX_OPTIONS)
						++segment.options[segment.optionCount - 1].length;

					++pos;
				}
			}

			// Skip the '}'.
			++pos;
		}
		else
		{
			// Create a segment with only one option string.
			Segment dummySegment;
			dummySegment.optionCount = 0;
			Segment& segment = (segmentCount < MAX_SEGMENTS ? segments[segmentCount++] : dummySegment);
			Option& option = segment.options[0];
			segment.optionCount = 1;
			option.start = pos;
			option.length = 0;
			while (pattern[pos] && pattern[pos] != '{')
				++pos, ++option.length;
		}
	}

	// Calculate the number of combinations.
	int combinationCount = 1;
	for (int segmentIndex = 0; segmentIndex < segmentCount; ++segmentIndex)
	{
		PREFAST_SUPPRESS_WARNING(6001);
		combinationCount *= segments[segmentIndex].optionCount;
	}

	// Loop through each combination.
	for (int combinationIndex = 0; combinationIndex < combinationCount; ++combinationIndex)
	{
		int outputPos = 0;
		int combinationAccumulator = combinationIndex;

		// Loop through all the segments.
		for (int segmentIndex = 0; segmentIndex < segmentCount; ++segmentIndex)
		{
			// Select the appropriate option from the list.
			const Segment& segment = segments[segmentIndex];
			int optionIndex = combinationAccumulator % segment.optionCount;
			combinationAccumulator /= segment.optionCount;
			const Option& option = segment.options[optionIndex];
			for (int i = 0; i < option.length; ++i)
				expansion[outputPos++] = pattern[option.start + i];
		}

		// Write the null terminator.
		expansion[outputPos] = 0;

		// Call the callback.
		if (enumCallback)
			enumCallback(userData, expansion);
	}
}
