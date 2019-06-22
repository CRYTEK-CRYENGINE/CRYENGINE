// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "../ParticleDataTypes.h"
#include "../ParticleDataStreams.h"
#include "../ParticleComponentRuntime.h"
#include "ParamTraits.h"
#include <CrySerialization/SmartPtr.h>
#include <CrySerialization/IClassFactory.h>
#include <cmath>

namespace pfx2
{

struct IModifier
{
public:
	bool                IsEnabled() const { return m_enabled; }
	virtual EDataDomain GetDomain() const = 0;
	virtual void        AddToParam(CParticleComponent* pComponent) {}
	virtual void        SetDataType(EParticleDataType dataType) {}
	virtual void        Serialize(Serialization::IArchive& ar) { ar(m_enabled); }
private:
	SEnable m_enabled;
};

template<typename T, typename TFrom = T>
struct ITypeModifier: IModifier
{
	virtual void           Modify(const CParticleComponentRuntime& runtime, const SUpdateRange& range, TIOStream<T> stream, EDataDomain domain) const = 0;
	virtual void           Sample(TVarArray<TFrom> samples) const {}
	virtual TRange<TFrom>  GetMinMax() const { return TRange<TFrom>(TFrom(0), TFrom(1)); }
	virtual ITypeModifier* VersionFixReplace() const { return nullptr; }
};

template<typename T, typename TFrom = T>
struct IFieldModifier: ITypeModifier<T, TFrom>
{
	virtual IFieldModifier* VersionFixReplace() const { return nullptr; }
};

struct IParamMod
{
	void Serialize(Serialization::IArchive& ar)
	{
		CRY_PFX2_PROFILE_DETAIL;
		SerializeImpl(ar);
	}
	virtual void SerializeImpl(Serialization::IArchive& ar) = 0;
};

template<EDataDomain Domain, typename T = SFloat>
class CParamMod: public IParamMod
{
public:
	typedef T TValue;
	typedef typename T::TStore TType;
	typedef typename T::TType TFrom;
	typedef TDataType<TType> ThisDataType;

	using TModifier = typename std::conditional<!!(Domain & EDD_HasUpdate),
		IFieldModifier<TType, TFrom>, 
		ITypeModifier<TType, TFrom>
	>::type;

	CParamMod(TFrom defaultValue = TFrom(1))
		: m_baseValue(defaultValue) {}

	void          AddToComponent(CParticleComponent* pComponent, CParticleFeature* pFeature);
	void          AddToComponent(CParticleComponent* pComponent, CParticleFeature* pFeature, ThisDataType dataType);
	void          SerializeImpl(Serialization::IArchive& ar) override;

	void          Init(CParticleComponentRuntime& runtime, ThisDataType dataType) const { Init(runtime, dataType, runtime.SpawnedRange(Domain)); }
	void          Init(CParticleComponentRuntime& runtime, ThisDataType dataType, SUpdateRange range) const;
	void          ModifyInit(const CParticleComponentRuntime& runtime, TIOStream<TType> stream, SUpdateRange range) const;

	void          Update(CParticleComponentRuntime& runtime, ThisDataType dataType) const { Update(runtime, dataType, runtime.FullRange(Domain)); }
	void          Update(CParticleComponentRuntime& runtime, ThisDataType dataType, SUpdateRange range) const;
	void          ModifyUpdate(const CParticleComponentRuntime& runtime, TIOStream<TType> stream, SUpdateRange range) const;

	TRange<TFrom> GetValues(const CParticleComponentRuntime& runtime, TVarArray<TType> data, EDataDomain domain) const;
	TRange<TFrom> GetValueRange(const CParticleComponentRuntime& runtime) const;
	TRange<TFrom> GetValueRange() const;
	void          Sample(TVarArray<TFrom> samples) const;

