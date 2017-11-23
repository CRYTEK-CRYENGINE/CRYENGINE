// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GenericWidgetImpl.h"

namespace Cry {
namespace SchematycEd {

CGenericWidget* CGenericWidget::Create()
{
	return new CGenericWidgetImpl();
}

} //namespace SchematycEd
} //namespace Cry
