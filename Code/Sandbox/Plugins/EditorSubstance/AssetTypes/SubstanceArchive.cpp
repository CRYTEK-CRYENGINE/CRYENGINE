// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SubstanceArchive.h"
#include "SandboxPlugin.h"
#include <CrySerialization/IArchiveHost.h>
#include "EditorSubstanceManager.h"


namespace EditorSubstance
{
	namespace AssetTypes
	{


		class PresetsDiffSerializer
		{
		public:
			struct PresetDiff : public SSubstanceOutput
			{
				enum ESerializedFlags
				{
					ESFenabled = 1,
					ESFpreset = 2,
					ESFresolution = 4,
					ESFoutputs = 8,
					ESFnew = 16,
					ESFall = ESFenabled | ESFpreset | ESFresolution | ESFoutputs | ESFnew
				};

				int serializationFlags;
				PresetDiff()
					: serializationFlags(ESFall)
				{

				}

				PresetDiff(const SSubstanceOutput& output)
					: SSubstanceOutput(output)
					, serializationFlags(ESFall)
				{

				}

				void CreateDiff(const SSubstanceOutput* originalOutput)
				{
					serializationFlags = 0;
					if (enabled != originalOutput->enabled)
					{
						serializationFlags |= ESFenabled;
					}
					if (preset != originalOutput->preset)
					{
						serializationFlags |= ESFpreset;
					}
					if (resolution != originalOutput->resolution)
					{
						serializationFlags |= ESFresolution;
					}
					if (IsValid() && (!SourcesTheSame(originalOutput->sourceOutputs) || !ChannelsTheSame(originalOutput->channels)))
					{
						// only serialize when output config is valid, otherwise assume it is not valid because of missing original outputs and it can change in the future
						serializationFlags |= ESFoutputs;

					}
					
				}

				void Apply(SSubstanceOutput* target)
				{
					if (serializationFlags & ESFenabled)
					{
						target->enabled = enabled;
					}
					if (serializationFlags & ESFpreset)
					{
						target->preset = preset;
					}
					if (serializationFlags & ESFresolution)
					{
						target->resolution = resolution;
					}
					if (serializationFlags & ESFoutputs)
					{
						target->sourceOutputs = sourceOutputs;
						for (size_t i = 0; i < 4; i++)
						{
							target->channels[i] = channels[i];
						}
					}
				}

				void Serialize(Serialization::IArchive& ar)
				{
					
					ar(serializationFlags, "diff");
					ar(name, "name");

					if (serializationFlags & ESFenabled)
					{
						ar(enabled, "enabled");
					}
					if (serializationFlags & ESFpreset)
					{
						ar(preset, "preset");
					}
					if (serializationFlags & ESFresolution)
					{
						ar(resolution, "resolution");
					}
					if (serializationFlags & ESFoutputs)
					{
						ar(sourceOutputs, "sourceoutputs");
						ar(channels, "mapping");							
					}
					
				}

			private:
				bool SourcesTheSame(const std::vector<string>& original)
				{
					if (sourceOutputs.size() != original.size())
						return false;

					for (size_t i = 0; i < sourceOutputs.size(); i++)
					{
						if (sourceOutputs[i].CompareNoCase(original[i]) != 0)
						{
							return false;
						}
					}

					return true;
				}

				bool ChannelsTheSame(const SSubstanceOutputChannelMapping original[4])
				{
					for (size_t i = 0; i < 4; i++)
					{
						if (channels[i].sourceId != original[i].sourceId || channels[i].channel != original[i].channel)
						{
							return false;
						}
					}
					return true;
				}

			};

			PresetsDiffSerializer(std::vector<SSubstanceOutput>& outputs)
				: m_outputs(outputs)
			{
				
			}

			void Serialize(Serialization::IArchive& ar)
			{
				const std::vector<SSubstanceOutput> originalOutputs = CManager::Instance()->GetProjectDefaultOutputSettings();
				if (ar.isInput())
				{
					m_outputs.assign(originalOutputs.begin(), originalOutputs.end());
					std::vector<PresetDiff> serializable;
					ar(serializable, "outputs_configuration");
					for (auto& output : serializable)
					{
						std::vector<SSubstanceOutput>::iterator it = std::find_if(m_outputs.begin(), m_outputs.end(), [=](const SSubstanceOutput& o) { return output.name.CompareNoCase(o.name) == 0; });
						if (it == m_outputs.end())
						{
							// only add new output, if the diff was serialized as new
							if (output.serializationFlags & PresetDiff::ESFnew)
							{
								m_outputs.push_back(output);
							}
						}
						else {
							output.Apply(&*it);
						}
					}
				}
				else {
					// saving
					std::vector<PresetDiff> serializable;
					for (const SSubstanceOutput& output : m_outputs)
					{
						auto search = std::find_if(originalOutputs.begin(), originalOutputs.end(), [=](const SSubstanceOutput& o) {
							return output.name.CompareNoCase(o.name) == 0;
						});

						serializable.push_back(PresetDiff(output));

						if (search != originalOutputs.end())
						{
							serializable.back().CreateDiff(const_cast<SSubstanceOutput*>(&*search));
							if (serializable.back().serializationFlags == 0)
							{
								serializable.pop_back();
							}
						}
					}
					ar(serializable, "outputs_configuration");

				}
			}

