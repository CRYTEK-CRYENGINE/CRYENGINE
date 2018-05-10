// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*
extern "C" void __assert_fail(
		const char *assertion,
		const char *file,
		unsigned int line,
		const char *function)
  __attribute__ ((visibility("default")))
	;

extern "C" void __assert_perror_fail(
		int errnum,
		const char *file,
		unsigned int line,
		const char *function)
	__attribute__ ((visibility("default")))
	;

void AssertFail(
		const char *assertion,
		int errnum,
		const char *file,
		unsigned int line,
		const char *function);

void __assert_fail(
		const char *assertion,
		const char *file,
		unsigned int line,
		const char *function)
{
	AssertFail(assertion, 0, file, line, function);
}

void __assert_perror_fail(
		int errnum,
		const char *file,
		unsigned int line,
		const char *function)
{
	AssertFail(0, errnum, file, line, function);
}
*/
// vim:ts=2

