// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __FLOWSERIALIZE_H__
#define __FLOWSERIALIZE_H__

#pragma once

#include <CryFlowGraph/IFlowSystem.h>

bool   SetFromString(TFlowInputData& value, const char* str);
string ConvertToString(const TFlowInputData& value);
bool   SetAttr(XmlNodeRef node, const char* attr, const TFlowInputData& value);

#endif
