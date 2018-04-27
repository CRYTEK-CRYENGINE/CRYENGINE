// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include <map>
#include <vector>

#include <CrySerialization/yasli/ClassFactory.h>
#include <CrySerialization/yasli/TypeID.h>
#include <CrySerialization/IClassFactory.h>

namespace Serialization
{
using yasli::ClassFactory;
using yasli::TypeDescription;
}

#define SERIALIZATION_CLASS_NULL(BaseType, name)                                                           YASLI_CLASS_NULL(BaseType, name)
#define SERIALIZATION_CLASS_NAME(BaseType, Type, name, label)                                              YASLI_CLASS_NAME(BaseType, Type, name, label)
#define SERIALIZATION_CLASS_NAME_FOR_FACTORY(Factory, BaseType, Type, name, label)                         YASLI_CLASS_NAME_FOR_FACTORY(Factory, BaseType, Type, name, label)
#define SERIALIZATION_CLASS_ANNOTATION(BaseType, Type, attributeName, attributeValue)                      YASLI_CLASS_ANNOTATION(BaseType, Type, attributeName, attributeValue)
#define SERIALIZATION_CLASS_ANNOTATION_FOR_FACTORY(factory, BaseType, Type, attributeName, attributeValue) YASLI_CLASS_ANNOTATION_FOR_FACTORY(factory, BaseType, Type, attributeName, attributeValue)
