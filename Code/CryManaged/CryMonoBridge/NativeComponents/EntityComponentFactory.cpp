#include "StdAfx.h"
#include "EntityComponentFactory.h"
#include "MonoRuntime.h"

#include "Wrappers/MonoLibrary.h"
#include "Wrappers/MonoDomain.h"

#include <CryPhysics/physinterface.h>

#include <CrySerialization/Decorators/Resources.h>
#include <array>

CManagedEntityComponentFactory::CManagedEntityComponentFactory(std::shared_ptr<CMonoClass> pMonoClass, const CryGUID& guid, const Schematyc::SSourceFileInfo& managedSourceFileInfo, const char* szName, const char* szUiCategory, const char* szUiDescription, const char* szIcon)
	: Schematyc::CEnvElementBase<Schematyc::IEnvComponent>(guid, pMonoClass->GetName(), managedSourceFileInfo)
	, m_pClass(pMonoClass)
	, m_eventMask(0)
{
	m_classDescription.SetGUID(guid);

	m_classDescription.SetName(Schematyc::CTypeName(pMonoClass->GetName()));
	m_classDescription.SetLabel(szName);
	m_classDescription.SetEditorCategory(szUiCategory);
	m_classDescription.SetDescription(szUiDescription);
	m_classDescription.SetIcon(szIcon);

	m_classDescription.SetComponentFlags({ IEntityComponent::EFlags::Transform, IEntityComponent::EFlags::Socket, IEntityComponent::EFlags::Attach });

	// Check which managed functions are implemented
	CMonoClass* pEntityComponentClass = gEnv->pMonoRuntime->GetCryCoreLibrary()->GetClass("CryEngine", "EntityComponent");

	m_pConstructorMethod = m_pClass->FindMethod(".ctor");
	m_pInternalSetEntityMethod = pEntityComponentClass->FindMethod("SetEntity", 2);
	m_pInitializeMethod = m_pClass->FindMethodWithDescInInheritedClasses("OnInitialize()", pEntityComponentClass);

	if (m_pGameStartMethod = m_pClass->FindMethodWithDescInInheritedClasses("OnGameplayStart()", pEntityComponentClass))
	{
		m_eventMask |= BIT64(ENTITY_EVENT_START_GAME);
	}
	if (m_pTransformChangedMethod = m_pClass->FindMethodWithDescInInheritedClasses("OnTransformChanged()", pEntityComponentClass))
	{
		m_eventMask |= BIT64(ENTITY_EVENT_XFORM);
	}
	if (m_pUpdateMethod = m_pClass->FindMethodWithDescInInheritedClasses("OnUpdate(single)", pEntityComponentClass))
	{
		m_eventMask |= BIT64(ENTITY_EVENT_UPDATE);
	}
	if (m_pUpdateMethodEditing = m_pClass->FindMethodWithDescInInheritedClasses("OnEditorUpdate(single)", pEntityComponentClass))
	{
		m_eventMask |= BIT64(ENTITY_EVENT_UPDATE);
	}
	if (m_pGameModeChangeMethod = m_pClass->FindMethodWithDescInInheritedClasses("OnEditorGameModeChange(bool)", pEntityComponentClass))
	{
		m_eventMask |= BIT64(ENTITY_EVENT_RESET);
	}
	if (m_pHideMethod = m_pClass->FindMethodWithDescInInheritedClasses("OnHide()", pEntityComponentClass))
	{
		m_eventMask |= BIT64(ENTITY_EVENT_HIDE);
	}
	if (m_pUnHideMethod = m_pClass->FindMethodWithDescInInheritedClasses("OnUnhide()", pEntityComponentClass))
	{
		m_eventMask |= BIT64(ENTITY_EVENT_UNHIDE);
	}
	if (m_pClass->FindMethodWithDescInInheritedClasses("OnCollision(CollisionEvent)", pEntityComponentClass))
	{
		m_pCollisionMethod = pEntityComponentClass->FindMethod("OnCollisionInternal", 2);

		m_eventMask |= BIT64(ENTITY_EVENT_COLLISION);
	}
	if (m_pPrePhysicsUpdateMethod = m_pClass->FindMethodWithDescInInheritedClasses("OnPrePhysicsUpdate(single)", pEntityComponentClass))
	{
		m_eventMask |= BIT64(ENTITY_EVENT_PREPHYSICSUPDATE);
	}
	if (m_pRemoveMethod = m_pClass->FindMethodWithDescInInheritedClasses("OnRemove()", pEntityComponentClass))
	{
		m_eventMask |= BIT64(ENTITY_EVENT_DONE);
	}
}

