// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "Controls/EditorDialog.h"

#include "Terrain/Ui/GenerateTerrainTextureUi.h"

class CResizeTerrainTextureDialog
	: public CEditorDialog
{
public:
	typedef CGenerateTerrainTextureUi::Result Result;

	CResizeTerrainTextureDialog(const Result& initial);

	Result GetResult() const;

private:
	CGenerateTerrainTextureUi* m_pTexture;
};

