// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace Base64
{
size_t DecodedSize(const void* b64_buf, size_t b64_buf_size);
bool   DecodeBuffer(const void* b64_buf, size_t b64_buf_size, void* orig_buf);
}
