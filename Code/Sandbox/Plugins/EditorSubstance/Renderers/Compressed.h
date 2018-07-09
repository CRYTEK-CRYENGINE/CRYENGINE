// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include "Base.h"
#include "EditorSubstanceManager.h"
#include "CryRenderer/ITexture.h"

namespace EditorSubstance
{
	namespace Renderers
	{
		struct SGeneratedOutputData
		{
			string path;
			string texturePreset;
			string suffix;
			int flags;
		};



		typedef std::map<string, std::shared_ptr<SGeneratedOutputData>> OutputDataMap;
		typedef std::unordered_map<SubstanceAir::UInt, OutputDataMap> PresetOutputsDataMap;


		class CCompressedRenderer : public CInstanceRenderer
		{
		public:

			CCompressedRenderer();
			virtual void FillVirtualOutputRenderData(const ISubstancePreset* preset, const SSubstanceOutput& output, std::vector<SSubstanceRenderData>& renderData) override;
			virtual void FillOriginalOutputRenderData(const ISubstancePreset* preset, SSubstanceOutput& output, std::vector<SSubstanceRenderData>& renderData) override {};
			virtual void ProcessComputedOutputs() override;
			virtual void RemovePresetRenderData(ISubstancePreset* preset) override;
		protected:
			unsigned int GetOutputCompressedFormat(const EditorSubstance::EPixelFormat& format, bool useSRGB);
			void UpdateTexture(SubstanceAir::RenderResult* result, SGeneratedOutputData* data);
		private:
			void AttachOutputUserData(const ISubstancePreset* preset, const SSubstanceOutput& output, SSubstanceRenderData& renderData, const int& flags);
			uint32 TextureDataSize(uint32 nWidth, uint32 nHeight, uint32 nDepth, uint32 nMips, const ETEX_Format eTF) const;


			PresetOutputsDataMap m_PresetOutputDataMap;
		};


	} // END namespace Renderers
} // END namespace EditorSubstance


