// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#ifndef _CREGameEffect_
#define _CREGameEffect_

#pragma once

//! Interface for game effect render elements.
//! Designed to be instantiated in game code, and called from the CREGameEffect within the engine.
//! This then allows render elements to be created in game code as well as in the engine.
struct IREGameEffect
{
	virtual ~IREGameEffect(){}

	virtual void mfPrepare(bool bCheckOverflow) = 0;
	virtual bool mfDraw(CShader* ef, SShaderPass* sfm, CRenderObject* renderObj) = 0;
};

//! Render element that uses the IREGameEffect interface for its functionality.
class CREGameEffect : public CRenderElement
{
public:

	CREGameEffect();
	~CREGameEffect();

	// CRenderElement interface
	void mfPrepare(bool bCheckOverflow);
	bool mfDraw(CShader* ef, SShaderPass* sfm);

	// CREGameEffect interface
	inline void           SetPrivateImplementation(IREGameEffect* pImpl) { m_pImpl = pImpl; }
	inline IREGameEffect* GetPrivateImplementation() const               { return m_pImpl; }

	virtual void          GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
	}
private:

	IREGameEffect* m_pImpl; //!< Implementation of of render element.

};

#endif // #ifndef _CREGameEffect_
