// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc/Script/IScriptElement.h>

// Forward declare classes.
class QWidget;
class QPoint;

namespace Schematyc
{
// Forward declare interfaces.
struct IScriptActionInstance;
struct IScriptClass;
struct IScriptComponentInstance;
struct IScriptEnum;
struct IScriptFunction;
struct IScriptInterface;
struct IScriptInterfaceFunction;
struct IScriptInterfaceImpl;
struct IScriptInterfaceTask;
struct IScriptModule;
struct IScriptSignal;
struct IScriptSignalReceiver;
struct IScriptState;
struct IScriptStateMachine;
struct IScriptStruct;
struct IScriptTimer;
struct IScriptVariable;

enum class EFilterType
{
	None = 0,
	Base,
	Components,
	Types,
	Variables,
	Signals,
	Graphs,
	StateMachine,
	Interfaces
};

struct SFilterAttributes
{
	const char* szName;
	const char* szOrder;
	EFilterType filterType;

	SFilterAttributes()
		: szName(nullptr)
		, szOrder(nullptr)
		, filterType(EFilterType::None)
	{}
};

namespace ScriptBrowserUtils
{
bool                      CanAddScriptElement(EScriptElementType elementType, IScriptElement* pScope);
bool                      CanRemoveScriptElement(const IScriptElement& element);
bool                      CanRenameScriptElement(const IScriptElement& element);
bool                      CanCopyScriptElement(const IScriptElement& element);
const char*               GetScriptElementTypeName(EScriptElementType scriptElementType);
SFilterAttributes         GetScriptElementFilterAttributes(EScriptElementType scriptElementType);
void                      AppendFilterTags(EScriptElementType scriptElementType, stack_string& filter);
const char*               GetScriptElementIcon(const IScriptElement& scriptElement);
void                      MakeScriptElementNameUnique(CStackString& name, IScriptElement* pScope);
void                      FindReferences(const IScriptElement& element);

IScriptEnum*              AddScriptEnum(IScriptElement* pScope);
IScriptStruct*            AddScriptStruct(IScriptElement* pScope);
IScriptSignal*            AddScriptSignal(IScriptElement* pScope);
IScriptFunction*          AddScriptFunction(IScriptElement* pScope);
IScriptInterface*         AddScriptInterface(IScriptElement* pScope);
IScriptInterfaceFunction* AddScriptInterfaceFunction(IScriptElement* pScope);
IScriptInterfaceTask*     AddScriptInterfaceTask(IScriptElement* pScope);
IScriptStateMachine*      AddScriptStateMachine(IScriptElement* pScope);
IScriptState*             AddScriptState(IScriptElement* pScope);
IScriptVariable*          AddScriptVariable(IScriptElement* pScope);
IScriptTimer*             AddScriptTimer(IScriptElement* pScope);
IScriptSignalReceiver*    AddScriptSignalReceiver(IScriptElement* pScope);
IScriptInterfaceImpl*     AddScriptInterfaceImpl(IScriptElement* pScope, const QPoint* pPosition = nullptr);
IScriptComponentInstance* AddScriptComponentInstance(IScriptElement* pScope, const QPoint* pPostion = nullptr);

bool                      RenameScriptElement(IScriptElement& element, const char* szName);
bool                      RemoveScriptElement(const IScriptElement& element);
} // ScriptBrowserUtils
} // Schematyc

