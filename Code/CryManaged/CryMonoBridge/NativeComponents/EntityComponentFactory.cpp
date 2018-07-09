// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "EntityComponentFactory.h"
#include "MonoRuntime.h"

#include "Wrappers/MonoLibrary.h"
#include "Wrappers/RootMonoDomain.h"
#include "Wrappers/AppDomain.h"

#include <CryPhysics/physinterface.h>

#include <CrySerialization/Decorators/Resources.h>
#include <array>

class CManagedEntityPropertyDefaultValue final : public Schematyc::Utils::IDefaultValue
{
public:
	CManagedEntityPropertyDefaultValue(const CManagedEntityComponentFactory::SPropertyTypeDescription& propertyDescription, std::shared_ptr<CMonoObject>& pDefaultValue) : m_property(propertyDescription, pDefaultValue, nullptr) {}
	virtual ~CManagedEntityPropertyDefaultValue() {}
	virtual const void* Get() const override { return &m_property; }

	CManagedEntityComponentFactory::SPropertyValue m_property;
};

CManagedEntityComponentFactory::CManagedEntityComponentFactory(std::shared_ptr<CMonoClass> pMonoClass, const CryGUID& guid, const Schematyc::SSourceFileInfo& managedSourceFileInfo, const char* szName, const char* szUiCategory, const char* szUiDescription, const char* szIcon)
	: Schematyc::CEnvElementBase<Schematyc::IEnvComponent>(guid, pMonoClass->GetName(), managedSourceFileInfo)
	, m_pClass(pMonoClass)
{
	m_classDescription.SetGUID(guid);

	m_classDescription.SetName(Schematyc::CTypeName(pMonoClass->GetName()));
	m_classDescription.SetLabel(szName);
	m_classDescription.SetEditorCategory(szUiCategory);
	m_classDescription.SetDescription(szUiDescription);
	m_classDescription.SetIcon(szIcon);

	m_classDescription.SetComponentFlags({ IEntityComponent::EFlags::Socket, IEntityComponent::EFlags::Attach });

	// TODO: Determine whether or not we allow a missing constructor
	CacheMethods(false);
}

void CManagedEntityComponentFactory::CacheMethods(bool isAbstract)
{
	// Check which managed functions are implemented
	CMonoClass* pEntityComponentClass = gEnv->pMonoRuntime->GetCryCoreLibrary()->GetClass("CryEngine", "EntityComponent");

	auto prevEventMask = m_eventMask;
	m_eventMask = ENTITY_EVENT_BIT(ENTITY_EVENT_LEVEL_LOADED);

	if (m_pClass.get() == pEntityComponentClass)
	{
		isAbstract = true;
	}

	m_pConstructorMethod = m_pClass->FindMethod(".ctor");

	// TODO: Check if we are supposed to be abstract, and re-implement the below
	if (m_pConstructorMethod.expired() && !isAbstract)
	{
		GetMonoRuntime()->GetCryCoreLibrary()->GetException(m_pClass->GetNamespace(), m_pClass->GetName(), "Constructor not found")->Throw();
		return;
	}

	m_pInternalSetEntityMethod = pEntityComponentClass->FindMethod("SetEntity", 2);

	m_pInitializeMethod = m_pClass->FindMethodWithDescInInheritedClasses("OnInitialize()", pEntityComponentClass);

	auto tryGetMethod = [this, pEntityComponentClass](const char* szMethodSignature, EEntityEvent associatedEvent) -> std::weak_ptr<CMonoMethod>
	{
		std::weak_ptr<CMonoMethod> pMethod = m_pClass->FindMethodWithDescInInheritedClasses(szMethodSignature, pEntityComponentClass);
		if (!pMethod.expired())
		{
			m_eventMask |= ENTITY_EVENT_BIT(associatedEvent);
		}
		else
		{
			m_eventMask &= ~ENTITY_EVENT_BIT(associatedEvent);
		}

		return pMethod;
	};

	m_pGameStartMethod = tryGetMethod("OnGameplayStart()", ENTITY_EVENT_START_GAME);
	m_pTransformChangedMethod = tryGetMethod("OnTransformChanged()", ENTITY_EVENT_XFORM);
	m_pGameModeChangeMethod = tryGetMethod("OnEditorGameModeChange(bool)", ENTITY_EVENT_RESET);
	m_pHideMethod = tryGetMethod("OnHide()", ENTITY_EVENT_HIDE);
	m_pUnHideMethod = tryGetMethod("OnUnhide()", ENTITY_EVENT_UNHIDE);
	m_pPrePhysicsUpdateMethod = tryGetMethod("OnPrePhysicsUpdate(single)", ENTITY_EVENT_PREPHYSICSUPDATE);
	m_pRemoveMethod = tryGetMethod("OnRemove()", ENTITY_EVENT_DONE);

	m_pUpdateMethod = tryGetMethod("OnUpdate(single)", ENTITY_EVENT_UPDATE);
	m_pUpdateMethodEditing = tryGetMethod("OnEditorUpdate(single)", ENTITY_EVENT_UPDATE);
	if (!m_pUpdateMethod.expired() || !m_pUpdateMethodEditing.expired())
	{
		m_eventMask |= ENTITY_EVENT_BIT(ENTITY_EVENT_UPDATE);
	}

	if (!m_pClass->FindMethodWithDescInInheritedClasses("OnCollision(CollisionEvent)", pEntityComponentClass).expired())
	{
		m_pCollisionMethod = pEntityComponentClass->FindMethod("OnCollisionInternal", 12);
		m_eventMask |= ENTITY_EVENT_BIT(ENTITY_EVENT_COLLISION);
	}
	else
	{
		m_pCollisionMethod.reset();
		m_eventMask &= ~ENTITY_EVENT_BIT(ENTITY_EVENT_COLLISION);
	}

	if (prevEventMask != m_eventMask)
	{
		for (const std::weak_ptr<CManagedEntityComponent>& weakComponent : m_componentInstances)
		{
			if (std::shared_ptr<CManagedEntityComponent> pComponent = weakComponent.lock())
			{
				pComponent->GetEntity()->UpdateComponentEventMask(pComponent.get());
			}
		}
	}
}