std::shared_ptr<IEntityComponent> CManagedEntityComponentFactory::CreateFromPool() const
{
	// We allocate extra memory for entity components in order to support getting properties from the Schematyc callbacks at the end of this file.
	// This allows us to get both the property being processed and the component itself from one pointer currently being handled by Schematyc
	// Otherwise we would need to refactor Schematyc to utilize std::function, introducing extra overhead in terms of both memory and performance.
	uint32 entityComponentSize = sizeof(CManagedEntityComponent) + sizeof(SProperty) * m_properties.size();
	CManagedEntityComponent* pRawComponent = (CManagedEntityComponent*)CryModuleMalloc(entityComponentSize);
	
	struct CustomDeleter
	{
		void operator()(CManagedEntityComponent* p)
		{
			// Explicit call to the destructor
			p->~CManagedEntityComponent();
			// Memory aligned free
			CryModuleFree(p);
		}
	};

	auto pComponent = std::shared_ptr<CManagedEntityComponent>(new(pRawComponent) CManagedEntityComponent(*this), CustomDeleter());

	for(auto it = m_properties.begin(); it != m_properties.end(); ++it)
	{
		SProperty* pProperty = (SProperty*)((uintptr_t)pComponent.get() + sizeof(CManagedEntityComponent) + std::distance(m_properties.begin(), it) * sizeof(SProperty));
		new(pProperty) SProperty(*it->get());
	}

	return pComponent;
}

void CManagedEntityComponentFactory::AddProperty(MonoInternals::MonoReflectionProperty* pReflectionProperty, const char* szPropertyName, const char* szPropertyLabel, const char* szPropertyDesc, EEntityPropertyType type)
{
	std::shared_ptr<CMonoProperty> pProperty = CMonoClass::MakeProperty(pReflectionProperty);

	MonoInternals::MonoTypeEnum monoType = (MonoInternals::MonoTypeEnum)MonoInternals::mono_type_get_type(MonoInternals::mono_signature_get_return_type(pProperty->GetGetMethod().GetSignature()));

	// Calculate offset of member from CManagedEntityComponent pointer, 0 being start of the component
	uint32 memberOffset = sizeof(CManagedEntityComponent) + sizeof(SProperty) * m_properties.size();

	m_properties.emplace_back(stl::make_unique<SProperty>(memberOffset, pProperty, type, monoType));
	
	m_classDescription.AddMember(*m_properties.back().get(), memberOffset, m_properties.size(), szPropertyName, szPropertyLabel, szPropertyDesc, nullptr);
}

const int hash_value = 29;

