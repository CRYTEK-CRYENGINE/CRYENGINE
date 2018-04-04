// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#pragma once

class CRELensOptics : public CRenderElement
{
public:
	CRELensOptics(void);
	~CRELensOptics(void) {}

	virtual bool mfCompile(CParserBin& Parser, SParserFrame& Frame) { return true; }
	virtual void mfPrepare(bool bCheckOverflow) {};
	virtual bool mfDraw(CShader* ef, SShaderPass* sfm) { return true; }
	virtual void mfExport(struct SShaderSerializeContext& SC)                 {};
	virtual void mfImport(struct SShaderSerializeContext& SC, uint32& offset) {};
};
