// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#ifdef USE_PYTHON_SCRIPTING
	#include <CryCore/BoostHelpers.h>
	#include <boost/python.hpp>
#endif

#include "IEditor.h"
#include "ICommandManager.h"

struct SPythonModule;
struct SPythonModule;
struct SPythonCommand;

class IPythonManager
{
public:
	virtual void  RegisterPythonCommand(const SPythonCommand& command) = 0;
	virtual void  RegisterPythonModule(const SPythonModule& module) = 0;
	virtual void  Execute(const char* command) = 0;
	virtual float GetAsFloat(const char* variable) = 0;
};

struct SPythonCommand
{
	SPythonCommand(const char* moduleName, const char* functionName, const char* description, const char* example, std::function<void()> registerFunc)
		: m_module(moduleName)
		, m_name(functionName)
		, m_description(description)
		, m_example(example)
		, m_registerFunc(registerFunc)
	{}

	void Register() { GetIEditor()->GetIPythonManager()->RegisterPythonCommand(*this); }

	string                m_module;
	string                m_name;
	string                m_description;
	string                m_example;
	std::function<void()> m_registerFunc;
};

struct SPythonModule
{
	SPythonModule(const char* name)
		: m_name(name)
	{}

	bool operator==(const SPythonModule& other) const
	{
		return m_name == other.m_name;
	}

	void RegisterFunctions();

	string                      m_name;
	std::vector<SPythonCommand> m_commands;
};

typedef CAutoRegister<SPythonModule>  CAutoRegisterPythonModuleHelper;
typedef CAutoRegister<SPythonCommand> CAutoRegisterPythonCommandHelper;

