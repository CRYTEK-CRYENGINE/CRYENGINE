// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "PropertyTreeTest.h"

#include "PropertyTree2.h"
#include "Serialization/QPropertyTree/QPropertyTree.h"

#include <CrySerialization/ClassFactory.h>
#include <CrySerialization/Decorators/ActionButton.h>
#include <CrySerialization/Decorators/Resources.h>
#include <CrySerialization/yasli/decorators/Button.h>
#include <CrySerialization/Callback.h>

#include "Notifications/NotificationCenter.h"

namespace PropertyTreeTest
{
//Note: for dynamic creation, classes are identified by their vtableptr, therefore they must have a virtual table
struct DynCreateBase
{
	int          baseint;
	virtual void Serialize(Serialization::IArchive& ar)
	{
		ar(baseint, "baseint", "Int (DynCreateBase)");
	}
};

SERIALIZATION_CLASS_NAME(DynCreateBase, DynCreateBase, "dyncreatebase", "DynCreate Base");

struct DynCreateDerived : public DynCreateBase
{
	float derivedfloat;

	virtual void Serialize(Serialization::IArchive& ar)
	{
		DynCreateBase::Serialize(ar);
		ar(derivedfloat, "derivedfloat", "Float (DynCreateDerived)");
	}
};

SERIALIZATION_CLASS_NAME(DynCreateBase, DynCreateDerived, "dyncreatederived", "DynCreate Derived");

SERIALIZATION_ENUM_DECLARE(ECountry, : uint8,
                           Albania,
                           Belarus,
                           Croatia,
                           Denmark,
                           Eritrea,
                           France
                           )

//Intended to match against MultiEditTest2 in multi-edit scenario
struct MultiEditTest
{
	int                            memberInt = 81231864;
	string                         memberString = "some string";
	string                         memberString2 = "oijzeofjiozejfoiezj";
	bool                           memberBool = true;
	std::vector<int>               intVector { 0, 1, 2, 1321 };
	std::shared_ptr<DynCreateBase> memberSharedPtr;
	ECountry                       country = ECountry::France;
	ColorB                         color = uint(0);

	void                           Serialize(Serialization::IArchive& ar)
	{
		ar(memberInt, "int", "Member Int");
		ar(memberString2, "str3", "Member String 2");
		ar(memberString, "string", "Member String");
		ar(memberBool, "bool", "Member Bool");
		ar(intVector, "intVector", "Int Vector");
		ar(memberSharedPtr, "memberSharedPtr", "Shared Ptr");
		ar(country, "enum", "Enum");
		ar(color, "color", "Color");
	}
};

struct MultiEditTest2
{
	MultiEditTest2()
	{
		memberInt = 156;
		memberFloat = 1512051.45;
		memberString = "some string";
		memberBool = false;
		memberSharedPtr = std::make_shared<DynCreateBase>();
		country = ECountry::Denmark;
		color = 0xFFFFFFFF;
	}

	int                            memberInt;
	float                          memberFloat;
	string                         memberString;
	bool                           memberBool;
	std::vector<int>               intVector { 0, 1, 2, 3, 4 };
	std::shared_ptr<DynCreateBase> memberSharedPtr;
	ECountry                       country;
	ColorB                         color;

	void                           Serialize(Serialization::IArchive& ar)
	{
		ar(memberInt, "int", "Member Int");
		ar(memberFloat, "float", "Member Float");
		ar(memberString, "string", "Member String");
		ar(memberBool, "bool", "Member Bool");
		ar(intVector, "intVector", "Int Vector");
		ar(memberSharedPtr, "memberSharedPtr", "Shared Ptr");
		ar(country, "enum", "Enum");
		ar(color, "color", "Color");
	}
};

struct TestNestedStruct
{
	TestNestedStruct()
	{
		memberInt = 123;
		memberFloat = 456.789;
		memberString = "some string";
	}

	int    memberInt;
	float  memberFloat;
	string memberString;

	void   Serialize(Serialization::IArchive& ar)
	{
		ar(memberInt, "int", "Member Int");
		ar(memberFloat, "float", "Member Float");
		ar(memberString, "string", "Member String");
	}
};

struct TestResourcePickers
{
	void Serialize(Serialization::IArchive& ar)
	{
		ar(Serialization::TextureFilename(m_texturePath), "tex", "Texture");
		ar(Serialization::TextureFilename(m_texturePath2), "tex2", "Texture2");
		ar(Serialization::MaterialPicker(m_materialPath), "mat", "Material");
		ar(Serialization::ModelFilename(m_modelPath), "mod", "Model");   //check can select multiple types
		ar(Serialization::GeomCachePicker(m_geomCachePath), "gc", "Geom Cache");
		ar(Serialization::ParticlePicker(m_pPath), "part", "Particles");
		ar(Serialization::ParticlePickerLegacy(m_plPath), "partl", "Particles Legacy");             //check legacy picker
		ar(Serialization::AnimationPath(m_animPath), "anim", "Animation");                          //caf
		ar(Serialization::AnimationPath(m_animPath2), "anim2", "AnimationOrBspace");                //caf
		ar(Serialization::CharacterPath(m_charPath), "char", "Character");                          //cdf
		ar(Serialization::SkeletonPath(m_skelPath), "skel", "Skeleton");                            //generic picker
		ar(Serialization::SkeletonOrCgaPath(m_skelPath2), "skel2", "Skeleton Or CGA");              //apparently this used to pick skel/chr/cga
		ar(Serialization::MakeResourceSelector(m_testPath, "SkinnedMesh"), "test", "Skinned Mesh"); //skin and not meshes, autogenerated
	}

