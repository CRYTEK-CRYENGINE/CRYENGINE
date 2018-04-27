// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

template<typename T, int constantBufferSlot>
class CTypedConstantBuffer
{
	T                m_hostBuffer;

	CConstantBuffer* m_constantBuffer;

public:
	CTypedConstantBuffer() : m_constantBuffer(nullptr) {}

	CConstantBuffer* GetDeviceConstantBuffer()
	{
		if (!m_constantBuffer)
		{
			CreateDeviceBuffer();
		}
		return m_constantBuffer;
	}

	void CreateDeviceBuffer()
	{
		int size = sizeof(T);
		m_constantBuffer = gcpRendD3D->m_DevBufMan.CreateConstantBuffer(size);
	}
	void CopyToDevice()
	{
		D3D11_MAPPED_SUBRESOURCE mapped;
		HRESULT hr =
		  gcpRendD3D.GetDeviceContext().Map(m_constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

		if (hr != S_OK)
			return;

		memcpy(mapped.pData, &m_hostBuffer, sizeof(T));

		gcpRendD3D.GetDeviceContext().Unmap(m_constantBuffer, 0);
#ifdef DURANGO
		gcpRendD3D.GetPerformanceDeviceContext().FlushGpuCacheRange(
		  D3D11_FLUSH_KCACHE_INVALIDATE, nullptr, D3D11_FLUSH_GPU_CACHE_RANGE_ALL);
#endif
	}
	void Bind()
	{
		ID3D11Buffer* buf[] = { m_constantBuffer };
		gcpRendD3D.GetDeviceContext().CSSetConstantBuffers(constantBufferSlot, 1u, buf);
	}
	void BindPixelShader()
	{
		ID3D11Buffer* buf[] = { m_constantBuffer };
		gcpRendD3D.GetDeviceContext().PSSetConstantBuffers(constantBufferSlot, 1u, buf);
	}
	void BindGeometryShader()
	{
		ID3D11Buffer* buf[] = { m_constantBuffer };
		gcpRendD3D.GetDeviceContext().GSSetConstantBuffers(constantBufferSlot, 1u, buf);
	}
	T*   operator->()              { return &m_hostBuffer; }
	bool IsDeviceBufferAllocated() { return m_constantBuffer != nullptr; }
	T&   operator=(const T& hostData)
	{
		return m_hostBuffer = hostData;
	}
};
