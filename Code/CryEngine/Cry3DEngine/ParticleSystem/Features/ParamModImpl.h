// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <CrySerialization/SmartPtr.h>
#include "ParamMod.h"

namespace pfx2
{

template<EDataDomain Domain, typename T>
void CParamMod<Domain, T>::AddToComponent(CParticleComponent* pComponent, CParticleFeature* pFeature)
{
	if (Domain & EDD_Particle)
		pComponent->InitParticles.add(pFeature);

	for (auto& pMod : m_modifiers)
	{
		if (pMod && pMod->IsEnabled())
			pMod->AddToParam(pComponent);
	}
}

template<EDataDomain Domain, typename T>
void CParamMod<Domain, T>::AddToComponent(CParticleComponent* pComponent, CParticleFeature* pFeature, ThisDataType dataType)
{
	AddToComponent(pComponent, pFeature);
	pComponent->AddParticleData(dataType);

	for (auto& pMod : m_modifiers)
	{
		if (pMod && pMod->IsEnabled())
			pMod->SetDataType(dataType);
	}

	if (Domain & EDD_HasUpdate && m_updateMask)
	{
		pComponent->AddParticleData(dataType.InitType());
		if (Domain & EDD_Particle)
			pComponent->UpdateParticles.add(pFeature);
	}
}

template<EDataDomain Domain, typename T>
void CParamMod<Domain, T>::SerializeImpl(Serialization::IArchive& ar)
{
	EDataDomain domain = Domain;
	Serialization::SContext _modContext(ar, static_cast<EDataDomain*>(&domain));
	ar(m_baseValue, T::ValueName(), "^");
	ar(SkipEmpty(m_modifiers), T::ModsName(), "^");

	if (ar.isInput())
	{
		m_initMask = m_updateMask = 0;
		stl::find_and_erase_all(m_modifiers, nullptr);
		m_modifiers.shrink_to_fit();
		uint16 mask = 1;
		for (auto& pMod : m_modifiers)
		{
			if (TModifier* pNewMod = pMod->VersionFixReplace())
			{
				pMod.reset(pNewMod);
			}
			if (pMod->IsEnabled())
			{
				if (pMod->GetDomain() & Domain & EDD_HasUpdate)
					m_updateMask |= mask;
				else
					m_initMask |= mask;
			}
			mask <<= 1;
			assert(mask != 0);
		}
	}
}

template<EDataDomain Domain, typename T>
void CParamMod<Domain, T>::Init(CParticleComponentRuntime& runtime, ThisDataType dataType, SUpdateRange range) const
{
	CParticleContainer& container = runtime.Container(Domain);
	if (!range.size() || !container.HasData(dataType))
		return;

	TIOStream<TType> stream = container.IOStream(dataType);
	ModifyInit(runtime, stream, range);

	if (Domain & EDD_HasUpdate)
		container.CopyData(dataType.InitType(), dataType, range);
}

template<EDataDomain Domain, typename T>
void CParamMod<Domain, T>::Update(CParticleComponentRuntime& runtime, ThisDataType dataType, SUpdateRange range) const
{
	if (!m_updateMask)
		return;
	CParticleContainer& container = runtime.Container(Domain);
	if (!range.size() || !container.HasData(dataType))
		return;

	runtime.Container(Domain).CopyData(dataType, dataType.InitType(), range);
	TIOStream<TType> stream = container.IOStream(dataType);
	ModifyUpdate(runtime, stream, range);
}

template<EDataDomain Domain, typename T>
void CParamMod<Domain, T>::ModifyInit(const CParticleComponentRuntime& runtime, TIOStream<TType> stream, SUpdateRange range) const
{
	stream.Fill(range, m_baseValue);

	for (uint16 mask = m_initMask, index = 0; mask; mask >>= 1, index++)
	{
		if (mask & 1)
			m_modifiers[index]->Modify(runtime, range, stream, Domain);
	}
}

template<EDataDomain Domain, typename T>
void CParamMod<Domain, T>::ModifyUpdate(const CParticleComponentRuntime& runtime, TIOStream<TType> stream, SUpdateRange range) const
{
	for (uint16 mask = m_updateMask, index = 0; mask; mask >>= 1, index++)
	{
		if (mask & 1)
			m_modifiers[index]->Modify(runtime, range, stream, Domain);
	}
}

template<EDataDomain Domain, typename T>
auto CParamMod<Domain, T>::GetValues(const CParticleComponentRuntime& runtime, TVarArray<TType> data, EDataDomain domain) const -> TRange<TFrom>
{
	TRange<TType> minmax(1);
	TIOStream<TType> stream(data.data());
	SUpdateRange range(0, data.size());

	data.fill(m_baseValue);

	for (auto& pMod : m_modifiers)
	{
		if (domain >= (min(pMod->GetDomain(), Domain) & ~EDD_HasUpdate))
			pMod->Modify(runtime, range, stream, domain);
		else
			minmax = minmax * pMod->GetMinMax();
	}
	return minmax;
}

template<EDataDomain Domain, typename T>
auto CParamMod<Domain, T>::GetValueRange(const CParticleComponentRuntime& runtime) const -> TRange<TFrom>
{
	floatv curValueV = convert<floatv>(0.0f);
	float& curValue = (float&)curValueV;
	auto minmax = GetValues(runtime, TVarArray<float>(&curValue, 1), EDD_None);
	return minmax * curValue;
}

template<EDataDomain Domain, typename T>
auto CParamMod<Domain, T>::GetValueRange() const -> TRange<TFrom>
{
	TRange<TType> minmax(m_baseValue);
	for (auto& pMod : m_modifiers)
		minmax = minmax * pMod->GetMinMax();
	return minmax;
}

template<EDataDomain Domain, typename T>
void pfx2::CParamMod<Domain, T>::Sample(TVarArray<TFrom> samples) const
{
	samples.fill(T::From(m_baseValue));
	for (auto& pMod : m_modifiers)
	{
		if (pMod->IsEnabled())
			pMod->Sample(samples);
	}
}

}
