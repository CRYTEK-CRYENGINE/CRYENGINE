// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AbstractNodeGraphViewModel.h"

namespace CryGraphEditor {

CNodeGraphViewModel::~CNodeGraphViewModel()
{
	SignalDestruction();
}

}

