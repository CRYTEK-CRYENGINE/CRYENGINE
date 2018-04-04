#pragma once

#include <CryEntitySystem/IEntityComponent.h>

#include "Wrappers/MonoProperty.h"

#include <CrySchematyc/CoreAPI.h>
#include <CrySchematyc/Env/Elements/EnvComponent.h>

// Type of property, has to be kept in sync with managed code's EntityPropertyType
enum class EEntityPropertyType : uint32
{
	Primitive = 0,
	Object,
	Texture,
	Particle,
	AnyFile,
	Sound,
	Material,
	Animation
};

struct CManagedEntityComponentFactory final : public Schematyc::CEnvElementBase<Schematyc::IEnvComponent>
{
public:
	struct SProperty : public Schematyc::CCommonTypeDesc
	{
		SProperty(size_t memberOffset, MonoInternals::MonoReflectionProperty* pReflectionProperty, std::shared_ptr<CMonoProperty> pMonoProperty, EEntityPropertyType serType,  MonoInternals::MonoTypeEnum type, CMonoClass& typeClass, std::shared_ptr<CMonoObject> pDefaultValue);

		SProperty(const SProperty& other)
			: pProperty(other.pProperty)
			, monoType(other.monoType)
			, serializationType(other.serializationType)
		{
		}

		static void DefaultConstruct(void* pPlacement);
		static void Destroy(void* pPlacement);
		static void CopyConstruct(void* pLHS, const void* pRHS);
		static void CopyAssign(void* pLHS, const void* pRHS);
		static bool Equals(const void* pLHS, const void* pRHS);
		static bool Serialize(Serialization::IArchive& archive, void* pValue, const char* szName, const char* szLabel);
		static void ToString(Schematyc::IString& output, const void* pInput);

		std::shared_ptr<CMonoProperty> pProperty;
		size_t offsetFromComponent;

		MonoInternals::MonoTypeEnum monoType;
		EEntityPropertyType serializationType;
	};

	CManagedEntityComponentFactory(std::shared_ptr<CMonoClass> pClass, const CryGUID& guid, const Schematyc::SSourceFileInfo& managedSourceFileInfo, const char* szName, const char* szUiCategory, const char* szUiDescription, const char* szIcon);

	// Schematyc::IEnvElement
	virtual bool IsValidScope(IEnvElement& scope) const override
	{
		switch (scope.GetType())
		{
		case Schematyc::EEnvElementType::Module:
		case Schematyc::EEnvElementType::Class:
		{
			return true;
		}
		default:
		{
			return false;
		}
		}
	}
	// ~Schematyc::IEnvElement

	// Schematyc::IEnvComponent
	virtual const CEntityComponentClassDesc& GetDesc() const override { return m_classDescription; }
	virtual std::shared_ptr<IEntityComponent> CreateFromPool() const override;
	// ~Schematyc::IEnvComponent

	void AddProperty(MonoInternals::MonoReflectionProperty* pProperty, const char* szPropertyName, const char* szPropertyLabel, const char* szPropertyDesc, EEntityPropertyType type, MonoInternals::MonoObject* pDefaultValue);

	class CManagedComponentClassDescription : public CEntityComponentClassDesc
	{
	public:
		Schematyc::CClassMemberDesc& AddMember(const Schematyc::CCommonTypeDesc& typeDesc, ptrdiff_t offset, uint32 id, const char* szName, const char* szLabel, const char* szDescription, Schematyc::Utils::IDefaultValuePtr&& pDefaultValue)
		{
			return CEntityComponentClassDesc::AddMember(typeDesc, offset, id, szName, szLabel, szDescription, std::forward<Schematyc::Utils::IDefaultValuePtr>(pDefaultValue));
		}

		bool AddBase(const Schematyc::CCommonTypeDesc& typeDesc)
		{
			return CEntityComponentClassDesc::AddBase(typeDesc, GetBases().size());
		}
	};

	CManagedComponentClassDescription m_classDescription;
	std::shared_ptr<CMonoClass> m_pClass;

	IEntityClass* m_pEntityClass;
	uint64 m_eventMask;

	std::vector<std::unique_ptr<SProperty>> m_properties;

	std::shared_ptr<CMonoMethod> m_pConstructorMethod;
	std::shared_ptr<CMonoMethod> m_pInternalSetEntityMethod;
	std::shared_ptr<CMonoMethod> m_pInitializeMethod;

	std::shared_ptr<CMonoMethod> m_pTransformChangedMethod;
	std::shared_ptr<CMonoMethod> m_pUpdateMethod;
	std::shared_ptr<CMonoMethod> m_pUpdateMethodEditing;
	std::shared_ptr<CMonoMethod> m_pGameModeChangeMethod;
	std::shared_ptr<CMonoMethod> m_pHideMethod;
	std::shared_ptr<CMonoMethod> m_pUnHideMethod;
	std::shared_ptr<CMonoMethod> m_pCollisionMethod;
	std::shared_ptr<CMonoMethod> m_pPrePhysicsUpdateMethod;

	std::shared_ptr<CMonoMethod> m_pGameStartMethod;
	std::shared_ptr<CMonoMethod> m_pRemoveMethod;
};