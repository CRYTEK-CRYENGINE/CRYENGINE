// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2012.
// -------------------------------------------------------------------------
//  File name:   AlembicCompiler.cpp
//  Created:     20. June 2012 by Axel Gneiting
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "AlembicCompiler.h"
#include "GeomCacheEncoder.h"
#include "StealingThreadPool.h"
#include "StringHelpers.h"
#include "ResourceCompiler.h"
#include "IAssetManager.h"
#include <CrySystem/XML/IXml.h>
#include "GeomCacheWriter.h"
#include "UpToDateFileHelpers.h"
#include "../../CryXML/ICryXML.h"
#include "../../CryXML/IXMLSerializer.h"
#include "../../CryEngine/Cry3DEngine/MeshCompiler/ForsythFaceReorderer.h"
#include "../../CryEngine/Cry3DEngine/MeshCompiler/TangentSpaceCalculation.h"
#include <CryString/StringUtils.h>

namespace
{
	QuatTNS FromAlembicMatrix(const Alembic::AbcGeom::M44d &abcMatrix)
	{
		Matrix34 matrix;

		matrix.m00 = (f32)abcMatrix.x[0][0];
		matrix.m10 = (f32)abcMatrix.x[0][1];
		matrix.m20 = (f32)abcMatrix.x[0][2];

		matrix.m01 = (f32)abcMatrix.x[1][0];
		matrix.m11 = (f32)abcMatrix.x[1][1];
		matrix.m21 = (f32)abcMatrix.x[1][2];

		matrix.m02 = (f32)abcMatrix.x[2][0];
		matrix.m12 = (f32)abcMatrix.x[2][1];
		matrix.m22 = (f32)abcMatrix.x[2][2];

		matrix.m03 = (f32)abcMatrix.x[3][0] / 100.0f;
		matrix.m13 = (f32)abcMatrix.x[3][1] / 100.0f;
		matrix.m23 = (f32)abcMatrix.x[3][2] / 100.0f;

		return QuatTNS(matrix);
	}

	Vec3 FromAlembicPosition(const Imath::Vec3<float> &abcPosition) 
	{
		Vec3 position;		
		position[0] = abcPosition.x / 100.0f;
		position[1] = abcPosition.y / 100.0f;
		position[2] = abcPosition.z / 100.0f;				
		return position;
	}	

	Vec4 FromAlembicColor(const Imath::Color3<float> &abcColor) 
	{
		Vec4 color;
		color[0] = abcColor.x;
		color[1] = abcColor.y;
		color[2] = abcColor.z;
		color[3] = 0.0f;
		return color;
	}

	Vec4 FromAlembicColor(const Imath::Color4<float> &abcColor) 
	{
		Vec4 color;
		color[0] = abcColor.r;
		color[1] = abcColor.g;
		color[2] = abcColor.b;
		color[3] = abcColor.a;
		return color;
	}

	Vec2 FromAlembicTexcoord(const Imath::Vec2<float> &abcTexcoord)
	{
		Vec2 texcoord;
		texcoord.x = abcTexcoord.x;
		texcoord.y = -abcTexcoord.y + 1.0f;
		return texcoord;
	}

	class GeomCacheMeshTriangleInputProxy : public ITriangleInputProxy
	{
	public:
		GeomCacheMeshTriangleInputProxy(const std::vector<uint32> &indices, const std::vector<AlembicCompilerVertex> &vertices)
			: m_indices(indices), m_vertices(vertices) 
		{
			assert(m_indices.size() % 3 == 0);
		}

		virtual uint32 GetTriangleCount() const
		{			
			return m_indices.size() / 3;
		}

		virtual void GetTriangleIndices(const uint32 indwTriNo, uint32 outdwPos[3], uint32 outdwNorm[3], uint32 outdwUV[3]) const
		{
			const uint32 indices[3] = 
			{ 
				m_indices[indwTriNo * 3], 
				m_indices[indwTriNo * 3 + 1], 
				m_indices[indwTriNo * 3 + 2] 
			};
			
			for (uint i = 0; i < 3; ++i)
			{
				// All attributes of one vertex share the same index
				outdwPos[i] = indices[i];
				outdwNorm[i] = indices[i];
				outdwUV[i] = indices[i];
			}
		}

		virtual void GetPos(const uint32 indwPos, Vec3& outfPos) const
		{
			outfPos[0] = m_vertices[indwPos].m_position[0];
			outfPos[1] = m_vertices[indwPos].m_position[1];
			outfPos[2] = m_vertices[indwPos].m_position[2];
		}

		virtual void GetUV(const uint32 indwPos, Vec2& outfUV) const
		{
			outfUV[0] = m_vertices[indwPos].m_texcoords[0];
			outfUV[1] = m_vertices[indwPos].m_texcoords[1];
		}

		virtual void GetNorm(const uint32 indwTriNo, const uint32 indwVertNo, Vec3& outfNorm) const
		{
			const uint32 index = m_indices[indwTriNo * 3 + indwVertNo];
			outfNorm[0] = m_vertices[index].m_normal[0];
			outfNorm[1] = m_vertices[index].m_normal[1];
			outfNorm[2] = m_vertices[index].m_normal[2];
		}

	private:
		const std::vector<uint32> &m_indices;
		const std::vector<AlembicCompilerVertex> &m_vertices;
	};

	// Helper class to wrap different color array types
	class AlembicColorSampleArray
	{
	public:
		AlembicColorSampleArray() : pColorSamplesC3h(0), pColorSamplesC3f(0), pColorSamplesC3c(0), 
			pColorSamplesC4h(0), pColorSamplesC4f(0), pColorSamplesC4c(0), pColorIndices(0) {}

		AlembicColorSampleArray(const std::string &colorParamName, Alembic::AbcGeom::IPolyMeshSchema &meshSchema, Alembic::Abc::index_t index)
			: pColorSamplesC3h(0), pColorSamplesC3f(0), pColorSamplesC3c(0), pColorSamplesC4h(0), pColorSamplesC4f(0), pColorSamplesC4c(0)
		{
			auto &arbGeomParams = meshSchema.getArbGeomParams();

			if (arbGeomParams)
			{
				const Alembic::Abc::PropertyHeader &propertyHeader = *arbGeomParams.getPropertyHeader(colorParamName);			

				if (Alembic::AbcGeom::IC3hGeomParam::matches(propertyHeader))
				{
					Alembic::AbcGeom::IC3hGeomParam param(arbGeomParams, colorParamName);
					Alembic::AbcGeom::IC3hGeomParam::Sample colorSample;
					param.getIndexed(colorSample, index);
					pColorSamplesC3h = colorSample.getVals();
					pColorIndices = colorSample.getIndices();
				}
				else if (Alembic::AbcGeom::IC3fGeomParam::matches(propertyHeader))
				{
					Alembic::AbcGeom::IC3fGeomParam param(arbGeomParams, colorParamName);
					Alembic::AbcGeom::IC3fGeomParam::Sample colorSample;
					param.getIndexed(colorSample, index);
					pColorSamplesC3f = colorSample.getVals();
					pColorIndices = colorSample.getIndices();
				}
				else if (Alembic::AbcGeom::IC3cGeomParam::matches(propertyHeader))
				{
					Alembic::AbcGeom::IC3cGeomParam param(arbGeomParams, colorParamName);
					Alembic::AbcGeom::IC3cGeomParam::Sample colorSample;
					param.getIndexed(colorSample, index);
					pColorSamplesC3c = colorSample.getVals();
					pColorIndices = colorSample.getIndices();
				}
				else if (Alembic::AbcGeom::IC4hGeomParam::matches(propertyHeader))
				{
					Alembic::AbcGeom::IC4hGeomParam param(arbGeomParams, colorParamName);
					Alembic::AbcGeom::IC4hGeomParam::Sample colorSample;
					param.getIndexed(colorSample, index);
					pColorSamplesC4h = colorSample.getVals();
					pColorIndices = colorSample.getIndices();
				}
				else if (Alembic::AbcGeom::IC4fGeomParam::matches(propertyHeader))
				{
					Alembic::AbcGeom::IC4fGeomParam param(arbGeomParams, colorParamName);
					Alembic::AbcGeom::IC4fGeomParam::Sample colorSample;
					param.getIndexed(colorSample, index);
					pColorSamplesC4f = colorSample.getVals();	
					pColorIndices = colorSample.getIndices();
				}
				else if (Alembic::AbcGeom::IC4cGeomParam::matches(propertyHeader))
				{
					Alembic::AbcGeom::IC4cGeomParam param(arbGeomParams, colorParamName);
					Alembic::AbcGeom::IC4cGeomParam::Sample colorSample;
					param.getIndexed(colorSample, index);
					pColorSamplesC4c = colorSample.getVals();
					pColorIndices = colorSample.getIndices();
				}
			}
		}

		int32_t getIndex(int32_t currentIndexArraysIndex) const
		{
			if (pColorIndices)
			{
				return (*pColorIndices)[currentIndexArraysIndex];
			}

			return 0;		
		}

		size_t GetSize() const
		{
			if (pColorSamplesC3h)
			{
				return pColorSamplesC3h->size();
			}
			else if(pColorSamplesC3f)
			{
				return pColorSamplesC3f->size();
			}
			else if(pColorSamplesC3c)
			{
				return pColorSamplesC3c->size();
			}
			if (pColorSamplesC4h)
			{
				return pColorSamplesC4h->size();
			}
			else if(pColorSamplesC4f)
			{
				return pColorSamplesC4f->size();
			}
			else if(pColorSamplesC4c)
			{
				return pColorSamplesC4c->size();
			}

			return 0;
		}

		size_t GetNumIndices() const
		{
			return pColorIndices->size();
		}

		Vec4 operator[](const size_t index) const
		{
			if (pColorSamplesC3h)
			{
				return FromAlembicColor((*pColorSamplesC3h)[index]);
			}
			else if (pColorSamplesC3f)
			{
				return FromAlembicColor((*pColorSamplesC3f)[index]);
			}
			else if (pColorSamplesC3c)
			{
				return FromAlembicColor((*pColorSamplesC3c)[index]);
			}
			else if (pColorSamplesC4h)
			{
				return FromAlembicColor((*pColorSamplesC4h)[index]);
			}
			else if (pColorSamplesC4f)
			{
				return FromAlembicColor((*pColorSamplesC4f)[index]);
			}
			else if (pColorSamplesC4c)
			{
				return FromAlembicColor((*pColorSamplesC4c)[index]);
			}

			return Vec4(0.0f, 0.0f, 0.0f, 0.0f);
		}

	private:
		Alembic::Abc::C3hArraySamplePtr pColorSamplesC3h;
		Alembic::Abc::C3fArraySamplePtr pColorSamplesC3f;
		Alembic::Abc::C3cArraySamplePtr pColorSamplesC3c;
		Alembic::Abc::C4hArraySamplePtr pColorSamplesC4h;
		Alembic::Abc::C4fArraySamplePtr pColorSamplesC4f;
		Alembic::Abc::C4cArraySamplePtr pColorSamplesC4c;
		Alembic::Abc::UInt32ArraySamplePtr pColorIndices;
	};

	struct CompileMeshJobData
	{
		CompileMeshJobData(AlembicCompiler::FrameJobGroupData *pJobGroupData, GeomCache::Mesh &mesh)
			: m_pJobGroupData(pJobGroupData), m_mesh(mesh) {}

		AlembicCompiler::FrameJobGroupData *m_pJobGroupData;
		GeomCache::Mesh &m_mesh;
	};
}

AlembicMeshDigest::AlembicMeshDigest(Alembic::AbcGeom::IPolyMeshSchema &meshSchema)
	: m_bHasNormals(meshSchema.getNormalsParam().valid()), m_bHasTexcoords(meshSchema.getUVsParam().valid()), m_bHasColors(false)
{
	Alembic::AbcGeom::ArraySampleKey positionDigest;
	meshSchema.getPositionsProperty().getKey(positionDigest);
	
	Alembic::AbcGeom::ArraySampleKey positionIndexDigest;
	meshSchema.getFaceIndicesProperty().getKey(positionIndexDigest);
	
	m_positionDigest = positionDigest;
	m_positionIndexDigest = positionIndexDigest;

	if (m_bHasNormals)
	{
		Alembic::AbcGeom::ArraySampleKey normalDigest;
		meshSchema.getNormalsParam().getValueProperty().getKey(normalDigest);
		m_normalsDigest = normalDigest;
	}

	if (m_bHasTexcoords)
	{
		Alembic::AbcGeom::ArraySampleKey texcoordDigest;
		meshSchema.getUVsParam().getValueProperty().getKey(texcoordDigest);
		m_texcoordDigest = texcoordDigest;
	}

	Alembic::AbcGeom::ArraySampleKey colorDigest;
	auto &arbParams = meshSchema.getArbGeomParams();	
	if (arbParams)
	{
		const uint numProperties = arbParams.getNumProperties();
		for (uint i = 0; i < numProperties; ++i)
		{
			auto &propertyHeader = arbParams.getPropertyHeader(i);
			const std::string &colorParamName = propertyHeader.getName();

			if (Alembic::AbcGeom::IC3hGeomParam::matches(propertyHeader))
			{
				m_bHasColors = true;
				Alembic::AbcGeom::IC3hGeomParam param(arbParams, colorParamName);
				param.getValueProperty().getKey(colorDigest);				
			}
			else if(Alembic::AbcGeom::IC3fGeomParam::matches(propertyHeader))
			{
				m_bHasColors = true;
				Alembic::AbcGeom::IC3fGeomParam param(arbParams, colorParamName);
				param.getValueProperty().getKey(colorDigest);				
			}
			else if(Alembic::AbcGeom::IC3cGeomParam::matches(propertyHeader))
			{
				m_bHasColors = true;
				Alembic::AbcGeom::IC3cGeomParam param(arbParams, colorParamName);
				param.getValueProperty().getKey(colorDigest);				
			}
			else if(Alembic::AbcGeom::IC4hGeomParam::matches(propertyHeader))
			{
				m_bHasColors = true;				
				Alembic::AbcGeom::IC4hGeomParam param(arbParams, colorParamName);
				param.getValueProperty().getKey(colorDigest);				
			}
			else if(Alembic::AbcGeom::IC4fGeomParam::matches(propertyHeader))
			{
				m_bHasColors = true;
				Alembic::AbcGeom::IC4fGeomParam param(arbParams, colorParamName);
				param.getValueProperty().getKey(colorDigest);
			}
			else if(Alembic::AbcGeom::IC4cGeomParam::matches(propertyHeader))
			{
				m_bHasColors = true;
				Alembic::AbcGeom::IC4cGeomParam param(arbParams, colorParamName);
				param.getValueProperty().getKey(colorDigest);
			}
		}		
	}
}