std::shared_ptr<IEntityComponent> CManagedEntityComponentFactory::CreateFromPool() const
{
	// We allocate extra memory for entity components in order to support getting properties from the Schematyc callbacks at the end of this file.
	// This allows us to get both the property being processed and the component itself from one pointer currently being handled by Schematyc
	// Otherwise we would need to refactor Schematyc to utilize std::function, introducing extra overhead in terms of both memory and performance.
	struct CustomDeleter
	{
		void operator()(CManagedEntityComponent* p)
		{
			// Explicitly call destructors of properties
			for (size_t i = 0, n = p->GetPropertyCount(); i < n; ++i)
			{
				SPropertyValue* pPropertyValue = reinterpret_cast<SPropertyValue*>(reinterpret_cast<uintptr_t>(p) + sizeof(CManagedEntityComponent) + i * sizeof(SPropertyValue));
				pPropertyValue->~SPropertyValue();
			}

			// Explicit call to the destructor
			p->~CManagedEntityComponent();

			// Memory aligned free
			CryModuleFree(p);
		}
	};

	void* pComponentBuffer = CryModuleMalloc(GetSize());
	std::shared_ptr<CManagedEntityComponent> pComponent = std::shared_ptr<CManagedEntityComponent>(new(pComponentBuffer) CManagedEntityComponent(*this), CustomDeleter());

	InitializeComponent(pComponent);

	return pComponent;
}

size_t CManagedEntityComponentFactory::GetSize() const
{
	return sizeof(CManagedEntityComponent) + sizeof(SPropertyValue) * m_properties.size();
}

std::shared_ptr<IEntityComponent> CManagedEntityComponentFactory::CreateFromBuffer(void* pComponentBuffer) const
{
	// We allocate extra memory for entity components in order to support getting properties from the Schematyc callbacks at the end of this file.
	// This allows us to get both the property being processed and the component itself from one pointer currently being handled by Schematyc
	// Otherwise we would need to refactor Schematyc to utilize std::function, introducing extra overhead in terms of both memory and performance.
	struct CustomDeleter
	{
		void operator()(CManagedEntityComponent* p)
		{
			// Explicitly call destructors of properties
			for (size_t i = 0, n = p->GetPropertyCount(); i < n; ++i)
			{
				SPropertyValue* pPropertyValue = reinterpret_cast<SPropertyValue*>(reinterpret_cast<uintptr_t>(p) + sizeof(CManagedEntityComponent) + i * sizeof(SPropertyValue));
				pPropertyValue->~SPropertyValue();
			}

			// Explicit call to the destructor
			p->~CManagedEntityComponent();
		}
	};

	std::shared_ptr<CManagedEntityComponent> pComponent = std::shared_ptr<CManagedEntityComponent>(new(pComponentBuffer) CManagedEntityComponent(*this), CustomDeleter());

	InitializeComponent(pComponent);

	return pComponent;
}