void CManagedEntityComponentFactory::AddFunction(MonoInternals::MonoReflectionMethod* pReflectionMethod)
{
	// Hacky workaround for no API access for getting MonoProperty from reflection data
	// Post-5.3 we should expose this to Mono and send the change back.
	struct InternalMonoReflectionMethod 
	{
		MonoInternals::MonoObject object;
		MonoInternals::MonoMethod *method;
		MonoInternals::MonoString *name;
		MonoInternals::MonoReflectionType *reftype;
	};

	InternalMonoReflectionMethod* pInternalMethod = (InternalMonoReflectionMethod*)pReflectionMethod;
	
	// Generate a GUID that's persistent based on the C# name (TODO: Allow using the GUID attribute)
	const char* szMethodName = MonoInternals::mono_method_get_name(pInternalMethod->method);
	CryGUID guid = GetGUID();
	for (const char* szMethodCharacter = szMethodName; szMethodCharacter[0] != '\0'; ++szMethodCharacter)
	{
		guid.lopart = guid.lopart * hash_value - szMethodCharacter[0];
		guid.hipart = guid.hipart * hash_value + szMethodCharacter[0];
	}

	m_schematycFunctions.emplace_back(std::make_shared<CSchematycFunction>(std::make_shared<CMonoMethod>(pInternalMethod->method), guid, &m_classDescription));
}

int CManagedEntityComponentFactory::AddSignal(const char* szEventName)
{
	// Generate a GUID that's persistent based on the C# name (TODO: Allow using the GUID attribute)
	CryGUID guid = GetGUID();
	for (const char* szMethodCharacter = szEventName; szMethodCharacter[0] != '\0'; ++szMethodCharacter)
	{
		guid.lopart = guid.lopart * hash_value - szMethodCharacter[0];
		guid.hipart = guid.hipart * hash_value + szMethodCharacter[0];
	}

	m_schematycSignals.emplace_back(std::make_shared<CSchematycSignal>(guid, szEventName));
	return m_schematycSignals.size() - 1;
}

void CManagedEntityComponentFactory::AddSignalParameter(int signalId, const char* szParameter, MonoInternals::MonoReflectionType* pType)
{
	CSchematycSignal& signal = *m_schematycSignals[signalId].get();
	signal.AddParameter(szParameter, pType);
}

Schematyc::ETypeCategory GetSchematycPropertyType(MonoInternals::MonoTypeEnum type)
{
	switch (type)
	{
	case MonoInternals::MONO_TYPE_BOOLEAN:
	case MonoInternals::MONO_TYPE_U1:
	case MonoInternals::MONO_TYPE_CHAR:
	case MonoInternals::MONO_TYPE_I1:
	case MonoInternals::MONO_TYPE_I2:
	case MonoInternals::MONO_TYPE_U2:
	case MonoInternals::MONO_TYPE_I4:
	case MonoInternals::MONO_TYPE_U4:
	case MonoInternals::MONO_TYPE_I8:
	case MonoInternals::MONO_TYPE_U8:
		return Schematyc::ETypeCategory::Integral;
	case MonoInternals::MONO_TYPE_R4:
	case MonoInternals::MONO_TYPE_R8:
		return Schematyc::ETypeCategory::FloatingPoint;
	case MonoInternals::MONO_TYPE_STRING:
		return Schematyc::ETypeCategory::String;
	}

	// Tried to use unsupported type
	CRY_ASSERT(false);
	return Schematyc::ETypeCategory::Unknown;
}

template <typename T>
static bool SerializePrimitive(Serialization::IArchive& archive, const CMonoProperty* pProperty, const char* szName, const char* szLabel, MonoInternals::MonoObject* pObject, MonoInternals::MonoClass* pClass)
{
	T value;
	if (archive.isOutput())
	{
		bool bEncounteredException;
		std::shared_ptr<CMonoObject> pPropertyValue = std::static_pointer_cast<CMonoObject>(pProperty->Get(pObject, bEncounteredException));
		if (bEncounteredException || pPropertyValue == nullptr)
		{
			return false;
		}

		value = *pPropertyValue->Unbox<T>();
	}

	archive(value, szName, szLabel);

	if (archive.isInput())
	{
		void* pParams[1];
		pParams[0] = &value;

		bool bEncounteredException;
		pProperty->Set(pObject, pParams, bEncounteredException);

		return !bEncounteredException;
	}

	return true;
}

