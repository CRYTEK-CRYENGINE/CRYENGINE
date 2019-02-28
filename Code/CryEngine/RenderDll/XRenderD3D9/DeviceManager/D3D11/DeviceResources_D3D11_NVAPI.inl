// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#if defined(USE_NV_API)
void CDeviceTexture::DisableMgpuSync()
{
	if (gRenDev->m_bVendorLibInitialized && (gRenDev->GetFeatures() & RFT_HW_NVIDIA))
	{
		NVDX_ObjectHandle handle = (NVDX_ObjectHandle&)m_handleMGPU;
		if (handle == NULL)
		{
			NvAPI_Status status = NvAPI_D3D_GetObjectHandleForResource(gcpRendD3D->GetDevice().GetRealDevice(), m_pNativeResource, &handle);
			assert(status == NVAPI_OK);
			m_handleMGPU = handle;
		}
		//if (nWidth >= 4 && nHeight >= 4)
		{
			NvU32 value = 1;
			// disable driver watching this texture - it will never be synced unless requested by the application
			NvAPI_Status status = NvAPI_D3D_SetResourceHint(gcpRendD3D->GetDevice().GetRealDevice(), handle, NVAPI_D3D_SRH_CATEGORY_SLI, NVAPI_D3D_SRH_SLI_APP_CONTROLLED_INTERFRAME_CONTENT_SYNC, &value);
			assert(status == NVAPI_OK);
		}
	}
}

void CDeviceTexture::MgpuResourceUpdate(bool bUpdating)
{
	if (gcpRendD3D->m_bVendorLibInitialized && (gRenDev->GetFeatures() & RFT_HW_NVIDIA))
	{
		ID3D11Device* pDevice = gcpRendD3D->GetDevice().GetRealDevice();
		NVDX_ObjectHandle handle = (NVDX_ObjectHandle&)m_handleMGPU;
		if (handle == NULL)
		{
			NvAPI_Status status = NvAPI_D3D_GetObjectHandleForResource(pDevice, m_pNativeResource, &handle);
			assert(status == NVAPI_OK);
			m_handleMGPU = handle;
		}
		if (handle)
		{
			NvAPI_Status status = NVAPI_OK;
			if (bUpdating)
			{
				status = NvAPI_D3D_BeginResourceRendering(pDevice, handle, NVAPI_D3D_RR_FLAG_FORCE_DISCARD_CONTENT);
			}
			else
			{
				status = NvAPI_D3D_EndResourceRendering(pDevice, handle, 0);
			}

			// TODO: Verify that NVAPI_WAS_STILL_DRAWING is valid here
			assert(status == NVAPI_OK || status == NVAPI_WAS_STILL_DRAWING);
		}
	}
}
#endif
