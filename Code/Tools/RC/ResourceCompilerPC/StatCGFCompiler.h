// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef STAT_CGF_COMPILER
#define STAT_CGF_COMPILER

#include <CryMath/Cry_Color.h>
#include "SingleThreadedConverter.h"
#include "../../CryEngine/Cry3DEngine/MeshCompiler/MeshCompiler.h"

struct ConvertContext;
class CContentCGF;
class CChunkFile;
class CPhysicsInterface;
struct CMaterialCGF;

class CStatCGFCompiler : public CSingleThreadedCompiler
{
public:
	class Error
	{
	public:
		explicit Error (int nCode);
		Error (const char* szFormat, ...);
		const char* c_str() const { return m_strReason.c_str(); }
	protected:
		string m_strReason;
	};

	CStatCGFCompiler();
	~CStatCGFCompiler();

	// IConverter methods.
	virtual const char* GetExt(int index) const
	{
		switch (index)
		{
		case 0:
			return "cga";
		case 1:
			return "cgf";
		case 2:
			return "i_cgf";
		default:
			return 0;
		}
	}

	// ICompiler methods.
	virtual void BeginProcessing(const IConfig* config) { }
	virtual void EndProcessing() { }
	virtual bool Process();

private:
	string GetOutputFileNameOnly() const;
	string GetOutputPath() const;
	void DeleteOldChunks(CContentCGF *pCGF, CChunkFile &chunkFile);
	bool IsLodFile( const string &filename ) const;

private:
	CPhysicsInterface *m_pPhysicsInterface;
};

#endif
