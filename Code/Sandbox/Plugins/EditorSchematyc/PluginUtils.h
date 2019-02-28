// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc/Utils/IString.h>

namespace CrySchematycEditor {
namespace Utils {

void     ConstructAbsolutePath(Schematyc::IString& output, const char* szFileName);

bool     WriteToClipboard(const char* szText, const char* szPrefix = nullptr);
bool     ReadFromClipboard(string& text, const char* szPrefix = nullptr);
bool     ValidateClipboardContents(const char* szPrefix);

}
}