CManagedEntityComponentFactory::SProperty::SProperty(uint32 membOffset, std::shared_ptr<CMonoProperty> pMonoProperty, EEntityPropertyType serType, MonoInternals::MonoTypeEnum type)
	: Schematyc::CCommonTypeDesc(GetSchematycPropertyType(type))
	, memberOffset(membOffset)
	, pProperty(pMonoProperty)
	, serializationType(serType)
	, monoType(type)
{
	m_operators.defaultConstruct = &SProperty::DefaultConstruct;
	m_operators.destruct = &SProperty::Destroy;
	m_operators.copyConstruct = &SProperty::CopyConstruct;
	m_operators.copyAssign = &SProperty::CopyAssign;
	m_operators.equals = &SProperty::Equals;
	m_operators.serialize = &SProperty::Serialize;
	m_operators.toString = &SProperty::ToString;
}

void CManagedEntityComponentFactory::SProperty::DefaultConstruct(void* pComponentAddress)
{
	// Not implemented, catch if this is actually used
	CRY_ASSERT(false);
}

void CManagedEntityComponentFactory::SProperty::Destroy(void* pComponentAddress)
{
	// Not implemented, catch if this is actually used
	CRY_ASSERT(false);
}

void CManagedEntityComponentFactory::SProperty::CopyConstruct(void* pTargetComponentAddress, const void* pSourceComponentAddress)
{
	// Not implemented, catch if this is actually used
	CRY_ASSERT(false);
}

void CManagedEntityComponentFactory::SProperty::CopyAssign(void* pTargetComponentAddress, const void* pSourceComponentAddress)
{
	CManagedEntityComponent* pTargetComponent = static_cast<CManagedEntityComponent*>(pTargetComponentAddress);
	const CManagedEntityComponent* pSourceComponent = static_cast<const CManagedEntityComponent*>(pSourceComponentAddress);

	pTargetComponent->GetObject()->CopyFrom(*pSourceComponent->GetObject());
}

bool CManagedEntityComponentFactory::SProperty::Equals(const void* pLeftComponentAddress, const void* pRightComponentAddress)
{
	const CManagedEntityComponent* pLeftComponent = static_cast<const CManagedEntityComponent*>(pLeftComponentAddress);
	const CManagedEntityComponent* pRightComponent = static_cast<const CManagedEntityComponent*>(pRightComponentAddress);

	// Horrible string comparison check, we should find a way to compare through Mono somehow if even possible
	std::shared_ptr<CMonoString> pLeftString = pLeftComponent->GetObject()->ToString();
	std::shared_ptr<CMonoString> pRightString = pRightComponent->GetObject()->ToString();

	return !strcmp(pLeftString->GetString(), pRightString->GetString());
}