bool AlembicMeshDigest::operator==(const AlembicMeshDigest &digest) const
{
	if (m_bHasNormals != digest.m_bHasNormals)
	{
		return false;
	}

	if (m_bHasTexcoords != digest.m_bHasTexcoords)
	{
		return false;
	}

	if (m_bHasColors != digest.m_bHasColors)
	{
		return false;
	}

	if (m_bHasNormals && m_normalsDigest != digest.m_normalsDigest)
	{
		return false;
	}

	if (m_bHasTexcoords && m_texcoordDigest != digest.m_texcoordDigest)
	{
		return false;
	}

	if (m_bHasColors && m_colorsDigest != digest.m_colorsDigest)
	{
		return false;
	}

	return m_positionDigest == digest.m_positionDigest 
			&& m_positionIndexDigest == digest.m_positionIndexDigest;			
}

AlembicCompiler::AlembicCompiler(ICryXML *pXMLParser) 
	: m_pXMLParser(pXMLParser), m_refCount(1), m_errorCount(0) 
	, m_numAnimatedMeshes(0), m_nextJobGroupIndex(0), m_numJobGroupsRunning(0)
	, m_nextFrameToWrite(0), m_numJobsFinished(0), m_b32BitIndices(false)
	, m_bConvertYUpToZUp(false)
	, m_bMeshPrediction(false)
	, m_bUseBFrames(false)
	, m_bPlaybackFromMemory(false)
	, m_positionPrecision(0.0)
{
	m_rootNode.m_type = GeomCacheFile::eNodeType_Transform;
	m_rootNode.m_transformType = GeomCacheFile::eTransformType_Constant;
	m_rootNode.m_staticNodeData.m_bVisible = true;
	m_rootNode.m_staticNodeData.m_transform.SetIdentity();
}

void AlembicCompiler::Release()
{
	if (--m_refCount <= 0)
	{
		delete this;
	}
}

bool AlembicCompiler::Process()
{
	const string &sourcePath = m_CC.GetSourcePath();

	const bool bDialog = m_CC.config->GetAsBool("userdialog", false, false);
	if (!bDialog && !m_CC.bForceRecompiling && UpToDateFileHelpers::FileExistsAndUpToDate(GetOutputPath(), sourcePath))
	{
		// The file is up-to-date
		m_CC.pRC->AddInputOutputFilePair(sourcePath, GetOutputPath());
		return true;
	}

	// Set thread name
	ThreadUtils::SetThreadName(GetCurrentThreadId(), "AlembicCompiler::Process");

	// Create thread pool
	ThreadUtils::StealingThreadPool threadPool((m_CC.threads < 2) ? 2 : m_CC.threads, true);
	threadPool.Start();

	// Open archive
	RCLog(string("Beginning to open archive: ") + sourcePath.c_str());

	Alembic::AbcCoreFactory::IFactory factory;
	factory.setPolicy(Alembic::Abc::ErrorHandler::kQuietNoopPolicy);
	Alembic::Abc::IArchive archive = factory.getArchive(sourcePath.c_str());

	if (!archive.valid())
	{
		RCLogError("Not a valid alembic file.");
		Cleanup();
		return false;
	}

	std::string appName;
	std::string libraryVersionString;
	uint32 libraryVersion;
	std::string whenWritten;
	std::string userDescription;
	
	Alembic::Abc::GetArchiveInfo(archive, appName, libraryVersionString, libraryVersion, whenWritten,	userDescription);
	if (appName != "")
	{
		RCLog(string("  File written by: ") + appName.c_str());
		RCLog(string("  Using alembic version: ") + libraryVersionString.c_str());
		RCLog(string("  Written on: ") + whenWritten.c_str());
		RCLog(string("  User description: ") + userDescription.c_str());
	}

	IXMLSerializer *pXMLSerializer = m_pXMLParser->GetXMLSerializer();
	const string configPath = PathUtil::ReplaceExtension(sourcePath, "cbc");
	XmlNodeRef config = ReadConfig(configPath, pXMLSerializer);

	if (!config)
	{
		Cleanup();
		return false;
	}

	const string s = m_CC.config->GetAsString("vertexIndexFormat", "u16", "u16");
	m_b32BitIndices = StringHelpers::EqualsIgnoreCase(s, "u32");

	// Reset stats
	m_numExportedMeshes = 0;
	m_numVertexSplits = 0;
	m_numSharedMeshNodes = 0;

	// Check time sampling
	if (!CheckTimeSampling(archive))
	{
		Cleanup();
		return false;
	}

	// Overwrite export file name if specified by command line
	string exportFileName = GetOutputPath();
	const size_t numFrames = m_frameTimes.size();
	GeomCacheWriter geomCacheWriter(exportFileName, m_blockCompressionFormat, threadPool, numFrames, m_bPlaybackFromMemory, m_b32BitIndices);
	GeomCacheEncoder geomCacheEncoder(geomCacheWriter, m_rootNode, m_meshes, threadPool, m_bUseBFrames, m_indexFrameDistance);

	// Export static data (create mesh topologies etc.)	
	if (!CompileStaticData(archive, threadPool))
	{
		Cleanup();
		return false;
	}	

	const int verbosityLevel = m_CC.config->HasKey("verbose") ? m_CC.config->GetAsInt("verbose", 1, 1) : 0;
	if (verbosityLevel > 0)
	{
		RCLog("Compiled node tree:");
		PrintNodeTreeRec(m_rootNode, "");
	}

	// Normalize frame times (first frame is always 0.0f)
	std::vector<Alembic::Abc::chrono_t> normalizedFrameTimes;
	const Alembic::Abc::chrono_t firstFrameTime = m_frameTimes.front();
	for(uint i = 0; i < m_frameTimes.size(); ++i)
	{
		normalizedFrameTimes.push_back(m_frameTimes[i] - firstFrameTime);
	}
	
	geomCacheWriter.WriteStaticData(normalizedFrameTimes, m_meshes, m_rootNode);
	
	// Export animated data (frames)
	geomCacheEncoder.Init();
	if (!CompileAnimationData(archive, geomCacheEncoder, threadPool))
	{
		Cleanup();
		return false;
	}
	
	geomCacheEncoder.Flush();
	GeomCacheWriterStats stats = geomCacheWriter.FinishWriting();
	threadPool.WaitAllJobs();

	const Alembic::Abc::chrono_t sequenceLength = m_frameTimes.back() - m_frameTimes.front();
	const double headerDataMegaBytes = static_cast<double>(stats.m_headerDataSize) / (1024.0 * 1024.0);
	const double staticDataMegaBytes = static_cast<double>(stats.m_staticDataSize) / (1024.0 * 1024.0);
	const double animationDataMegaBytes = static_cast<double>(stats.m_animationDataSize) / (1024.0 * 1024.0);		

	RCLog("Stats");
	RCLog("  %.2f MiB header data", headerDataMegaBytes);
	RCLog("  %.2f MiB static data", staticDataMegaBytes);
	RCLog("  %.2f MiB animation data", animationDataMegaBytes);
	
	if (stats.m_uncompressedAnimationSize > 0)
	{
		const double compressionRate = static_cast<double>(stats.m_animationDataSize) / static_cast<double>(stats.m_uncompressedAnimationSize) * 100.0f;
		RCLog("  Compression ratio: %.1f%%", compressionRate);
	}	

	if (sequenceLength > 0.0)
	{
		RCLog("  Average data rate: %.2f MiB/s", (double)animationDataMegaBytes / sequenceLength);
	}	

	RCLog("Writing build configuration file...");
	pXMLSerializer->Write(config, configPath);

	Cleanup();

	if (!UpToDateFileHelpers::SetMatchingFileTime(GetOutputPath(), sourcePath))
	{
		return false;
	}

	m_CC.pRC->AddInputOutputFilePair(sourcePath, GetOutputPath());
	return m_CC.pRC->GetAssetManager()->SaveCryasset(m_CC.config, sourcePath, { GetOutputPath() });
}

string AlembicCompiler::GetOutputFileNameOnly() const
{
	const string sourceFileFinal = m_CC.config->GetAsString("overwritefilename", m_CC.sourceFileNameOnly.c_str(), m_CC.sourceFileNameOnly.c_str());
	return PathUtil::ReplaceExtension(sourceFileFinal, CRY_GEOM_CACHE_FILE_EXT);
}

string AlembicCompiler::GetOutputPath() const
{
	return PathUtil::Make(m_CC.GetOutputFolder(), GetOutputFileNameOnly());
}

bool AlembicCompiler::CheckTimeSampling(Alembic::Abc::IArchive &archive)
{
	RCLog("Checking scene time sampling...");	
	
	m_minTime = std::numeric_limits<Alembic::Abc::chrono_t>::max();
	m_maxTime = -std::numeric_limits<Alembic::Abc::chrono_t>::max();

	Alembic::Abc::IObject topObject = archive.getTop();
	CheckTimeSamplingRec(topObject);

	const uint numTimeSamplings = m_timeSamplings.size();
	if (m_minTime >= m_maxTime)
	{
		RCLogWarning("  Scene is constant");
		m_minTime = 0.0;
		m_maxTime = 0.0;
		m_frameTimes.push_back(0.0);
	}
	else if (numTimeSamplings == 1)
	{
		const Alembic::Abc::TimeSamplingType &timeSamplingType = m_timeSamplings[0].getTimeSamplingType();				
		OutputTimeSamplingType(timeSamplingType);

		const size_t numSamplesPerCycles = timeSamplingType.getNumSamplesPerCycle();
		const Alembic::Abc::chrono_t timePerCycle = timeSamplingType.getTimePerCycle();
		const size_t numCycles = (timePerCycle > 0.0f) ? (size_t)ceil((m_maxTime - m_minTime) / timePerCycle) : 0;

		for (size_t cycle = 0; cycle < numCycles; ++cycle)
		{
			for (size_t sample = 0; sample < numSamplesPerCycles; ++sample)
			{
				m_frameTimes.push_back(cycle * timePerCycle + m_timeSamplings[0].getSampleTime(sample));
			}
		}
	}
	else 
	{
		RCLogWarning("  Found %d different time samplings. Will bake scene to fixed 30 FPS.", numTimeSamplings);
		
		Alembic::Abc::chrono_t frameTime = 1.0 / 30.0;
		const size_t numFrames = (size_t)ceil((m_maxTime - m_minTime) / frameTime) + 1;
		for (size_t i = 0; i < numFrames; ++i)
		{
			m_frameTimes.push_back(i + frameTime);
		}

		for (uint i = 0; i < numTimeSamplings; ++i)
		{
			const Alembic::Abc::TimeSamplingType &timeSamplingType = m_timeSamplings[i].getTimeSamplingType();
			OutputTimeSamplingType(timeSamplingType);
		}
	}
	
	m_timeSamplings.clear();

	RCLog("  Min time in Alembic is %g seconds, max time is %g seconds.", m_minTime, m_maxTime);
	RCLog("  Exporting %Iu frames", m_frameTimes.size());

	return true;
}

void AlembicCompiler::CheckTimeSamplingRec(Alembic::Abc::IObject &currentObject)
{
	Alembic::Abc::ICompoundProperty &compoundProperty = currentObject.getProperties();	
	CheckTimeSamplingRec(compoundProperty);

	const size_t numChildren = currentObject.getNumChildren();
	for (size_t i = 0; i < numChildren; ++i)
	{
		CheckTimeSamplingRec(currentObject.getChild(i));
	}
}

void AlembicCompiler::CheckTimeSamplingRec(Alembic::Abc::ICompoundProperty &currentProperty)
{
	const size_t numProperties = currentProperty.getNumProperties();
	for (size_t i = 0; i < numProperties; ++i)
	{
		Alembic::Abc::PropertyHeader propertyHeader = currentProperty.getPropertyHeader(i);				

		if (propertyHeader.isSimple())
		{
			Alembic::Abc::TimeSamplingPtr timeSampling = propertyHeader.getTimeSampling();		

			if (propertyHeader.isArray())
			{
				Alembic::Abc::IArrayProperty childProperty(currentProperty, propertyHeader.getName());

				if (!childProperty.isConstant())
				{					
					stl::push_back_unique(m_timeSamplings, *timeSampling);
					size_t numSamples = childProperty.getNumSamples();
					m_minTime = std::min(m_minTime, timeSampling->getSampleTime(0));
					m_maxTime = std::max(m_maxTime, timeSampling->getSampleTime(numSamples - 1));
				}
			}
			else if (propertyHeader.isScalar())
			{
				Alembic::Abc::IScalarProperty childProperty(currentProperty, propertyHeader.getName());

				if (!childProperty.isConstant())
				{
					stl::push_back_unique(m_timeSamplings, *timeSampling);
					size_t numSamples = childProperty.getNumSamples();
					m_minTime = std::min(m_minTime, timeSampling->getSampleTime(0));
					m_maxTime = std::max(m_maxTime, timeSampling->getSampleTime(numSamples - 1));
				}
			}
		}
		else
		{
			Alembic::Abc::ICompoundProperty childProperty(currentProperty, propertyHeader.getName());			
			CheckTimeSamplingRec(childProperty);
		}
	}
}

