// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

class CChunkFile;

namespace CgfUtil
{

// Write to a temporary file then rename it to prevent external file readers from reading half-written file.
bool WriteTempRename(CChunkFile& chunkFile, const string& targetFilename);

};


