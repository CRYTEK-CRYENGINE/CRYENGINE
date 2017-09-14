#pragma once

#include <CryEntitySystem/IEntityComponent.h>

#include "Wrappers/MonoProperty.h"
#include "Wrappers/MonoMethod.h"

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
	void AddFunction(MonoInternals::MonoReflectionMethod* pMethod);
	int AddSignal(const char* szEventName);
	void AddSignalParameter(int signalId, const char* szParameter, MonoInternals::MonoReflectionType* pType);

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

	class CSchematycFunction final : public Schematyc::CEnvElementBase<Schematyc::IEnvFunction>
	{
		struct SParameter
		{
			int index;
			MonoInternals::MonoType* pType;
			string name;
			Schematyc::CAnyConstPtr defaultValue;
		};

	public:
		CSchematycFunction(std::shared_ptr<CMonoMethod>&& pMethod, const CryGUID& guid, Schematyc::CCommonTypeDesc* pOwnerDesc);

		// IEnvElement
		virtual bool IsValidScope(Schematyc::IEnvElement& scope) const override
		{
			switch (scope.GetType())
			{
			case Schematyc::EEnvElementType::Module:
			case Schematyc::EEnvElementType::DataType:
			case Schematyc::EEnvElementType::Class:
			case Schematyc::EEnvElementType::Component:
			case Schematyc::EEnvElementType::Action:
			{
				return true;
			}
			default:
			{
				return false;
			}
			}
		}
		// ~IEnvElement

		// IEnvFunction
		virtual bool Validate() const override
		{
			return m_pMethod != nullptr && m_pMethod->GetHandle() != nullptr;
		}

		virtual Schematyc::EnvFunctionFlags GetFunctionFlags() const override
		{
			return { Schematyc::EEnvFunctionFlags::Member, Schematyc::EEnvFunctionFlags::Construction };
		}

		virtual const Schematyc::CCommonTypeDesc* GetObjectTypeDesc() const override
		{
			return m_pOwnerDescription;
		}

		virtual uint32 GetInputCount() const override
		{
			return m_inputs.size();
		}

		virtual uint32 GetInputId(uint32 inputIdx) const override
		{
			return m_inputs[inputIdx].index;
		}

		virtual const char* GetInputName(uint32 inputIdx) const override
		{
			return m_inputs[inputIdx].name;
		}

		virtual const char* GetInputDescription(uint32 inputIdx) const override
		{
			return "";
		}

		virtual Schematyc::CAnyConstPtr GetInputData(uint32 inputIdx) const override
		{
			return m_inputs[inputIdx].defaultValue;
		}

		virtual uint32 GetOutputCount() const override
		{
			return m_outputs.size();
		}

		virtual uint32 GetOutputId(uint32 outputIdx) const override
		{
			return m_outputs[outputIdx].index;
		}

		virtual const char* GetOutputName(uint32 outputIdx) const override
		{
			return m_outputs[outputIdx].name;
		}

		virtual const char* GetOutputDescription(uint32 outputIdx) const override
		{
			return "";
		}

		virtual Schematyc::CAnyConstPtr GetOutputData(uint32 outputIdx) const override
		{
			return m_outputs[outputIdx].defaultValue;
		}

		virtual void Execute(Schematyc::CRuntimeParamMap& params, void* pObject) const override;
		// ~IEnvFunction

	protected:
		Schematyc::CCommonTypeDesc* m_pOwnerDescription;
		std::shared_ptr<CMonoMethod> m_pMethod;
		std::vector<SParameter> m_outputs;
		std::vector<SParameter> m_inputs;
		size_t m_numParameters;
	};

	class CSchematycSignal final : public Schematyc::CEnvElementBase<Schematyc::IEnvSignal>
	{
	public:
		CSchematycSignal(const CryGUID& guid, const char* szLabel)
			: Schematyc::CEnvElementBase<Schematyc::IEnvSignal>(guid, szLabel, Schematyc::SSourceFileInfo("Unknown .NET source file", 0))
			, m_classDesc(guid, szLabel) {}

		// IEnvElement
		virtual bool IsValidScope(Schematyc::IEnvElement& scope) const override
		{
			switch (scope.GetType())
			{
			case Schematyc::EEnvElementType::Root:
			case Schematyc::EEnvElementType::Module:
			case Schematyc::EEnvElementType::Component:
			case Schematyc::EEnvElementType::Action:
			{
				return true;
			}
			default:
			{
				return false;
			}
			}
		}
		// ~IEnvElement

		// IEnvSignal
		virtual const Schematyc::CClassDesc& GetDesc() const override { return m_classDesc; }
		// ~IEnvSignal

		void AddParameter(const char* szParameter, MonoInternals::MonoReflectionType* pType)
		{
			m_classDesc.AddParameter(szParameter, pType);
		}

		class CSignalClassDesc : public Schematyc::CClassDesc
		{
		public:
			CSignalClassDesc(const CryGUID& guid, const char* szLabel)
				: m_label(szLabel)
			{
				SetGUID(guid);
				SetLabel(m_label.c_str());
			}

			void AddParameter(const char* szParameter, MonoInternals::MonoReflectionType* pType);

			string m_label;
			std::vector<string> m_memberNames;
		};

		CSignalClassDesc m_classDesc;
	};

	std::vector<std::shared_ptr<CSchematycFunction>> m_schematycFunctions;
	std::vector<std::shared_ptr<CSchematycSignal>>   m_schematycSignals;
};