void AlembicCompiler::OutputTimeSamplingType(const Alembic::Abc::TimeSamplingType &timeSamplingType)
{
	if (timeSamplingType.isUniform())
	{
		Alembic::Abc::chrono_t frameTime = timeSamplingType.getTimePerCycle();
		RCLog("  Found uniform time sampling with %g FPS.", 1.0 / frameTime);
	}
	else if (timeSamplingType.isAcyclic())
	{
		RCLog("  Found acyclic time sampling with %d frames.", timeSamplingType.getNumSamplesPerCycle());
	}
	else if (timeSamplingType.isCyclic())
	{
		RCLog("  Found cyclic time sampling with %g second cycle time and %d frames per cycle.", 
			timeSamplingType.getTimePerCycle(), timeSamplingType.getNumSamplesPerCycle());
	}
}

bool AlembicCompiler::CompileStaticData(Alembic::Abc::IArchive &archive, ThreadUtils::StealingThreadPool &threadPool)
{
	try
	{
		RCLog("Compiling static data...");

		Alembic::Abc::IObject topObject = archive.getTop();	
		std::vector<Alembic::AbcGeom::IXform> abcXformStack;	
		CompileStaticDataRec(&m_rootNode, topObject, QuatTNS(IDENTITY), abcXformStack, threadPool, false, GeomCacheFile::eTransformType_Constant);

		// Sanity check, this should never happen
		if (m_rootNode.m_transformType != GeomCacheFile::eTransformType_Constant || 
			m_rootNode.m_staticNodeData.m_transform.q != Quat(IDENTITY) || m_rootNode.m_staticNodeData.m_transform.t != Vec3(ZERO) || m_rootNode.m_staticNodeData.m_transform.s != Diag33(IDENTITY))
		{
			RCLogError("  Internal error: Root node is not constant or is not identity.");
			return false;
		}

		if (m_errorCount > 0)
		{
			RCLogError("  Failed to compile %d meshes", m_errorCount);
			return false;
		}
		
		if (m_numExportedMeshes == 0)
		{
			RCLogError("  Failed to compile any mesh");
			return false;
		}

		RCLog("  Compiled %d meshes", m_numExportedMeshes);
		RCLog("  %d nodes with shared mesh", m_numSharedMeshNodes);
		RCLog("  Split %d vertices", m_numVertexSplits);
	}
	catch(const Alembic::Util::Exception &alembicException)
	{
		RCLogError("Alembic exception while processing %s static data: %s",
			m_currentObjectPath.c_str(), alembicException.c_str());
		return false;
	}
	catch(...)
	{
		RCLogError("Unknown exception while processing %s static data", m_currentObjectPath.c_str());
		return false;
	}

	if (m_bConvertYUpToZUp)
	{
		m_rootNode.m_staticNodeData.m_transform = m_rootNode.m_staticNodeData.m_transform * Quat(Ang3(gf_PI / 2.0f, 0.0f, 0.0f));
	}

	return true;
}

bool AlembicCompiler::CompileStaticDataRec(GeomCache::Node *pParentNode, Alembic::Abc::IObject &currentObject,
	const QuatTNS &localTransform, std::vector<Alembic::AbcGeom::IXform> abcXformStack, ThreadUtils::StealingThreadPool &threadPool,
	const bool bParentRemoved, const GeomCacheFile::ETransformType parentTransform)
{
	m_currentObjectPath = currentObject.getFullName();

	QuatTNS newLocalTransform = localTransform;
	const size_t numChildren = currentObject.getNumChildren();

	// If this node doesn't have children, discard this node
	if(numChildren == 0 && !Alembic::AbcGeom::IPolyMesh::matches(currentObject.getHeader()))
	{
		return false;
	}

	// Check if node is always invisible
	Alembic::AbcGeom::IVisibilityProperty visibilityProperty = Alembic::AbcGeom::GetVisibilityProperty(currentObject);

	if (visibilityProperty && visibilityProperty.isConstant())
	{		
		int8_t rawVisibilityValue;
		rawVisibilityValue = visibilityProperty.getValue();
		Alembic::AbcGeom::ObjectVisibility visibility = Alembic::AbcGeom::ObjectVisibility(rawVisibilityValue);
				
		if (visibility == Alembic::AbcGeom::kVisibilityHidden)
		{
			RCLogWarning("  Ignoring invisible node:\n    %s.", currentObject.getFullName().c_str());
			return false;
		}
	}
		
	std::unique_ptr<GeomCache::Node> pCurrentNode(new GeomCache::Node);
	pCurrentNode->m_staticNodeData.m_transform = localTransform;

	pCurrentNode->m_name = currentObject.getFullName().c_str();

	// Flag to indicate to flatten hierarchy when encountering static node transform
	bool bFlatten = false;

	// Stores if sub tree is valid
	bool bValidSubTree = false;

	// If node is mesh
	bool bNodeIsTransform = false;

	// If transform inherits parents transform
	bool bInheritsTransform = true;

	if (Alembic::AbcGeom::IPolyMesh::matches(currentObject.getHeader()))
	{
		pCurrentNode->m_transformType = bParentRemoved ? parentTransform : GeomCacheFile::eTransformType_Constant;
		Alembic::AbcGeom::IPolyMesh mesh(currentObject, Alembic::Abc::kWrapExisting);

		if (CryStringUtils::stristr(pCurrentNode->m_name.c_str(), "cryphys"))
		{
			// Export physics proxy
			pCurrentNode->m_type = GeomCacheFile::eNodeType_PhysicsGeometry;
			bValidSubTree = CompilePhysicsGeometry(*pCurrentNode, mesh, threadPool);
		}
		else
		{
			// Export mesh
			pCurrentNode->m_type = GeomCacheFile::eNodeType_Mesh;
			bValidSubTree = CompileStaticMeshData(*pCurrentNode, mesh, threadPool);
		}
	}
	else if (Alembic::AbcGeom::IXform::matches(currentObject.getHeader()))
	{
		Alembic::AbcGeom::IXform xForm(currentObject, Alembic::Abc::kWrapExisting);		
		Alembic::AbcGeom::IXformSchema &schema = xForm.getSchema();
		bFlatten = schema.isConstant();

		const Alembic::AbcGeom::M44d &matrix = schema.getValue().getMatrix();
		const QuatTNS abcLocalTransform = FromAlembicMatrix(matrix);

		if (schema.getInheritsXforms())
		{			
			abcXformStack.push_back(Alembic::AbcGeom::IXform(currentObject, Alembic::Abc::kWrapExisting));
			newLocalTransform = localTransform * abcLocalTransform;

			pCurrentNode->m_transformType = schema.isConstant() 
				? (bParentRemoved ? parentTransform : GeomCacheFile::eTransformType_Constant)
				: GeomCacheFile::eTransformType_Animated;
		}
		else
		{
			// Completely new base transform, discard parent transforms and re-parent to root.
			bInheritsTransform = false;
			pParentNode = &m_rootNode;
			abcXformStack.clear();
			abcXformStack.push_back(Alembic::AbcGeom::IXform(currentObject, Alembic::Abc::kWrapExisting));			
			newLocalTransform = abcLocalTransform;
			
			pCurrentNode->m_transformType = schema.isConstant() 
				? GeomCacheFile::eTransformType_Constant 
				: GeomCacheFile::eTransformType_Animated;
		}

		bNodeIsTransform = true;
		pCurrentNode->m_staticNodeData.m_transform = newLocalTransform;
	}
	else
	{
		bFlatten = true;
	}

	pCurrentNode->m_abcObject = Alembic::Abc::IObject(currentObject, Alembic::Abc::kWrapExisting);
	pCurrentNode->m_abcXForms = abcXformStack;

	// Flatten hierarchy if possible
	if (bFlatten || (bNodeIsTransform && numChildren == 1))
	{
		for (size_t i = 0; i < numChildren; ++i)
		{
			bValidSubTree = CompileStaticDataRec(pParentNode, currentObject.getChild(i), 
				newLocalTransform, abcXformStack, threadPool, true, pCurrentNode->m_transformType) || bValidSubTree;
		}
	}
	else
	{			
		// Otherwise use this node as new parent. The children need a new transform stack
		abcXformStack.clear();

		for (size_t i = 0; i < numChildren; ++i)
		{
			bValidSubTree = CompileStaticDataRec(pCurrentNode.get(), currentObject.getChild(i), 
				QuatTNS(IDENTITY), abcXformStack, threadPool, false, pCurrentNode->m_transformType) || bValidSubTree;
		}

		if (bValidSubTree)
		{
			assert(pParentNode != pCurrentNode.get());
			pParentNode->m_children.push_back(std::move(pCurrentNode));
		}		
	}

	if (!bValidSubTree)
	{
		RCLogWarning("  Node contains no meshes:\n    %s", currentObject.getFullName().c_str());
	}

	// If node does not inherit parents transform return false, because path 
	// to this node is irrelevant and tree can potentially be thrown away.
	return bValidSubTree && bInheritsTransform;
}

bool AlembicCompiler::CompileStaticMeshData(GeomCache::Node &node, Alembic::AbcGeom::IPolyMesh &mesh, ThreadUtils::StealingThreadPool &threadPool)
{
#if defined(DEBUG)
	RCLog("  Processing %s", mesh.getFullName().c_str());
#endif

	// Check basic mesh parameters
	Alembic::AbcGeom::IPolyMeshSchema &meshSchema = mesh.getSchema();
	Alembic::AbcGeom::MeshTopologyVariance topologyVariance = meshSchema.getTopologyVariance();	

	std::shared_ptr<GeomCache::Mesh> pMesh(new GeomCache::Mesh);
	pMesh->m_constantStreams = GeomCacheFile::EStreams(0);
	pMesh->m_animatedStreams = GeomCacheFile::EStreams(0);

	switch (topologyVariance)
	{
	case Alembic::AbcGeom::kConstantTopology:
		pMesh->m_constantStreams = GeomCacheFile::EStreams(pMesh->m_constantStreams | GeomCacheFile::eStream_Indices);
		pMesh->m_constantStreams = GeomCacheFile::EStreams(pMesh->m_constantStreams | GeomCacheFile::eStream_Positions);
		break;
	case Alembic::AbcGeom::kHomogenousTopology:
		pMesh->m_constantStreams = GeomCacheFile::EStreams(pMesh->m_constantStreams | GeomCacheFile::eStream_Indices);
		pMesh->m_animatedStreams = GeomCacheFile::EStreams(pMesh->m_animatedStreams | GeomCacheFile::eStream_Positions);
		break;
	case Alembic::AbcGeom::kHeterogenousTopology:
		// TODO: There is code that supports heterogeneous topology, but it's not complete and untested.
		RCLogWarning("  Heterogeneous topology is currently not supported. Skipped.");
		return false;
	default:
		RCLogWarning("  Unknown alembic topology variance. Skipped.");
		return false;
	}

	// Check for normals & texcoords. We assume this is fixed over time	
	pMesh->m_bHasNormals = meshSchema.getNormalsParam().valid();
	pMesh->m_bHasTexcoords = meshSchema.getUVsParam().valid();
	CheckMeshForColors(meshSchema, *pMesh);
		
	if (!pMesh->m_bHasNormals)
	{
		RCLogWarning("  Mesh doesn't have normals. Generating smooth normals:\n    %s", mesh.getFullName().c_str());		
	}

	if (!pMesh->m_bHasTexcoords)
	{
		RCLogWarning("  Mesh doesn't have texcoords:\n    %s", mesh.getFullName().c_str());		
	}
	
	if (!pMesh->m_bHasTexcoords || meshSchema.getUVsParam().getValueProperty().isConstant())
	{
		pMesh->m_constantStreams = GeomCacheFile::EStreams(pMesh->m_constantStreams | GeomCacheFile::eStream_Texcoords);
	}
	else
	{
		pMesh->m_animatedStreams = GeomCacheFile::EStreams(pMesh->m_animatedStreams | GeomCacheFile::eStream_Texcoords);
	}

	const bool bConstantNormals = pMesh->m_bHasNormals ? meshSchema.getNormalsParam().getValueProperty().isConstant() : true;
	if (bConstantNormals && (pMesh->m_animatedStreams & (GeomCacheFile::eStream_Positions | GeomCacheFile::eStream_Texcoords)) == 0)
	{
		// If normals, positions & texcoords are constant we can use a constant qtangent stream
		pMesh->m_constantStreams = GeomCacheFile::EStreams(pMesh->m_constantStreams | GeomCacheFile::eStream_QTangents);
	}
	else
	{
		pMesh->m_animatedStreams = GeomCacheFile::EStreams(pMesh->m_animatedStreams | GeomCacheFile::eStream_QTangents);
	}

	assert((pMesh->m_constantStreams & pMesh->m_animatedStreams) == 0);

	pMesh->m_abcMesh = Alembic::AbcGeom::IPolyMesh(mesh, Alembic::Abc::kWrapExisting);
	if (pMesh->m_animatedStreams == 0)
	{
		AlembicMeshDigest meshDigest(meshSchema);

		// For constant meshes we allow mesh sharing. Check if we already 
		// have a mesh with that digest. If so share it.
		auto findIter = m_digestToMeshMap.find(meshDigest);
		if (findIter != m_digestToMeshMap.end())
		{
			++m_numSharedMeshNodes;
			node.m_pMesh = findIter->second;
			return true;
		}
				
		if (!CompileFullMesh(*pMesh, 0, node.m_staticNodeData.m_transform))
		{
			return false;
		}

		// Add mesh to digest map
		m_digestToMeshMap[meshDigest] = pMesh;
	}
	else
	{
		if (!CompileFullMesh(*pMesh, 0, node.m_staticNodeData.m_transform))
		{
			return false;
		}
	}

	node.m_pMesh = pMesh;
	
	m_meshes.push_back(pMesh.get());

	if (pMesh->m_animatedStreams != 0)
	{
		++m_numAnimatedMeshes;
	}

	// Yay, we exported one more mesh
	++m_numExportedMeshes;

	return true;
}

