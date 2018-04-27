// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

class CRELensOptics : public CRenderElement
{
public:
	CRELensOptics(void);
	~CRELensOptics(void) {}

	virtual void mfExport(struct SShaderSerializeContext& SC)                 {};
	virtual void mfImport(struct SShaderSerializeContext& SC, uint32& offset) {};
};
