#pragma once
#include <memory>
#include <vector>

#include "XYZ/Core/Ref/Ref.h"
#include "XYZ/Renderer/Texture.h"
#include "XYZ/Renderer/Image.h"

#include <glm/glm.hpp>

namespace XYZ {
	enum class FramebufferBlendMode
	{
		None = 0,
		OneZero,
		SrcAlphaOneMinusSrcAlpha,
		Additive,
		Zero_SrcColor
	};
	struct XYZ_API FramebufferTextureSpecification
	{
		FramebufferTextureSpecification() = default;
		FramebufferTextureSpecification(ImageFormat format, bool generateMips = false)
			: Format(format), GenerateMips(generateMips) {}

		ImageFormat			 Format;
		bool				 GenerateMips;
		bool				 Blend = true;

		FramebufferBlendMode BlendMode = FramebufferBlendMode::SrcAlphaOneMinusSrcAlpha;
	};


	struct FramebufferSpecification
	{
		uint32_t  Width = 0;
		uint32_t  Height = 0;
		uint32_t  Samples = 1; // multisampling
		glm::vec4 ClearColor  = { 0.0f, 0.0f, 0.0f, 1.0f };
		bool	  ClearOnLoad = true;
		std::vector<FramebufferTextureSpecification> Attachments;

		bool SwapChainTarget = false;
		bool Multiview = false;
	};


	class XYZ_API Framebuffer : public RefCount
	{
	public:
		virtual ~Framebuffer() = default;

		virtual void Resize(uint32_t width, uint32_t height, bool forceResize = false) = 0;

		virtual void SetSpecification(const FramebufferSpecification& specs, bool recreate = false) = 0;

		virtual const uint32_t GetNumColorAttachments() const = 0;
		virtual Ref<Image2D> GetImage(uint32_t attachmentIndex = 0) const = 0;
		virtual Ref<Image2D> GetDepthImage() const = 0;

		virtual const FramebufferSpecification& GetSpecification() const = 0;
		
		
		static Ref<Framebuffer> Create(const FramebufferSpecification& specs);
	};

}