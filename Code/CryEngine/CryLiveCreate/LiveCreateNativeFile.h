// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _H_LIVECREATENATIVEFILE_H_
#define _H_LIVECREATENATIVEFILE_H_

namespace LiveCreate
{
/// Native (platform related) file reader
class CLiveCreateFileReader : public IDataReadStream
{
private:
	FILE* m_handle;

public:
	CLiveCreateFileReader();

	// IDataReadStream interface
	virtual void        Delete();
	virtual void        Skip(const uint32 size);
	virtual void        Read(void* pData, const uint32 size);
	virtual void        Read8(void* pData);
	virtual void        Read4(void* pData);
	virtual void        Read2(void* pData);
	virtual void        Read1(void* pData);
	virtual const void* GetPointer();

	// Create a reader for a native platform path
	static CLiveCreateFileReader* CreateReader(const char* szNativePath);

private:
	~CLiveCreateFileReader();
};
}

#endif