	uint16        HasInitModifiers() const   { return m_initMask; }
	uint16        HasUpdateModifiers() const { return m_updateMask; }
	uint16        HasModifiers() const       { return m_initMask | m_updateMask; }
	TType         GetBaseValue() const       { return m_baseValue; }
	bool          IsEnabled() const          { return crymath::valueisfinite(m_baseValue); }

protected:
	T                      m_baseValue;
	uint16                 m_initMask = 0, m_updateMask = 0;
	DynArray<std::unique_ptr<TModifier>, uint> m_modifiers;
};

//////////////////////////////////////////////////////////////////////////
// Copy factory creators from base class to derived class
template<typename BaseType, typename Type>
struct ClassFactoryInheritor
{
	using BaseFactory = Serialization::ClassFactory<BaseType>;
	using TypeFactory = Serialization::ClassFactory<Type>;

	ClassFactoryInheritor()
	{
		auto creators = BaseFactory::the().creatorChain();
		TypeFactory::the().registerChain(reinterpret_cast<typename TypeFactory::CreatorBase*>(creators));
	}
};

#define SERIALIZATION_INHERIT_CREATORS(BaseType, Type) \
	static ClassFactoryInheritor<BaseType, Type> ClassFactoryInherit_ ## Type;

///////////////////////////////////////////////////////////////////////////////
// Temp buffers

template<typename T>
struct STempBuffer: TIStream<T>
{
	template<typename TParamMod>
	STempBuffer(const CParticleComponentRuntime& runtime, TParamMod& paramMod)
		: TIStream<T>(nullptr, paramMod.GetBaseValue())
		, m_buffer(runtime.MemHeap())
	{}

	TIOStream<T> RawStream()
	{
		assert(this->IsValid());
		return *this;
	}

protected:

	THeapArray<T> m_buffer;

	void Allocate(SGroupRange range)
	{
		m_buffer.resize(range.size());
		T* data = m_buffer.data() - +*range.begin();
		new(this) TIStream<T>(data);
	}
};

template<typename T>
struct STempInitBuffer: STempBuffer<T>
{
	template<typename TParamMod>
	STempInitBuffer(const CParticleComponentRuntime& runtime, TParamMod& paramMod, SUpdateRange range, bool alwaysAlloc = false)
		: STempBuffer<T>(runtime, paramMod)
	{
		if (alwaysAlloc || paramMod.HasInitModifiers())
		{
			this->Allocate(range);
			paramMod.ModifyInit(runtime, this->RawStream(), range);
		}
	}

	template<typename TParamMod>
	STempInitBuffer(const CParticleComponentRuntime& runtime, TParamMod& paramMod)
		: STempInitBuffer<T>(runtime, paramMod, runtime.SpawnedRange())
	{}
};

template<typename T>
struct STempUpdateBuffer: STempBuffer<T>
{
	template<typename TParamMod>
	STempUpdateBuffer(const CParticleComponentRuntime& runtime, TParamMod& paramMod, SUpdateRange range, bool alwaysAlloc = false)
		: STempBuffer<T>(runtime, paramMod)
	{
		if (alwaysAlloc || paramMod.HasModifiers())
		{
			this->Allocate(range);
			paramMod.ModifyInit(runtime, this->RawStream(), range);
			paramMod.ModifyUpdate(runtime, this->RawStream(), range);
		}
	}
};

template<typename T>
struct SSpawnerUpdateBuffer: STempBuffer<T>
{
	template<typename TParamMod>
	SSpawnerUpdateBuffer(const CParticleComponentRuntime& runtime, TParamMod& paramMod, EDataDomain domain, bool alwaysAlloc = false)
		: STempBuffer<T>(runtime, paramMod)
		, m_parentIds(runtime.IStream(ESDT_ParentId))
		, m_range(1, 1)
	{
		if (alwaysAlloc || paramMod.HasModifiers())
		{
			this->Allocate(SUpdateRange(0, runtime.DomainSize(domain)));
			m_range = paramMod.GetValues(runtime, this->m_buffer, domain);
		}
	}

	TRange<T> operator[](uint id) const
	{
		if (this->IsValid())
			return m_range * this->SafeLoad(m_parentIds[id]);
		else
			return this->Load(0);
	}

private:
	IPidStream m_parentIds;
	TRange<T>  m_range;
};


///////////////////////////////////////////////////////////////////////////////
// Domain distribution

SERIALIZATION_ENUM_DECLARE(EDistribution, ,
	Random,
	Ordered
)

template<uint Dim, typename T = std::array<float, Dim>>
struct SDistribution
{
	void Serialize(Serialization::IArchive& ar)
	{
		SERIALIZE_VAR(ar, m_distribution);
		if (m_distribution == EDistribution::Ordered)
			SERIALIZE_VAR(ar, m_modulus);
	}