void CManagedEntityComponentFactory::InitializeComponent(std::shared_ptr<CManagedEntityComponent> pComponent) const
{
	// Keep a weak reference to all objects, this allows us to reallocate components on deserialization
	const_cast<CManagedEntityComponentFactory*>(this)->m_componentInstances.emplace_back(pComponent);

	// Construct properties
	for (auto it = m_properties.begin(); it != m_properties.end(); ++it)
	{
		size_t offsetFromParent = sizeof(CManagedEntityComponent) + std::distance(m_properties.begin(), it) * sizeof(SPropertyValue);
		SPropertyValue* pPropertyValue = reinterpret_cast<SPropertyValue*>((reinterpret_cast<uintptr_t>(pComponent.get()) + offsetFromParent));

		new(pPropertyValue) SPropertyValue(*it->get(), nullptr, pComponent->GetObject());
	}
}

void CManagedEntityComponentFactory::OnClassDeserialized(MonoInternals::MonoClass* pMonoClass, const Schematyc::SSourceFileInfo& managedSourceFileInfo, const char* szName, const char* szUiCategory, const char* szUiDescription, const char* szIcon)
{
	m_pClass->SetMonoClass(pMonoClass);

	m_classDescription.SetName(Schematyc::CTypeName(m_pClass->GetName()));
	m_classDescription.SetLabel(szName);
	m_classDescription.SetEditorCategory(szUiCategory);
	m_classDescription.SetDescription(szUiDescription);
	m_classDescription.SetIcon(szIcon);

	m_classDescription.SetComponentFlags({ IEntityComponent::EFlags::Socket, IEntityComponent::EFlags::Attach });

	m_schematycFunctions.clear();
	m_schematycSignals.clear();

	m_classDescription.Clear();

	m_isRegistered = true;

	CacheMethods(false);
}

void CManagedEntityComponentFactory::FinalizeComponentRegistration()
{
	if (m_pClass->GetAssembly()->GetDomain()->IsReloading())
	{
		// Re-allocate all components, since the number of properties has changed
		// TODO: Only do this if needed
		for (auto it = m_componentInstances.begin(); it != m_componentInstances.end(); )
		{
			if (std::shared_ptr<CManagedEntityComponent> pComponent = it->lock())
			{
				// Reallocate the component
				std::shared_ptr<CManagedEntityComponent> pOldComponent = pComponent;

				uint32 entityComponentSize = sizeof(CManagedEntityComponent) + sizeof(SPropertyValue) * m_properties.size();
				CManagedEntityComponent* pRawComponent = static_cast<CManagedEntityComponent*>(CryModuleMalloc(entityComponentSize));

				struct CustomDeleter
				{
					void operator()(CManagedEntityComponent* p)
					{
						// Explicitly call destructors of properties
						for (size_t i = 0, n = p->GetPropertyCount(); i < n; ++i)
						{
							SPropertyValue* pPropertyValue = reinterpret_cast<SPropertyValue*>(reinterpret_cast<uintptr_t>(p) + sizeof(CManagedEntityComponent) + i * sizeof(SPropertyValue));
							pPropertyValue->~SPropertyValue();
						}

						// Explicit call to the destructor
						p->~CManagedEntityComponent();

						// Memory aligned free
						CryModuleFree(p);
					}
				};

				pComponent = std::shared_ptr<CManagedEntityComponent>(new(pRawComponent) CManagedEntityComponent(std::move(*pOldComponent)), CustomDeleter());
				// Re-assign the weak pointer in m_componentInstances
				*it = pComponent;

				// Replace the component in the entity
				pOldComponent->GetEntity()->ReplaceComponent(pOldComponent.get(), pComponent);

				for (auto propertyIt = m_properties.begin(), propertyEnd = m_properties.end(); propertyIt != propertyEnd; ++propertyIt)
				{
					size_t offsetFromParent = sizeof(CManagedEntityComponent) + std::distance(m_properties.begin(), propertyIt) * sizeof(SPropertyValue);
					SPropertyValue* pPropertyValue = reinterpret_cast<SPropertyValue*>((reinterpret_cast<uintptr_t>(pRawComponent) + offsetFromParent));
					bool foundOldValue = false;

					// Try to copy previous property data
					for (uint16 i = 0, n = pOldComponent->GetPropertyCount(); i < n; ++i)
					{
						size_t oldOffsetFromParent = sizeof(CManagedEntityComponent) + i * sizeof(SPropertyValue);
						const SPropertyValue* pOldPropertyValue = reinterpret_cast<const SPropertyValue*>((reinterpret_cast<uintptr_t>(pOldComponent.get()) + oldOffsetFromParent));

						if (!strcmp(pOldPropertyValue->typeDescription.pProperty->GetName(), propertyIt->get()->pProperty->GetName()))
						{
							new(pPropertyValue) SPropertyValue(*propertyIt->get(), pOldPropertyValue->pObject, pComponent->GetObject());
							foundOldValue = true;

							break;
						}
					}

					// Default initialize
					if (!foundOldValue)
					{
						new(pPropertyValue) SPropertyValue(*propertyIt->get(), nullptr, pComponent->GetObject());
					}
				}

				++it;
			}
			else
			{
				// Clear out invalid components
				it = m_componentInstances.erase(it);
			}
		}
	}
}

