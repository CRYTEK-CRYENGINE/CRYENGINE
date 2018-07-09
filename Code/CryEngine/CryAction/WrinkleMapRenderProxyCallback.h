// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __WrinkleMapRenderProxyCallback_H__
#define __WrinkleMapRenderProxyCallback_H__

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CryRenderer/IShaderParamCallback.h>
#include <CryExtension/ClassWeaver.h>

class CWrinkleMapShaderParamCallback : public IShaderParamCallback
{
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(IShaderParamCallback)
	CRYINTERFACE_END()

	CRYGENERATE_CLASS_GUID(CWrinkleMapShaderParamCallback, "WrinkleMapShaderParamCallback", "68c7f0e0-c364-46fe-82a3-bc01b54dc7bf"_cry_guid)

	CWrinkleMapShaderParamCallback();
	virtual ~CWrinkleMapShaderParamCallback();

public:

	//////////////////////////////////////////////////////////////////////////
	//	Implement IShaderParamCallback
	//////////////////////////////////////////////////////////////////////////

	virtual void SetCharacterInstance(ICharacterInstance* pCharInstance) override
	{
		m_pCharacterInstance = pCharInstance;
	}

	virtual ICharacterInstance* GetCharacterInstance() const override
	{
		return m_pCharacterInstance;
	}

	virtual bool Init() override;
	virtual void SetupShaderParams(IShaderPublicParams* pParams, IMaterial* pMaterial) override;
	virtual void Disable(IShaderPublicParams* pParams) override;

protected:

	void SetupBoneWrinkleMapInfo();

	//////////////////////////////////////////////////////////////////////////

	ICharacterInstance* m_pCharacterInstance;

	struct SWrinkleBoneInfo
	{
		int16 m_nChannelID;
		int16 m_nJointID;
	};
	typedef std::vector<SWrinkleBoneInfo> TWrinkleBoneInfo;
	TWrinkleBoneInfo m_WrinkleBoneInfo;

	IMaterial*       m_pCachedMaterial;

	uint8            m_eSemantic[3];

	bool             m_bWrinklesEnabled;
};

class CWrinkleMapShaderParamCallbackUI : public CWrinkleMapShaderParamCallback
{
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(CWrinkleMapShaderParamCallback)
	CRYINTERFACE_END()
		
	CRYGENERATE_CLASS_GUID(CWrinkleMapShaderParamCallbackUI, "bWrinkleMap", "1b9d4692-5918-485b-b731-2c8fb3f5b763"_cry_guid)
};

#endif //__WrinkleMapRenderProxyCallback_H__