bool CManagedEntityComponentFactory::SProperty::Serialize(Serialization::IArchive& archive, void* pComponentAddress, const char* szName, const char* szLabel)
{
	const CManagedEntityComponentFactory::SProperty* pProperty = static_cast<const CManagedEntityComponentFactory::SProperty*>(pComponentAddress);
	const CManagedEntityComponent* pComponent = (const CManagedEntityComponent*)((uintptr_t)pProperty - pProperty->memberOffset);

	switch (pProperty->monoType)
	{
	case MonoInternals::MONO_TYPE_BOOLEAN:
	{
		return SerializePrimitive<bool>(archive, pProperty->pProperty.get(), szName, szLabel, pComponent->GetObject()->GetManagedObject(), MonoInternals::mono_get_boolean_class());
	}
	break;
	case MonoInternals::MONO_TYPE_STRING:
	{
		string value;
		if (archive.isOutput())
		{
			bool bEncounteredException;
			std::shared_ptr<CMonoObject> pPropertyValue = std::static_pointer_cast<CMonoObject>(pProperty->pProperty->Get(pComponent->GetObject()->GetManagedObject(), bEncounteredException));
			if (bEncounteredException || pPropertyValue == nullptr)
			{
				return false;
			}

			std::shared_ptr<CMonoString> pValue = pPropertyValue->ToString();
			value = pValue->GetString();
		}

		switch (pProperty->serializationType)
		{
		case EEntityPropertyType::Primitive:
			archive(value, szName, szLabel);
			break;
		case EEntityPropertyType::Object:
			archive(Serialization::ModelFilename(value), szName, szLabel);
			break;
		case EEntityPropertyType::Texture:
			archive(Serialization::TextureFilename(value), szName, szLabel);
			break;
		case EEntityPropertyType::Particle:
			archive(Serialization::ParticleName(value), szName, szLabel);
			break;
		case EEntityPropertyType::AnyFile:
			archive(Serialization::GeneralFilename(value), szName, szLabel);
			break;
		case EEntityPropertyType::Sound:
			archive(Serialization::SoundFilename(value), szName, szLabel);
			break;
		case EEntityPropertyType::Material:
			archive(Serialization::MaterialPicker(value), szName, szLabel);
			break;
		case EEntityPropertyType::Animation:
			archive(Serialization::CharacterAnimationPicker(value), szName, szLabel);
			break;
		}

		if (archive.isInput())
		{
			CMonoDomain* pDomain = pComponent->GetObject()->GetClass()->GetAssembly()->GetDomain();

			std::shared_ptr<CMonoString> pValue = pDomain->CreateString(value);

			bool bEncounteredException;
			pProperty->pProperty->Set(pComponent->GetObject()->GetManagedObject(), (MonoInternals::MonoObject*)pValue->GetManagedObject(), bEncounteredException);

			return !bEncounteredException;
		}

		return true;
	}
	break;
	case MonoInternals::MONO_TYPE_U1:
	case MonoInternals::MONO_TYPE_CHAR: // Char is unsigned by default for .NET
	{
		return SerializePrimitive<uchar>(archive, pProperty->pProperty.get(), szName, szLabel, pComponent->GetObject()->GetManagedObject(), MonoInternals::mono_get_char_class());
	}
	break;
	case MonoInternals::MONO_TYPE_I1:
	{
		return SerializePrimitive<char>(archive, pProperty->pProperty.get(), szName, szLabel, pComponent->GetObject()->GetManagedObject(), MonoInternals::mono_get_byte_class());
	}
	break;
	case MonoInternals::MONO_TYPE_I2:
	{
		return SerializePrimitive<int16>(archive, pProperty->pProperty.get(), szName, szLabel, pComponent->GetObject()->GetManagedObject(), MonoInternals::mono_get_int16_class());
	}
	break;
	case MonoInternals::MONO_TYPE_U2:
	{
		return SerializePrimitive<uint16>(archive, pProperty->pProperty.get(), szName, szLabel, pComponent->GetObject()->GetManagedObject(), MonoInternals::mono_get_uint16_class());
	}
	break;
	case MonoInternals::MONO_TYPE_I4:
	{
		return SerializePrimitive<int32>(archive, pProperty->pProperty.get(), szName, szLabel, pComponent->GetObject()->GetManagedObject(), MonoInternals::mono_get_int32_class());
	}
	break;
	case MonoInternals::MONO_TYPE_U4:
	{
		return SerializePrimitive<uint32>(archive, pProperty->pProperty.get(), szName, szLabel, pComponent->GetObject()->GetManagedObject(), MonoInternals::mono_get_uint32_class());
	}
	break;
	case MonoInternals::MONO_TYPE_I8:
	{
		return SerializePrimitive<int64>(archive, pProperty->pProperty.get(), szName, szLabel, pComponent->GetObject()->GetManagedObject(), MonoInternals::mono_get_int64_class());
	}
	break;
	case MonoInternals::MONO_TYPE_U8:
	{
		return SerializePrimitive<uint64>(archive, pProperty->pProperty.get(), szName, szLabel, pComponent->GetObject()->GetManagedObject(), MonoInternals::mono_get_uint64_class());
	}
	break;
	case MonoInternals::MONO_TYPE_R4:
	{
		return SerializePrimitive<float>(archive, pProperty->pProperty.get(), szName, szLabel, pComponent->GetObject()->GetManagedObject(), MonoInternals::mono_get_single_class());
	}
	break;
	case MonoInternals::MONO_TYPE_R8:
	{
		return SerializePrimitive<double>(archive, pProperty->pProperty.get(), szName, szLabel, pComponent->GetObject()->GetManagedObject(), MonoInternals::mono_get_double_class());
	}
	break;
	}

	return false;
}

