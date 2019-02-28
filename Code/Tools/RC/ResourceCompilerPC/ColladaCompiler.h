// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2006.
// -------------------------------------------------------------------------
//  File name:   ColladaCompiler.h
//  Version:     v1.00
//  Created:     3/4/2006 by Michael Smith
//  Compilers:   Visual Studio.NET 2005
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __COLLADACOMPILER_H__
#define __COLLADACOMPILER_H__

#include "SingleThreadedConverter.h"

class ICryXML;
class CContentCGF;
class CPhysicsInterface;
struct sMaterialLibrary;
enum ColladaAuthorTool;
struct IPakSystem;
struct ExportFile;

class ColladaCompiler : public CSingleThreadedCompiler
{
public:
	ColladaCompiler(ICryXML* pCryXML, IPakSystem* pPakSystem);
	virtual ~ColladaCompiler();

	// IConverter methods.
	virtual const char* GetExt(int index) const;

	// ICompiler methods.
	virtual void BeginProcessing(const IConfig* config) { }
	virtual void EndProcessing() { }
	virtual bool Process();
	string GetOutputFileNameOnly() const;

private:
	void WriteMaterialLibrary(sMaterialLibrary& materialLibrary, const string& matFileName);
	
	void SetExportInfo(CContentCGF* const pCompiledCGF, ExportFile &exportFile);

	bool CompileToCGF(ExportFile &exportFile, const string& exportFileName);
	bool CompileToCHR(ExportFile &exportFile, const string& exportFileName);
	bool CompileToANM(ExportFile &exportFile, const string& exportFileName);
	bool CompileToCAF(ExportFile &exportFile, const string& exportFileName);

	void PrepareCGF(CContentCGF* pCGF);
	void FindUsedMaterialIDs(std::vector<int>& usedMaterialIDs, CContentCGF* pCGF);

private:
	ICryXML* pCryXML;
	IPakSystem* pPakSystem;
	CPhysicsInterface* pPhysicsInterface;
};

#endif //__COLLADACOMPILER_H__
