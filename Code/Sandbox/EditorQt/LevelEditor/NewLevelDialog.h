// Copyright 2001-2015 Crytek GmbH. All rights reserved.
#pragma once

#include "Controls/EditorDialog.h"

#include "LevelAssetType.h"

#include <memory>

class CNewLevelDialog
	: public CEditorDialog
{
public:
	explicit CNewLevelDialog();
	~CNewLevelDialog();

	CLevelType::SCreateParams GetResult() const;

private:
	struct Implementation;
	std::unique_ptr<Implementation> p;
};