void CManagedEntityComponentFactory::SProperty::ToString(Schematyc::IString& output, const void* pComponentAddress)
{
	const CManagedEntityComponentFactory::SProperty* pProperty = static_cast<const CManagedEntityComponentFactory::SProperty*>(pComponentAddress);
	const CManagedEntityComponent* pComponent = (const CManagedEntityComponent*)((uintptr_t)pProperty - pProperty->memberOffset);

	bool bEncounteredException;
	std::shared_ptr<CMonoObject> pPropertyValue = std::static_pointer_cast<CMonoObject>(pProperty->pProperty->Get(pComponent->GetObject()->GetManagedObject(), bEncounteredException));
	if (!bEncounteredException)
	{
		std::shared_ptr<CMonoString> pValue = pPropertyValue->ToString();
		output.assign(pValue->GetString());
	}
}

CManagedEntityComponentFactory::CSchematycFunction::CSchematycFunction(std::shared_ptr<CMonoMethod>&& pMethod, const CryGUID& guid, Schematyc::CCommonTypeDesc* pOwnerDesc)
	: Schematyc::CEnvElementBase<Schematyc::IEnvFunction>(guid, pMethod->GetName(), Schematyc::SSourceFileInfo("Unknown .NET source file", 0))
	, m_pMethod(std::move(pMethod))
	, m_pOwnerDescription(pOwnerDesc)
	, m_numParameters(0)
{
	MonoInternals::MonoMethodSignature* pSignature = m_pMethod->GetSignature();

	MonoInternals::MonoType* pReturnType = MonoInternals::mono_signature_get_return_type(pSignature);
	if (pReturnType != nullptr && MonoInternals::mono_type_get_type(pReturnType) != MonoInternals::MONO_TYPE_VOID)
	{
		m_outputs.emplace_back(SParameter{ 0, pReturnType, "Result" });
	}

	m_pMethod->VisitParameters([this, pSignature](int index, MonoInternals::MonoType* pType, const char* szName)
	{
		Schematyc::CAnyConstPtr defaultValue;

		switch (MonoInternals::mono_type_get_type(pType))
		{
		case MonoInternals::MONO_TYPE_BOOLEAN:
		{
			bool defaultVal = false;
			defaultValue = Schematyc::CAnyValue::MakeShared(Schematyc::GetTypeDesc<bool>(), &defaultVal);
		}
		break;
		case MonoInternals::MONO_TYPE_STRING:
		{
			Schematyc::CSharedString defaultVal = "";
			defaultValue = Schematyc::CAnyValue::MakeShared(Schematyc::GetTypeDesc<Schematyc::CSharedString>(), &defaultVal);
		}
		break;
		case MonoInternals::MONO_TYPE_U1:
		case MonoInternals::MONO_TYPE_CHAR: // Char is unsigned by default for .NET
		case MonoInternals::MONO_TYPE_U2:
		case MonoInternals::MONO_TYPE_U4:
		case MonoInternals::MONO_TYPE_U8: // Losing precision! TODO: Add Schematyc support for 64?
		{
			uint32 defaultVal = 0;
			defaultValue = Schematyc::CAnyValue::MakeShared(Schematyc::GetTypeDesc<uint32>(), &defaultVal);
		}
		break;
		case MonoInternals::MONO_TYPE_I1:
		case MonoInternals::MONO_TYPE_I2:
		case MonoInternals::MONO_TYPE_I4:
		case MonoInternals::MONO_TYPE_I8: // Losing precision! TODO: Add Schematyc support for 64?
		{
			int32 defaultVal = 0;
			defaultValue = Schematyc::CAnyValue::MakeShared(Schematyc::GetTypeDesc<int32>(), &defaultVal);
		}
		break;
		case MonoInternals::MONO_TYPE_R4:
		case MonoInternals::MONO_TYPE_R8: // Losing precision! TODO: Add Schematyc support for 64?
		{
			float defaultVal = 0;
			defaultValue = Schematyc::CAnyValue::MakeShared(Schematyc::GetTypeDesc<float>(), &defaultVal);
		}
		break;

		default:
			CRY_ASSERT_MESSAGE(false, "Tried to register Schematyc function with non-primitive parameter type!");
			break;
		}

		if (MonoInternals::mono_signature_param_is_out(pSignature, index) != 0)
		{
			m_outputs.emplace_back(SParameter{ index + 1, pType, szName, defaultValue });
		}
		else
		{
			m_inputs.emplace_back(SParameter{ index + 1, pType, szName, defaultValue });
		}

		m_numParameters++;
	});
}

