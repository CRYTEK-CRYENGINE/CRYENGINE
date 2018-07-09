// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "../DeviceObjects.h"
#include "DriverD3D.h"
#include "../../../Common/ReverseDepth.h"

#include "DeviceResourceSet_D3D11.h"

const void* const CDeviceResourceSet_DX11::InvalidPointer = (const void* const)0xBADA55;

void CDeviceResourceSet_DX11::ClearCompiledData()
{
	// Releasing resources is allowed to be done by any thread, just not concurrently
	for (auto& stage : compiledTextureSRVs)
		stage.fill((ID3D11ShaderResourceView*)InvalidPointer);

	for (auto& stage : compiledSamplers)
		stage.fill((ID3D11SamplerState*)InvalidPointer);

	ZeroMemory(&compiledCBs, sizeof(compiledCBs));
	ZeroMemory(&numCompiledCBs, sizeof(numCompiledCBs));

	ZeroMemory(&compiledBufferSRVs, sizeof(compiledBufferSRVs));
	ZeroMemory(&numCompiledBufferSRVs, sizeof(numCompiledBufferSRVs));

	ZeroMemory(&compiledUAVs, sizeof(compiledUAVs));
	ZeroMemory(&numCompiledUAVs, sizeof(numCompiledUAVs));
}

bool CDeviceResourceSet_DX11::UpdateImpl(const CDeviceResourceSetDesc& desc, CDeviceResourceSetDesc::EDirtyFlags dirtyFlags)
{
	CRY_ASSERT(gRenDev->m_pRT->IsRenderThread());

	ClearCompiledData();

	for (const auto& it : desc.GetResources())
	{
		const SResourceBindPoint& bindPoint = it.first;
		const SResourceBinding&   resource = it.second;

		CRY_ASSERT(bindPoint.slotNumber < MAX_TMU);

		if (!resource.IsValid())
		{
			CRY_ASSERT_MESSAGE(false, "Invalid resource in resource set desc. Update failed");
			return false;
		}

		switch (bindPoint.slotType)
		{
		case SResourceBindPoint::ESlotType::ConstantBuffer:
		{
			SCompiledConstantBuffer compiledCB;

			buffer_size_t offset, size;

			compiledCB.pBuffer = resource.pConstantBuffer->GetD3D(&offset, &size);
			compiledCB.code = resource.pConstantBuffer->GetCode();
			compiledCB.offset = offset;
			compiledCB.size = size;
			compiledCB.slot = it.first.slotNumber;

			// Shader stages are ordered by usage-frequency and loop exists according to usage-frequency (VS+PS fast, etc.)
			int validShaderStages = it.first.stages;
			for (EHWShaderClass shaderClass = eHWSC_Vertex; validShaderStages; shaderClass = EHWShaderClass(shaderClass + 1), validShaderStages >>= 1)
			{
				if (validShaderStages & 1)
					compiledCBs[shaderClass][numCompiledCBs[shaderClass]++] = compiledCB;
			}
		}
		break;

		case SResourceBindPoint::ESlotType::TextureAndBuffer:
		{
			CDeviceResource* pResource = uint8(bindPoint.flags & SResourceBindPoint::EFlags::IsTexture)
				? (CDeviceResource*)resource.pTexture->GetDevTexture()
				: (CDeviceResource*)resource.pBuffer->GetDevBuffer();

			ID3D11ShaderResourceView* pSRV = pResource->LookupSRV(resource.view);

			// Shader stages are ordered by usage-frequency and loop exists according to usage-frequency (VS+PS fast, etc.)
			int validShaderStages = bindPoint.stages;
			for (EHWShaderClass shaderClass = eHWSC_Vertex; validShaderStages; shaderClass = EHWShaderClass(shaderClass + 1), validShaderStages >>= 1)
			{
				if (validShaderStages & 1)
					compiledTextureSRVs[shaderClass][bindPoint.slotNumber] = pSRV;
			}
		};
		break;

		case SResourceBindPoint::ESlotType::UnorderedAccessView:
		{
			CRY_ASSERT((bindPoint.stages & (EShaderStage_Vertex | EShaderStage_Domain | EShaderStage_Hull | EShaderStage_Geometry)) == 0);

			CDeviceResource* pResource = uint8(bindPoint.flags & SResourceBindPoint::EFlags::IsTexture)
				? (CDeviceResource*)resource.pTexture->GetDevTexture()
				: (CDeviceResource*)resource.pBuffer->GetDevBuffer();

			ID3D11UnorderedAccessView* pUAV = pResource->LookupUAV(resource.view);

			if (bindPoint.stages & EShaderStage_Compute)
			{
				SCompiledUAV compiledUAV = { pUAV, CSubmissionQueue_DX11::TYPE_CS, bindPoint.slotNumber };
				compiledUAVs[numCompiledUAVs++] = compiledUAV;

				CRY_ASSERT(numCompiledUAVs <= D3D11_PS_CS_UAV_REGISTER_COUNT);
			}

			if (bindPoint.stages & EShaderStage_Pixel)
			{
				SCompiledUAV compiledUAV = { pUAV, CSubmissionQueue_DX11::TYPE_PS, bindPoint.slotNumber };
				compiledUAVs[numCompiledUAVs++] = compiledUAV;
			}

		};
		break;

		case SResourceBindPoint::ESlotType::Sampler:
		{
			ID3D11SamplerState* pSamplerState = CDeviceObjectFactory::LookupSamplerState(resource.samplerState).second;

			// Shader stages are ordered by usage-frequency and loop exists according to usage-frequency (VS+PS fast, etc.)
			int validShaderStages = bindPoint.stages;
			for (EHWShaderClass shaderClass = eHWSC_Vertex; validShaderStages; shaderClass = EHWShaderClass(shaderClass + 1), validShaderStages >>= 1)
			{
				if (validShaderStages & 1)
					compiledSamplers[shaderClass][bindPoint.slotNumber] = pSamplerState;
			}
		}
		break;
		}

	}

	return true;
}

