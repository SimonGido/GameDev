#pragma once
#include "XYZ/Renderer/Framebuffer.h"

#include <GL/glew.h>

namespace XYZ {

	class OpenGLFramebuffer : public Framebuffer
	{
	public:
		OpenGLFramebuffer(const FramebufferSpecs& specs);
		virtual ~OpenGLFramebuffer();
		virtual void Release() const override;
		virtual void Resize(uint32_t width, uint32_t height, bool forceResize = false) override;

		virtual void Bind() const override;
		virtual void Unbind() const override;
		virtual void Clear() const override;

		virtual void BindTexture(uint32_t attachmentIndex, uint32_t slot) const override;
		virtual void BindImage(uint32_t attachmentIndex, uint32_t slot, uint32_t miplevel, BindImageType type) const override;
		virtual void SetSpecification(const FramebufferSpecs& specs) override;


		virtual const uint32_t GetColorAttachmentRendererID(uint32_t index) const override { return m_ColorAttachments[index].ID; }
		virtual const uint32_t GetDetphAttachmentRendererID() const override { return m_DepthAttachment; }
		virtual const uint32_t GetNumColorAttachments() const override { return (uint32_t)m_ColorAttachments.size(); }
	
		virtual const FramebufferSpecs& GetSpecification() const override { return m_Specification; }
		virtual void ReadPixel(int32_t& pixel, uint32_t mx, uint32_t my, uint32_t attachmentIndex) const override;
		virtual void ClearColorAttachment(uint32_t colorAttachmentIndex, void* clearValue) const override;
	private:
		struct ColorAttachment
		{
			uint32_t ID;
			bool     GenerateMips;
			GLenum   InternalFormat;
		};

		void destroyFramebuffer() const;

		static void attachColorTexture(ColorAttachment& attachment, int samples, GLenum format, uint32_t width, uint32_t height, int index);
	private:
		uint32_t m_RendererID = 0;
		FramebufferSpecs m_Specification;

		
		std::vector<ColorAttachment> m_ColorAttachments;
		uint32_t					 m_DepthAttachment;

		std::vector<ImageFormat> m_ColorAttachmentFormats;
		ImageFormat m_DepthAttachmentFormat = ImageFormat::None;
	};
}