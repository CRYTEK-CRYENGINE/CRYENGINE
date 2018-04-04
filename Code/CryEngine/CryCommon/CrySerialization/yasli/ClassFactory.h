/**
 *  yasli - Serialization Library.
 *  Copyright (C) 2007-2013 Evgeny Andreeshchev <eugene.andreeshchev@gmail.com>
 *                          Alexander Kotliar <alexander.kotliar@gmail.com>
 * 
 *  This code is distributed under the MIT License:
 *                          http://www.opensource.org/licenses/MIT
 */

#pragma once
#include <map>
#include <vector>

#include "Config.h"
#include "Assert.h"
#include "ClassFactoryBase.h"
#include "TypeID.h"

// #ifdef _MSC_VER
// # pragma warning(disable : 4251)
// #endif

namespace yasli{

class Archive;



class ClassFactoryManager{
public:
	static ClassFactoryManager& the(){
		static ClassFactoryManager factoryManager;
		return factoryManager;
	}

	const ClassFactoryBase* find(TypeID baseType) const{
		Factories::const_iterator it = factories_.find(baseType);
		if(it == factories_.end())
			return 0;
		else
			return it->second;
	}

	void registerFactory(TypeID type, const ClassFactoryBase* factory){
		factories_[type] = factory;
	}
protected:
	typedef std::map<TypeID, const ClassFactoryBase*> Factories;
	Factories factories_;
};

template<class BaseType>
class ClassFactory : public ClassFactoryBase{
public:
	static ClassFactory& the(){
		static ClassFactory factory;
		return factory;
	}

	class CreatorBase{
	public:
		CreatorBase() {
			if (head() == this) {
				YASLI_ASSERT(0);
			}
			if (head()) {
				next = head();
				head()->previous = this;
			}
			else {
				next = 0;
				previous = 0;
			}
			head() = this;
		}

		virtual ~CreatorBase() {
			if (previous) {
				previous->next = next;
			}
			if (this == head()) {
				head() = next;
				if (next)
					next->previous = 0;
			}
		}
		virtual BaseType* create() const = 0;
		virtual TypeID typeID() const = 0;
		const TypeDescription& description() const{ return *description_; }
#if YASLI_NO_RTTI
		void* vptr() const{ return vptr_; }
#endif
		static CreatorBase*& head() { static CreatorBase* head; return head; }
		CreatorBase* next;
		CreatorBase* previous;
	protected:
		const TypeDescription* description_;
#if YASLI_NO_RTTI
		void* vptr_;
#endif
	};

#if YASLI_NO_RTTI
	static void* extractVPtr(BaseType* ptr)
	{
		return *((void**)ptr);
	}

#endif

	template<class Derived>
	struct Annotation
	{
		Annotation(ClassFactoryBase* factory, const char* name, const char* value) { static_cast<ClassFactory<BaseType>*>(factory)->addAnnotation<Derived>(name, value); }
	};

	template<class Derived>
	class Creator : public CreatorBase{
	public:
		Creator(const TypeDescription* description, ClassFactory* factory = 0){
			this->description_ = description;
#if YASLI_NO_RTTI
			// TODO: remove unnecessary static initialisation
			Derived vptrProbe;
            CreatorBase::vptr_ = extractVPtr(&vptrProbe);
#endif
			if (!factory)
				factory = &ClassFactory::the();
			factory->registerCreator(this);
		}
		BaseType* create() const override { return new Derived(); }
		TypeID typeID() const override{ return yasli::TypeID::get<Derived>(); }
	};

	ClassFactory()
	: ClassFactoryBase(TypeID::get<BaseType>())
	{
		ClassFactoryManager::the().registerFactory(baseType_, this);
	}

	typedef std::map<string, CreatorBase*> TypeToCreatorMap;

	BaseType* create(const char* registeredName) const
	{
		if (!registeredName)
			return 0;
		if (registeredName[0] == '\0')
			return 0;
		typename TypeToCreatorMap::const_iterator it = typeToCreatorMap_.find(registeredName);
		if(it != typeToCreatorMap_.end())
			return it->second->create();
		else
			return 0;
	}

	virtual const char* getRegisteredTypeName(BaseType* ptr) const
	{
		if (ptr == 0)
			return "";
		void* vptr = extractVPtr(ptr);
		typename VPtrToCreatorMap::const_iterator it = vptrToCreatorMap_.find(vptr);
		if (it == vptrToCreatorMap_.end())
			return "";
		return it->second->description().name();
	}

	BaseType* createByIndex(int index) const
	{
		YASLI_ASSERT(size_t(index) < creators_.size());
		return creators_[index]->create();
	}

	void serializeNewByIndex(Archive& ar, int index, const char* name, const char* label) override
	{
		YASLI_ESCAPE(size_t(index) < creators_.size(), return);
		BaseType* ptr = creators_[index]->create();
		ar(*ptr, name, label);
		delete ptr;
	}
	// from ClassFactoryInterface:
	size_t size() const override{ return creators_.size(); }
	const TypeDescription* descriptionByIndex(int index) const override{
		if(size_t(index) >= int(creators_.size()))
			return 0;
		return &creators_[index]->description();
	}

	const TypeDescription* descriptionByRegisteredName(const char* name) const override{
		const size_t numCreators = creators_.size();
		for (size_t i = 0; i < numCreators; ++i) {
			if (strcmp(creators_[i]->description().name(), name) == 0)
				return &creators_[i]->description();
		}
		return 0;
	}
	// ^^^

