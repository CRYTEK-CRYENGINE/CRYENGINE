// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __EXCEPTIONS_H__
#define __EXCEPTIONS_H__

#include <stdexcept>
#include <string>

class BaseException : public std::exception
{
public:
	explicit BaseException(const string& msg): msg(msg) {}
	virtual const char* what() const {return msg.c_str();}

private:
	string msg;
};

template <typename Tag> class Exception : public BaseException
{
public:
	explicit Exception(const string& msg): BaseException(msg) {}
};

#endif //__EXCEPTIONS_H__
