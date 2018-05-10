// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved. 

#include <CrySerialization/SmartPtr.h>
#include "ParamMod.h"

namespace pfx2
{

ILINE void IModifier::Serialize(Serialization::IArchive& ar)
{
	ar(m_enabled);
}

template<typename TParamModContext, typename T>
CParamMod<TParamModContext, T>::CParamMod(TType defaultValue)
	: m_baseValue(defaultValue)
{
}

template<typename TParamModContext, typename T>
void CParamMod<TParamModContext, T >::AddToComponent(CParticleComponent* pComponent, CParticleFeature* pFeature)
{
	m_modInit.clear();
	m_modUpdate.clear();
	stl::find_and_erase_all(m_modifiers, nullptr);

	if (Context().GetDomain() == EMD_PerParticle)
		pComponent->InitParticles.add(pFeature);

	for (auto& pModifier : m_modifiers)
	{
		if (pModifier && pModifier->IsEnabled())
			pModifier->AddToParam(pComponent, this);
	}
}

template<typename TParamModContext, typename T>
void CParamMod<TParamModContext, T >::AddToComponent(CParticleComponent* pComponent, CParticleFeature* pFeature, TDataType<TType> dataType)
{
	AddToComponent(pComponent, pFeature);
	pComponent->AddParticleData(dataType);

	if (dataType.info().hasInit && !m_modUpdate.empty())
	{
		pComponent->AddParticleData(dataType.InitType());
		if (Context().GetDomain() == EMD_PerParticle)
			pComponent->UpdateParticles.add(pFeature);
	}
}

template<typename TParamModContext, typename T>
void CParamMod<TParamModContext, T >::Serialize(Serialization::IArchive& ar)
{
	TParamModContext modContext;
	Serialization::SContext _modContext(ar, static_cast<IParamModContext*>(&modContext));
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

template<typename TParamModContext, typename T>
void CParamMod<TParamModContext, T >::InitParticles(const SUpdateContext& context, TDataType<TType> dataType) const
{
	CRY_PFX2_PROFILE_DETAIL;

	CParticleContainer& container = context.m_container;
	if (container.GetMaxParticles() == 0 || !container.HasData(dataType))
		return;

	TIOStream<TType> stream = container.IOStream(dataType);
	ModifyInit(context, stream, context.GetSpawnedRange(), dataType);

	if (dataType.info().hasInit)
		container.CopyData(dataType.InitType(), dataType, context.GetSpawnedRange());
}

template<typename TParamModContext, typename T>
void CParamMod<TParamModContext, T >::Update(const SUpdateContext& context, TDataType<TType> dataType) const
{
	CParticleContainer& container = context.m_container;
	if (container.GetMaxParticles() == 0 || !container.HasData(dataType))
		return;

	CRY_PFX2_ASSERT(context.GetUpdateRange().m_end <= container.GetNumParticles());
	TIOStream<TType> stream = container.IOStream(dataType);
	ModifyUpdate(context, stream, context.GetUpdateRange(), dataType);
}

template<typename TParamModContext, typename T>
void CParamMod<TParamModContext, T >::ModifyInit(const SUpdateContext& context, TIOStream<TType>& stream, SUpdateRange range, TDataType<TType> dataType) const
{
	CRY_PFX2_PROFILE_DETAIL;
	stream.Fill(range, m_baseValue);

	const EModDomain domain = Context().GetDomain();
	for (auto& pModifier : m_modInit)
		pModifier->Modify(context, range, stream, dataType, domain);
}

template<typename TParamModContext, typename T>
void CParamMod<TParamModContext, T >::ModifyUpdate(const SUpdateContext& context, TIOStream<TType>& stream, SUpdateRange range, TDataType<TType> dataType) const
{
	CRY_PFX2_PROFILE_DETAIL;
	const EModDomain domain = Context().GetDomain();
	for (auto& pModifier : m_modUpdate)
		pModifier->Modify(context, range, stream, dataType, domain);
}

template<typename TParamModContext, typename T>
TRange<typename T::TType> CParamMod<TParamModContext, T >::GetValues(const SUpdateContext& context, TType* data, SUpdateRange range, EModDomain domain, bool updating) const
{
	TRange<TType> minmax(1);
	TIOStream<TType> stream(data);

	stream.Fill(range, m_baseValue);

	for (auto & pMod : m_modInit)
	{
		if (pMod->GetDomain() >= domain)
			pMod->Modify(context, range, stream, TDataType<TType>(), domain);
		else
			minmax = minmax * pMod->GetMinMax();
	}
	for (auto& pMod : m_modUpdate)
	{
		if (updating && pMod->GetDomain() >= domain)
			pMod->Modify(context, range, stream, TDataType<TType>(), domain);
		else
			minmax = minmax * pMod->GetMinMax();
	}
	return minmax;
}

template<typename TParamModContext, typename T>
TRange<typename T::TType> CParamMod<TParamModContext, T >::GetValues(const SUpdateContext& context, TVarArray<TType> data, EModDomain domain, bool updating) const
{
	return GetValues(context, data.data(), SUpdateRange(0, data.size()), domain, updating);
}

template<typename TParamModContext, typename T>
void CParamMod<TParamModContext, T >::AddToInitParticles(IModifier* pMod)
{
	if (std::find(m_modInit.begin(), m_modInit.end(), pMod) == m_modInit.end())
		m_modInit.push_back(pMod);
}

template<typename TParamModContext, typename T>
void CParamMod<TParamModContext, T >::AddToUpdate(IModifier* pMod)
{
	if (std::find(m_modUpdate.begin(), m_modUpdate.end(), pMod) == m_modUpdate.end())
		m_modUpdate.push_back(pMod);
}

template<typename TParamModContext, typename T>
TRange<typename T::TType> CParamMod<TParamModContext, T >::GetValueRange(const SUpdateContext& context) const
{
	floatv curValue;
	auto minmax = GetValues(context, (float*)&curValue, SUpdateRange(0, 1), EMD_PerEffect, true);
	return minmax * (*(float*)&curValue);
}

template<typename TParamModContext, typename T>
TRange<typename T::TType> CParamMod<TParamModContext, T >::GetValueRange() const
{
	TRange<TType> minmax(m_baseValue);
	for (auto& pMod : m_modInit)
		minmax = minmax * pMod->GetMinMax();
	for (auto& pMod : m_modUpdate)
		minmax = minmax * pMod->GetMinMax();
	return minmax;
}

template<typename TParamModContext, typename T>
void pfx2::CParamMod<TParamModContext, T >::Sample(TType* samples, int numSamples) const
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