void CManagedEntityComponentFactory::RemoveAllComponentInstances()
{
	for (auto it = m_componentInstances.begin(); it != m_componentInstances.end(); ++it)
	{
		if (std::shared_ptr<CManagedEntityComponent> pComponent = it->lock())
		{
			pComponent->GetEntity()->RemoveComponent(pComponent.get());
		}
	}

	m_componentInstances.clear();
}

void CManagedEntityComponentFactory::AddProperty(MonoInternals::MonoReflectionProperty* pReflectionProperty, const char* szPropertyName, const char* szPropertyLabel, const char* szPropertyDesc, EEntityPropertyType type, MonoInternals::MonoObject* pDefaultValue)
{
	if (std::shared_ptr<CMonoProperty> pProperty = CMonoClass::MakeProperty(pReflectionProperty, szPropertyName))
	{
		MonoInternals::MonoTypeEnum monoType = (MonoInternals::MonoTypeEnum)MonoInternals::mono_type_get_type(MonoInternals::mono_signature_get_return_type(pProperty->GetGetMethod().GetSignature()));

		std::shared_ptr<CMonoObject> pDefaultValueObject;
		if (pDefaultValue != nullptr)
		{
			std::shared_ptr<CMonoClass> pUnderlyingClass = pProperty->GetUnderlyingClass();
			pDefaultValueObject = pUnderlyingClass->CreateFromMonoObject(pDefaultValue);
		}

		// First check if the property was already registered (if deserializing)
		for (auto it = m_properties.begin(), end = m_properties.end(); it != end; ++it)
		{
			SPropertyTypeDescription& existingTypeDescription = *it->get();

			if (!strcmp(existingTypeDescription.pProperty->GetName(), szPropertyName))
			{
				uint16 propertyIndex = static_cast<uint16>(std::distance(m_properties.begin(), it));
				// Calculate offset of member from CManagedEntityComponent pointer, 0 being start of the component
				uint32 memberOffset = sizeof(CManagedEntityComponent) + sizeof(SPropertyValue) * propertyIndex;

				existingTypeDescription.pProperty = pProperty;
				existingTypeDescription.SetDefaultValue(pDefaultValueObject);

				existingTypeDescription.monoType = monoType;
				existingTypeDescription.serializationType = type;

				m_classDescription.AddMember(existingTypeDescription, memberOffset, static_cast<uint32>(std::distance(m_properties.begin(), it)), szPropertyName, szPropertyLabel, szPropertyDesc,
				                             stl::make_unique<CManagedEntityPropertyDefaultValue>(*m_properties.back().get(), pDefaultValueObject));

				return;
			}
		}

		uint16 propertyIndex = static_cast<uint16>(m_properties.size());
		// Calculate offset of member from CManagedEntityComponent pointer, 0 being start of the component
		uint32 memberOffset = sizeof(CManagedEntityComponent) + sizeof(SPropertyValue) * propertyIndex;

		m_properties.emplace_back(stl::make_unique<SPropertyTypeDescription>(pProperty, type, monoType, pDefaultValueObject));

		m_classDescription.AddMember(*m_properties.back().get(), memberOffset, m_properties.size(), szPropertyName, szPropertyLabel, szPropertyDesc,
		                             stl::make_unique<CManagedEntityPropertyDefaultValue>(*m_properties.back().get(), pDefaultValueObject));
	}
}

