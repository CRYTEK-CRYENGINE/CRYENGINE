// Copyright 2001-2016 Crytek GmbH. All rights reserved.

#pragma once

#include <CryString/CryString.h>

namespace TextureHelpers
{

//! CreateCryTif tries to convert an image to CryTif.
//! \return Returns path to CryTif file; or empty string, if conversion failed.
string CreateCryTif(const string& filePath, const string& settings = "");

string TranslateFilePath(const string& originalFilePath, const string& importedFilePath, const string& sourceTexturePath);

} // namespace TextureHelpers