	TypeID typeIDByRegisteredName(const char* registeredTypeName) const {
		RegisteredNameToTypeID::const_iterator it = registeredNameToTypeID_.find(registeredTypeName);
		if (it == registeredNameToTypeID_.end())
			return TypeID();
		return it->second;
	}
	// ^^^
	
	CreatorBase* creatorChain() { return CreatorBase::head(); }
	
	void registerChain(CreatorBase* head)
	{
		CreatorBase* current = head;
		while (current) {
			registerCreator(current);
			current = current->next;
		}
	}

	void unregisterChain(CreatorBase* head)
	{
		CreatorBase* current = head;
		while (current) {
			typeToCreatorMap_.erase(current->description().typeID());
#if YASLI_NO_RTTI
			vptrToCreatorMap_.erase(current->vptr());
#endif
			for (size_t i = 0; i < creators_.size(); ++i) {
				if (creators_[i] == current) {
					creators_.erase(creators_.begin() + i);
					--i;
				}
			}
			current = current->next;
		}
	}

	const char* findAnnotation(const char* registeredTypeName, const char* name) const override {
		TypeID typeID = typeIDByRegisteredName(registeredTypeName);
		AnnotationMap::const_iterator it = annotations_.find(typeID);
		if (it == annotations_.end())
			return "";
		for (size_t i = 0; i < it->second.size(); ++i) {
			if (strcmp(it->second[i].first, name) == 0)
				return it->second[i].second;
		}
		return "";
	}

protected:
	virtual void registerCreator(CreatorBase* creator){
		if (!typeToCreatorMap_.insert(std::make_pair(creator->description().name(), creator)).second) {
			YASLI_ASSERT(0 && "Type registered twice in the same factory. Was YASLI_CLASS_NAME put into header file by mistake");
		}
		creators_.push_back(creator);
		registeredNameToTypeID_[creator->description().name()] = creator->typeID();
		vptrToCreatorMap_[creator->vptr()] =  creator;
	}

	template<class T>
	void addAnnotation(const char* name, const char* value) {
		addAnnotation(yasli::TypeID::get<T>(), name, value);
	}

	virtual void addAnnotation(const yasli::TypeID& id, const char* name, const char* value) {
		annotations_[id].push_back(std::make_pair(name, value));
	}

	TypeToCreatorMap typeToCreatorMap_;
	std::vector<CreatorBase*> creators_;

	typedef std::map<void*, CreatorBase*> VPtrToCreatorMap;
	VPtrToCreatorMap vptrToCreatorMap_;

	typedef std::map<string, TypeID> RegisteredNameToTypeID;
	RegisteredNameToTypeID registeredNameToTypeID_;

	typedef std::map<TypeID, std::vector<std::pair<const char*, const char*> > > AnnotationMap;
	AnnotationMap annotations_;
};

}

#define YASLI_JOIN_HELPER(x,y,z,w) x ## y ## z ## w
#define YASLI_JOIN(x,y,z,w) YASLI_JOIN_HELPER(x,y,z,w)

#define YASLI_CLASS_NULL_HELPER(Counter, BaseType, name) \
	static bool YASLI_JOIN(baseType, _NullRegistered_, _,Counter) = yasli::ClassFactory<BaseType>::the().setNullLabel(name); \

#define YASLI_CLASS_NULL(BaseType, name) \
	YASLI_CLASS_NULL_HELPER(__COUNTER__, BaseType, name)

#define YASLI_CLASS(BaseType, Type, label) \
	static const yasli::TypeDescription Type##BaseType##_DerivedDescription(yasli::TypeID::get<Type>(), #Type, label); \
	static yasli::ClassFactory<BaseType>::Creator<Type> Type##BaseType##_Creator(&Type##BaseType##_DerivedDescription); \
	int dummyForType_##Type##BaseType;

#define YASLI_CLASS_NAME_HELPER(Counter, BaseType, Type, name, label) \
	static yasli::TypeDescription YASLI_JOIN(globalDerivedDescription,__LINE__,_,Counter)(yasli::TypeID::get<Type>(), name, label); \
	static const yasli::ClassFactory<BaseType>::Creator<Type> YASLI_JOIN(globalCreator,__LINE__,_,Counter)(&YASLI_JOIN(globalDerivedDescription,__LINE__,_,Counter)); \

#define YASLI_CLASS_NAME(BaseType, Type, name, label) \
	YASLI_CLASS_NAME_HELPER(__COUNTER__, BaseType, Type, name, label)

#define YASLI_CLASS_NAME_FOR_FACTORY(Factory, BaseType, Type, name, label) \
	static const yasli::TypeDescription Type##BaseType##_DerivedDescription(yasli::TypeID::get<Type>(), name, label); \
	static yasli::ClassFactory<BaseType>::Creator<Type> Type##BaseType##_Creator(&Type##BaseType##_DerivedDescription, &(Factory));

#define YASLI_CLASS_ANNOTATION(BaseType, Type, attributeName, attributeValue) \
	static yasli::ClassFactory<BaseType>::Annotation<Type> Type##BaseType##_Annotation(&yasli::ClassFactory<BaseType>::the(), attributeName, attributeValue);

#define YASLI_CLASS_ANNOTATION_FOR_FACTORY(factory, BaseType, Type, attributeName, attributeValue) \
	static yasli::ClassFactory<BaseType>::Annotation<Type> Type##BaseType##_Annotation(&factory, attributeName, attributeValue);

#define YASLI_FORCE_CLASS(BaseType, Type) \
	extern int dummyForType_##Type##BaseType; \
	int* dummyForTypePtr_##Type##BaseType = &dummyForType_##Type##BaseType + 1;

#if YASLI_INLINE_IMPLEMENTATION
#include "ClassFactoryImpl.h"
#endif

