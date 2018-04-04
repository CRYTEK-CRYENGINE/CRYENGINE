#include "StdAfx.h"
#include "EntityComponentFactory.h"
#include "MonoRuntime.h"

#include "Wrappers/MonoLibrary.h"
#include "Wrappers/RootMonoDomain.h"

#include <CryPhysics/physinterface.h>

#include <CrySerialization/Decorators/Resources.h>
#include <array>

class CManagedEntityPropertyDefaultValue final : public Schematyc::Utils::IDefaultValue
{
public:
	CManagedEntityPropertyDefaultValue(const CManagedEntityComponent::SProperty& property) : m_property(property) {}
	virtual ~CManagedEntityPropertyDefaultValue() {}
	virtual const void* Get() const override { return &m_property; }

	CManagedEntityComponent::SProperty m_property;
};

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
	if (m_pConstructorMethod == nullptr)
	{
		GetMonoRuntime()->GetCryCoreLibrary()->GetException("MyNamespace", "MyClass", "Constructor not found")->Throw();
		return;
	}

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
	uint32 entityComponentSize = sizeof(CManagedEntityComponent) + sizeof(CManagedEntityComponent::SProperty) * m_properties.size();
	CManagedEntityComponent* pRawComponent = (CManagedEntityComponent*)CryModuleMalloc(entityComponentSize);
	
	struct CustomDeleter
	{
		void operator()(CManagedEntityComponent* p)
		{
			for (size_t i = 0, n = p->GetManagedFactory().m_properties.size(); i < n; ++i)
			{
				CManagedEntityComponent::SProperty* pPropertyObject = (CManagedEntityComponent::SProperty*)((uintptr_t)p + sizeof(CManagedEntityComponent) + i * sizeof(CManagedEntityComponent::SProperty));
				pPropertyObject->~SProperty();
			}

			// Explicit call to the destructor
			p->~CManagedEntityComponent();

			// Memory aligned free
			CryModuleFree(p);
		}
	};

	auto pComponent = std::shared_ptr<CManagedEntityComponent>(new(pRawComponent) CManagedEntityComponent(*this), CustomDeleter());

	for(auto it = m_properties.begin(); it != m_properties.end(); ++it)
	{
		size_t offsetFromParent = sizeof(CManagedEntityComponent) + std::distance(m_properties.begin(), it) * sizeof(CManagedEntityComponent::SProperty);
		CManagedEntityComponent::SProperty* pPropertyObject = (CManagedEntityComponent::SProperty*)((uintptr_t)pComponent.get() + offsetFromParent);
		
		new(pPropertyObject) CManagedEntityComponent::SProperty((uint16)std::distance(m_properties.begin(), it), offsetFromParent, it->get()->monoType, it->get()->serializationType, nullptr, true);
	}

	return pComponent;
}

