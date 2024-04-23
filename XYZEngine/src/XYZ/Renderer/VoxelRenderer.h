#pragma once
#include "PipelineCompute.h"
#include "StorageBufferSet.h"
#include "UniformBufferSet.h"
#include "StorageBufferAllocator.h"
#include "VoxelRendererDebug.h"
#include "VoxelMesh.h"
#include "Material.h"


#include "XYZ/Scene/Scene.h"
#include "XYZ/Utils/DataStructures/BVH.h"
#include "XYZ/Utils/DataStructures/AABBGrid.h"
#include "XYZ/Utils/DataStructures/Octree.h"
#include "XYZ/Asset/Renderer/VoxelMeshSource.h"

namespace XYZ {



	struct UBVoxelScene
	{
		glm::mat4 InverseProjection;
		glm::mat4 InverseView;
		glm::vec4 CameraPosition;
		glm::vec4 ViewportSize;
		
		// Light info
		DirectionalLight DirectionalLight;

		static constexpr uint32_t Binding = 16;
		static constexpr uint32_t Set = 0;
	};


	struct SSBOVoxels
	{
		static constexpr uint32_t MaxVoxels = 2 * 1024 * 1024 * 1024; // 2gb
				

		static constexpr uint32_t Binding = 17;
		static constexpr uint32_t Set = 0;
	};



	struct VoxelModel
	{
		glm::mat4	InverseTransform;

		uint32_t	VoxelOffset;
		uint32_t	Width;
		uint32_t	Height;
		uint32_t	Depth;
		uint32_t    ColorIndex;

		float		VoxelSize;	
		uint32_t    CellsOffset;
		uint32_t    CompressScale;
		Bool32		Compressed = false;
		float		DistanceFromCamera;
		bool		Opaque;
		Padding<4>	Padding;
	};



	struct SSBOVoxelModels
	{
		static constexpr uint32_t MaxModels = 1024;
		
		uint32_t OpaqueModelCount;
		uint32_t TransparentModelCount;

		Padding<8> Padding;
		
		VoxelModel	 Models[MaxModels];

		static constexpr uint32_t Binding = 18;
		static constexpr uint32_t Set = 0;
	};

	struct SSBOColors
	{
		static constexpr uint32_t MaxColors = 1024;
		static constexpr uint32_t ColorPalleteSize = 256 * sizeof(uint32_t);
		static constexpr uint32_t MaxSize = MaxColors * ColorPalleteSize;

		static constexpr uint32_t Binding = 19;
		static constexpr uint32_t Set = 0;
	};

	struct VoxelCompressedCell
	{
		uint32_t VoxelCount;
		uint32_t VoxelOffset;
	};

	struct SSBOVoxelCompressed
	{
		static constexpr uint32_t MaxCompressedCells = 40 * 1024 * 1024;

		VoxelCompressedCell CompressedCells[MaxCompressedCells];

		static constexpr uint32_t Binding = 20;
		static constexpr uint32_t Set = 0;
	};

	struct SSBOVoxelComputeData
	{
		static constexpr uint32_t MaxSize = 1024 * 1024 * 1024 * 2; // 2gb

		static constexpr uint32_t Binding = 21;
		static constexpr uint32_t Set = 0;
	};


	struct VoxelRendererCamera
	{
		glm::mat4 ViewProjectionMatrix;
		glm::mat4 ViewMatrix;
		glm::mat4 Projection;
		glm::vec3 CameraPosition;
		Math::Frustum Frustum;
	};


	class XYZ_API VoxelRenderer : public RefCount
	{
	public:
		struct VoxelRenderModel
		{
			uint32_t  SubmeshIndex;
			uint32_t  ModelIndex;
			AABB	  BoundingBox;
			glm::mat4 Transform;
			Ref<VoxelMesh> Mesh;
			float	  DistanceFromCamera;
		};

	public:
		VoxelRenderer();

		void BeginScene(const VoxelRendererCamera& camera);
		void EndScene();
		void EndScene(const std::function<void()>& postPrepareModels);

		void SetViewportSize(uint32_t width, uint32_t height);

		bool SubmitMesh(const Ref<VoxelMesh>& mesh, const glm::mat4& transform);
		bool SubmitMesh(const Ref<VoxelMesh>& mesh, const glm::mat4& transform, int32_t* indices);
		bool SubmitMesh(const Ref<VoxelMesh>& mesh, const glm::mat4& transform, const uint32_t* keyFrames);
		void SubmitMesh(const Ref<MaterialAsset>& material, const Ref<Mesh>& mesh, const glm::mat4& transform, uint32_t instanceCount);

		void SubmitEffect(const Ref<MaterialAsset>& material, const glm::ivec3& workGroups, const PushConstBuffer& constants);

		void OnImGuiRender();

		bool CreateComputeAllocation(uint32_t size, StorageBufferAllocation& allocation);
		void SubmitComputeData(const void* data, uint32_t size, uint32_t offset, const StorageBufferAllocation& allocation, bool allFrames = false);
	
		bool IsMeshAllocated(const Ref<VoxelMesh>& mesh) const;
		const VoxelRenderModel& GetVoxelRenderModel(uint32_t index) const { return m_RenderModels[index]; }

		uint32_t	 GetModelCount() const { return m_SSBOVoxelModels.OpaqueModelCount + m_SSBOVoxelModels.TransparentModelCount; }
		Ref<Image2D> GetFinalPassImage() const;
	private:
		

		struct VoxelMeshBucket
		{
			Ref<VoxelMesh> Mesh;
			std::vector<VoxelRenderModel*> Models;
		};

	
		struct VoxelEffectInvocation
		{
			glm::ivec3 WorkGroups;
			PushConstBuffer Constants;
		};

