#pragma once
#include "XYZ/Renderer/RendererAPI.h"

namespace XYZ {

	class VulkanRendererAPI : public RendererAPI
	{
	public:
		virtual void Init() override;
		virtual void TestDraw(const Ref<RenderPass>& renderPass, const Ref<RenderCommandBuffer>& commandBuffer, const Ref<Pipeline>& pipeline) override;
	};
}