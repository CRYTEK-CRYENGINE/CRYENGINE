// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include "SourceAssetContent.h"

#ifdef SOURCE_ASSET_SUPPORT

	#include <IBackgroundTaskManager.h>
	#include <IEditor.h>
	#include "CharacterToolSystem.h"

namespace CharacterTool
{

using std::vector;

struct SourceNodeView
{
	int nodeIndex;

	SourceNodeView()
		: nodeIndex(-1)
	{
	}

	SourceNodeView(int nodeIndex)
		: nodeIndex(nodeIndex)
	{
	}

	void Serialize(IArchive& ar)
	{
		SourceAssetContent* content = ar.context<SourceAssetContent>();
		if (!content)
			return;

		const SourceAsset::Scene& scene = content->scene;
		SourceAsset::Settings& settings = content->settings;

		if (size_t(nodeIndex) >= scene.nodes.size())
			return;

		const SourceAsset::Node& node = scene.nodes[nodeIndex];
		ar(node.name, "name", "!^");
		vector<SourceNodeView> children;
		children.resize(node.children.size());
		for (size_t i = 0; i < children.size(); ++i)
			children[i].nodeIndex = node.children[i];
		ar(children, "children", "^=");

		if (node.mesh >= 0)
		{
			bool usedAsMesh = settings.UsedAsMesh(node.name.c_str());
			bool wasUsed = usedAsMesh;
			ar(usedAsMesh, "usedAsMesh", "^^");
			if (usedAsMesh && !wasUsed)
			{
				content->changingView = true;
				SourceAsset::MeshImport mesh;
				mesh.nodeName = node.name;
				settings.meshes.push_back(mesh);
			}
			else if (!usedAsMesh && wasUsed)
			{
				content->changingView = true;
				for (size_t i = 0; i < settings.meshes.size(); ++i)
					if (settings.meshes[i].nodeName == node.name)
					{
						settings.meshes.erase(settings.meshes.begin() + i);
						break;
					}
			}
		}
		else
		{
			bool usedAsSkeleton = settings.UsedAsSkeleton(node.name.c_str());
			bool wasUsed = usedAsSkeleton;
			ar(usedAsSkeleton, "usedAsSkeleton", "^^");
			if (usedAsSkeleton && !wasUsed)
			{
				content->changingView = true;
				SourceAsset::SkeletonImport skel;
				skel.nodeName = node.name;
				settings.skeletons.push_back(skel);
			}
			else if (!usedAsSkeleton && wasUsed)
			{
				content->changingView = true;
				for (size_t i = 0; i < settings.meshes.size(); ++i)
					if (settings.skeletons[i].nodeName == node.name)
					{
						settings.skeletons.erase(settings.skeletons.begin() + i);
						break;
					}
			}
		}
	}
};

void SourceAssetContent::Serialize(IArchive& ar)
{
	if (ar.isEdit())
	{
		if (state == STATE_LOADING)
		{
			string loading = "Loading Asset...";
			ar(loading, "loading", "!<");
		}
		else
		{
			Serialization::SContext x(ar, this);

			changingView = false;
			if (ar.openBlock("scene", "Scene"))
			{
				SourceNodeView root(0);
				ar(root, "root", "<+");
				ar.closeBlock();
			}

			if (!changingView)
				ar(settings, "settings", "Import Settings");
		}
	}
	else
	{
		settings.Serialize(ar);
	}
}

// ---------------------------------------------------------------------------

static void LoadTestScene(SourceAsset::Scene* scene)
{
	SourceAsset::Mesh mesh;
	mesh.name = "Mesh 1";
	scene->meshes.push_back(mesh);
	mesh.name = "Mesh 2";
	scene->meshes.push_back(mesh);
	mesh.name = "Mesh 3";
	scene->meshes.push_back(mesh);

	SourceAsset::Node node;
	node.name = "Root";
	node.children.push_back(1);
	node.children.push_back(2);
	scene->nodes.push_back(node);
	node.children.clear();
	node.name = "Node 1";
	node.mesh = 0;
	scene->nodes.push_back(node);
	node.name = "Node 2";
	node.mesh = 1;
	scene->nodes.push_back(node);
}

struct CreateAssetManifestTask : IBackgroundTask
{
	ExplorerFileList* m_sourceAssetList;
	string            m_assetFilename;
	string            m_description;
	unsigned int      m_entryId;

	CreateAssetManifestTask(ExplorerFileList* sourceAssetList, const char* assetFilename, unsigned int entryId)
		: m_sourceAssetList(sourceAssetList)
		, m_assetFilename(assetFilename)
		, m_entryId(entryId)
	{
		m_description = "Preview ";
		m_description += assetFilename;

		SEntry<SourceAssetContent>* entry = m_sourceAssetList->GetEntryById<SourceAssetContent>(m_entryId);

		entry->content.state = SourceAssetContent::STATE_LOADING;
		m_sourceAssetList->CheckIfModified(m_entryId, 0, true);
	}

	void        Delete() override             { delete this; }
	const char* Description() const override  { return m_description.c_str(); }
	const char* ErrorMessage() const override { return ""; }

	ETaskResult Work() override
	{
		CrySleep(1000);
		return eTaskResult_Completed;
	}

	void Finalize() override
	{
		SEntry<SourceAssetContent>* entry = m_sourceAssetList->GetEntryById<SourceAssetContent>(m_entryId);
		if (!entry)
			return;

		if (entry->content.loadingTask != this)
			return;

		entry->content.loadingTask = 0;

		LoadTestScene(&entry->content.scene);
		entry->content.state = SourceAssetContent::STATE_LOADED;
		m_sourceAssetList->CheckIfModified(m_entryId, 0, true);
	}

};

// ---------------------------------------------------------------------------

bool RCAssetLoader::Load(EntryBase* entryBase, const char* filename)
{
	SEntry<SourceAssetContent>* entry = static_cast<SEntry<SourceAssetContent>*>(entryBase);
	if (entry->content.loadingTask)
		entry->content.loadingTask->Cancel();
	CreateAssetManifestTask* task = new CreateAssetManifestTask(fileList, filename, entryBase->id);
	GetIEditor()->GetBackgroundTaskManager()->AddTask(task, eTaskPriority_RealtimePreview, eTaskThreadMask_IO);
	entry->content.loadingTask = task;
	return true;
}

}

#endif