	string m_texturePath;
	string m_texturePath2;
	string m_materialPath;
	string m_modelPath;
	string m_geomCachePath;
	string m_pPath;
	string m_plPath;
	string m_animPath;
	string m_animPath2;
	string m_charPath;
	string m_skelPath;
	string m_skelPath2;
	string m_testPath;
};

struct TestMath
{
	Vec3 pos;
	Quat rot;
	Vec3 scl;
	Vec3 sclLegacy;
	Matrix33_tpl<float> matrix33;
	Matrix44_tpl<float> matrix44;

	void Serialize(Serialization::IArchive& ar)
	{
		ar(Serialization::SPosition(pos), "xform.pos", "Position");
		ar(Serialization::SRotation(rot), "xform.rot", "Rotation");
		ar(Serialization::SScale(scl), "xform.scl", "Scale");
		ar(Serialization::SUniformScale(sclLegacy), "xform.scl", "Uniform Scale");   // legacy scale
		ar(matrix33, "matrix33", "matrix33");
		ar(matrix44, "matrix44", "matrix44");
	}
};

struct TestDecorators
{
	struct TestDecorators()
	{
		listvalue = "abc";
		list.push_back("abc");
		list.push_back("def");
		list.push_back("ghi");

		staticlist.push_back("abcd");
		staticlist.push_back("efgh");
		staticlist.push_back("ijkl");
	}

	void Serialize(Serialization::IArchive& ar)
	{
		ar(Serialization::Decorators::Range(i, 0, 100, 1), "i", "Int Range");
		ar(Serialization::Decorators::Range(f, 0.f, 1.f, 0.01f), "f", "Float Range");

		{
			Serialization::StringListValue value(list, listvalue.c_str());
			ar(value, "stringlistvalue", "StringListValue");
			listvalue = value.c_str();
		}

		{
			Serialization::StringListStaticValue value(staticlist, staticlistindex);
			ar(value, "staticstringlistvalue", "StringListStaticValue");
			staticlistindex = value.index();
		}
	}

	int                             i = 50;
	float                           f = 0.33f;
	string                          listvalue;
	Serialization::StringList       list;
	int                             staticlistindex = 0;
	Serialization::StringListStatic staticlist;
};

struct TestSpecialTypes
{
	TestSpecialTypes()
		: colorb((uint)0xAABBCCDD)
		, colorf((uint)0xCCDDEEFF)
		, vec3(1, 2, 3)
		, ptcolor((uint)0xAABBEEFF)
	{

	}

	void Serialize(Serialization::IArchive& ar)
	{
		ar(colorb, "colorb", "ColorB");
		ar(colorf, "colorf", "ColorF");
		ar(Serialization::Vec3AsColor(vec3), "Vec3AsColor", "Vec3AsColor");
		ar(ptcolor, "ptcolor", "property_tree::Color");
	}

	ColorB               colorb;
	ColorF               colorf;
	Vec3                 vec3;
	property_tree::Color ptcolor;
};

struct InliningTest
{
	int    i = 1234;
	string s = "abc";
	bool   b = false;

	void   Serialize(Serialization::IArchive& ar)
	{
		ar(i, "i", "^");
		ar(s, "s", "^");
		ar(b, "b", "^");
	}
};

struct TestStruct
{
	TestStruct()
	{
		memberInt = 123;
		memberFloat = 456.789;
		memberString = "some string";
		memberSharedPtr = std::make_shared<DynCreateBase>();
		memberSharedPtr = std::make_shared<DynCreateDerived>();

		structVector.emplace_back();
		structVector.emplace_back();
		structVector.emplace_back();

		strintmap["aaa"] = 1;
		strintmap["bbb"] = 2;
		strintmap["ccc"] = 3;

		callbackValue = 123.4;
	}