		struct VoxelEffectCommand
		{
			Ref<MaterialAsset> Material;
			std::vector<VoxelEffectInvocation> Invocations;
		};

		struct RenderCommandData
		{
			Ref<Mesh> Mesh;
			glm::mat4 Transform;
			uint32_t  InstanceCount;
		};

		struct RenderCommand
		{
			Ref<MaterialAsset> Material;
			std::vector<RenderCommandData> Data;
		};

		struct SSGIValues
		{
			uint32_t SampleCount = 1;
			float	 IndirectAmount = 1.0f;
			float	 NoiseAmount = 1.0f;
			Bool32	 Noise = true;
		};

		struct MeshAllocation
		{
			StorageBufferAllocation VoxelAllocation;
			StorageBufferAllocation	CompressAllocation;
			StorageBufferAllocation	ColorAllocation;

			struct OffsetPair
			{
				uint32_t Voxel = 0;
				uint32_t CompressedCell = 0;
			};

			std::vector<OffsetPair> Offsets;
		};

		struct Statistics
		{
			uint32_t ModelCount = 0;
		};

	private:
		bool submitSubmesh(const Ref<VoxelMesh>& mesh, const VoxelSubmesh& submesh, const glm::mat4& transform, uint32_t submeshIndex);

		void clearPass();
		void lightPass();
		void effectPass();
		void raymarchPass();
		void renderPass();
		void postRasterPass();
		void ssgiPass();

		void debugPass();
		
		void imageBarrier(Ref<PipelineCompute> pipeline, Ref<Image2D> image);
		void ssboBarrier(Ref<PipelineCompute> pipeline);

		void updateViewportSize();
		void updateUniformBufferSet();

		void prepareModels();
	
		void updateVoxelModelsSSBO();

		void createDefaultPipelines();
		Ref<PipelineCompute> getEffectPipeline(const Ref<MaterialAsset>& material);
		Ref<Pipeline>		 getEffectPipelineRaster(const Ref<MaterialAsset>& material);

		MeshAllocation&		 createMeshAllocation(const Ref<VoxelMesh>& mesh);	

		void reallocateVoxels(const Ref<VoxelMesh>& mesh, MeshAllocation& allocation);

		void createRenderPass();
		void createPostRasterRenderPass();
		void createPostRasterPipeline();

	private:
		Ref<PrimaryRenderCommandBuffer> m_CommandBuffer;
		Ref<RenderPass>			m_RenderPass;
		Ref<PipelineCompute>	m_RaymarchPipeline;
		Ref<Material>			m_RaymarchMaterial;

		Ref<RenderPass>			m_PostRasterRenderPass;
		Ref<Pipeline>			m_PostRasterPipeline;
		Ref<Material>			m_PostRasterMaterial;
		Ref<MaterialInstance>	m_PostRasterMaterialInstance;


		Ref<PipelineCompute>	m_ClearPipeline;
		Ref<Material>			m_ClearMaterial;

		Ref<PipelineCompute>	m_LightPipeline;
		Ref<Material>			m_LightMaterial;

		Ref<PipelineCompute>	m_ShadowPipeline;
		Ref<Material>			m_ShadowMaterial;

		Ref<PipelineCompute>	m_SSGIPipeline;
		Ref<Material>			m_SSGIMaterial;

		Ref<StorageBufferSet>	m_StorageBufferSet;
		Ref<UniformBufferSet>	m_UniformBufferSet;


		Ref<StorageBufferAllocator> m_VoxelStorageAllocator;
		Ref<StorageBufferAllocator> m_CompressedCellAllocator;
		Ref<StorageBufferAllocator> m_ColorStorageAllocator;
		Ref<StorageBufferAllocator> m_ComputeStorageAllocator;


		Ref<Texture2D>			m_OutputTexture;
		Ref<Texture2D>			m_NormalTexture;
		Ref<Texture2D>			m_PositionTexture;
		Ref<Texture2D>			m_DepthTexture;
		Ref<Texture2D>			m_SSGITexture;

		UBVoxelScene			m_UBVoxelScene;
		SSBOVoxelModels			m_SSBOVoxelModels;

		SSGIValues				m_SSGIValues;
		glm::ivec2				m_ViewportSize;
		glm::ivec2				m_WorkGroups;
		bool				    m_ViewportSizeChanged = false;

		Statistics				m_Statistics;
		Math::Frustum			m_Frustum;
	
		bool					m_UseSSGI = false;
		bool					m_ShowDepth = false;
		bool					m_ShowNormals = false;

		bool					m_Debug = false;
		bool					m_DebugOpaque = false;

		int32_t					m_ShowBVHDepth = -1;

		std::vector<VoxelRenderModel*>					 m_RenderModelsSorted;
		std::vector<VoxelRenderModel>					 m_RenderModels;
		std::unordered_map<AssetHandle, VoxelMeshBucket> m_VoxelMeshBuckets;
		std::map<AssetHandle, VoxelEffectCommand>		 m_EffectCommands;
		std::map<AssetHandle, RenderCommand>			 m_RenderCommands;


		std::unordered_map<AssetHandle, MeshAllocation> m_MeshAllocations;
		std::unordered_map<AssetHandle, MeshAllocation> m_LastFrameMeshAllocations;

		std::unordered_map<AssetHandle, Ref<PipelineCompute>> m_EffectPipelines;
		std::unordered_map<AssetHandle, Ref<Pipeline>>		  m_EffectPipelinesRaster;

		struct GPUTimeQueries
		{
			uint32_t GPUTime = 0;

			static constexpr uint32_t Count() { return sizeof(GPUTimeQueries) / sizeof(uint32_t); }
		};
		GPUTimeQueries m_GPUTimeQueries;

		std::unique_ptr<VoxelRendererDebug> m_DebugRenderer;
	};

}
