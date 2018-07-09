// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

////////////////////////////////////////////////////////////////////////////
template<typename T>
static inline XG_BIND_FLAG ConvertToXGBindFlags(const T& desc)
{
	CRY_ASSERT_MESSAGE((desc & (CDeviceObjectFactory::BIND_RENDER_TARGET | CDeviceObjectFactory::BIND_DEPTH_STENCIL)) != (CDeviceObjectFactory::BIND_RENDER_TARGET | CDeviceObjectFactory::BIND_DEPTH_STENCIL), "RenderTarget and DepthStencil can't be requested together!");

	// *INDENT-OFF*
	return XG_BIND_FLAG(
		((desc & CDeviceObjectFactory::BIND_VERTEX_BUFFER   ) ? XG_BIND_VERTEX_BUFFER              : 0) |
		((desc & CDeviceObjectFactory::BIND_INDEX_BUFFER    ) ? XG_BIND_INDEX_BUFFER               : 0) |
		((desc & CDeviceObjectFactory::BIND_CONSTANT_BUFFER ) ? XG_BIND_CONSTANT_BUFFER            : 0) |
		((desc & CDeviceObjectFactory::BIND_SHADER_RESOURCE ) ? XG_BIND_SHADER_RESOURCE            : 0) |
	//	((desc & CDeviceObjectFactory::BIND_STREAM_OUTPUT   ) ? XG_BIND_STREAM_OUTPUT              : 0) |
		((desc & CDeviceObjectFactory::BIND_RENDER_TARGET   ) ? XG_BIND_RENDER_TARGET              : 0) |
		((desc & CDeviceObjectFactory::BIND_DEPTH_STENCIL   ) ? XG_BIND_DEPTH_STENCIL              : 0) |
		((desc & CDeviceObjectFactory::BIND_UNORDERED_ACCESS) ? XG_BIND_UNORDERED_ACCESS           : 0));
	// *INDENT-ON*
}