	EDistribution m_distribution = EDistribution::Random;
	PosInt        m_modulus[Dim];
};

template<uint Dim, typename T = std::array<float, Dim>>
struct SDistributor
{
	SDistributor(const SDistribution<Dim, T>& dist, CParticleComponentRuntime& runtime)
		: m_distribution(dist.m_distribution)
		, m_chaos(runtime.Chaos())
		, m_order(runtime.GetContainer().TotalSpawned() - runtime.GetContainer().NumSpawned())
	{
		if (m_distribution == EDistribution::Ordered)
		{
			for (uint e = 0; e < Dim; ++e)
				m_order.SetModulus(dist.m_modulus[e], e);
		}
	}

	void SetRange(uint e, Range range, bool isOpen = false)
	{
		if (m_distribution == EDistribution::Random)
			m_chaos.SetRange(e, range, isOpen);
		else
			m_order.SetRange(e, range, isOpen);
	}

	T operator()()
	{
		if (m_distribution == EDistribution::Random)
			return m_chaos();
		else
			return m_order();
	}

private:
	const EDistribution m_distribution;
	SChaosKeyN<Dim, T>  m_chaos;
	SOrderKeyN<Dim, T>  m_order;
};

///////////////////////////////////////////////////////////////////////////////
// Distributor specializations

struct ChaosTypes
{
	template<uint Dim, typename T = std::array<float, Dim>> using type = SChaosKeyN<Dim, T>;
};
struct OrderTypes
{
	template<uint Dim, typename T = std::array<float, Dim>> using type = SOrderKeyN<Dim, T>;
};
struct DistributorTypes
{
	template<uint Dim, typename T = std::array<float, Dim>> using type = SDistributor<Dim, T>;
};

template<typename Types>
struct SCircleDistributor
{
	template<class... Args>
	SCircleDistributor(Args&&... args)
		: m_base(std::forward<Args>(args)...)
	{
		m_base.SetRange(0, {0, gf_PI2}, true);
	}
	Vec2 operator()()
	{
		return CirclePoint(m_base()[0]);
	}

protected:
	typename Types::template type<1> m_base;
};

template<typename Types>
struct SDiskDistributor
{
	template<class... Args>
	SDiskDistributor(Args&&... args)
		: m_base(std::forward<Args>(args)...)
	{
		m_base.SetRange(0, {0, gf_PI2}, true);
		m_base.SetRange(1, {0, 1});
	}
	void SetRadiusRange(Range radii)
	{
		m_base.SetRange(1, {sqr(radii.start), sqr(radii.end)});
	}
	Vec2 operator()()
	{
		auto vals = m_base();
		return DiskPoint(vals[0], vals[1]);
	}

protected:
	typename Types::template type<2> m_base;
};

template<typename Types>
struct SSphereDistributor
{
	template<class... Args>
	SSphereDistributor(Args&&... args)
		: m_base(std::forward<Args>(args)...)
	{
		m_base.SetRange(0, {0, gf_PI2}, true);
		m_base.SetRange(1, {-1, +1});
	}
	Vec3 operator()()
	{
		auto vals = m_base();
		return SpherePoint(vals[0], vals[1]);
	}

protected:
	typename Types::template type<2> m_base;
};

template<typename Types>
struct SBallDistributor
{
	template<class... Args>
	SBallDistributor(Args&&... args)
		: m_base(std::forward<Args>(args)...)
	{
		m_base.SetRange(0, {0, gf_PI2}, true);
		m_base.SetRange(1, {-1, +1});
		m_base.SetRange(2, {0, 1});
	}
	void SetRadiusRange(Range radii)
	{
		this->m_base.SetRange(2, {cube(radii.start), cube(radii.end)});
	}
	Vec3 operator()()
	{
		auto vals = m_base();
		return BallPoint(vals[0], vals[1], vals[2]);
	}

protected:
	typename Types::template type<3> m_base;
};


}

#include "ParamModImpl.h"

