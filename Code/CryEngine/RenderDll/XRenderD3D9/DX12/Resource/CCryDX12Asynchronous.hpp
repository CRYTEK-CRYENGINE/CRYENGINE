// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "DX12/Device/CCryDX12DeviceChild.hpp"

template<typename T>
class CCryDX12Asynchronous : public CCryDX12DeviceChild<T>
{
public:
	DX12_OBJECT(CCryDX12Asynchronous, CCryDX12DeviceChild<T> );

	#pragma region /* ID3D11Asynchronous implementation */

	VIRTUALGFX UINT STDMETHODCALLTYPE GetDataSize()
	{
		return 0;
	}

	#pragma endregion

protected:
	CCryDX12Asynchronous()
		: Super(nullptr, nullptr)
	{

	}

private:

};
