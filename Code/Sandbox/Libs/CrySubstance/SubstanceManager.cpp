// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SubstanceManager.h"
#include "SubstanceCommon.h"
#include "SubstancePreset.h"
#include "ISubstanceInstanceRenderer.h"

#include <fstream>
#include <substance/framework/framework.h>
#include <substance/framework/callbacks.h>
#include <substance/pixelformat.h>

#include <CrySerialization/IArchiveHost.h> 
#include <CryString/CryPath.h>
#include <CryString/StringUtils.h>
#include <CryCore/CryCrc32.h>
#include <CryCore/ToolsHelpers/ResourceCompilerHelper.h>

CSubstanceManager* CSubstanceManager::_instance = 0;

ISubstanceManager* ISubstanceManager::Instance()
{
	return CSubstanceManager::Instance();
}


CSubstanceManager* CSubstanceManager::Instance() {
	if (_instance == 0) {
		_instance = new CSubstanceManager;
	}
	return _instance;
}


void CSubstanceManager::CrySubstanceCallbacks::outputComputed(
		SubstanceAir::UInt runUid,
		size_t userData,
		const SubstanceAir::GraphInstance* graphInstance,
		SubstanceAir::OutputInstanceBase* outputInstance)
	{
		ISubstanceInstanceRenderer* renderer = (ISubstanceInstanceRenderer*)userData;
		if (CSubstanceManager::Instance()->m_renderers.count(std::type_index(typeid(*renderer))))
		{
			CSubstanceManager::Instance()->m_renderers[std::type_index(typeid(*renderer))]->OnOutputAvailable(runUid, graphInstance, outputInstance);
			
		}
		else {
			assert(0);
		}

	
	}

void CSubstanceManager::CrySubstanceCallbacks::outputComputed(SubstanceAir::UInt runUid, const SubstanceAir::GraphInstance* graphInstance, SubstanceAir::OutputInstanceBase* outputInstance)
{
	outputComputed(runUid, 0, graphInstance, outputInstance);
}

void CSubstanceManager::CreateInstance(const string& archiveName, const string& instanceName, const string& instanceGraph, const std::vector<SSubstanceOutput>& outputs, const Vec2i& resolution)
{
	CSubstancePreset preset(instanceName, archiveName, instanceGraph, outputs, resolution);
	preset.Save();
}

SubstanceAir::PackageDesc* CSubstanceManager::GetPackage(const string& archiveName)
{
	const uint32 crc = CCrc32::ComputeLowercase(archiveName);
	PackageMap::iterator search = m_loadedPackages.find(crc);
	if (search != m_loadedPackages.end()) {
		return search->second;
	}
	else {
		SubstanceAir::PackageDesc* package = LoadPackage(archiveName);
		m_refCount[crc] = 0;
		return package;
	}

	
}

SubstanceAir::GraphInstance* CSubstanceManager::InstantiateGraph(const string& archiveName, const string& graphName)
{
	SubstanceAir::PackageDesc* package = GetPackage(archiveName);
	SubstanceAir::GraphInstance* instance = nullptr;
	// Whole package
	for (SubstanceAir::PackageDesc::Graphs::const_iterator gite =
		package->getGraphs().begin();
		gite != package->getGraphs().end();
		++gite)
	{
		if (string(&*gite->mLabel.c_str()) == graphName)
		{
			instance = new SubstanceAir::GraphInstance(*&*gite);
			if (instance)
			{
				const uint32 crc = CCrc32::ComputeLowercase(archiveName);
				int& refCount = m_refCount[crc];
				refCount++;
				break;
			}
		}
	}
	return instance;
}


void CSubstanceManager::RemovePresetInstance(CSubstancePreset* preset)
{
	const uint32 crc = CCrc32::ComputeLowercase(preset->GetSubstanceArchive());
	PackageMap::iterator search = m_loadedPackages.find(crc);
	if (search != m_loadedPackages.end()) {
		int& refcount = m_refCount[crc];
		refcount--;
		if (refcount == 0)
		{
			SubstanceAir::PackageDesc* package = search->second;
			m_refCount.erase(crc);
			m_loadedPackages.erase(crc);
			delete package;
		}
	}
	else
	{
		assert(0);
	}

}