		private:
			std::vector<SSubstanceOutput>& m_outputs;
		};




		REGISTER_ASSET_TYPE(CSubstanceArchiveType)

		#define SUBSTANCE_ARCHIVE_DETAIL_NAME "SubstanceCache"

		CAssetEditor* CSubstanceArchiveType::Edit(CAsset* asset) const
		{
			return CAssetEditor::OpenAssetForEdit("Substance Archive Graph Editor", asset);
		}

		CryIcon CSubstanceArchiveType::GetIcon() const
		{
			return CryIcon("icons:substance/archive.ico");
		}

		void CSubstanceArchiveType::AppendContextMenuActions(const std::vector<CAsset*>& assets, CAbstractMenu* menu) const
		{
			if (assets.size() == 1)
			{
				CManager::Instance()->AddSubstanceArchiveContextMenu(assets[0], menu);
			}
		}

		void CSubstanceArchiveType::SetArchiveCache(CEditableAsset* editableAsset, const std::map<string, std::vector<string>>& archiveData) const
		{
			CSubstanceArchiveType::SSubstanceArchiveCache cache = GetArchiveCache(&editableAsset->GetAsset());
			
			// cleanup deleted graphs from cache
			for (const string& graphName : cache.GetGraphNames())
			{
				if (archiveData.count(graphName) == 0)
				{
					cache.m_graphs.erase(graphName);
				}
			}


			for (auto data : archiveData)
			{
				cache.m_graphs[data.first].m_outputs = data.second;
				cache.m_graphs[data.first].m_name = data.first;
			}

			SaveGraphCache(editableAsset, cache);
		}

		CSubstanceArchiveType::SSubstanceArchiveCache CSubstanceArchiveType::GetArchiveCache(CAsset* asset) const
		{
			string data = asset->GetDetailValue(SUBSTANCE_ARCHIVE_DETAIL_NAME);
			CSubstanceArchiveType::SSubstanceArchiveCache instance;
			if (!data.empty())
			{
				Serialization::LoadXmlNode(instance, GetIEditor()->GetSystem()->LoadXmlFromBuffer(data.c_str(), data.length()));
			}
			return instance;
		}

		void CSubstanceArchiveType::SetGraphOutputsConfiguration(CEditableAsset* editableAsset, const string& name, const std::vector<SSubstanceOutput>& outputs, const Vec2i& resolution) const
		{
			CSubstanceArchiveType::SSubstanceArchiveCache cache = GetArchiveCache(&editableAsset->GetAsset());
			if (!cache.HasGraph(name))
			{
				return;
			}

			SSubstanceArchiveCache::Graph& graph = cache.m_graphs[name];
			graph.m_resolutionX = resolution.x;
			graph.m_resolutionY = resolution.y;
			graph.m_outputsConfiguration.assign(outputs.begin(), outputs.end());
			SaveGraphCache(editableAsset, cache);
			
		}

		void CSubstanceArchiveType::SaveGraphCache(CEditableAsset* editableAsset, const CSubstanceArchiveType::SSubstanceArchiveCache& cache) const
		{
			XmlNodeRef node = Serialization::SaveXmlNode(cache, "cache");
			std::pair<string, string> info(SUBSTANCE_ARCHIVE_DETAIL_NAME, node->getXML().c_str());
			editableAsset->SetDetails({ info });
		}

		const CSubstanceArchiveType::SSubstanceArchiveCache::Graph& CSubstanceArchiveType::SSubstanceArchiveCache::GetGraph(const string& name) const
		{
			return m_graphs.at(name);
		}

		const bool CSubstanceArchiveType::SSubstanceArchiveCache::HasGraph(const string& name) const
		{
			return m_graphs.count(name) > 0;
		}

		const std::vector<string> CSubstanceArchiveType::SSubstanceArchiveCache::GetGraphNames()
		{
			std::vector<string> results;
			for (auto pair : m_graphs)
			{
				results.push_back(pair.first);
			}

			return results;
		}

		const std::vector<const CSubstanceArchiveType::SSubstanceArchiveCache::Graph*> CSubstanceArchiveType::SSubstanceArchiveCache::GetGraphs()
		{
			std::vector<const CSubstanceArchiveType::SSubstanceArchiveCache::Graph*> results;
			for (auto pair : m_graphs)
			{
				results.push_back(&pair.second);
			}

			return results;
		}

		void CSubstanceArchiveType::SSubstanceArchiveCache::Serialize(Serialization::IArchive& ar)
		{
			ar(m_graphs, "graphs");
		}

		CSubstanceArchiveType::SSubstanceArchiveCache::Graph::Graph()
			: m_resolutionX(10)
			, m_resolutionY(10)
		{

		}

		const Vec2i CSubstanceArchiveType::SSubstanceArchiveCache::Graph::GetResolution() const
		{
			return Vec2i(m_resolutionX, m_resolutionY);
		}

		void CSubstanceArchiveType::SSubstanceArchiveCache::Graph::Serialize(Serialization::IArchive& ar)
		{
			ar(m_name, "name");
			ar(m_outputs, "outputs");
			PresetsDiffSerializer diffSerializer(m_outputsConfiguration);
			diffSerializer.Serialize(ar);
			ar(m_resolutionX, "resolutionX");
			ar(m_resolutionY, "resolutionY");
		}
	}
}