bool AlembicCompiler::CompilePhysicsGeometry(GeomCache::Node &node, Alembic::AbcGeom::IPolyMesh &mesh, ThreadUtils::StealingThreadPool &threadPool)
{
	const uint kSmallMeshNumIndices = 30;
	const int kMinTrisPerNode = 2;
	const int kMaxTrisPerNode = 4;

	int flags = mesh_multicontact0;
	float tolerance = 0.05f;
	flags |= mesh_full_serialization;

	if (CryStringUtils::stristr(node.m_name.c_str(), "cryphys_cylinder"))
	{
		flags |= mesh_approx_cylinder;
		tolerance = 1.0f;
	}
	else if (CryStringUtils::stristr(node.m_name.c_str(), "cryphys_box"))
	{
		flags |= mesh_approx_box;
		tolerance = 1.0f;	
	}
	else if (CryStringUtils::stristr(node.m_name.c_str(), "cryphys_sphere"))
	{
		flags |= mesh_approx_sphere;
		tolerance = 1.0f;	
	}
	else if (CryStringUtils::stristr(node.m_name.c_str(), "cryphys_capsule"))
	{
		flags |= mesh_approx_capsule;
		tolerance = 1.0f;	
	}
	else if (CryStringUtils::stristr(node.m_name.c_str(), "cryphys_mesh"))
	{
		flags |= mesh_approx_box | mesh_approx_sphere | mesh_approx_cylinder | mesh_approx_capsule;
	}
	else
	{
		RCLogWarning("  Invalid proxy type in name '%s'. Skipped.", node.m_name.c_str());
		return false;
	}

	Alembic::AbcGeom::IPolyMeshSchema &meshSchema = mesh.getSchema();
	Alembic::Abc::TimeSampling &meshTimeSampling = *meshSchema.getTimeSampling();
	size_t numMeshSamples = meshSchema.getNumSamples();

	const Alembic::Abc::chrono_t frameTime = m_frameTimes[0];
	const auto index = meshTimeSampling.getNearIndex(frameTime, numMeshSamples);

	Alembic::AbcGeom::IPolyMeshSchema::Sample sample = meshSchema.getValue(index.first);
	const auto &abcPositions = *sample.getPositions();
	const auto &abcFaceCounts = *sample.getFaceCounts();
	const size_t numAbcFaces = abcFaceCounts.size();
	const auto &abcIndices = *sample.getFaceIndices();

	std::vector<uint32> faceIndices;
	std::vector<uint16> indices;
	std::vector<Vec3> positions;
	std::vector<char> faceMaterials;
	std::unordered_map<uint32, uint16> materialIdMap = GetMeshMaterialMap(mesh, frameTime);

	size_t currentIndexArraysIndex = 0;
	for (size_t face = 0; face < numAbcFaces; ++face)
	{
		size_t numFaceVertices = abcFaceCounts[face];

		if (numFaceVertices < 3)
		{
			currentIndexArraysIndex += numFaceVertices;
			continue;
		}

		// If face is not contained in a face set, the default material ID is 0
		auto findIter = materialIdMap.find(face);
		const uint16 faceMaterialId = (findIter != materialIdMap.end()) ? findIter->second : 0;

		// First loop through face and create indices/vertices
		faceIndices.resize(0);
		for (size_t faceVertexIndex = 0; faceVertexIndex < numFaceVertices; ++faceVertexIndex)
		{
			const int32_t positionIndex = abcIndices[currentIndexArraysIndex];

			// Get position
			const Imath::Vec3<float> &abcPosition = abcPositions[positionIndex];
			const Vec3 position = FromAlembicPosition(abcPosition);

			size_t newIndex = positions.size();

			// Check if index fits in 16 bits if necessary
			if (newIndex >= 65535)
			{
				RCLogWarning("  Proxy mesh has more than 65536 vertices. Skipped:\n    %s", mesh.getFullName().c_str());
				return false;
			}

			// Add the vertex to the vertex buffer
			positions.push_back(position);

			// Add to index buffer
			faceIndices.push_back((uint16)newIndex);

			// Advance to next index
			++currentIndexArraysIndex;
		}

		// Triangulate face
		for (size_t i = 1; i < numFaceVertices - 1; ++i)
		{
			indices.push_back(faceIndices[0]);
			indices.push_back(faceIndices[i + 1]);
			indices.push_back(faceIndices[i]);
			faceMaterials.push_back((char)faceMaterialId);
		}
	}

	flags |= indices.size() <= kSmallMeshNumIndices ? mesh_SingleBB : mesh_OBB | mesh_AABB | mesh_AABB_rotated;

	IPhysicalWorld* pWorld = m_physLoader.GetWorldPtr();
	IGeomManager* pGeomManager = pWorld->GetGeomManager();
	_smart_ptr<IGeometry> pGeometry = pGeomManager->CreateMesh(positions.data(), indices.data(), faceMaterials.data(),
		NULL, indices.size() / 3, flags, tolerance, kMinTrisPerNode, kMaxTrisPerNode, 2.5f);

	if (pGeometry)
	{
		CMemStream memStream(false);
		phys_geometry *pPhysGeometry = pGeomManager->RegisterGeometry(pGeometry, faceMaterials[0]);
		pGeomManager->SavePhysGeometry(memStream, pPhysGeometry);

		node.m_physicsGeometry.resize(memStream.GetUsedSize());
		std::copy(static_cast<char*>(memStream.GetBuf()), static_cast<char*>(memStream.GetBuf()) + memStream.GetUsedSize(), node.m_physicsGeometry.begin());

		pGeomManager->UnregisterGeometry(pPhysGeometry);
		return true;
	}

	return false;
}

XmlNodeRef AlembicCompiler::ReadConfig(const string &configPath, IXMLSerializer *pXMLSerializer)
{	
	RCLog(string("Reading cache build configuration: ") + configPath.c_str());
	XmlNodeRef config = pXMLSerializer->Read(FileXmlBufferSource(configPath), false, 0, 0);

	// Read in axis from config file
	string upAxis = "Y";
	string meshPrediction = "0";
	string useBFrames = "0";
	string indexFrameDistance = "10";
	string blockCompressionFormat = "deflate";	
	string playbackFromMemory = "0";	
	string positionPrecision = "1";

	if (config)
	{
		if (config->haveAttr("UpAxis"))
		{
			upAxis = config->getAttr("UpAxis");
		}

		if (config->haveAttr("MeshPrediction"))
		{
			meshPrediction = config->getAttr("MeshPrediction");
		}

		if (config->haveAttr("UseBFrames"))
		{
			useBFrames = config->getAttr("UseBFrames");
		}

		if (config->haveAttr("IndexFrameDistance"))
		{
			indexFrameDistance = config->getAttr("IndexFrameDistance");
		}

		if (config->haveAttr("BlockCompressionFormat"))
		{
			blockCompressionFormat = config->getAttr("BlockCompressionFormat");
		}

		if (config->haveAttr("PlaybackFromMemory"))
		{
			playbackFromMemory = config->getAttr("PlaybackFromMemory");
		}

		if (config->haveAttr("PositionPrecision"))
		{
			positionPrecision = config->getAttr("PositionPrecision");
		}
	}
	else
	{
		const bool bSkipFilesWithoutBuildConfig = m_CC.config->HasKey("skipFilesWithoutBuildConfig");
		
		if (!bSkipFilesWithoutBuildConfig)
		{
			RCLogWarning("  Build configuration file not found, writing new one");
			config = pXMLSerializer->CreateNode("CacheBuildConfiguration");
		}
		else
		{
			RCLogError("  Build configuration file not found. Skipped.");
			return NULL;
		}
	}

	// Command line overrides
	upAxis = m_CC.config->GetAsString("upAxis", upAxis, upAxis);	
	meshPrediction = m_CC.config->GetAsString("meshPrediction", meshPrediction, meshPrediction);	
	useBFrames = m_CC.config->GetAsString("useBFrames", useBFrames, useBFrames);	
	indexFrameDistance = m_CC.config->GetAsString("indexFrameDistance", indexFrameDistance, indexFrameDistance);	
	blockCompressionFormat = m_CC.config->GetAsString("blockCompressionFormat", blockCompressionFormat, blockCompressionFormat);
	playbackFromMemory = m_CC.config->GetAsString("playbackFromMemory", playbackFromMemory, playbackFromMemory);
	positionPrecision = m_CC.config->GetAsString("positionPrecision", positionPrecision, positionPrecision);

	// Check if we need to convert the axis
	m_bConvertYUpToZUp = upAxis.compareNoCase("Y") == 0;
	if (m_bConvertYUpToZUp)
	{
		RCLog("  Converting Y up to Z up");
	}

	m_bPlaybackFromMemory = playbackFromMemory.compareNoCase("1") == 0;
	if (m_bPlaybackFromMemory)
	{
		RCLog("  Playback from memory");
	}

	m_bMeshPrediction = (meshPrediction.compareNoCase("1") == 0) && (blockCompressionFormat != "store");
	if (m_bMeshPrediction)
	{
		RCLog("  Using mesh prediction");
	}

	m_indexFrameDistance = 15;
	m_bUseBFrames = (useBFrames.compareNoCase("1") == 0) && (blockCompressionFormat != "store");
	if (m_bUseBFrames)
	{
		m_indexFrameDistance = atoi(indexFrameDistance.c_str());
		RCLog("  Using bi-directional predicted frames");
		RCLog("  Index frame distance is %d", m_indexFrameDistance);
	}

	if (blockCompressionFormat == "store")
	{
		m_blockCompressionFormat = GeomCacheFile::eBlockCompressionFormat_None;
		RCLog("  No frame compression");
	}
	else if (blockCompressionFormat == "lz4hc")
	{
		m_blockCompressionFormat = GeomCacheFile::eBlockCompressionFormat_LZ4HC;
		RCLog("  Using LZ4 HC compression");
	}
	else
	{
		m_blockCompressionFormat = GeomCacheFile::eBlockCompressionFormat_Deflate;
		RCLog("  Using deflate (zlib) compression");
	}

	m_positionPrecision = std::max(atof(positionPrecision), 0.0);
	if (m_positionPrecision == 0.0)
	{
		RCLog("  Maximum position precision", m_positionPrecision);
	}
	else
	{
		RCLog("  %g mm position precision", m_positionPrecision);
	}

	config->setAttr("UpAxis", upAxis);
	config->setAttr("MeshPrediction", meshPrediction);
	config->setAttr("UseBFrames", useBFrames);
	config->setAttr("IndexFrameDistance", indexFrameDistance);
	config->setAttr("BlockCompressionFormat", blockCompressionFormat);
	config->setAttr("PlaybackFromMemory", playbackFromMemory);
	config->setAttr("PositionPrecision", positionPrecision);
	return config;
}

template<class ParamType> inline void CheckColorParam(Alembic::Abc::ICompoundProperty &arbParams, 
	const Alembic::Abc::PropertyHeader &propertyHeader, GeomCache::Mesh &mesh, bool &bFoundColorProperty)
{
	const std::string &propertyName = propertyHeader.getName();

	if (!bFoundColorProperty)
	{
		mesh.m_bHasColors = true;
		mesh.m_colorParamName = propertyHeader.getName();			

		ParamType param(arbParams, propertyName);
		if (param.isConstant())
		{
			mesh.m_constantStreams = GeomCacheFile::EStreams(mesh.m_constantStreams | GeomCacheFile::eStream_Colors);
		}
		else
		{
			mesh.m_animatedStreams = GeomCacheFile::EStreams(mesh.m_animatedStreams | GeomCacheFile::eStream_Colors);
		}

		bFoundColorProperty = true;
	}
	else
	{
		RCLogWarning("   Multiple color streams. Ignoring color stream %s", propertyName.c_str());
	}	
}

void AlembicCompiler::CheckMeshForColors(Alembic::AbcGeom::IPolyMeshSchema &meshSchema, GeomCache::Mesh &mesh) const
{
	mesh.m_bHasColors = false;

	Alembic::Abc::ICompoundProperty &arbParams = meshSchema.getArbGeomParams();
	if (!arbParams)
	{
		return;
	}

	bool bFoundColorProperty = false;

	const uint numProperties = arbParams.getNumProperties();
	for (uint i = 0; i < numProperties; ++i)
	{
		const Alembic::Abc::PropertyHeader &propertyHeader = arbParams.getPropertyHeader(i);		

		if (Alembic::AbcGeom::IC3hGeomParam::matches(propertyHeader))
		{
			CheckColorParam<Alembic::AbcGeom::IC3hGeomParam>(arbParams, propertyHeader, mesh, bFoundColorProperty);
		}
		else if(Alembic::AbcGeom::IC3fGeomParam::matches(propertyHeader))
		{
			CheckColorParam<Alembic::AbcGeom::IC3fGeomParam>(arbParams, propertyHeader, mesh, bFoundColorProperty);
		}
		else if(Alembic::AbcGeom::IC3cGeomParam::matches(propertyHeader))
		{
			CheckColorParam<Alembic::AbcGeom::IC3cGeomParam>(arbParams, propertyHeader, mesh, bFoundColorProperty);
		}
		else if (Alembic::AbcGeom::IC4hGeomParam::matches(propertyHeader))
		{
			CheckColorParam<Alembic::AbcGeom::IC4hGeomParam>(arbParams, propertyHeader, mesh, bFoundColorProperty);
		}
		else if (Alembic::AbcGeom::IC4fGeomParam::matches(propertyHeader))
		{
			CheckColorParam<Alembic::AbcGeom::IC4fGeomParam>(arbParams, propertyHeader, mesh, bFoundColorProperty);
		}
		else if (Alembic::AbcGeom::IC4cGeomParam::matches(propertyHeader))
		{
			CheckColorParam<Alembic::AbcGeom::IC4cGeomParam>(arbParams, propertyHeader, mesh, bFoundColorProperty);
		}
	}
}