const int hash_value = 29;

void CManagedEntityComponentFactory::AddFunction(MonoInternals::MonoReflectionMethod* pReflectionMethod)
{
	// Hacky workaround for no API access for getting MonoProperty from reflection data
	// We should expose this to Mono and send the change back.
	struct InternalMonoReflectionMethod
	{
		MonoInternals::MonoObject          object;
		MonoInternals::MonoMethod*         method;
		MonoInternals::MonoString*         name;
		MonoInternals::MonoReflectionType* reftype;
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

Schematyc::ETypeCategory GetSchematycPropertyType(MonoInternals::MonoTypeEnum type, std::shared_ptr<CMonoProperty> pMonoProperty)
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

	if (type == MonoInternals::MONO_TYPE_VALUETYPE)
	{
		auto pClass = pMonoProperty->GetUnderlyingClass();
		CAppDomain* pAppDomain = static_cast<CAppDomain*>(GetMonoRuntime()->GetActiveDomain());

		if (pClass.get() == pAppDomain->GetVector2Class() ||
			pClass.get() == pAppDomain->GetVector3Class() ||
			pClass.get() == pAppDomain->GetVector4Class() ||
			pClass.get() == pAppDomain->GetQuaternionClass() ||
			pClass.get() == pAppDomain->GetAngles3Class() ||
			pClass.get() == pAppDomain->GetColorClass())
		{
			return Schematyc::ETypeCategory::Class;
		}
	}

	// Tried to use unsupported type
	CRY_ASSERT(false);
	return Schematyc::ETypeCategory::Unknown;
}

template<typename T>
static bool SerializePrimitive(Serialization::IArchive& archive, const CManagedEntityComponentFactory::SPropertyValue& propertyValue, const char* szName, const char* szLabel, MonoInternals::MonoClass* pMonoClass)
{
	T value;
	if (archive.isOutput())
	{
		propertyValue.CacheManagedValueFromOwner();

		value = *propertyValue.pObject->Unbox<T>();
	}

	bool validValue = archive(value, szName, szLabel);

	if (archive.isInput())
	{
		if (!validValue)
		{
			propertyValue.CacheManagedValueFromOwner();
			value = *propertyValue.pObject->Unbox<T>();
		}

		void* pParams[1];
		pParams[0] = &value;

		MonoInternals::MonoObject* pNewValue = MonoInternals::mono_value_box(MonoInternals::mono_domain_get(), pMonoClass, pParams[0]);

		if (propertyValue.pObject != nullptr)
		{
			propertyValue.pObject->CopyFrom(pNewValue);
		}
		else
		{
			std::shared_ptr<CMonoClass> pClass = GetMonoRuntime()->GetRootDomain()->GetNetCoreLibrary().GetClassFromMonoClass(pMonoClass);

			const_cast<CManagedEntityComponentFactory::SPropertyValue&>(propertyValue).pObject = pClass->CreateFromMonoObject(pNewValue);
		}

		propertyValue.ApplyCachedValueToOwner();
	}

	return true;
}

CManagedEntityComponentFactory::SPropertyTypeDescription::SPropertyTypeDescription(std::shared_ptr<CMonoProperty> pMonoProperty, EEntityPropertyType serType, MonoInternals::MonoTypeEnum type, std::shared_ptr<CMonoObject> pDefaultValue)
	: Schematyc::CCommonTypeDesc(GetSchematycPropertyType(type, pMonoProperty))
	, pProperty(pMonoProperty)
	, serializationType(serType)
	, monoType(type)
{
	m_operators.defaultConstruct = &SPropertyTypeDescription::DefaultConstruct;
	m_operators.destruct = &SPropertyTypeDescription::Destroy;
	m_operators.copyConstruct = &SPropertyTypeDescription::CopyConstruct;
	m_operators.copyAssign = &SPropertyTypeDescription::CopyAssign;
	m_operators.equals = &SPropertyTypeDescription::Equals;
	m_operators.serialize = &SPropertyTypeDescription::Serialize;
	m_operators.toString = &SPropertyTypeDescription::ToString;

	m_size = sizeof(CManagedEntityComponentFactory::SPropertyValue);

	m_pDefaultValue = stl::make_unique<CManagedEntityPropertyDefaultValue>(*this, pDefaultValue);
}

void CManagedEntityComponentFactory::SPropertyTypeDescription::SetDefaultValue(std::shared_ptr<CMonoObject> pDefaultValue)
{
	m_pDefaultValue = stl::make_unique<CManagedEntityPropertyDefaultValue>(*this, pDefaultValue);
}

void CManagedEntityComponentFactory::SPropertyTypeDescription::DefaultConstruct(void* pPropertyAddress)
{
	// Not implemented, catch if this is actually used
	CRY_ASSERT(false);
}

void CManagedEntityComponentFactory::SPropertyTypeDescription::Destroy(void* pPropertyAddress)
{
	SPropertyValue* pPropertyValue = static_cast<CManagedEntityComponentFactory::SPropertyValue*>(pPropertyAddress);
	pPropertyValue->pObject.reset();
}

void CManagedEntityComponentFactory::SPropertyTypeDescription::CopyConstruct(void* pTargetPropertyAddress, const void* pSourcePropertyAddress)
{
	SPropertyValue* pTargetPropertyValue = static_cast<CManagedEntityComponentFactory::SPropertyValue*>(pTargetPropertyAddress);
	const SPropertyValue* pSourcePropertyValue = static_cast<const CManagedEntityComponentFactory::SPropertyValue*>(pSourcePropertyAddress);

	new(pTargetPropertyValue) CManagedEntityComponentFactory::SPropertyValue(pSourcePropertyValue->typeDescription, pSourcePropertyValue->pObject != nullptr ? pSourcePropertyValue->pObject->Clone() : nullptr, nullptr);
}

void CManagedEntityComponentFactory::SPropertyTypeDescription::CopyAssign(void* pTargetPropertyAddress, const void* pSourcePropertyAddress)
{
	SPropertyValue* pTargetPropertyValue = static_cast<CManagedEntityComponentFactory::SPropertyValue*>(pTargetPropertyAddress);
	const SPropertyValue* pSourcePropertyValue = static_cast<const CManagedEntityComponentFactory::SPropertyValue*>(pSourcePropertyAddress);

	// Acquire source from the managed object (component usually)
	pSourcePropertyValue->CacheManagedValueFromOwner();

	// Copy the data to the local object
	if (pTargetPropertyValue->pObject != nullptr)
	{
		if (pSourcePropertyValue->pObject != nullptr)
		{
			pTargetPropertyValue->pObject->CopyFrom(*pSourcePropertyValue->pObject.get());
		}
		else
		{
			pTargetPropertyValue->pObject.reset();
		}
	}
	else if (pSourcePropertyValue->pObject != nullptr)
	{
		pTargetPropertyValue->pObject = pSourcePropertyValue->pObject->Clone();
	}

	// Apply to managed object, if any
	pTargetPropertyValue->ApplyCachedValueToOwner();
}

bool CManagedEntityComponentFactory::SPropertyTypeDescription::Equals(const void* pLeftPropertyAddress, const void* pRightPropertyAddress)
{
	const SPropertyValue* pLeftPropertyValue = static_cast<const CManagedEntityComponentFactory::SPropertyValue*>(pLeftPropertyAddress);
	const SPropertyValue* pRightPropertyValue = static_cast<const CManagedEntityComponentFactory::SPropertyValue*>(pRightPropertyAddress);

	// Make sure we have latest managed data
	pLeftPropertyValue->CacheManagedValueFromOwner();
	pRightPropertyValue->CacheManagedValueFromOwner();

	if (pLeftPropertyValue->pObject != nullptr && pRightPropertyValue->pObject != nullptr)
	{
		return pLeftPropertyValue->pObject->ReferenceEquals(*pRightPropertyValue->pObject.get());
	}

	return pLeftPropertyValue == nullptr && pRightPropertyValue == nullptr;
}

bool CManagedEntityComponentFactory::SPropertyTypeDescription::Serialize(Serialization::IArchive& archive, void* pPropertyAddress, const char* szName, const char* szLabel)
{
	SPropertyValue* pPropertyValue = static_cast<CManagedEntityComponentFactory::SPropertyValue*>(pPropertyAddress);

	switch (pPropertyValue->typeDescription.monoType)
	{
	case MonoInternals::MONO_TYPE_BOOLEAN:
		{
			return SerializePrimitive<bool>(archive, *pPropertyValue, szName, szLabel, MonoInternals::mono_get_boolean_class());
		}
		break;
	case MonoInternals::MONO_TYPE_STRING:
		{
			string value;
			if (archive.isOutput())
			{
				pPropertyValue->CacheManagedValueFromOwner();

				if (std::shared_ptr<CMonoString> pValue = pPropertyValue->pObject != nullptr ? pPropertyValue->pObject->ToString() : nullptr)
				{
					value = pValue->GetString();
				}
			}

			switch (pPropertyValue->typeDescription.serializationType)
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
				archive(Serialization::ParticlePicker(value), szName, szLabel);
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
			case EEntityPropertyType::Character:
				archive(Serialization::CharacterPath(value), szName, szLabel);
				break;
			case EEntityPropertyType::ActionMapName:
				archive(Serialization::ActionMapName(value), szName, szLabel);
				break;
			case EEntityPropertyType::ActionMapActionName:
				archive(Serialization::ActionMapActionName(value), szName, szLabel);
				break;
			}

			if (archive.isInput())
			{
				MonoInternals::MonoObject* pString = (MonoInternals::MonoObject*)MonoInternals::mono_string_new(MonoInternals::mono_domain_get(), value);

				if (pPropertyValue->pObject != nullptr)
				{
					pPropertyValue->pObject->CopyFrom(pString);
				}
				else
				{
					MonoInternals::MonoClass* pStringClass = MonoInternals::mono_object_get_class(pString);
					MonoInternals::MonoImage* pStringImage = MonoInternals::mono_class_get_image(pStringClass);
					MonoInternals::MonoAssembly* pStringAssembly = MonoInternals::mono_image_get_assembly(pStringImage);

					CMonoLibrary& library = GetMonoRuntime()->GetActiveDomain()->GetLibraryFromMonoAssembly(pStringAssembly, pStringImage);

					pPropertyValue->pObject = library.GetClassFromMonoClass(pStringClass)->CreateFromMonoObject(pString);
				}

				pPropertyValue->ApplyCachedValueToOwner();
			}

			return true;
		}
		break;
	case MonoInternals::MONO_TYPE_U1:
	case MonoInternals::MONO_TYPE_CHAR: // Char is unsigned by default for .NET
		{
			return SerializePrimitive<uchar>(archive, *pPropertyValue, szName, szLabel, MonoInternals::mono_get_char_class());
		}
		break;
	case MonoInternals::MONO_TYPE_I1:
		{
			return SerializePrimitive<char>(archive, *pPropertyValue, szName, szLabel, MonoInternals::mono_get_byte_class());
		}
		break;
	case MonoInternals::MONO_TYPE_I2:
		{
			return SerializePrimitive<int16>(archive, *pPropertyValue, szName, szLabel, MonoInternals::mono_get_int16_class());
		}
		break;
	case MonoInternals::MONO_TYPE_U2:
		{
			return SerializePrimitive<uint16>(archive, *pPropertyValue, szName, szLabel, MonoInternals::mono_get_uint16_class());
		}
		break;
	case MonoInternals::MONO_TYPE_I4:
		{
			return SerializePrimitive<int32>(archive, *pPropertyValue, szName, szLabel, MonoInternals::mono_get_int32_class());
		}
		break;
	case MonoInternals::MONO_TYPE_U4:
		{
			return SerializePrimitive<uint32>(archive, *pPropertyValue, szName, szLabel, MonoInternals::mono_get_uint32_class());
		}
		break;
	case MonoInternals::MONO_TYPE_I8:
		{
			return SerializePrimitive<int64>(archive, *pPropertyValue, szName, szLabel, MonoInternals::mono_get_int64_class());
		}
		break;
	case MonoInternals::MONO_TYPE_U8:
		{
			return SerializePrimitive<uint64>(archive, *pPropertyValue, szName, szLabel, MonoInternals::mono_get_uint64_class());
		}
		break;
	case MonoInternals::MONO_TYPE_R4:
		{
			return SerializePrimitive<float>(archive, *pPropertyValue, szName, szLabel, MonoInternals::mono_get_single_class());
		}
		break;
	case MonoInternals::MONO_TYPE_R8:
		{
			return SerializePrimitive<double>(archive, *pPropertyValue, szName, szLabel, MonoInternals::mono_get_double_class());
		}
		break;
	case MonoInternals::MONO_TYPE_VALUETYPE:
		{
			std::shared_ptr<CMonoClass> pClass = pPropertyValue->typeDescription.pProperty->GetUnderlyingClass();

			CAppDomain* pAppDomain = static_cast<CAppDomain*>(GetMonoRuntime()->GetActiveDomain());

			if (pClass.get() == pAppDomain->GetVector2Class())
			{
				return SerializePrimitive<Vec2>(archive, *pPropertyValue, szName, szLabel, pClass->GetMonoClass());
			}
			else if (pClass.get() == pAppDomain->GetVector3Class())
			{
				return SerializePrimitive<Vec3>(archive, *pPropertyValue, szName, szLabel, pClass->GetMonoClass());
			}
			else if (pClass.get() == pAppDomain->GetVector4Class())
			{
				return SerializePrimitive<Vec4>(archive, *pPropertyValue, szName, szLabel, pClass->GetMonoClass());
			}
			else if (pClass.get() == pAppDomain->GetQuaternionClass())
			{
				return SerializePrimitive<Quat>(archive, *pPropertyValue, szName, szLabel, pClass->GetMonoClass());
			}
			else if (pClass.get() == pAppDomain->GetAngles3Class())
			{
				return SerializePrimitive<Ang3>(archive, *pPropertyValue, szName, szLabel, pClass->GetMonoClass());
			}
			else if (pClass.get() == pAppDomain->GetColorClass())
			{
				return SerializePrimitive<ColorF>(archive, *pPropertyValue, szName, szLabel, pClass->GetMonoClass());
			}
		}
	}

	return false;
}

