// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace AssetLoader
{

struct IFileReader;

std::unique_ptr<IFileReader> CreateCompletionPortFileReader();

}
