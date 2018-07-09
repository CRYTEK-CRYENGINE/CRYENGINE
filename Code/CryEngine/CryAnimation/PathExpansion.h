// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace PathExpansion
{
// Expand patterns into paths. An example of a pattern before expansion:
// animations/facial/idle_{0,1,2,3}.fsq
// {} is used to specify options for parts of the string.
// output must point to buffer that is at least as large as the pattern.
void SelectRandomPathExpansion(const char* pattern, char* output);
void EnumeratePathExpansions(const char* pattern, void (* enumCallback)(void* userData, const char* expansion), void* userData);
}
