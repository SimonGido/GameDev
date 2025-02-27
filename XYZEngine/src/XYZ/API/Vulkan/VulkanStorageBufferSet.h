#pragma once
#include "XYZ/Renderer/StorageBufferSet.h"
#include "VulkanStorageBuffer.h"
#include "VulkanShader.h"
#include "Vulkan.h"

namespace XYZ {

	class VulkanStorageBufferSet : public StorageBufferSet
	{
	public:
		VulkanStorageBufferSet(uint32_t frames);
		virtual ~VulkanStorageBufferSet() override;

		virtual void Update(const void* data, uint32_t size, uint32_t offset, uint32_t binding, uint32_t set = 0) override;
		virtual void Update(void** data, uint32_t size, uint32_t offset, uint32_t binding, uint32_t set = 0) override;
		virtual void UpdateEachFrame(const void* data, uint32_t size, uint32_t offset, uint32_t binding, uint32_t set = 0) override;
		virtual void CreateDescriptors(const Ref<Shader>& shader) override;
		virtual void Create(uint32_t size, uint32_t set, uint32_t binding, bool indirect = false, bool shared = false) override;
		virtual void Set(Ref<StorageBuffer> storageBuffer, uint32_t set = 0, uint32_t frame = 0) override;
		virtual void Resize(uint32_t size, uint32_t set, uint32_t binding) override;
		virtual void SetBufferInfo(uint32_t size, uint32_t offset, uint32_t binding, uint32_t set = 0) override;
		virtual Ref<StorageBuffer> Get(uint32_t binding, uint32_t set = 0, uint32_t frame = 0) const override;
		

		const vector2D<Ref<VulkanStorageBuffer>>& GetIndirect() const { return m_IndirectBuffers; }
		const vector3D<VkWriteDescriptorSet>&     GetDescriptors(size_t hash) const;
		const vector3D<VkWriteDescriptorSet>&     GetDescriptors(const Ref<Shader>& shader);
		const map3D<uint32_t, uint32_t, uint32_t, Ref<VulkanStorageBuffer>>& GetStorageBuffers() const { return m_StorageBuffers; }
	private:
		void RT_createDescriptors(const Ref<VulkanShader>& shader);

		ByteBuffer getBuffer(uint32_t set, uint32_t binding, uint32_t size);

	private:
		uint32_t m_Frames;

		// shader hash -> per frame write data -> per set
		std::unordered_map<size_t, vector3D<VkWriteDescriptorSet>> m_WriteDescriptors;

		// frame -> set -> binding
		map3D<uint32_t, uint32_t, uint32_t, Ref<VulkanStorageBuffer>> m_StorageBuffers;

		// set -> binding
		map2D<uint32_t, uint32_t, ThreadQueue<ByteBuffer>> m_DataTransferBuffers;
	
		vector2D<Ref<VulkanStorageBuffer>> m_IndirectBuffers;
	};
}