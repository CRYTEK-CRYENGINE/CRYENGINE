// Copyright 2001-2016 Crytek GmbH. All rights reserved.

#pragma once

namespace AsseThumbnailsGenerator
{

void GenerateThumbnailsAsync(const string& folder, const std::function<void()>& finalize = std::function<void()>());

};