// Prints the node tree
void AlembicCompiler::PrintNodeTreeRec(GeomCache::Node &node, string padding)
{
	if (&node != &m_rootNode)
	{
		padding += "\t";
		RCLog("%s%s - %s", padding.c_str(), node.m_name.c_str(), (node.m_transformType == GeomCacheFile::eTransformType_Constant) ? "constant" : "animated");
	}

	const uint numChildren = node.m_children.size();
	for (uint i = 0; i < numChildren; ++i)
	{
		PrintNodeTreeRec(*node.m_children[i], padding);
	}
}

bool AlembicCompiler::CompileAnimationData(Alembic::Abc::IArchive &archive, GeomCacheEncoder &geomCacheEncoder, ThreadUtils::StealingThreadPool &threadPool)
{
	RCLog("Compiling animation data...");

	// Set up parallel frame processing
	const uint numJobGroups = threadPool.GetNumThreads() * 2;
	m_jobGroupData.resize(numJobGroups);

	for (uint i = 0; i < numJobGroups; ++i)
	{
		m_jobGroupData[i].m_pAlembicCompiler = this;
		m_jobGroupData[i].m_jobIndex = i;
	}
	
	for (size_t meshIndex = 0; meshIndex < m_meshes.size(); ++meshIndex)
	{
		m_meshes[meshIndex]->m_meshDataBuffer.resize(numJobGroups);
	}

	std::function<void(GeomCache::Node &node)> resizeTransformBuffers = [&](GeomCache::Node &node)
	{ 
		node.m_nodeDataBuffer.resize(numJobGroups);
		const uint numChildren = node.m_children.size();
		for (uint i = 0; i < numChildren; ++i)
		{
			resizeTransformBuffers(*node.m_children[i]);
		}
	};
	resizeTransformBuffers(m_rootNode);

	// Compile frames with jobs
	const size_t numFrames = m_frameTimes.size();	
	for (size_t currentFrame = 0; currentFrame < numFrames; ++currentFrame)
	{
		{
			ThreadUtils::AutoLock lock(m_jobFinishedCS);
			while(m_numJobGroupsRunning == numJobGroups)
			{			
				m_jobFinishedCV.Sleep(m_jobFinishedCS);
				PushCompletedFrames(geomCacheEncoder, numJobGroups);			
			}
		}

		FrameJobGroupData &jobGroupData = m_jobGroupData[m_nextJobGroupIndex];
		
		// Fill job data
		jobGroupData.m_bFinished = false;
		jobGroupData.m_frameIndex = currentFrame;
		jobGroupData.m_frameTime = m_frameTimes[currentFrame];
		jobGroupData.m_bWritten = false;
		jobGroupData.m_frameAABB.Reset();

		for (size_t meshIndex = 0; meshIndex < m_meshes.size(); ++meshIndex)
		{
			m_meshes[meshIndex]->m_meshDataBuffer[m_nextJobGroupIndex].m_frameUseCount = 0;
		}

		const uint numJobs = m_jobGroupData.size();
		m_nextJobGroupIndex = (m_nextJobGroupIndex + 1) % numJobs;

		ThreadUtils::JobGroup *pJobGroup = threadPool.CreateJobGroup<>(&FrameGroupFinished, &jobGroupData);	

		// Async mesh updates
		for (size_t meshIndex = 0; meshIndex < m_meshes.size(); ++meshIndex)
		{
			GeomCache::Mesh *pMesh = m_meshes[meshIndex];

			if (pMesh->m_animatedStreams != 0)
			{
				CompileMeshJobData *pJobData = new CompileMeshJobData(&jobGroupData, *pMesh);
				pJobGroup->Add(&AlembicCompiler::UpdateVertexDataJob, pJobData);
			}
		}

		// Async transform update
		++m_numJobGroupsRunning;
		pJobGroup->Add(&AlembicCompiler::UpdateTransformsJob, &jobGroupData);		
		pJobGroup->Submit();
	}	

	// Flush
	ThreadUtils::AutoLock lock(m_jobFinishedCS);
	while(m_numJobGroupsRunning > 0)
	{	
		PushCompletedFrames(geomCacheEncoder, numJobGroups);
		
		if (m_numJobGroupsRunning > 0)
		{
			m_jobFinishedCV.Sleep(m_jobFinishedCS);		
		}		
	}

	return true;
}

void AlembicCompiler::PushCompletedFrames(GeomCacheEncoder &geomCacheEncoder, const uint numJobGroups)
{
	// Need to loop if any job finished while the lock was down
	uint currentJobsFinished = 0;	
	while (currentJobsFinished != m_numJobsFinished)
	{
		currentJobsFinished = m_numJobsFinished;
		std::vector<FrameJobGroupData*> unwrittenFinishedGroups;

		for (uint i = 0; i < numJobGroups; ++i)
		{
			FrameJobGroupData &jobGroupData = m_jobGroupData[i];
			if (jobGroupData.m_bFinished && !jobGroupData.m_bWritten)
			{
				unwrittenFinishedGroups.push_back(&jobGroupData);
			}
		}
		m_jobFinishedCS.Unlock();

		std::sort(unwrittenFinishedGroups.begin(), unwrittenFinishedGroups.end(), 
			[=](const FrameJobGroupData *pA, const FrameJobGroupData *pB) {
				return pA->m_frameIndex < pB->m_frameIndex;
			}
		);	

		for (uint i = 0; i < unwrittenFinishedGroups.size(); ++i)
		{
			FrameJobGroupData &jobGroupData = *unwrittenFinishedGroups[i];

			if (jobGroupData.m_frameIndex == m_nextFrameToWrite)
			{
				const uint numMeshes = m_meshes.size();
				for (uint i = 0; i < numMeshes; ++i)
				{
					GeomCache::Mesh &mesh = *m_meshes[i];

					mesh.m_rawFramesCS.Lock();
					mesh.m_rawFrames.push_back(std::move(mesh.m_meshDataBuffer[jobGroupData.m_jobIndex]));
					mesh.m_rawFramesCS.Unlock();
				}

				AppendTransformFrameDataRec(m_rootNode, jobGroupData.m_jobIndex);
				const bool bIsLastFrame = (jobGroupData.m_frameIndex == (m_frameTimes.size() - 1));

				geomCacheEncoder.AddFrame(jobGroupData.m_frameTime, jobGroupData.m_frameAABB, bIsLastFrame);			

				jobGroupData.m_bWritten = true;
				--m_numJobGroupsRunning;
				++m_nextFrameToWrite;			
			}
			else
			{
				break;
			}
		}
		
		m_jobFinishedCS.Lock();		
	}
}

void AlembicCompiler::UpdateTransformsJob(FrameJobGroupData *pData)
{
	AlembicCompiler *pSelf = pData->m_pAlembicCompiler;
	std::string currentObjectPath;

	try
	{
		ThreadUtils::AutoLock lock(pSelf->m_abcLock);
		TMatrixMap matrixMap;
		TVisibilityMap visibilityMap;
		pSelf->UpdateTransformsRec(pSelf->m_rootNode, pData->m_jobIndex, pData->m_frameTime, 
			pData->m_frameAABB, QuatTNS(IDENTITY), matrixMap, visibilityMap, currentObjectPath);
	}
	catch(const Alembic::Util::Exception &alembicException)
	{
		RCLogError("Alembic exception while processing %s in frame %u, time %g: %s",
			currentObjectPath.c_str(), pData->m_frameIndex, pData->m_frameTime, alembicException.c_str());
		InterlockedIncrement(&pData->m_errorCount);
	}
	catch(...)
	{
		RCLogError("Unknown exception while processing %s in frame %u, time %g", 
			currentObjectPath.c_str(), pData->m_frameIndex, pData->m_frameTime);
		InterlockedIncrement(&pData->m_errorCount);
	}
}

void AlembicCompiler::UpdateTransformsRec(GeomCache::Node &node, const uint bufferIndex, const Alembic::Abc::chrono_t frameTime, 
	AABB &frameAABB, QuatTNS currentTransform, TMatrixMap &matrixMap, TVisibilityMap &visibilityMap, std::string &currentObjectPath)
{	
	if (node.m_transformType != GeomCacheFile::eTransformType_Constant)
	{
		node.m_nodeDataBuffer[bufferIndex].m_transform.SetIdentity();
		
		for (auto iter = node.m_abcXForms.begin(); iter != node.m_abcXForms.end(); ++iter)
		{
			Alembic::AbcGeom::IXform &xform = *iter;

			currentObjectPath = xform.getFullName().c_str();

			auto findIter = matrixMap.find(currentObjectPath);
			if (findIter != matrixMap.end())
			{
				const Alembic::AbcGeom::M44d &matrix = findIter->second;
				node.m_nodeDataBuffer[bufferIndex].m_transform = node.m_nodeDataBuffer[bufferIndex].m_transform * FromAlembicMatrix(matrix);
			}
			else
			{
				Alembic::AbcGeom::IXformSchema &schema = xform.getSchema();
				Alembic::Abc::TimeSampling &timeSampling = *schema.getTimeSampling();

				auto index = timeSampling.getNearIndex(frameTime, schema.getNumSamples());

				const Alembic::AbcGeom::M44d &matrix = schema.getValue(index.first).getMatrix();
				node.m_nodeDataBuffer[bufferIndex].m_transform = node.m_nodeDataBuffer[bufferIndex].m_transform * FromAlembicMatrix(matrix);

				matrixMap[currentObjectPath] = matrix;
			}
		}
	}
	else
	{
		node.m_nodeDataBuffer[bufferIndex].m_transform = node.m_staticNodeData.m_transform;
	}

	currentTransform = currentTransform * node.m_nodeDataBuffer[bufferIndex].m_transform;	

	node.m_nodeDataBuffer[bufferIndex].m_bVisible = true;

	if (node.m_type == GeomCacheFile::eNodeType_Mesh || node.m_type == GeomCacheFile::eNodeType_PhysicsGeometry)
	{
		bool bVisible = true;
		for (Alembic::Abc::IObject currentObject = node.m_abcObject; currentObject; currentObject = currentObject.getParent())
		{
			currentObjectPath = currentObject.getFullName().c_str();

			auto findIter = visibilityMap.find(currentObjectPath);
			if (findIter != visibilityMap.end())
			{
				const Alembic::AbcGeom::ObjectVisibility &visibility = findIter->second;
				if (visibility == Alembic::AbcGeom::kVisibilityHidden)
				{
					bVisible = false;
					break;
				}
			}
			else
			{
				Alembic::AbcGeom::IVisibilityProperty visibilityProperty = Alembic::AbcGeom::GetVisibilityProperty(currentObject);

				if (visibilityProperty)
				{
					Alembic::Abc::TimeSamplingPtr timeSampling = visibilityProperty.getTimeSampling();
					auto index = timeSampling->getNearIndex(frameTime, visibilityProperty.getNumSamples());

					int8_t rawVisibilityValue;
					rawVisibilityValue = visibilityProperty.getValue(index.first);
					Alembic::AbcGeom::ObjectVisibility visibility = Alembic::AbcGeom::ObjectVisibility(rawVisibilityValue);

					visibilityMap[currentObjectPath] = visibility;

					if (visibility == Alembic::AbcGeom::kVisibilityHidden)
					{
						bVisible = false;
						break;
					}
				}
			}
		}

		node.m_nodeDataBuffer[bufferIndex].m_bVisible = bVisible;

		if (bVisible && (node.m_type == GeomCacheFile::eNodeType_Mesh))
		{
			AABB transformedMeshAABB;
			transformedMeshAABB.SetTransformedAABB(Matrix34(currentTransform), node.m_pMesh->m_aabb);
			frameAABB.Add(transformedMeshAABB);		

			InterlockedIncrement(&node.m_pMesh->m_meshDataBuffer[bufferIndex].m_frameUseCount);
		}
	}

	const size_t numChildren = node.m_children.size();
	for (size_t i = 0; i < numChildren; ++i)
	{
		UpdateTransformsRec(*node.m_children[i], bufferIndex, frameTime, frameAABB, 
			currentTransform, matrixMap, visibilityMap, currentObjectPath);
	}
}

void AlembicCompiler::UpdateVertexDataJob(struct CompileMeshJobData *pData)
{
	FrameJobGroupData *pJobGroupData = pData->m_pJobGroupData;
	AlembicCompiler *pSelf = pJobGroupData->m_pAlembicCompiler;
	const char *pMeshName = pData->m_mesh.m_abcMesh.getFullName().c_str();	

	try
	{
		ThreadUtils::AutoLock lock(pSelf->m_abcLock);
		if (!pSelf->UpdateVertexData(pData->m_mesh, pJobGroupData->m_jobIndex, pJobGroupData->m_frameIndex))
		{
			InterlockedIncrement(&pJobGroupData->m_errorCount);
		}
	}
	catch(const Alembic::Util::Exception &alembicException)
	{
		RCLogError("Alembic exception while processing %s in frame %u, time %g: %s",
			pMeshName, pJobGroupData->m_frameIndex, pJobGroupData->m_frameTime, alembicException.c_str());
		InterlockedIncrement(&pJobGroupData->m_errorCount);
	}
	catch(...)
	{
		RCLogError("Unknown exception while processing %s in frame %u, time %g", 
			pMeshName, pJobGroupData->m_frameIndex, pJobGroupData->m_frameTime);
		InterlockedIncrement(&pJobGroupData->m_errorCount);
	}

	delete pData;
}