void CManagedEntityComponentFactory::CSchematycFunction::Execute(Schematyc::CRuntimeParamMap& params, void* pObject) const
{
	if (m_pMethod != nullptr)
	{
		const CManagedEntityComponent* pComponent = (const CManagedEntityComponent*)pObject;

		MonoInternals::MonoArray* pParameters = nullptr;
		if (m_inputs.size() > 0)
		{
			MonoInternals::MonoDomain* pDomain = MonoInternals::mono_domain_get();
			pParameters = MonoInternals::mono_array_new(pDomain, MonoInternals::mono_get_object_class(), m_numParameters);

			for (const SParameter& parameter : m_inputs)
			{
				MonoInternals::MonoClass* pParameterClass = MonoInternals::mono_class_from_mono_type(parameter.pType);
				MonoInternals::MonoObject* pBoxedObject = MonoInternals::mono_value_box(pDomain, pParameterClass, const_cast<void*>((*params.GetInput(Schematyc::CUniqueId::FromUInt32(parameter.index))).GetValue()));
				void** pAddress = ((void**)MonoInternals::mono_array_addr_with_size(pParameters, sizeof(void*), parameter.index - 1));
				MonoInternals::mono_gc_wbarrier_set_arrayref(pParameters, pAddress, pBoxedObject);
			}
		}

		std::shared_ptr<CMonoObject> pResult = m_pMethod->Invoke(pComponent->GetObject(), pParameters);
		for (const SParameter& parameter : m_outputs)
		{
			MonoInternals::MonoObject* pObject = *(MonoInternals::MonoObject**)MonoInternals::mono_array_addr_with_size(pParameters, sizeof(void*), parameter.index - 1);
			
			switch (MonoInternals::mono_type_get_type(parameter.pType))
			{
			case MonoInternals::MONO_TYPE_BOOLEAN:
			{
				params.SetOutput(Schematyc::CUniqueId::FromUInt32(parameter.index), *(bool *)MonoInternals::mono_object_unbox(pObject));
			}
			break;
			case MonoInternals::MONO_TYPE_STRING:
			{
				char* szValue = MonoInternals::mono_string_to_utf8((MonoInternals::MonoString*)pObject);
				params.SetOutput(Schematyc::CUniqueId::FromUInt32(parameter.index), Schematyc::CSharedString(szValue));
				MonoInternals::mono_free(szValue);
			}
			break;
			case MonoInternals::MONO_TYPE_U1:
			case MonoInternals::MONO_TYPE_CHAR: // Char is unsigned by default for .NET
			case MonoInternals::MONO_TYPE_U2:
			case MonoInternals::MONO_TYPE_U4:
			case MonoInternals::MONO_TYPE_U8: // Losing precision! TODO: Add Schematyc support for 64?
			{
				params.SetOutput(Schematyc::CUniqueId::FromUInt32(parameter.index), *(uint32 *)MonoInternals::mono_object_unbox(pObject));
			}
			break;
			case MonoInternals::MONO_TYPE_I1:
			case MonoInternals::MONO_TYPE_I2:
			case MonoInternals::MONO_TYPE_I4:
			case MonoInternals::MONO_TYPE_I8: // Losing precision! TODO: Add Schematyc support for 64?
			{
				params.SetOutput(Schematyc::CUniqueId::FromUInt32(parameter.index), *(int32 *)MonoInternals::mono_object_unbox(pObject));
			}
			break;
			case MonoInternals::MONO_TYPE_R4:
			case MonoInternals::MONO_TYPE_R8: // Losing precision! TODO: Add Schematyc support for 64?
			{
				params.SetOutput(Schematyc::CUniqueId::FromUInt32(parameter.index), *(float *)MonoInternals::mono_object_unbox(pObject));
			}
			break;

		default:
			CRY_ASSERT_MESSAGE(false, "Tried to execute Schematyc function with non-primitive parameter type!");
			break;
			}
		}
	}
}