bool CSubstanceManager::GetArchiveContents(const string& archiveName, std::map<string, std::vector<string>>& contents)
{
	SubstanceAir::PackageDesc* package = LoadPackage(archiveName);
	if (!package)
		return false;

	for each (const SubstanceAir::GraphDesc& graph in package->getGraphs())
	{
		string graphName = graph.mLabel.c_str();
		for each (SubstanceAir::OutputDesc output in graph.mOutputs)
		{
			contents[graphName].push_back(output.mIdentifier.c_str());
		}
	}
	return true;
}


void CSubstanceManager::GenerateOutputs(ISubstancePreset* preset, ISubstanceInstanceRenderer* renderer)
{
	GenerateOutputs({ preset }, renderer, GenerateType::Sync);
}

SubstanceAir::UInt CSubstanceManager::GenerateOutputs(const std::vector<ISubstancePreset*>& presets, ISubstanceInstanceRenderer* renderer, GenerateType jobType)
{
	for (auto itr = presets.begin(); itr != presets.end(); ++itr) {
		CSubstancePreset* preset = static_cast<CSubstancePreset*>(*itr);
		PushPreset(preset, renderer);
	}

	SubstanceAir::UInt flags = 0;
	switch (jobType)
	{
	case CSubstanceManager::Sync:
		flags = SubstanceAir::Renderer::Run_Default;
		break;
	case CSubstanceManager::Async:
		flags = SubstanceAir::Renderer::Run_Asynchronous | SubstanceAir::Renderer::Run_Replace | SubstanceAir::Renderer::Run_PreserveRun | SubstanceAir::Renderer::Run_First;
		break;
	default:
		assert(0);
		break;
	}
	return m_pRenderer->run(flags, (size_t)renderer);
}

SubstanceAir::UInt CSubstanceManager::GenerateOutputsAsync(ISubstancePreset* preset, ISubstanceInstanceRenderer* renderer)
{
	return GenerateOutputs({ preset }, renderer, GenerateType::Async);
}

SubstanceAir::UInt CSubstanceManager::GenerateOutputsAsync(const std::vector<ISubstancePreset*>& presets, ISubstanceInstanceRenderer* renderer)
{
	return GenerateOutputs(presets, renderer, GenerateType::Async);
}


void CSubstanceManager::PushPreset(CSubstancePreset* preset, ISubstanceInstanceRenderer* renderer)
{
	m_pRenderer->push(preset->PrepareRenderInstance(renderer));
}



bool CSubstanceManager::IsRenderPending(const SubstanceAir::UInt id) const
{
	if (!m_pRenderer)
		return false;
	return m_pRenderer->isPending(id);
}

void CSubstanceManager::RegisterInstanceRenderer(ISubstanceInstanceRenderer* renderer)
{
	if (m_renderers.count(std::type_index(typeid(*renderer))))
	{
		assert(0);
		return;
	}

	m_renderers[std::type_index(typeid(*renderer))] = renderer;
}

CSubstanceManager::CSubstanceManager()
{
	m_pRenderer = new SubstanceAir::Renderer();
	m_pCallbacks = new CSubstanceManager::CrySubstanceCallbacks();
	m_pRenderer->setRenderCallbacks(m_pCallbacks);
}

SubstanceAir::PackageDesc* CSubstanceManager::LoadPackage(const string& archiveName)
{
	std::vector<char> buffer;
	size_t bytesRead;
	g_fileManipulator->ReadFile(archiveName, buffer, bytesRead, "rb");
	SubstanceAir::PackageDesc* package = new SubstanceAir::PackageDesc(&buffer[0], bytesRead);
	m_loadedPackages.emplace(CCrc32::ComputeLowercase(archiveName), package);
	return package;
}