void AlembicCompiler::FrameGroupFinished(FrameJobGroupData *pData)
{
	AlembicCompiler *pSelf = pData->m_pAlembicCompiler;

	if (pData->m_errorCount > 0)
	{
		InterlockedExchangeAdd(&pSelf->m_errorCount, pData->m_errorCount);
		RCLogError("  Failed to compile %d meshes", pData->m_errorCount);
		return;
	}

	// Set finished flag and wakeup compiler main thread
	pSelf->m_jobFinishedCS.Lock();
	pData->m_bFinished = true;
	InterlockedIncrement(&pSelf->m_numJobsFinished);
	pSelf->m_jobFinishedCS.Unlock();

	pSelf->m_jobFinishedCV.Wake();
}

std::unordered_map<uint32, uint16> AlembicCompiler::GetMeshMaterialMap(const Alembic::AbcGeom::IPolyMesh& mesh, const Alembic::Abc::chrono_t frameTime)
{
	// Read config value and clamp it to [0, 1].
	// Engine uses 0 based indices, but the UI displays them 1 based in sandbox.
	// By default, subtract 1 from user input in DCC applications to be consistent.
	const int faceSetNumberingBase = max(min(m_CC.config->GetAsInt("faceSetNumberingBase", 1, 1), 1), 0);

	std::unordered_map<uint32, uint16> materialIdMap;

	const uint numChildren = mesh.getNumChildren();
	for (uint i = 0; i < numChildren; ++i)
	{
		Alembic::Abc::IObject child = mesh.getChild(i);
		if (Alembic::AbcGeom::IFaceSet::matches(child.getHeader()))
		{
			Alembic::AbcGeom::IFaceSet faceSet(child,	Alembic::Abc::kWrapExisting);

			Alembic::AbcGeom::IFaceSetSchema::Sample sample;
			faceSet.getSchema().get(sample, Alembic::Abc::ISampleSelector(frameTime));

			const std::string faceSetName = faceSet.getName();

			// Parse first number in face set name
			const char *pName = faceSetName.c_str();
			while (*pName && !isdigit(*pName))
			{
				++pName;
			}

			if (*pName == 0)
			{
				RCLogWarning("  Face set name '%s' contains no number, will map faces to material ID 1", faceSetName.c_str());
				continue;
			}

			int materialId = atoi(pName);

			if (materialId < faceSetNumberingBase || materialId > 65536)
			{
				RCLogWarning("  Face set name '%s' refers to material ID out of range %d-65536, will map faces to material ID 1", faceSetName.c_str(), faceSetNumberingBase);
				continue;
			}

			materialId -= faceSetNumberingBase;

			const int32 *pFaces = static_cast<const int32*>(sample.getFaces()->getData());
			const uint numFaces = sample.getFaces()->size();

			for (uint i = 0; i < numFaces; ++i)
			{
				const int32 face = pFaces[i];
				if (materialIdMap.find(face) != materialIdMap.end())
				{
					RCLogWarning("  Face %i of mesh is referenced by more than one face set:\n    %s", pFaces[i], mesh.getFullName());
				}

				materialIdMap[face] = materialId;
			}
		}
	}

	return materialIdMap;
}

bool AlembicCompiler::ComputeVertexHashes(std::vector<uint64> &abcVertexHashes, size_t currentFrame, const size_t numAbcIndices, GeomCache::Mesh &mesh, 
	Alembic::Abc::TimeSampling &meshTimeSampling, size_t numMeshSamples, Alembic::AbcGeom::IPolyMeshSchema &meshSchema, const bool bHasNormals, 
	const bool bHasTexcoords, const bool bHasColors, const size_t numAbcNormalIndices, const size_t numAbcTexcoordsIndices, const size_t numAbcFaces)
{
	abcVertexHashes.resize(numAbcIndices, 0);

	size_t firstFrame;
	size_t lastFrame;
	if (mesh.m_animatedStreams == 0)
	{
		// Only need to process first frame for constant meshes, as they all are equal
		firstFrame = 0;
		lastFrame = 1;
	}
	else if ((mesh.m_animatedStreams & GeomCacheFile::eStream_Indices) == 0)
	{
		// If topology is not homogeneous we create one index buffer for the whole animation per mesh. 
		// This means we need to break vertices up if at any point in time normals or texcoords differ
		// for two triangles that share the same vertex. We do this by creating a hash over the data for
		// each triangle vertex over time.
		firstFrame = 0;
		lastFrame = m_frameTimes.size();
	}
	else
	{
		// For heterogeneous meshes we just check the current frame
		firstFrame = currentFrame;
		lastFrame = currentFrame + 1;
	}

	// Reset mesh AABB
	mesh.m_aabb.Reset();

	for (size_t currentFrame = firstFrame; currentFrame < lastFrame; ++currentFrame)
	{
		Alembic::Abc::chrono_t frameTime = m_frameTimes[currentFrame];
		auto index = meshTimeSampling.getNearIndex(frameTime, numMeshSamples);

		Alembic::AbcGeom::IPolyMeshSchema::Sample frameSample = meshSchema.getValue(index.first);

		// Just check to make sure. This should not happen.
		if (bHasNormals != meshSchema.getNormalsParam().valid() || bHasTexcoords != meshSchema.getUVsParam().valid())
		{
			RCLogWarning("  Mesh schema differs from frame 0 to frame %Iu. Skipped:\n    %s", currentFrame, mesh.m_abcMesh.getFullName().c_str());
			return false;
		}

		// Get normal & texcoord samples
		Alembic::AbcGeom::IN3fGeomParam::Sample frameNormalSample;	
		if (bHasNormals)
		{
			meshSchema.getNormalsParam().getIndexed(frameNormalSample, index.first);
		}		

		Alembic::AbcGeom::IV2fGeomParam::Sample frameTexcoordSample;
		if (bHasTexcoords)
		{
			meshSchema.getUVsParam().getIndexed(frameTexcoordSample, index.first);
		}

		// Get sample arrays
		const auto &frameAbcPositions = *frameSample.getPositions();	
		const size_t frameNumAbcVertices = frameAbcPositions.size();
		const auto &frameAbcFaceCounts = *frameSample.getFaceCounts();
		const size_t frameNumAbcFaces = frameAbcFaceCounts.size();
		const auto &frameAbcIndices = *frameSample.getFaceIndices();	
		const size_t frameNumAbcIndices = frameAbcIndices.size();

		const Alembic::Abc::N3fArraySamplePtr pFrameAbcNormals = bHasNormals ? frameNormalSample.getVals() : 0;
		const size_t frameNumAbcNormals = bHasNormals ? pFrameAbcNormals->size() : 0;
		const Alembic::Abc::UInt32ArraySamplePtr pFrameAbcNormalIndices = bHasNormals ? frameNormalSample.getIndices() : 0;
		const size_t frameNumAbcNormalIndices = bHasNormals ? pFrameAbcNormalIndices->size() : 0;

		const Alembic::Abc::V2fArraySamplePtr pFrameAbcTexcoords = bHasTexcoords ? frameTexcoordSample.getVals() : 0;
		const size_t frameNumAbcTexcoords = bHasTexcoords ? pFrameAbcTexcoords->size() : 0;
		const Alembic::Abc::UInt32ArraySamplePtr pFrameAbcTexcoordIndices = bHasTexcoords ? frameTexcoordSample.getIndices() : 0;
		const size_t frameNumAbcTexcoordsIndices = bHasTexcoords ? pFrameAbcTexcoordIndices->size() : 0;

		AlembicColorSampleArray frameColors = bHasColors ? AlembicColorSampleArray(mesh.m_colorParamName, meshSchema, index.first) : AlembicColorSampleArray();

		// Just check to make sure. This should not happen.
		if (frameNumAbcIndices != numAbcIndices || frameNumAbcNormalIndices != numAbcNormalIndices 
			|| frameNumAbcTexcoordsIndices != numAbcTexcoordsIndices || frameNumAbcFaces != numAbcFaces)
		{
			RCLogWarning("  Mesh index/face count differs from frame 0 to frame %Iu. Skipped:\n    %s", currentFrame, mesh.m_abcMesh.getFullName().c_str());
			return false;
		}

		size_t currentIndexArraysIndex = 0;
		for (size_t face = 0; face < frameNumAbcFaces; ++face)
		{
			size_t numFaceVertices = frameAbcFaceCounts[face];

			if (numFaceVertices < 3)
			{
				currentIndexArraysIndex += numFaceVertices;
				continue;
			}

			for (size_t faceVertexIndex = 0; faceVertexIndex < numFaceVertices; ++faceVertexIndex)
			{
				// Just to make sure check index array position
				if (currentIndexArraysIndex >= numAbcIndices)
				{
					RCLogWarning("  Mesh contains invalid data. Skipped:\n    %s", mesh.m_abcMesh.getFullName().c_str());
					return false;
				}

				const int32_t positionIndex = frameAbcIndices[currentIndexArraysIndex];
				const int32_t normalIndex = bHasNormals ? (*pFrameAbcNormalIndices)[currentIndexArraysIndex] : 0;
				const int32_t texcoordsIndex = bHasTexcoords ? (*pFrameAbcTexcoordIndices)[currentIndexArraysIndex] : 0;				
				const int32_t colorsIndex = frameColors.getIndex(currentIndexArraysIndex);

				// Just to make sure check indices
				if ((size_t)positionIndex >= frameNumAbcVertices 
					|| (bHasNormals && (size_t)normalIndex >= frameNumAbcNormals)
					|| (bHasTexcoords && (size_t)texcoordsIndex >= frameNumAbcTexcoords)
					|| (bHasColors && (size_t)colorsIndex >= frameColors.GetSize()))
				{
					RCLogWarning("  Mesh contains invalid data. Skipped:\n    %s", mesh.m_abcMesh.getFullName().c_str());
					return false;
				}

				// Convert to geom cache vertex
				AlembicCompilerVertex vertex;

				const Imath::Vec3<float> &abcPosition = frameAbcPositions[positionIndex];
				const Imath::Vec3<float> &abcNormal = bHasNormals ? (*pFrameAbcNormals)[normalIndex] : Imath::Vec3<float>(0.0f, 0.0f, 0.0f);
				const Imath::Vec2<float> &abcTexcoord = bHasTexcoords ? (*pFrameAbcTexcoords)[texcoordsIndex] : Imath::Vec2<float>(0.0f, 0.0f);

				vertex.m_position = FromAlembicPosition(abcPosition);
				vertex.m_normal = Vec3(abcNormal.x, abcNormal.y, abcNormal.z);
				vertex.m_texcoords = FromAlembicTexcoord(abcTexcoord);
				vertex.m_rgba = frameColors[colorsIndex];

				mesh.m_aabb.Add(vertex.m_position);

				// Combine with hash from previous frames
				AlembicCompilerHashCombine(abcVertexHashes[currentIndexArraysIndex], vertex);

				// Advance index array index
				++currentIndexArraysIndex;
			}
		}
	}

	return true;
}

