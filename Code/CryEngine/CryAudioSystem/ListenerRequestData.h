// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "RequestData.h"
#include "Common/PoolObject.h"
#include <CryAudio/IAudioSystem.h>

namespace CryAudio
{
class CListener;

enum class EListenerRequestType : EnumFlagsType
{
	None,
	SetTransformation,
#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	SetName,
#endif // CRY_AUDIO_USE_DEBUG_CODE
};

//////////////////////////////////////////////////////////////////////////
struct SListenerRequestDataBase : public SRequestData
{
	explicit SListenerRequestDataBase(EListenerRequestType const listenerRequestType_)
		: SRequestData(ERequestType::ListenerRequest)
		, listenerRequestType(listenerRequestType_)
	{}

	virtual ~SListenerRequestDataBase() override = default;

	EListenerRequestType const listenerRequestType;
};

//////////////////////////////////////////////////////////////////////////
template<EListenerRequestType T>
struct SListenerRequestData final : public SListenerRequestDataBase
{
	SListenerRequestData()
		: SListenerRequestDataBase(T)
	{}

	explicit SListenerRequestData(SListenerRequestData<T> const* const pLRData)
		: SListenerRequestDataBase(T)
	{}

	virtual ~SListenerRequestData() override = default;
};

//////////////////////////////////////////////////////////////////////////
template<>
struct SListenerRequestData<EListenerRequestType::SetTransformation> final : public SListenerRequestDataBase, public CPoolObject<SListenerRequestData<EListenerRequestType::SetTransformation>, stl::PSyncMultiThread>
{
	explicit SListenerRequestData(
		CTransformation const& transformation_,
		CListener* const pListener_)
		: SListenerRequestDataBase(EListenerRequestType::SetTransformation)
		, transformation(transformation_)
		, pListener(pListener_)
	{}

	explicit SListenerRequestData(SListenerRequestData<EListenerRequestType::SetTransformation> const* const pLRData)
		: SListenerRequestDataBase(EListenerRequestType::SetTransformation)
		, transformation(pLRData->transformation)
		, pListener(pLRData->pListener)
	{}

	virtual ~SListenerRequestData() override = default;

	CTransformation const transformation;
	CListener* const      pListener;
};

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
//////////////////////////////////////////////////////////////////////////
template<>
struct SListenerRequestData<EListenerRequestType::SetName> final : public SListenerRequestDataBase
{
	explicit SListenerRequestData(char const* const szName, CListener* const pListener_)
		: SListenerRequestDataBase(EListenerRequestType::SetName)
		, pListener(pListener_)
		, name(szName)
	{}

	explicit SListenerRequestData(SListenerRequestData<EListenerRequestType::SetName> const* const pLRData)
		: SListenerRequestDataBase(EListenerRequestType::SetName)
		, pListener(pLRData->pListener)
		, name(pLRData->name)
	{}

	virtual ~SListenerRequestData() override = default;

	CListener* const                           pListener;
	CryFixedStringT<MaxObjectNameLength> const name;
};
#endif // CRY_AUDIO_USE_DEBUG_CODE
}      // namespace CryAudio
