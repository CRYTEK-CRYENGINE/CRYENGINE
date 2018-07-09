// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __particleexporter_h__
#define __particleexporter_h__
#pragma once

// forward declarations.
class CPakFile;

/*! Class responsible for exporting particles to game format.
 */
/** Export brushes from specified Indoor to .bld file.
 */
class CParticlesExporter
{
public:
	void ExportParticles(const string& path, const string& levelName, CPakFile& pakFile);
private:
};

#endif // __particleexporter_h__