bool AlembicCompiler::CompileFullMesh(GeomCache::Mesh &mesh, const size_t currentFrame, const QuatTNS &transform)
{
	const Alembic::Abc::chrono_t frameTime = m_frameTimes[currentFrame];
	mesh.m_materialIdMap = GetMeshMaterialMap(mesh.m_abcMesh, frameTime);

	GeomCache::MeshData &meshData = mesh.m_staticMeshData;

	mesh.m_indicesMap.clear();		
	meshData.m_positions.clear();
	meshData.m_texcoords.clear();
	meshData.m_qTangents.clear();
	meshData.m_reds.clear();
	meshData.m_greens.clear();
	meshData.m_blues.clear();
	meshData.m_alphas.clear();

	const bool bHasNormals = mesh.m_bHasNormals;
	const bool bHasTexcoords = mesh.m_bHasTexcoords;	
	const bool bHasColors = mesh.m_bHasColors;

	Alembic::AbcGeom::IPolyMeshSchema &meshSchema = mesh.m_abcMesh.getSchema();	
	Alembic::Abc::TimeSampling &meshTimeSampling = *meshSchema.getTimeSampling();
	size_t numMeshSamples = meshSchema.getNumSamples();

	auto index = meshTimeSampling.getNearIndex(frameTime, numMeshSamples);

	Alembic::AbcGeom::IN3fGeomParam::Sample normalSample;	
	if (bHasNormals)
	{
		meshSchema.getNormalsParam().getIndexed(normalSample, index.first);
	}	

	Alembic::AbcGeom::IV2fGeomParam::Sample texcoordSample;
	if (bHasTexcoords)
	{
		meshSchema.getUVsParam().getIndexed(texcoordSample, index.first);
	}

	// Get & check mesh data of current frame
	Alembic::AbcGeom::IPolyMeshSchema::Sample sample = meshSchema.getValue(index.first);

	const auto &abcPositions = *sample.getPositions();	
	const size_t numAbcVertices = abcPositions.size();
	const auto &abcFaceCounts = *sample.getFaceCounts();
	const size_t numAbcFaces = abcFaceCounts.size();
	const auto &abcIndices = *sample.getFaceIndices();	
	const size_t numAbcIndices = abcIndices.size();

	const Alembic::Abc::N3fArraySamplePtr pAbcNormals = bHasNormals ? normalSample.getVals() : 0;
	const Alembic::Abc::UInt32ArraySamplePtr pAbcNormalIndices = bHasNormals ? normalSample.getIndices() : 0;
	const size_t numAbcNormalIndices = bHasNormals ? pAbcNormalIndices->size() : 0;

	const Alembic::Abc::V2fArraySamplePtr pAbcTexcoords = bHasTexcoords ? texcoordSample.getVals() : 0;
	const Alembic::Abc::UInt32ArraySamplePtr pAbcTexcoordIndices = bHasTexcoords ? texcoordSample.getIndices() : 0;
	const size_t numAbcTexcoordsIndices = bHasTexcoords ? pAbcTexcoordIndices->size() : 0;

	if (bHasTexcoords && numAbcIndices != numAbcTexcoordsIndices)
	{
		RCLogWarning("  Mesh number of position indices doesn't equal number of "
			"texcoord indices (position indices: %u, texcoord indices: %u). Skipped:\n    %s", 
			(uint)numAbcIndices, (uint)numAbcTexcoordsIndices, mesh.m_abcMesh.getFullName().c_str());
		return false;
	}

	AlembicColorSampleArray colors = bHasColors ? AlembicColorSampleArray(mesh.m_colorParamName, meshSchema, 0) : AlembicColorSampleArray();

	if (bHasColors && numAbcIndices != colors.GetNumIndices())
	{
		RCLogWarning("  Mesh number of position indices doesn't equal number of "
			"color indices (position indices: %u, color indices: %u). Skipped:\n    %s",
			(uint)numAbcIndices, (uint)colors.GetNumIndices(), mesh.m_abcMesh.getFullName().c_str());
		return false;
	}

	// Initialize hashes to detect same vertices. 
	// TODO: There could be hash collisions, but they are so unlikely, that we don't care for now.
	std::vector<uint64> abcVertexHashes;
	if (!ComputeVertexHashes(abcVertexHashes, currentFrame, numAbcIndices, mesh, meshTimeSampling, numMeshSamples, 
		meshSchema, bHasNormals, bHasTexcoords, bHasColors, numAbcNormalIndices, numAbcTexcoordsIndices, numAbcFaces))
	{
		return false;
	}

	// Now that we have the vertex hashes we can create the triangle topology.
	// Convert to triangles (alembic supports arbitrary faces) and split vertices if necessary
	// (GPU expects vertex, normals, texcoords, etc. buffers to have same cardinality)		
	
	// Temp buffer for face indices
	std::vector<uint32> faceIndices;
	
	// Map from material ID to indices
	std::unordered_map<uint, std::vector<uint32>> indices;

	// Stores the positions of vertices in the vertex buffer
	std::unordered_map<uint64, uint32> vertexDigestToVertexBufferIndexMap;
	std::vector<AlembicCompilerVertex> vertices;

	size_t currentIndexArraysIndex = 0;
	for (size_t face = 0; face < numAbcFaces; ++face)
	{
		size_t numFaceVertices = abcFaceCounts[face];

		if (numFaceVertices < 3)
		{				
			currentIndexArraysIndex += numFaceVertices;
			continue;
		}

		// If face is not contained in a face set, the default material ID is 0
		auto findIter = mesh.m_materialIdMap.find(face);		
		const uint faceMaterialId = (findIter != mesh.m_materialIdMap.end()) ? findIter->second : 0;

		// First loop through face and create indices/vertices
		faceIndices.resize(0);
		for (size_t faceVertexIndex = 0; faceVertexIndex < numFaceVertices; ++faceVertexIndex)
		{
			const int32_t positionIndex = abcIndices[currentIndexArraysIndex];
			const int32_t normalIndex = bHasNormals ? (*pAbcNormalIndices)[currentIndexArraysIndex] : 0;
			const int32_t texcoordsIndex = bHasTexcoords ? (*pAbcTexcoordIndices)[currentIndexArraysIndex] : 0;
			const int32_t colorsIndex = colors.getIndex(currentIndexArraysIndex);

			// Search if vertex is already in vertex buffer by its digest
			const uint64 vertexDigest = abcVertexHashes[currentIndexArraysIndex];

			// Get normal & texcoords for that vertex			
			const Imath::Vec3<float> &abcPosition = abcPositions[positionIndex];
			const Imath::Vec3<float> &abcNormal = bHasNormals ? (*pAbcNormals)[normalIndex] : Imath::Vec3<float>(0.0f, 0.0f, 0.0f);
			const Imath::Vec2<float> &abcTexcoord = bHasTexcoords ? (*pAbcTexcoords)[texcoordsIndex] : Imath::Vec2<float>(0.0f, 0.0f);

			// Convert to geom cache vertex
			AlembicCompilerVertex vertex;
			vertex.m_position = FromAlembicPosition(abcPosition);
			vertex.m_normal = Vec3(abcNormal.x, abcNormal.y, abcNormal.z);
			vertex.m_texcoords = FromAlembicTexcoord(abcTexcoord);
			vertex.m_rgba = colors[colorsIndex];

			uint32 vertexIndex;
			auto findIter = vertexDigestToVertexBufferIndexMap.find(vertexDigest);
			if (findIter != vertexDigestToVertexBufferIndexMap.end())
			{
				// Vertex already in buffer
				vertexIndex = findIter->second;
			}
			else
			{
				// We need to add a vertex
				size_t newIndex = vertices.size();

				// Check if index fits in 16 bits if necessary
				if (!m_b32BitIndices && newIndex >= 65535)
				{
					RCLogWarning("  Mesh results in more than 65536 compiled vertices. Skipped:\n    %s", mesh.m_abcMesh.getFullName().c_str());
					return false;
				}

				vertexIndex = (uint32)newIndex;
				vertexDigestToVertexBufferIndexMap[vertexDigest] = vertexIndex;
				
				// Add the vertex to the vertex buffer
				vertices.push_back(vertex);
			}

			if (mesh.m_animatedStreams != 0 && (mesh.m_animatedStreams & GeomCacheFile::eStream_Indices) == 0 || !mesh.m_bHasNormals)
			{
				// Add to index mapping list
				mesh.m_abcIndexToGeomCacheIndex.push_back(vertexIndex);
			}

			// Add to index buffer
			faceIndices.push_back(vertexIndex);

			// Advance to next index
			++currentIndexArraysIndex;
		}

		// Triangulate face
		for (size_t i = 1; i < numFaceVertices - 1; ++i)
		{
			indices[faceMaterialId].push_back(faceIndices[0]);
			indices[faceMaterialId].push_back(faceIndices[i + 1]);
			indices[faceMaterialId].push_back(faceIndices[i]);
		}
	}

	if (!mesh.m_bHasNormals)
	{
		if (!CalculateSmoothNormals(vertices, mesh, abcFaceCounts, abcIndices, abcPositions))
		{
			return false;
		}
	}

	// Compute mesh hash
	uint64 meshHash = 0;
	for (size_t i = 0; i < abcVertexHashes.size(); ++i)
	{
		uint64 vertexHash = abcVertexHashes[i];
		AlembicCompilerHashCombine<uint64>(meshHash, vertexHash);
	}
	mesh.m_hash = meshHash;

	// Optimize indices
	for (auto iter = indices.begin(); iter != indices.end(); ++iter)
	{
		const size_t cacheSize = 16;
		const uint verticesPerFace = 3;

		const uint materialId = iter->first;
		std::vector<uint32> &indices = iter->second;		

		ForsythFaceReorderer faceReorderer;
		std::vector<uint32> optimizedIndices;
		optimizedIndices.resize(indices.size());
		faceReorderer.reorderFaces(cacheSize, verticesPerFace, indices.size(), &indices[0], &optimizedIndices[0], 0);

		mesh.m_indicesMap[materialId] = optimizedIndices;
	}

	if (vertices.size() < numAbcVertices)
	{
		RCLogWarning("  Mesh contains unused vertices:\n    %s", mesh.m_abcMesh.getFullName().c_str());
	}
	else
	{
		InterlockedExchangeAdd(&m_numVertexSplits, static_cast<LONG>(vertices.size() - numAbcVertices));
	}

	if (m_positionPrecision != 0.0)
	{
		// Calculate needed position precision
		const Vec3 aabbSize = mesh.m_aabb.GetSize();
		const Vec3d worldSize = Vec3d(aabbSize.x * transform.s.x, aabbSize.y * transform.s.y, aabbSize.z * transform.s.z);
		const Vec3d wantedQuantization = (worldSize * 1000.0) / m_positionPrecision;	
		mesh.m_positionPrecision[0] = (wantedQuantization.x > 0.0) ? std::max((uint8)(std::min(ceil(log(wantedQuantization.x) / log(2.0)), 16.0)), (uint8)1) : 1;
		mesh.m_positionPrecision[1] = (wantedQuantization.y > 0.0) ? std::max((uint8)(std::min(ceil(log(wantedQuantization.y) / log(2.0)), 16.0)), (uint8)1) : 1;
		mesh.m_positionPrecision[2] = (wantedQuantization.z > 0.0) ? std::max((uint8)(std::min(ceil(log(wantedQuantization.z) / log(2.0)), 16.0)), (uint8)1) : 1;
	}
	else
	{
		mesh.m_positionPrecision[0] = 16;
		mesh.m_positionPrecision[1] = 16;
		mesh.m_positionPrecision[2] = 16;
	}

	// Finally compile vertices to stored format
	if (!CompileVertices(vertices, mesh, mesh.m_staticMeshData, false))
	{
		return false;
	}

	if (mesh.m_animatedStreams != 0 && (mesh.m_animatedStreams & GeomCacheFile::eStream_Indices) == 0)
	{
		// Pass mesh to encoder to optimize vertex order for compression
		if (!GeomCacheEncoder::OptimizeMeshForCompression(mesh, m_bMeshPrediction))
		{
			RCLogWarning("  Could not optimize for compression:\n    %s", mesh.m_abcMesh.getFullName().c_str());
		}
	}

	return true;
}

bool AlembicCompiler::UpdateVertexData(GeomCache::Mesh &mesh, const uint bufferIndex, const size_t currentFrame)
{
	const bool bHasNormals = mesh.m_bHasNormals;
	const bool bHasTexcoords = mesh.m_bHasTexcoords;	
	const bool bHasColors = mesh.m_bHasColors;

	const Alembic::Abc::chrono_t frameTime = m_frameTimes[currentFrame];

	Alembic::AbcGeom::IPolyMeshSchema &meshSchema = mesh.m_abcMesh.getSchema();	
	Alembic::Abc::TimeSampling &meshTimeSampling = *meshSchema.getTimeSampling();
	size_t numMeshSamples = meshSchema.getNumSamples();

	auto index = meshTimeSampling.getNearIndex(frameTime, numMeshSamples);

	Alembic::AbcGeom::IN3fGeomParam::Sample normalSample;	
	if (bHasNormals)
	{
		meshSchema.getNormalsParam().getIndexed(normalSample, index.first);
	}	

	Alembic::AbcGeom::IV2fGeomParam::Sample texcoordSample;
	if (bHasTexcoords)
	{
		meshSchema.getUVsParam().getIndexed(texcoordSample, index.first);
	}

	// Get & check mesh data of first frame
	Alembic::AbcGeom::IPolyMeshSchema::Sample sample = meshSchema.getValue(index.first);

	const auto &abcPositions = *sample.getPositions();	
	const auto &abcFaceCounts = *sample.getFaceCounts();
	const size_t numAbcFaces = abcFaceCounts.size();
	const auto &abcIndices = *sample.getFaceIndices();	

	const Alembic::Abc::N3fArraySamplePtr pAbcNormals = bHasNormals ? normalSample.getVals() : 0;
	const Alembic::Abc::UInt32ArraySamplePtr pAbcNormalIndices = bHasNormals ? normalSample.getIndices() : 0;

	const Alembic::Abc::V2fArraySamplePtr pAbcTexcoords = bHasTexcoords ? texcoordSample.getVals() : Alembic::Abc::V2fArraySamplePtr();
	const Alembic::Abc::UInt32ArraySamplePtr pAbcTexcoordIndices = bHasTexcoords ? texcoordSample.getIndices() : Alembic::Abc::UInt32ArraySamplePtr();

	AlembicColorSampleArray colors = mesh.m_bHasColors ? AlembicColorSampleArray(mesh.m_colorParamName, meshSchema, index.first) : AlembicColorSampleArray();

	std::vector<AlembicCompilerVertex> vertices;
	vertices.resize(mesh.m_staticMeshData.m_positions.size());

	size_t currentIndexArraysIndex = 0;
	for (size_t face = 0; face < numAbcFaces; ++face)
	{
		size_t numFaceVertices = abcFaceCounts[face];

		if (numFaceVertices < 3)
		{				
			currentIndexArraysIndex += numFaceVertices;
			continue;
		}

		// First loop through face and create indices/vertices
		for (size_t faceVertexIndex = 0; faceVertexIndex < numFaceVertices; ++faceVertexIndex)
		{
			const int32 positionIndex = abcIndices[currentIndexArraysIndex];
			const int32 normalIndex = bHasNormals ? (*pAbcNormalIndices)[currentIndexArraysIndex] : 0;
			const int32 texcoordsIndex = bHasTexcoords ? (*pAbcTexcoordIndices)[currentIndexArraysIndex] : 0;
			const int32 colorsIndex = bHasColors ? colors.getIndex(currentIndexArraysIndex) : 0;

			// Get normal & texcoords for that vertex			
			const Imath::Vec3<float> &abcPosition = abcPositions[positionIndex];
			const Imath::Vec3<float> &abcNormal = bHasNormals ? (*pAbcNormals)[normalIndex] : Imath::Vec3<float>(0.0f, 0.0f, 0.0f);
			const Imath::Vec2<float> &abcTexcoord = bHasTexcoords ? (*pAbcTexcoords)[texcoordsIndex] : Imath::Vec2<float>(0.0f, 0.0f);

			// Convert to geom cache vertex
			AlembicCompilerVertex vertex;
			vertex.m_position = FromAlembicPosition(abcPosition);
			vertex.m_normal = Vec3(abcNormal.x, abcNormal.y, abcNormal.z);
			vertex.m_texcoords = FromAlembicTexcoord(abcTexcoord);
			vertex.m_rgba = bHasColors ? colors[colorsIndex] : Vec4(0.0f, 0.0f, 0.0f, 0.0f);

			if (currentIndexArraysIndex >= mesh.m_abcIndexToGeomCacheIndex.size())
			{
				RCLogError("  Invalid index mapping:\n    %s", mesh.m_abcMesh.getFullName().c_str());
				return false;
			}

			// Update the vertex in the index buffer. This write can happen multiple times to the same 
			// location, if the vertex is referred multiple times. The values are equal, so we don't care.
			const uint32 vertexIndex = mesh.m_abcIndexToGeomCacheIndex[currentIndexArraysIndex];
			vertices[vertexIndex] = vertex;

			++currentIndexArraysIndex;
		}
	}

	if (!mesh.m_bHasNormals)
	{
		if (!CalculateSmoothNormals(vertices, mesh, abcFaceCounts, abcIndices, abcPositions))
		{
			return false;
		}
	}
		
	if (!CompileVertices(vertices, mesh, mesh.m_meshDataBuffer[bufferIndex].m_meshData, true))
	{
		return false;
	}

	return true;
}

