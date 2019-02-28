// Copyright 2015-2018 Crytek GmbH / Crytek Group. All rights reserved. 
#pragma once

#include "../ParticleDataTypes.h"
#include "../ParticleDataStreams.h"
#include "../ParticleComponentRuntime.h"
#include "ParamTraits.h"
#include <CrySerialization/SmartPtr.h>
#include <CrySerialization/IClassFactory.h>

namespace pfx2
{

struct IModifier : public _i_reference_target_t
{
public:
	bool                IsEnabled() const { return m_enabled; }
	virtual EDataDomain GetDomain() const = 0;
	virtual Range       GetMinMax() const = 0;
	virtual void        AddToParam(CParticleComponent* pComponent) {}
	virtual void        Modify(CParticleComponentRuntime& runtime, const SUpdateRange& range, IOFStream stream, TDataType<float> streamType, EDataDomain domain) const {}
	virtual void        Sample(float* samples, uint numSamples) const {}
	virtual void        Serialize(Serialization::IArchive& ar) { ar(m_enabled); }
	virtual IModifier*  VersionFixReplace() const { return nullptr; }
private:
	SEnable m_enabled;
};

struct IFieldModifier: IModifier
{
	virtual IFieldModifier* VersionFixReplace() const { return nullptr; }
};

template<EDataDomain Domain, typename T = SFloat>
class CParamMod
{
public:
	typedef T TValue;
	typedef typename T::TType TType;

	CParamMod(TType defaultValue = TType(1))
		: m_baseValue(defaultValue) {}

	void                           AddToComponent(CParticleComponent* pComponent, CParticleFeature* pFeature);
	void                           AddToComponent(CParticleComponent* pComponent, CParticleFeature* pFeature, TDataType<TType> dataType);
	void                           Serialize(Serialization::IArchive& ar);

	void                           InitParticles(CParticleComponentRuntime& runtime, TDataType<TType> dataType) const;
	void                           ModifyInit(CParticleComponentRuntime& runtime, TIOStream<TType>& stream, SUpdateRange range, TDataType<TType> dataType = TDataType<TType>()) const;

	void                           Update(CParticleComponentRuntime& runtime, TDataType<TType> dataType) const;
	void                           ModifyUpdate(CParticleComponentRuntime& runtime, TIOStream<TType>& stream, SUpdateRange range, TDataType<TType> dataType = TDataType<TType>()) const;

	TRange<TType>                  GetValues(const CParticleComponentRuntime& runtime, TType* data, SUpdateRange range, EDataDomain domain) const;
	TRange<TType>                  GetValues(const CParticleComponentRuntime& runtime, TVarArray<TType> data, EDataDomain domain) const;
	TRange<TType>                  GetValueRange(const CParticleComponentRuntime& runtime) const;
	TRange<TType>                  GetValueRange() const;
	void                           Sample(TType* samples, uint numSamples) const;

	bool                           HasInitModifiers() const   { return !m_modInit.empty(); }
	bool                           HasUpdateModifiers() const { return !m_modUpdate.empty(); }
	bool                           HasModifiers() const       { return !m_modifiers.empty(); }
	TType                          GetBaseValue() const       { return m_baseValue; }
	bool                           IsEnabled() const          { return crymath::valueisfinite(m_baseValue); }

private:
	using TModifier = typename std::conditional<!!(Domain & EDD_HasUpdate), IFieldModifier, IModifier>::type;
	using PModifier = _smart_ptr<TModifier>;

	T                      m_baseValue;
	std::vector<PModifier> m_modifiers;
	std::vector<PModifier> m_modInit;
	std::vector<PModifier> m_modUpdate;
};

template<typename T>
struct STempBuffer: TIStream<T>
{
	template<typename TParamMod>
	STempBuffer(CParticleComponentRuntime& runtime, TParamMod& paramMod)
		: TIStream<T>(nullptr, paramMod.GetBaseValue())
		, m_buffer(runtime.MemHeap())
	{}

protected:

	THeapArray<T> m_buffer;

	void Allocate(SGroupRange range)
	{
		m_buffer.resize(range.size());
		T* data = m_buffer.data() - +*range.begin();
		static_cast<TIStream<T>&>(*this) = TIStream<T>(data);
	}
};

template<typename T>
struct STempInitBuffer: STempBuffer<T>
{
	template<typename TParamMod>
	STempInitBuffer(CParticleComponentRuntime& runtime, TParamMod& paramMod, SUpdateRange range)
		: STempBuffer<T>(runtime, paramMod)
	{
		if (paramMod.HasInitModifiers())
		{
			this->Allocate(range);
			paramMod.ModifyInit(runtime, *this, range);
		}
	}

	template<typename TParamMod>
	STempInitBuffer(CParticleComponentRuntime& runtime, TParamMod& paramMod)
		: STempInitBuffer<T>(runtime, paramMod, runtime.SpawnedRange())
	{}
};

template<typename T>
struct STempUpdateBuffer: STempBuffer<T>
{
	template<typename TParamMod>
	STempUpdateBuffer(CParticleComponentRuntime& runtime, TParamMod& paramMod, SUpdateRange range)
		: STempBuffer<T>(runtime, paramMod)
	{
		if (paramMod.HasModifiers())
		{
			this->Allocate(range);
			paramMod.ModifyInit(runtime, *this, range);
			paramMod.ModifyUpdate(runtime, *this, range);
		}
	}
};



}

#include "ParamModImpl.h"