void CManagedEntityComponentFactory::CSchematycSignal::CSignalClassDesc::AddParameter(const char* szParameter, MonoInternals::MonoReflectionType* pType)
{
	m_memberNames.push_back(szParameter);
	szParameter = m_memberNames.back();

	switch (MonoInternals::mono_type_get_type(MonoInternals::mono_reflection_type_get_type(pType)))
	{
	case MonoInternals::MONO_TYPE_BOOLEAN:
	{
		AddMember(Schematyc::GetTypeDesc<bool>(), GetMembers().size(), GetMembers().size(), szParameter, szParameter, "", Schematyc::Utils::CDefaultValue<bool>::MakeUnique());
	}
	break;
	case MonoInternals::MONO_TYPE_STRING:
	{
		AddMember(Schematyc::GetTypeDesc<Schematyc::CSharedString>(), GetMembers().size(), GetMembers().size(), szParameter, szParameter, "", Schematyc::Utils::CDefaultValue<Schematyc::CSharedString>::MakeUnique());
	}
	break;
	case MonoInternals::MONO_TYPE_U1:
	case MonoInternals::MONO_TYPE_CHAR: // Char is unsigned by default for .NET
	case MonoInternals::MONO_TYPE_U2:
	case MonoInternals::MONO_TYPE_U4:
	case MonoInternals::MONO_TYPE_U8: // Losing precision! TODO: Add Schematyc support for 64?
	{
		AddMember(Schematyc::GetTypeDesc<uint32>(), GetMembers().size(), GetMembers().size(), szParameter, szParameter, "", Schematyc::Utils::CDefaultValue<uint32>::MakeUnique());
	}
	break;
	case MonoInternals::MONO_TYPE_I1:
	case MonoInternals::MONO_TYPE_I2:
	case MonoInternals::MONO_TYPE_I4:
	case MonoInternals::MONO_TYPE_I8: // Losing precision! TODO: Add Schematyc support for 64?
	{
		AddMember(Schematyc::GetTypeDesc<int32>(), GetMembers().size(), GetMembers().size(), szParameter, szParameter, "", Schematyc::Utils::CDefaultValue<int32>::MakeUnique());
	}
	break;
	case MonoInternals::MONO_TYPE_R4:
	case MonoInternals::MONO_TYPE_R8: // Losing precision! TODO: Add Schematyc support for 64?
	{
		AddMember(Schematyc::GetTypeDesc<float>(), GetMembers().size(), GetMembers().size(), szParameter, szParameter, "", Schematyc::Utils::CDefaultValue<float>::MakeUnique());
	}
	break;

	default:
		CRY_ASSERT_MESSAGE(false, "Tried to add Schematyc signal parameter with non-primitive type!");
		break;
	}
}