#pragma once
#include "RenderCommandBuffer.h"
#include "RenderPass.h"

#include <glm/glm.hpp>


struct GLFWwindow;
namespace XYZ {
	class APIContext : public RefCount
	{
	public:
		virtual ~APIContext() = default;

		virtual void Init(GLFWwindow* window) = 0;
		virtual void CreateSwapChain(uint32_t* width, uint32_t* height, bool vSync) {};
		virtual void SwapBuffers() = 0;
		virtual void BeginFrame() {}
		virtual void OnResize(uint32_t width, uint32_t height) {};

		virtual Ref<RenderCommandBuffer> GetRenderCommandBuffer() = 0;
		virtual Ref<RenderPass>			 GetRenderPass() { return Ref<RenderPass>(); }

		static Ref<APIContext> Create();
	};
}