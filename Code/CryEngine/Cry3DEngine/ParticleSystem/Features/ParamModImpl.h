// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved. 

#include <CrySerialization/SmartPtr.h>
#include "ParamMod.h"

namespace pfx2
{

ILINE void IModifier::Serialize(Serialization::IArchive& ar)
{
	ar(m_enabled);
}

template<EDataDomain Domain, typename T>
CParamMod<Domain, T>::CParamMod(TType defaultValue)
	: m_baseValue(defaultValue)
{
}

template<EDataDomain Domain, typename T>
void CParamMod<Domain, T >::AddToComponent(CParticleComponent* pComponent, CParticleFeature* pFeature)
{
	m_modInit.clear();
	m_modUpdate.clear();
	stl::find_and_erase_all(m_modifiers, nullptr);

	if (Domain & EDD_PerParticle)
		pComponent->InitParticles.add(pFeature);

	for (auto& pModifier : m_modifiers)
	{
		if (pModifier && pModifier->IsEnabled())
			pModifier->AddToParam(pComponent, this);
	}
}

template<EDataDomain Domain, typename T>
void CParamMod<Domain, T >::AddToComponent(CParticleComponent* pComponent, CParticleFeature* pFeature, TDataType<TType> dataType)
{
	AddToComponent(pComponent, pFeature);
	pComponent->AddParticleData(dataType);

	if (dataType.info().domain & EDD_HasUpdate && !m_modUpdate.empty())
	{
		pComponent->AddParticleData(dataType.InitType());
		if (Domain & EDD_PerParticle)
			pComponent->UpdateParticles.add(pFeature);
	}
}

template<EDataDomain Domain, typename T>
void CParamMod<Domain, T >::Serialize(Serialization::IArchive& ar)
{
	EDataDomain domain = Domain;
	Serialization::SContext _modContext(ar, static_cast<EDataDomain*>(&domain));
	ar(m_baseValue, "value", "^");
	ar(SkipEmpty(m_modifiers), "modifiers", "^");

	if (ar.isInput())
	{
		for (auto& pMod : m_modifiers)
		{
			if (!pMod)
				continue;
			if (TModifier* pNewMod = pMod->VersionFixReplace())
			{
				pMod.reset(pNewMod);
			}
		}
	}
}

template<EDataDomain Domain, typename T>
void CParamMod<Domain, T >::InitParticles(CParticleComponentRuntime& runtime, TDataType<TType> dataType) const
{
	CRY_PFX2_PROFILE_DETAIL;

	CParticleContainer& container = runtime.GetContainer();
	if (container.GetMaxParticles() == 0 || !container.HasData(dataType))
		return;

	TIOStream<TType> stream = container.IOStream(dataType);
	ModifyInit(runtime, stream, runtime.SpawnedRange(), dataType);

	if (dataType.info().domain & EDD_HasUpdate)
		container.CopyData(dataType.InitType(), dataType, runtime.SpawnedRange());
}

template<EDataDomain Domain, typename T>
void CParamMod<Domain, T >::Update(CParticleComponentRuntime& runtime, TDataType<TType> dataType) const
{
	CParticleContainer& container = runtime.GetContainer();
	if (container.GetMaxParticles() == 0 || !container.HasData(dataType))
		return;

	CRY_PFX2_ASSERT(runtime.FullRange().m_end <= container.GetNumParticles());
	TIOStream<TType> stream = container.IOStream(dataType);
	ModifyUpdate(runtime, stream, runtime.FullRange(), dataType);
}

template<EDataDomain Domain, typename T>
void CParamMod<Domain, T >::ModifyInit(CParticleComponentRuntime& runtime, TIOStream<TType>& stream, SUpdateRange range, TDataType<TType> dataType) const
{
	CRY_PFX2_PROFILE_DETAIL;
	stream.Fill(range, m_baseValue);

	for (auto& pModifier : m_modInit)
		pModifier->Modify(runtime, range, stream, dataType, Domain);
}

template<EDataDomain Domain, typename T>
void CParamMod<Domain, T >::ModifyUpdate(CParticleComponentRuntime& runtime, TIOStream<TType>& stream, SUpdateRange range, TDataType<TType> dataType) const
{
	CRY_PFX2_PROFILE_DETAIL;
	for (auto& pModifier : m_modUpdate)
		pModifier->Modify(runtime, range, stream, dataType, Domain);
}

template<EDataDomain Domain, typename T>
TRange<typename T::TType> CParamMod<Domain, T >::GetValues(const CParticleComponentRuntime& runtime, TType* data, SUpdateRange range, EDataDomain domain, bool updating) const
{
	TRange<TType> minmax(1);
	TIOStream<TType> stream(data);

	stream.Fill(range, m_baseValue);

	for (auto & pMod : m_modInit)
	{
		if (pMod->GetDomain() & domain)
			pMod->Modify(non_const(runtime), range, stream, TDataType<TType>(), domain);
		else
			minmax = minmax * pMod->GetMinMax();
	}
	for (auto& pMod : m_modUpdate)
	{
		if (updating && pMod->GetDomain() & domain)
			pMod->Modify(non_const(runtime), range, stream, TDataType<TType>(), domain);
		else
			minmax = minmax * pMod->GetMinMax();
	}
	return minmax;
}

template<EDataDomain Domain, typename T>
TRange<typename T::TType> CParamMod<Domain, T >::GetValues(const CParticleComponentRuntime& runtime, TVarArray<TType> data, EDataDomain domain, bool updating) const
{
	return GetValues(runtime, data.data(), SUpdateRange(0, data.size()), domain, updating);
}

template<EDataDomain Domain, typename T>
TRange<typename T::TType> CParamMod<Domain, T >::GetValueRange(const CParticleComponentRuntime& runtime) const
{
	floatv curValue;
	auto minmax = GetValues(runtime, (float*)&curValue, SUpdateRange(0, 1), EDD_None, true);
	return minmax * (*(float*)&curValue);
}

template<EDataDomain Domain, typename T>
TRange<typename T::TType> CParamMod<Domain, T >::GetValueRange() const
{
	TRange<TType> minmax(m_baseValue);
	for (auto& pMod : m_modInit)
		minmax = minmax * pMod->GetMinMax();
	for (auto& pMod : m_modUpdate)
		minmax = minmax * pMod->GetMinMax();
	return minmax;
}

template<EDataDomain Domain, typename T>
void pfx2::CParamMod<Domain, T >::Sample(TType* samples, int numSamples) const
{
	TIOStream<TType> stream(samples);
	stream.Fill(SUpdateRange(0, numSamples), m_baseValue);

	for (auto& pModifier : m_modifiers)
	{
		if (pModifier->IsEnabled())
			pModifier->Sample(samples, numSamples);
	}
}

}
