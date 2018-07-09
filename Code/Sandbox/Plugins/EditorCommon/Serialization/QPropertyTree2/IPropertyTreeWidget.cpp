// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "IPropertyTreeWidget.h"

PROPERTY_TREE_API CPropertyTreeWidgetFactory& GetPropertyTreeWidgetFactory()
{
	static CPropertyTreeWidgetFactory sFactory;
	return sFactory;
}

