// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifndef HAVE_MONO_API
namespace MonoInternals
{
struct MonoException;
}
#endif

struct InternalMonoException
{
	MonoInternals::MonoObject  object;
	MonoInternals::MonoString* class_name;
	MonoInternals::MonoString* message;
	MonoInternals::MonoObject* _data;
	MonoInternals::MonoObject* inner_ex;
	MonoInternals::MonoString* help_link;
	/* Stores the IPs and the generic sharing infos
	   (vtable/MRGCTX) of the frames. */
	MonoInternals::MonoArray*  trace_ips;
	MonoInternals::MonoString* stack_trace;
	MonoInternals::MonoString* remote_stack_trace;
	// Mono uses a gint32, but in the end that's a typedef for int.
	int                        remote_stack_index;
	/* Dynamic methods referenced by the stack trace */
	MonoInternals::MonoObject* dynamic_methods;
	int                        hresult;
	MonoInternals::MonoString* source;
	MonoInternals::MonoObject* serialization_manager;
	MonoInternals::MonoObject* captured_traces;
	MonoInternals::MonoArray*  native_trace_ips;
};

class CMonoException
{
	// Begin public API
public:
	CMonoException(MonoInternals::MonoException* pException);
	void Throw();

	// Returns the exception in a nicely formatted string. Includes the inner-exception.
	string GetExceptionString();

	// Returns the name of the Type of the exception.
	string GetExceptionTypeName();

	// Returns the message part of the exception.
	string GetMessage();

	// Returns the stacktrace of the exception.
	string GetStackTrace();

	// Returns the inner-exception in a nicely formatted string.
	string GetInnerExceptionString();

protected:
	MonoInternals::MonoException* m_pException;

private:
	string MonoStringToUtf8String(MonoInternals::MonoString* pMonoString);
};