bool AlembicCompiler::CalculateSmoothNormals(std::vector<AlembicCompilerVertex> &vertices, GeomCache::Mesh &mesh, 
	const Alembic::Abc::Int32ArraySample &faceCounts, const Alembic::Abc::Int32ArraySample &faceIndices, 
	const Alembic::Abc::P3fArraySample &facePositions)
{
	const size_t numFaces = faceCounts.size();
	const size_t numPositions = facePositions.size();

	std::vector<Vec3> normals(numPositions, Vec3(0.0f, 0.0f, 0.0f));
	std::vector<Vec3> tempFacePositions;

	// Compute face normals of alembic mesh and add up normals at each vertex
	size_t currentIndexArraysIndex = 0;
	for (size_t face = 0; face < numFaces; ++face)
	{
		size_t numFaceVertices = faceCounts[face];

		if (numFaceVertices < 3)
		{				
			currentIndexArraysIndex += numFaceVertices;
			continue;
		}

		tempFacePositions.clear();
		for (size_t i = 0; i < numFaceVertices; ++i)
		{
			uint index = faceIndices[currentIndexArraysIndex + i];
			tempFacePositions.push_back(FromAlembicPosition(facePositions[index]));
		}

		for (size_t i = 1; i < numFaceVertices - 1; ++i)
		{
			Vec3 p1 = tempFacePositions[0];
			Vec3 p2 = tempFacePositions[i + 1];
			Vec3 p3 = tempFacePositions[i];

			Vec3 edge12 = p2 - p1;
			Vec3 edge23 = p3 - p2;
			Vec3 edge31 = p1 - p3;

			const float influence1 = edge12.Cross(edge31).GetLength();
			const float influence2 = edge23.Cross(edge12).GetLength();
			const float influence3 = edge31.Cross(edge23).GetLength();

			Vec3 faceNormal = edge31.Cross(edge12);
			faceNormal.Normalize();

			normals[faceIndices[currentIndexArraysIndex]] += influence1 * faceNormal;
			normals[faceIndices[currentIndexArraysIndex + i + 1]] += influence2 * faceNormal;
			normals[faceIndices[currentIndexArraysIndex + i]] += influence3 * faceNormal;
		}

		currentIndexArraysIndex += numFaceVertices;
	}

	// Normalize all the normals
	for (uint i = 0; i < numPositions; ++i)
	{
		normals[i].Normalize();
	}

	// Assign them to mesh vertices
	currentIndexArraysIndex = 0;
	for (size_t face = 0; face < numFaces; ++face)
	{
		size_t numFaceVertices = faceCounts[face];

		if (numFaceVertices < 3)
		{				
			currentIndexArraysIndex += numFaceVertices;
			continue;
		}

		for (size_t i = 0; i < numFaceVertices; ++i)
		{
			uint index = mesh.m_abcIndexToGeomCacheIndex[currentIndexArraysIndex];
			vertices[index].m_normal = normals[faceIndices[currentIndexArraysIndex++]];
		}
	}

	return true;
}

namespace
{
	GeomCacheFile::QTangent EncodeQTangent(Matrix33 frame, const bool bReflection)
	{
		frame.OrthonormalizeFast();
		if (!frame.IsOrthonormalRH(0.1f))
		{
			frame.SetIdentity();
		}

		Quat qFrame(frame);

		qFrame.v = -qFrame.v;
		if (qFrame.w < 0.0f)
		{
			qFrame = -qFrame;
		}

		const float kMultiplier = float((2 << (GeomCacheFile::kTangentQuatPrecision - 1)) - 1);

		// Make sure w is never 0 by applying the smallest possible bias.		
		static const float kBias = 1.0f / kMultiplier;
		static const float kBiasScale = sqrtf(1.0f - kBias * kBias);
		if (qFrame.w < kBias && qFrame.w > -kBias)
		{
			qFrame *= kBiasScale;
			qFrame.w = kBias;
		}

		if (bReflection)
		{
			qFrame = -qFrame;
		}

		GeomCacheFile::QTangent compressedQTangent;		
		compressedQTangent[0] = static_cast<int16>(qFrame.v[0] * kMultiplier);
		compressedQTangent[1] = static_cast<int16>(qFrame.v[1] * kMultiplier);
		compressedQTangent[2] = static_cast<int16>(qFrame.v[2] * kMultiplier);
		compressedQTangent[3] = static_cast<int16>(qFrame.w * kMultiplier);

		return compressedQTangent;
	}
}

bool AlembicCompiler::CompileVertices(std::vector<AlembicCompilerVertex> &vertices, GeomCache::Mesh &mesh, GeomCache::MeshData &meshData, const bool bUpdate)
{
	// Resize arrays if necessary
	if (meshData.m_positions.size() == 0)
	{
		meshData.m_positions.resize(vertices.size());
		meshData.m_texcoords.resize(vertices.size());
		meshData.m_qTangents.resize(vertices.size());
		
		if (mesh.m_bHasColors)
		{
			meshData.m_reds.resize(vertices.size());
			meshData.m_greens.resize(vertices.size());
			meshData.m_blues.resize(vertices.size());
			meshData.m_alphas.resize(vertices.size());
		}
	}
	else
	{
		assert(meshData.m_positions.size() == vertices.size());
		if (!meshData.m_positions.size() == vertices.size())
		{
			return false;
		}
	}

	if (!bUpdate)
	{
		mesh.m_reflections.resize(vertices.size());
	}

	// Avoid division by zero if mesh has no extent in a dimension
	Vec3 aabbSize = mesh.m_aabb.GetSize();
	aabbSize.x = (aabbSize.x == 0.0f) ? 1.0f : aabbSize.x;
	aabbSize.y = (aabbSize.y == 0.0f) ? 1.0f : aabbSize.y;
	aabbSize.z = (aabbSize.z == 0.0f) ? 1.0f : aabbSize.z;

	const float kMultiplierX = float((2 << (mesh.m_positionPrecision[0] - 1)) - 1);
	const float kMultiplierY = float((2 << (mesh.m_positionPrecision[1] - 1)) - 1);
	const float kMultiplierZ = float((2 << (mesh.m_positionPrecision[2] - 1)) - 1);

	// Quantize positions & texcoords
	const size_t numVertices = meshData.m_positions.size();
	for (uint32 vertexIndex = 0; vertexIndex < numVertices; ++vertexIndex)
	{
		const AlembicCompilerVertex &vertex = vertices[vertexIndex];

		// Remap position to [0, 1] range in AABB
		Vec3 mappedPosition = (vertex.m_position - mesh.m_aabb.min) / aabbSize;
		
		// Now map to range of 16 bit unsigned integer and store
		GeomCacheFile::Position &compressedPosition = meshData.m_positions[vertexIndex];
		compressedPosition.x = (uint16)(mappedPosition.x * kMultiplierX);
		compressedPosition.y = (uint16)(mappedPosition.y * kMultiplierY);
		compressedPosition.z = (uint16)(mappedPosition.z * kMultiplierZ);
		
		// Wrap around texcoords at kTexcoordRange		
		Vec2 mappedTexcoords = Vec2(fmod(vertex.m_texcoords.x, (float)GeomCacheFile::kTexcoordRange),
			fmod(vertex.m_texcoords.y, (float)GeomCacheFile::kTexcoordRange));

		// Now map to range of 16 bit unsigned integer and store
		GeomCacheFile::Texcoords &compressedTexcoords = meshData.m_texcoords[vertexIndex];
		compressedTexcoords = (mappedTexcoords / (float)GeomCacheFile::kTexcoordRange) * 32767.0f;

		if (mesh.m_bHasColors)
		{
			meshData.m_reds[vertexIndex] = static_cast<GeomCacheFile::Color>(clamp_tpl(vertex.m_rgba[0], 0.0f, 1.0f) * 255.0f);
			meshData.m_greens[vertexIndex]  = static_cast<GeomCacheFile::Color>(clamp_tpl(vertex.m_rgba[1], 0.0f, 1.0f) * 255.0f);
			meshData.m_blues[vertexIndex]  = static_cast<GeomCacheFile::Color>(clamp_tpl(vertex.m_rgba[2], 0.0f, 1.0f) * 255.0f);
			meshData.m_alphas[vertexIndex]  = static_cast<GeomCacheFile::Color>(clamp_tpl(vertex.m_rgba[3], 0.0f, 1.0f) * 255.0f);
		}
	}

	// Compute new tangents
	for (auto iter = mesh.m_indicesMap.begin(); iter != mesh.m_indicesMap.end(); ++iter)
	{
		const std::vector<uint32> &indices = iter->second;
		assert (indices.size() % 3 == 0);

		GeomCacheMeshTriangleInputProxy inputProxy(indices, vertices);
		CTangentSpaceCalculation tangentSpaceCalculation;

		string errorMessage;
		eCalculateTangentSpaceErrorCode errorCode = 
			tangentSpaceCalculation.CalculateTangentSpace(inputProxy, true, false, errorMessage);

		if (errorCode != CALCULATE_TANGENT_SPACE_NO_ERRORS)
		{
			RCLogError("Tangent space calculation failed");
			return false;
		}

		// Get tangents back and convert them to qtangents
		const size_t numTriangles = indices.size() / 3;
		for (uint32 triangleIndex = 0; triangleIndex < numTriangles; ++triangleIndex)
		{
			uint32 triangleBaseIndices[3];
			tangentSpaceCalculation.GetTriangleBaseIndices(triangleIndex, triangleBaseIndices);

			for (uint vertex = 0; vertex < 3; ++vertex)
			{
				Vec3 tangent, biTangent, normal;
				tangentSpaceCalculation.GetBase(triangleBaseIndices[vertex], (float*)&tangent, (float*)&biTangent, (float*)&normal);

				// Convert to q tangent
				Vec3 crossedNormal = tangent.Cross(biTangent).GetNormalized();
				bool bReflected = crossedNormal.Dot(normal) < 0.0f;

				const size_t index = indices[triangleIndex * 3 + vertex];

				if (!bUpdate)
				{
					// Store tangent reflection values
					mesh.m_reflections[index] = bReflected;
				}
				else if (bReflected != mesh.m_reflections[index])
				{
					// Enforce reflection of first frame					
					bReflected = !bReflected;					
					biTangent = -biTangent;
					crossedNormal = tangent.Cross(biTangent).GetNormalized();					
				}

				Matrix33 frame;
				frame.SetRow(0, tangent);
				frame.SetRow(1, biTangent);
				frame.SetRow(2, crossedNormal);

				// Quantize and store				
				meshData.m_qTangents[index] = EncodeQTangent(frame, bReflected);
			}
		}
	}

	return true;
}

void AlembicCompiler::AppendTransformFrameDataRec(GeomCache::Node &node, const uint bufferIndex) const
{	
	node.m_animatedNodeDataCS.Lock();
	node.m_animatedNodeData.push_back(std::move(node.m_nodeDataBuffer[bufferIndex]));
	node.m_animatedNodeDataCS.Unlock();

	const size_t numChildren = node.m_children.size();
	for (size_t i = 0; i < numChildren; ++i)
	{
		AppendTransformFrameDataRec(*node.m_children[i], bufferIndex);
	}
}

void AlembicCompiler::Cleanup()
{
	m_rootNode.m_type = GeomCacheFile::eNodeType_Transform;
	m_rootNode.m_transformType = GeomCacheFile::eTransformType_Constant;
	m_rootNode.m_staticNodeData.m_bVisible = true;
	m_rootNode.m_staticNodeData.m_transform.SetIdentity();
	m_rootNode.m_pMesh.reset();
	m_rootNode.m_physicsGeometry.clear();
	m_rootNode.m_children.clear();
	m_rootNode.m_abcObject = Alembic::Abc::IObject();
	m_rootNode.m_abcXForms.clear();
	m_rootNode.m_animatedNodeData.clear();
	m_rootNode.m_name.clear();

	m_timeSamplings.clear();
	m_frameTimes.clear();
	m_meshes.clear();
	m_digestToMeshMap.clear();
	m_errorCount = 0;
	m_numAnimatedMeshes = 0;
	m_numJobGroupsRunning = 0;		
	m_nextFrameToWrite = 0;
	m_numJobsFinished = 0;
}
