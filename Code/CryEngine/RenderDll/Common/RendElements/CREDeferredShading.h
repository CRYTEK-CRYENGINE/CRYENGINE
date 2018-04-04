// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#ifndef __CREDEFERREDSHADING_H__
#define __CREDEFERREDSHADING_H__

class CREDeferredShading : public CRenderElement
{
	friend class CD3D9Renderer;
	friend class CGLRenderer;

public:

	// constructor/destructor
	CREDeferredShading();

	virtual ~CREDeferredShading();

	// prepare screen processing
	virtual void mfPrepare(bool bCheckOverflow);
	// render screen processing
	virtual bool mfDraw(CShader* ef, SShaderPass* sfm);

	// begin screen processing
	virtual void mfActivate(int iProcess);
	// reset
	virtual void mfReset(void);

	virtual void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
	}
};

#endif