void CManagedEntityComponentFactory::SPropertyTypeDescription::ToString(Schematyc::IString& output, const void* pPropertyAddress)
{
	const SPropertyValue* pPropertyValue = static_cast<const CManagedEntityComponentFactory::SPropertyValue*>(pPropertyAddress);
	pPropertyValue->CacheManagedValueFromOwner();

	if (pPropertyValue->pObject != nullptr)
	{
		std::shared_ptr<CMonoString> pValue = pPropertyValue->pObject->ToString();
		output.assign(pValue->GetString());
	}
	else
	{
		output.clear();
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
		m_outputs.emplace_back(SParameter { 0, pReturnType, "Result" });
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
		  m_outputs.emplace_back(SParameter { index + 1, pType, szName, defaultValue });
		}
		else
		{
		  m_inputs.emplace_back(SParameter { index + 1, pType, szName, defaultValue });
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

		std::shared_ptr<CMonoObject> pResult = m_pMethod->Invoke(pComponent->GetObject().get(), pParameters);
		for (const SParameter& parameter : m_outputs)
		{
			MonoInternals::MonoObject* pObject = *(MonoInternals::MonoObject**)MonoInternals::mono_array_addr_with_size(pParameters, sizeof(void*), parameter.index - 1);

			switch (MonoInternals::mono_type_get_type(parameter.pType))
			{
			case MonoInternals::MONO_TYPE_BOOLEAN:
				{
					params.SetOutput(Schematyc::CUniqueId::FromUInt32(parameter.index), *(bool*)MonoInternals::mono_object_unbox(pObject));
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
					params.SetOutput(Schematyc::CUniqueId::FromUInt32(parameter.index), *(uint32*)MonoInternals::mono_object_unbox(pObject));
				}
				break;
			case MonoInternals::MONO_TYPE_I1:
			case MonoInternals::MONO_TYPE_I2:
			case MonoInternals::MONO_TYPE_I4:
			case MonoInternals::MONO_TYPE_I8: // Losing precision! TODO: Add Schematyc support for 64?
				{
					params.SetOutput(Schematyc::CUniqueId::FromUInt32(parameter.index), *(int32*)MonoInternals::mono_object_unbox(pObject));
				}
				break;
			case MonoInternals::MONO_TYPE_R4:
			case MonoInternals::MONO_TYPE_R8: // Losing precision! TODO: Add Schematyc support for 64?
				{
					params.SetOutput(Schematyc::CUniqueId::FromUInt32(parameter.index), *(float*)MonoInternals::mono_object_unbox(pObject));
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
