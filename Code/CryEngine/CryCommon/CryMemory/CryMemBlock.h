// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CRYMEMBLOCK_H__
#define __CRYMEMBLOCK_H__

class CryMemBlock
{
public:
	union
	{
		void* pv;
		char* pc;
	};

	uint32 size;
	uint32 ref;
	bool   own;

	CryMemBlock()
	{
		pv = 0;
		size = 0;
		ref = 0;
		own = true;
	}

	CryMemBlock(uint32 _size)
	{
		ref = 0;
		pv = 0;
		size = 0;
		Alloc(_size);
	}

	CryMemBlock(void* _pv, uint32 _size)
	{
		ref = 0;
		pv = _pv;
		size = _size;
		own = false;
	}

	~CryMemBlock()
	{
		if (own)
			Free();
		else
			Detach();
	}

	void AddRef()
	{
		ref++;
	}

	void Release()
	{
		ref--;
		if (!ref)
			delete this;
	}

	void Alloc(uint32 _size)
	{
		if (own)
			Free();
		else
			Detach();

		pv = new byte[_size];
		size = _size;
		own = true;
	}

	void Free()
	{
		if (!own)
			return;

		if (pv)
			delete[] (byte*) pv;

		pv = 0;
		size = 0;
		own = false;
	}

	void Attach(void* _pv, int _size)
	{
		if (own)
			Free();
		else
			Detach();

		pv = _pv;
		size = _size;
		own = false;
	}

	void Detach()
	{
		if (own)
			return;

		pv = 0;
		size = 0;
	}

};

#endif // __CRYMEMBLOCK_H__