#ifdef USE_PYTHON_SCRIPTING

	#define DECLARE_PYTHON_MODULE(moduleName)																										\
	  namespace Private_CryPython																													\
	  {																																				\
	  void RegisterPythonSubModule ## moduleName()																									\
	  {																																				\
		boost::python::docstring_options local_docstring_options(true, true, false);																\
		boost::python::object mainModule(boost::python::handle<>(boost::python::borrowed(PyImport_ImportModule("sandbox"))));						\
		boost::python::scope main_scope = mainModule;																								\
		boost::python::object currentModule(boost::python::handle<>(boost::python::borrowed(PyImport_AddModule("sandbox." ## # moduleName))));		\
		main_scope.attr( # moduleName) = currentModule;																								\
		boost::python::scope util_scope = currentModule;																							\
		GetIEditor()->GetIPythonManager()->RegisterPythonModule(SPythonModule( # moduleName));														\
	  }																																				\
	  CAutoRegisterPythonModuleHelper g_AutoRegPythonModuleHelper ## moduleName(RegisterPythonSubModule ## moduleName);								\
	  }

	#define REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(functionPtr, moduleName, functionName, description, example)                                  \
	  namespace Private_CryPython                                                                                                                   \
	  {                                                                                                                                             \
	  void DoRegisterPythonCommand ## moduleName ## functionName()                                                                                  \
	  {                                                                                                                                             \
	    boost::python::def( # functionName, functionPtr, # description);                                                                            \
	  }                                                                                                                                             \
	                                                                                                                                                \
	  void RegisterPythonCommand ## moduleName ## functionName()                                                                                    \
	  {                                                                                                                                             \
	    SPythonCommand( # moduleName, # functionName, # description, # example, DoRegisterPythonCommand ## moduleName ## functionName).Register();  \
	  }                                                                                                                                             \
	  CAutoRegisterPythonCommandHelper g_AutoRegPythonCmdHelper ## moduleName ## functionName(RegisterPythonCommand ## moduleName ## functionName); \
	  }

	#define REGISTER_PYTHON_OVERLOAD_COMMAND(functionPtr, moduleName, functionName, functionOverload, description, example)                         \
	  namespace Private_CryPython                                                                                                                   \
	  {                                                                                                                                             \
	  void DoRegisterPythonCommand ## moduleName ## functionName()                                                                                  \
	  {                                                                                                                                             \
	    boost::python::def( # functionName, functionPtr, functionOverload());                                                                       \
	  }                                                                                                                                             \
	                                                                                                                                                \
	  void RegisterPythonCommand ## moduleName ## functionName()                                                                                    \
	  {                                                                                                                                             \
	    SPythonCommand( # moduleName, # functionName, # description, # example, DoRegisterPythonCommand ## moduleName ## functionName).Register();  \
	  }                                                                                                                                             \
	  CAutoRegisterPythonCommandHelper g_AutoRegPythonCmdHelper ## moduleName ## functionName(RegisterPythonCommand ## moduleName ## functionName); \
	  }

	#define REGISTER_ONLY_PYTHON_COMMAND(functionPtr, moduleName, functionName, description) \
	  REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(functionPtr, moduleName, functionName, description, "");

	#define REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(functionPtr, moduleName, functionName, description, example)                                \
	  namespace Private_CryPython                                                                                                            \
	  {                                                                                                                                      \
	  void RegisterCommand ## moduleName ## functionName()                                                                                   \
	  {                                                                                                                                      \
	    ICommandManager* cmdMgr = GetIEditor()->GetICommandManager();                                                                        \
	    CommandManagerHelper::RegisterCommand(cmdMgr, # moduleName, # functionName, CCommandDescription(description), functor(functionPtr)); \
	    cmdMgr->GetCommand( # moduleName ## "." ## # functionName)->SetAvailableInScripting();                                               \
	  }                                                                                                                                      \
	  CAutoRegisterCommandHelper g_AutoRegCmdHelper ## moduleName ## functionName(RegisterCommand ## moduleName ## functionName);            \
	  }                                                                                                                                      \
	  REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(functionPtr, moduleName, functionName, description, example);

	#define REGISTER_PYTHON_COMMAND(functionPtr, moduleName, functionName, description) \
	  REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(functionPtr, moduleName, functionName, description, "");

//TODO : !("REGISTER_PYTHON_SERIALIZABLE_ENUM must do !");
	#define REGISTER_PYTHON_ENUM_BEGIN(enumType, moduleName, enumName)                                                                    \
	  namespace Private_CryPython                                                                                                         \
	  {                                                                                                                                   \
	  void DoRegisterPythonEnum ## moduleName ## enumName();                                                                              \
	  void RegisterPythonEnum ## moduleName ## enumName()                                                                                 \
	  {                                                                                                                                   \
	    SPythonCommand( # moduleName, # enumName, "enumeration", "", DoRegisterPythonEnum ## moduleName ## enumName).Register();          \
	  }                                                                                                                                   \
	  CAutoRegisterPythonCommandHelper g_AutoRegPythonEnumHelper ## moduleName ## enumName(RegisterPythonEnum ## moduleName ## enumName); \
	  void DoRegisterPythonEnum ## moduleName ## enumName()                                                                               \
	  {                                                                                                                                   \
	    using namespace boost::python;                                                                                                    \
	    enum_<enumType>( # enumName)                                                                                                      \


	#define REGISTER_PYTHON_ENUM_ITEM(item, name) .value( # name, item)

	#define REGISTER_PYTHON_ENUM_END ; } \
	  }

#else // USE_PYTHON_SCRIPTING

	#define DECLARE_PYTHON_MODULE(moduleName)
	#define REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(functionPtr, moduleName, functionName, description, example)
	#define REGISTER_PYTHON_OVERLOAD_COMMAND(functionPtr, moduleName, functionName, functionOverload, description, example)
	#define REGISTER_ONLY_PYTHON_COMMAND(functionPtr, moduleName, functionName, description)
	#define REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(functionPtr, moduleName, functionName, description, example)                    \
	  void RegisterCommand ## moduleName ## functionName()                                                                       \
	  {                                                                                                                          \
	    ICommandManager* cmdMgr = GetIEditor()->GetICommandManager();                                                            \
	    CommandManagerHelper::RegisterCommand(cmdMgr, # moduleName, # functionName, description, example, functor(functionPtr)); \
	    cmdMgr->GetCommand( # moduleName ## "." ## # functionName)->SetAvailableInScripting();                                   \
	  }                                                                                                                          \
	  CAutoRegisterCommandHelper g_AutoRegCmdHelper ## moduleName ## functionName(RegisterCommand ## moduleName ## functionName);
	#define RGEISTER_PYTHON_COMMAND(functionPtr, moduleName, functionName, description) \
	  REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(functionPtr, moduleName, functionName, description, "");
	#define REGISTER_PYTHON_ENUM_BEGIN(enumType, moduleName, enumName)
	#define REGISTER_PYTHON_ENUM_ITEM(item, name)
	#define REGISTER_PYTHON_ENUM_END

#endif // USE_PYTHON_SCRIPTING