	int                                         memberInt;
	float                                       memberFloat;
	bool                                        memberBool;
	char                                        memberChar;
	string                                      memberString;
	TestNestedStruct                            memberStruct;
	int                                         memberInt2;
	ECountry                                    memberEnum;
	bool                                        memberBoolLinked1;
	bool                                        memberBoolLinked2;
	bool                                        memberBoolXOR;
	std::shared_ptr<DynCreateBase>              memberSharedPtr;
	std::shared_ptr<DynCreateBase>              memberSharedPtr2;
	std::vector<TestNestedStruct>               structVector;
	std::vector<int>                            intVector { 0, 1, 2, 3, 4 };
	std::vector<std::shared_ptr<DynCreateBase>> memberPtrArray;
	int                                         intStaticArray[5] { 0, 1, 2, 3, 4 };
	std::map<string, int>                       strintmap;
	std::uint16_t                               callbackValue;
	bool                                        showHiddenField;
	float                                       hiddenField;
	TestResourcePickers                         resourcePickers;
	TestDecorators                              decorators;
	TestSpecialTypes                            specialTypes;
	InliningTest                                inlining;
	TestMath                                    mathTypes;

	void                                        Serialize(Serialization::IArchive& ar)
	{
		ar(memberInt, "int", "Member Int (tooltip)");
		ar.doc("This is a tooltip! Shocking i know.");
		ar(memberFloat, "float", "Member Float");
		ar(memberBool, "bool", "Member Bool");
		ar(memberChar, "char", "Member Char");
		ar(memberString, "string", "Member String");
		ar(memberStruct, "struct", "Member Struct (tooltip)");
		ar.doc("This is a tooltip! Shocking i know.");
		ar(memberInt2, "int2", "Member Int 2");
		ar(memberEnum, "enum", "Member Enum");

		ar.openBlock("bl1", "Open Block test");
		ar(memberBoolLinked1, "booll1", "Bool 1");
		ar(memberBoolLinked2, "booll2", "Bool 2");
		ar(memberBoolXOR, "xor", "1 XOR 2");
		if (ar.isInput())
			memberBoolXOR = memberBoolLinked1 ^ memberBoolLinked2;
		ar.closeBlock();

		ar(memberSharedPtr, "sharedptr", "Member Shared Ptr");
		ar(memberSharedPtr2, "sharedptr2", "Member Shared Ptr 2");

		ar(intStaticArray, "intstaticarray", "Int Static Array");
		ar(intVector, "intvector", "Int Vector");
		ar(structVector, "structArray", "Struct Vector");
		ar(strintmap, "intmap", "String Int Map");
		ar(memberPtrArray, "ptrarray", "Dynamic Ptr Vector");

		//Buttons
		ar(Serialization::ActionButton([]() { GetIEditor()->GetNotificationCenter()->ShowInfo("Property Tree Test", "Action Button pressed!"); }), "actionbutton", "Action Button");

		yasli::Button yasliButton("Yasli Button");
		ar(yasliButton, "yaslibutton", "Yasli Button");
		if (yasliButton)
			GetIEditor()->GetNotificationCenter()->ShowInfo("Property Tree Test", "Yasli Button pressed!");

		auto callback = [&](std::uint16_t v)
		{
			GetIEditor()->GetNotificationCenter()->ShowInfo("Property Tree Test", "Callback called!");
		};

		ar(Serialization::Callback(callbackValue, callback), "callback", "Callback");

		ar.openBlock("bl2", "Test Structure Update");
		ar(showHiddenField, "showhidden", "Show Hidden Field");
		if (showHiddenField)
			ar(hiddenField, "hidden", "Hidden field");
		ar.closeBlock();

		ar(mathTypes, "mathTypes", "Math Types");
		ar(resourcePickers, "resourcePickers", "Resource Pickers");
		ar(decorators, "decorators", "Decorators");
		ar(specialTypes, "specialTypes", "Special Types");
		ar(inlining, "inlining", "Inlining");
	}
};

EDITOR_COMMON_API QWidget* CreateLegacyPTWidget()
{
	static TestStruct instance;

	auto tree = new QPropertyTree();
	tree->attach(Serialization::SStruct(instance));
	return tree;
}

EDITOR_COMMON_API QWidget* CreatePropertyTreeTestWidget()
{
	static TestStruct instance;

	auto tree = new QPropertyTree2();
	tree->attach(Serialization::SStruct(instance));
	return tree;
}

EDITOR_COMMON_API QWidget* CreatePropertyTreeTestWidget_MultiEdit()
{
	static MultiEditTest obj1;
	static MultiEditTest2 obj2;

	auto tree = new QPropertyTree2();
	tree->attach({ Serialization::SStruct(obj1), Serialization::SStruct(obj2) });
	return tree;
}

EDITOR_COMMON_API QWidget* CreatePropertyTreeTestWidget_MultiEdit2()
{
	static TestStruct obj1;
	static TestStruct obj2;

	auto tree = new QPropertyTree2();
	tree->attach({ Serialization::SStruct(obj1), Serialization::SStruct(obj2) });
	return tree;
}

EDITOR_COMMON_API QWidget* CreateLegacyPTWidget_MultiEdit()
{
	static MultiEditTest obj1;
	static MultiEditTest2 obj2;

	auto tree = new QPropertyTree();
	tree->attach({ Serialization::SStruct(obj1), Serialization::SStruct(obj2) });
	return tree;
}
}