void CManagedEntityComponentFactory::AddProperty(MonoInternals::MonoReflectionProperty* pReflectionProperty, const char* szPropertyName, const char* szPropertyLabel, const char* szPropertyDesc, EEntityPropertyType type, MonoInternals::MonoObject* pDefaultValue)
{
	std::shared_ptr<CMonoProperty> pProperty = CMonoClass::MakeProperty(pReflectionProperty);
	
	// Calculate offset of member from CManagedEntityComponent pointer, 0 being start of the component
	uint32 memberOffset = sizeof(CManagedEntityComponent) + sizeof(CManagedEntityComponent::SProperty) * m_properties.size();
	uint16 propertyIndex = (uint16)m_properties.size();

	MonoInternals::MonoClass* pUnderlyingClass = pProperty->GetUnderlyingClass(pReflectionProperty);
	MonoInternals::MonoImage* pClassImage = MonoInternals::mono_class_get_image(pUnderlyingClass);
	MonoInternals::MonoAssembly* pClassAssembly = MonoInternals::mono_image_get_assembly(pClassImage);

	CMonoLibrary* pClassLibrary = GetMonoRuntime()->GetActiveDomain()->GetLibraryFromMonoAssembly(pClassAssembly);
	std::shared_ptr<CMonoClass> pMonoClass = pClassLibrary->GetClassFromMonoClass(pUnderlyingClass);

	std::shared_ptr<CMonoObject> pDefaultValueObject = pDefaultValue != nullptr ? pMonoClass->CreateFromMonoObject(pDefaultValue) : nullptr;

	m_properties.emplace_back(stl::make_unique<SProperty>(m_properties.size(), pReflectionProperty, pProperty, type, pProperty->GetType(pReflectionProperty), *pMonoClass, pDefaultValueObject));

	m_classDescription.AddMember(*m_properties.back().get(), memberOffset, m_properties.size(), szPropertyName, szPropertyLabel, szPropertyDesc, 
		stl::make_unique<CManagedEntityPropertyDefaultValue>(CManagedEntityComponent::SProperty(propertyIndex, memberOffset, m_properties.back()->monoType, m_properties.back()->serializationType, pDefaultValueObject, false)));
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
static bool SerializePrimitive(Serialization::IArchive& archive, const CMonoProperty* pProperty, std::shared_ptr<CMonoObject> pPropertyValue, const char* szName, const char* szLabel, const CMonoObject* pComponentObject)
{
	T value;
	if (archive.isOutput())
	{
		if (pProperty != nullptr)
		{
			bool bEncounteredException;
			pPropertyValue = pProperty->Get(pComponentObject, bEncounteredException);
			if (bEncounteredException || pPropertyValue == nullptr)
			{
				return false;
			}
		}

		value = *pPropertyValue->Unbox<T>();
	}

	archive(value, szName, szLabel);

	if (archive.isInput())
	{
		void* pParams[1];
		pParams[0] = &value;

		if (pProperty != nullptr)
		{
			bool bEncounteredException;
			pProperty->Set(pComponentObject, pParams, bEncounteredException);
			return !bEncounteredException;
		}
		else
		{
			MonoInternals::MonoObject* pNewValue = MonoInternals::mono_value_box(MonoInternals::mono_domain_get(), pPropertyValue->GetClass()->GetMonoClass(), pParams[0]);
			MonoInternals::mono_gc_wbarrier_object_copy(pPropertyValue->GetManagedObject(), pNewValue);
			return true;
		}
	}

	return true;
}

CManagedEntityComponentFactory::SProperty::SProperty(size_t memberOffset, MonoInternals::MonoReflectionProperty* pReflectionProperty, std::shared_ptr<CMonoProperty> pMonoProperty, EEntityPropertyType serType, MonoInternals::MonoTypeEnum type, CMonoClass& typeClass, std::shared_ptr<CMonoObject> pDefaultValue)
	: Schematyc::CCommonTypeDesc(GetSchematycPropertyType(type))
	, pProperty(pMonoProperty)
	, serializationType(serType)
	, monoType(type)
	, offsetFromComponent(memberOffset)
{
	m_operators.defaultConstruct = &SProperty::DefaultConstruct;
	m_operators.destruct = &SProperty::Destroy;
	m_operators.copyConstruct = &SProperty::CopyConstruct;
	m_operators.copyAssign = &SProperty::CopyAssign;
	m_operators.equals = &SProperty::Equals;
	m_operators.serialize = &SProperty::Serialize;
	m_operators.toString = &SProperty::ToString;

	m_size = sizeof(CManagedEntityComponent::SProperty);
	m_pDefaultValue = stl::make_unique<CManagedEntityPropertyDefaultValue>(CManagedEntityComponent::SProperty{ 0, memberOffset, type, serType, pDefaultValue, false });
}

void CManagedEntityComponentFactory::SProperty::DefaultConstruct(void* pPropertyAddress)
{
	// Not implemented, catch if this is actually used
	CRY_ASSERT(false);

	CManagedEntityComponent::SProperty* pPropertyObject = static_cast<CManagedEntityComponent::SProperty*>(pPropertyAddress);
}

void CManagedEntityComponentFactory::SProperty::Destroy(void* pPropertyAddress)
{
	CManagedEntityComponent::SProperty* pPropertyObject = static_cast<CManagedEntityComponent::SProperty*>(pPropertyAddress);
	pPropertyObject->pTempObject.reset();
}

void CManagedEntityComponentFactory::SProperty::CopyConstruct(void* pTargetPropertyAddress, const void* pSourcePropertyAddress)
{
	CManagedEntityComponent::SProperty* pTargetPropertyObject = static_cast<CManagedEntityComponent::SProperty*>(pTargetPropertyAddress);
	const CManagedEntityComponent::SProperty* pSourcePropertyObject = static_cast<const CManagedEntityComponent::SProperty*>(pSourcePropertyAddress);
	CRY_ASSERT(!pSourcePropertyObject->bBelongsToComponentInstance);

	new(pTargetPropertyObject) CManagedEntityComponent::SProperty(*pSourcePropertyObject);
}

void CManagedEntityComponentFactory::SProperty::CopyAssign(void* pTargetPropertyAddress, const void* pSourcePropertyAddress)
{
	CManagedEntityComponent::SProperty* pTargetPropertyObject = static_cast<CManagedEntityComponent::SProperty*>(pTargetPropertyAddress);
	const CManagedEntityComponent::SProperty* pSourcePropertyObject = static_cast<const CManagedEntityComponent::SProperty*>(pSourcePropertyAddress);

	if (!pTargetPropertyObject->bBelongsToComponentInstance)
	{
		if (!pSourcePropertyObject->bBelongsToComponentInstance)
		{
			if (pTargetPropertyObject->pTempObject != nullptr)
			{
				if (pSourcePropertyObject->pTempObject != nullptr)
				{
					pTargetPropertyObject->pTempObject->CopyFrom(*pSourcePropertyObject->pTempObject.get());
				}
				else
				{
					pTargetPropertyObject->pTempObject.reset();
				}
			}
			else if (pSourcePropertyObject->pTempObject != nullptr)
			{
				pTargetPropertyObject->pTempObject = pSourcePropertyObject->pTempObject->Clone();
			}
		}
		else
		{
			const CManagedEntityComponent* pSourceComponent = (const CManagedEntityComponent*)((uintptr_t)pSourcePropertyAddress - pSourcePropertyObject->offsetFromComponent);
			CManagedEntityComponentFactory::SProperty& sourcePropertyInfo = *pSourceComponent->GetManagedFactory().m_properties[pSourcePropertyObject->index].get();

			bool bException;
			std::shared_ptr<CMonoObject> pSourceObject = sourcePropertyInfo.pProperty->Get(pSourceComponent->GetObject(), bException);

			if (pSourceObject != nullptr)
			{
				if (pTargetPropertyObject->pTempObject == nullptr)
				{
					pTargetPropertyObject->pTempObject = pSourceObject->Clone();
				}
				else
				{
					CRY_ASSERT(pTargetPropertyObject->pTempObject->GetClass() == pSourceObject->GetClass());
					pTargetPropertyObject->pTempObject->CopyFrom(*pSourceObject.get());
				}
			}
			else
			{
				pTargetPropertyObject->pTempObject.reset();
			}
		}
	}
	else
	{
		bool bException;
		std::shared_ptr<CMonoObject> pSourceObject;

		if (!pSourcePropertyObject->bBelongsToComponentInstance)
		{
			pSourceObject = pSourcePropertyObject->pTempObject;
		}
		else
		{
			const CManagedEntityComponent* pSourceComponent = (const CManagedEntityComponent*)((uintptr_t)pSourcePropertyAddress - pSourcePropertyObject->offsetFromComponent);
			CManagedEntityComponentFactory::SProperty& sourcePropertyInfo = *pSourceComponent->GetManagedFactory().m_properties[pSourcePropertyObject->index].get();

			pSourceObject = sourcePropertyInfo.pProperty->Get(pSourceComponent->GetObject(), bException);
		}

		const CManagedEntityComponent* pTargetComponent = (const CManagedEntityComponent*)((uintptr_t)pTargetPropertyAddress - pTargetPropertyObject->offsetFromComponent);
		CManagedEntityComponentFactory::SProperty& targetPropertyInfo = *pTargetComponent->GetManagedFactory().m_properties[pTargetPropertyObject->index].get();
		targetPropertyInfo.pProperty->Set(pTargetComponent->GetObject(), pSourceObject.get(), bException);
	}
}

bool CManagedEntityComponentFactory::SProperty::Equals(const void* pLeftPropertyAddress, const void* pRightPropertyAddress)
{
	bool bException;
	std::shared_ptr<CMonoObject> pLeftObject, pRightObject;

	const CManagedEntityComponent::SProperty* pLeftPropertyObject = static_cast<const CManagedEntityComponent::SProperty*>(pLeftPropertyAddress);
	if (!pLeftPropertyObject->bBelongsToComponentInstance)
	{
		pLeftObject = pLeftPropertyObject->pTempObject;
	}
	else
	{
		const CManagedEntityComponent* pLeftComponent = (const CManagedEntityComponent*)((uintptr_t)pLeftPropertyAddress - pLeftPropertyObject->offsetFromComponent);
		CManagedEntityComponentFactory::SProperty& leftPropertyInfo = *pLeftComponent->GetManagedFactory().m_properties[pLeftPropertyObject->index].get();

		pLeftObject = leftPropertyInfo.pProperty->Get(pLeftComponent->GetObject(), bException);
	}

	const CManagedEntityComponent::SProperty* pRightPropertyObject = static_cast<const CManagedEntityComponent::SProperty*>(pRightPropertyAddress);
	if (!pRightPropertyObject->bBelongsToComponentInstance)
	{
		pRightObject = pRightPropertyObject->pTempObject;
	}
	else
	{
		const CManagedEntityComponent* pRightComponent = (const CManagedEntityComponent*)((uintptr_t)pRightPropertyAddress - pRightPropertyObject->offsetFromComponent);
		CManagedEntityComponentFactory::SProperty& rightPropertyInfo = *pRightComponent->GetManagedFactory().m_properties[pRightPropertyObject->index].get();

		pRightObject = rightPropertyInfo.pProperty->Get(pRightComponent->GetObject(), bException);
	}

	if (pLeftObject != nullptr && pRightObject != nullptr)
	{
		std::shared_ptr<CMonoString> pLeftString = pLeftObject->ToString();
		std::shared_ptr<CMonoString> pRightString = pRightObject->ToString();

		// Horrible string comparison check, we should find a way to compare through Mono somehow if even possible
		return !strcmp(pLeftString->GetString(), pRightString->GetString());
	}

	return pLeftObject == nullptr && pRightObject == nullptr;
}

bool CManagedEntityComponentFactory::SProperty::Serialize(Serialization::IArchive& archive, void* pPropertyAddress, const char* szName, const char* szLabel)
{
	CManagedEntityComponent::SProperty* pPropertyObject = static_cast<CManagedEntityComponent::SProperty*>(pPropertyAddress);
	
	const CMonoProperty* pProperty;
	CMonoObject* pComponentObject;

	if (pPropertyObject->bBelongsToComponentInstance)
	{
		CManagedEntityComponent* pComponent = (CManagedEntityComponent*)((uintptr_t)pPropertyAddress - pPropertyObject->offsetFromComponent);
		CManagedEntityComponentFactory::SProperty& propertyInfo = *pComponent->GetManagedFactory().m_properties[pPropertyObject->index].get();

		pProperty = propertyInfo.pProperty.get();
		pComponentObject = pComponent->GetObject();
	}
	else
	{
		pProperty = nullptr;
	}

	switch (pPropertyObject->monoType)
	{
	case MonoInternals::MONO_TYPE_BOOLEAN:
	{
		return SerializePrimitive<bool>(archive, pProperty, pPropertyObject->pTempObject, szName, szLabel, pComponentObject);
	}
	break;
	case MonoInternals::MONO_TYPE_STRING:
	{
		string value;
		if (archive.isOutput())
		{
			std::shared_ptr<CMonoObject> pPropertyValue = pPropertyObject->pTempObject;

			if (pProperty != nullptr)
			{
				bool bEncounteredException;
				pPropertyValue = pProperty->Get(pComponentObject, bEncounteredException);
				if (bEncounteredException || pPropertyValue == nullptr)
				{
					return false;
				}
			}

			if (std::shared_ptr<CMonoString> pValue = pPropertyValue != nullptr ? pPropertyValue->ToString() : nullptr)
			{
				value = pValue->GetString();
			}
		}

		switch (pPropertyObject->serializationType)
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
		}

		if (archive.isInput())
		{
			MonoInternals::MonoObject* pString = (MonoInternals::MonoObject*)MonoInternals::mono_string_new(MonoInternals::mono_domain_get(), value);

			if (pProperty != nullptr)
			{
				void* pParams[1];
				pParams[0] = pString;

				bool bEncounteredException;
				pProperty->Set(pComponentObject, pParams, bEncounteredException);

				return !bEncounteredException;
			}
			else
			{
				if (pPropertyObject->pTempObject != nullptr)
				{
					//MonoInternals::MonoObject* pBoxedString = MonoInternals::mono_value_box(MonoInternals::mono_domain_get(), pPropertyObject->pTempObject->GetClass()->GetMonoClass(), pString);
					pPropertyObject->pTempObject->CopyFrom(pString);
				}
				else
				{
					MonoInternals::MonoClass* pStringClass = MonoInternals::mono_object_get_class(pString);
					MonoInternals::MonoAssembly* pStringAssembly = MonoInternals::mono_image_get_assembly(MonoInternals::mono_class_get_image(pStringClass));

					CMonoLibrary* pLibrary = GetMonoRuntime()->GetActiveDomain()->GetLibraryFromMonoAssembly(pStringAssembly);
					
					pPropertyObject->pTempObject = pLibrary->GetClassFromMonoClass(pStringClass)->CreateFromMonoObject(pString);
				}

				return true;
			}
		}

		return true;
	}
	break;
	case MonoInternals::MONO_TYPE_U1:
	case MonoInternals::MONO_TYPE_CHAR: // Char is unsigned by default for .NET
	{
		return SerializePrimitive<uchar>(archive, pProperty, pPropertyObject->pTempObject, szName, szLabel, pComponentObject);
	}
	break;
	case MonoInternals::MONO_TYPE_I1:
	{
		return SerializePrimitive<char>(archive, pProperty, pPropertyObject->pTempObject, szName, szLabel, pComponentObject);
	}
	break;
	case MonoInternals::MONO_TYPE_I2:
	{
		return SerializePrimitive<int16>(archive, pProperty, pPropertyObject->pTempObject, szName, szLabel, pComponentObject);
	}
	break;
	case MonoInternals::MONO_TYPE_U2:
	{
		return SerializePrimitive<uint16>(archive, pProperty, pPropertyObject->pTempObject, szName, szLabel, pComponentObject);
	}
	break;
	case MonoInternals::MONO_TYPE_I4:
	{
		return SerializePrimitive<int32>(archive, pProperty, pPropertyObject->pTempObject, szName, szLabel, pComponentObject);
	}
	break;
	case MonoInternals::MONO_TYPE_U4:
	{
		return SerializePrimitive<uint32>(archive, pProperty, pPropertyObject->pTempObject, szName, szLabel, pComponentObject);
	}
	break;
	case MonoInternals::MONO_TYPE_I8:
	{
		return SerializePrimitive<int64>(archive, pProperty, pPropertyObject->pTempObject, szName, szLabel, pComponentObject);
	}
	break;
	case MonoInternals::MONO_TYPE_U8:
	{
		return SerializePrimitive<uint64>(archive, pProperty, pPropertyObject->pTempObject, szName, szLabel, pComponentObject);
	}
	break;
	case MonoInternals::MONO_TYPE_R4:
	{
		return SerializePrimitive<float>(archive, pProperty, pPropertyObject->pTempObject, szName, szLabel, pComponentObject);
	}
	break;
	case MonoInternals::MONO_TYPE_R8:
	{
		return SerializePrimitive<double>(archive, pProperty, pPropertyObject->pTempObject, szName, szLabel, pComponentObject);
	}
	break;
	}

	return false;
}

void CManagedEntityComponentFactory::SProperty::ToString(Schematyc::IString& output, const void* pPropertyAddress)
{
	std::shared_ptr<CMonoObject> pObject;

	const CManagedEntityComponent::SProperty* pPropertyObject = static_cast<const CManagedEntityComponent::SProperty*>(pPropertyAddress);
	if (!pPropertyObject->bBelongsToComponentInstance)
	{
		pObject = pPropertyObject->pTempObject;
	}
	else
	{
		const CManagedEntityComponent* pComponent = (const CManagedEntityComponent*)((uintptr_t)pPropertyAddress - pPropertyObject->offsetFromComponent);
		CManagedEntityComponentFactory::SProperty& propertyInfo = *pComponent->GetManagedFactory().m_properties[pPropertyObject->index].get();

		bool bException;
		pObject = propertyInfo.pProperty->Get(pComponent->GetObject(), bException);
	}

	if (pObject != nullptr)
	{
		std::shared_ptr<CMonoString> pValue = pObject->ToString();
		output.assign(pValue->GetString());
	}
	else
	{
		output.clear();
	}
}