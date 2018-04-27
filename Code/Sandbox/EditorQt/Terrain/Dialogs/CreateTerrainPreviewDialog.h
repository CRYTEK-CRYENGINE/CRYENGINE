// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "Controls/EditorDialog.h"

#include "Terrain/Ui/TerrainTextureDimensionsUi.h"

class CCreateTerrainPreviewDialog
	: public CEditorDialog
{
public:
	typedef CTerrainTextureDimensionsUi::Result Result;

	CCreateTerrainPreviewDialog(const Result& initial);

	Result GetResult() const;

private:
	CTerrainTextureDimensionsUi* m_pDimensions;
};

