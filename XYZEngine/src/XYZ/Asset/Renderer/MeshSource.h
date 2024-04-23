#pragma once
#include "XYZ/Asset/Asset.h"
#include "XYZ/Asset/Animation/SkeletonAsset.h"

#include "XYZ/Renderer/Buffer.h"

#include "XYZ/Utils/Math/AABB.h"

#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>

struct aiNode;
struct aiAnimation;
struct aiNodeAnim;
struct aiScene;

namespace XYZ {
	struct Vertex
	{
		glm::vec3 Position;
		glm::vec3 Normal;
		glm::vec3 Tangent;
		glm::vec3 Binormal;
		glm::vec2 TexCoord;
	};

	struct ColoredVertex
	{
		glm::vec3 Position;
		glm::vec3 Normal;
		glm::vec4 Color;
	};

	struct XYZ_API AnimatedVertex
	{
		glm::vec3 Position;
		glm::vec3 Normal;
		glm::vec3 Tangent;
		glm::vec3 Binormal;
		glm::vec2 TexCoord;
		uint32_t IDs[4] = { 0,0,0,0 };
		float Weights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

		void AddBoneData(uint32_t boneID, float weight);
	};

	struct Triangle
	{
		glm::vec3 V0, V1, V2;
	};

	struct XYZ_API BoneInfo
	{
		BoneInfo() = default;
		BoneInfo(const ozz::math::Float4x4& offset, const ozz::math::Float4x4& invTransform, uint32_t jointIndex)
			: BoneOffset(offset), InverseTransform(invTransform), JointIndex(jointIndex)
		{}
		ozz::math::Float4x4 BoneOffset;
		ozz::math::Float4x4 InverseTransform;
		uint32_t  JointIndex = 0;
	};


	class XYZ_API MeshSource : public Asset
	{
	public:
		MeshSource(const std::string& filepath);
		MeshSource(const aiScene* scene, const std::string& filepath);
		MeshSource(std::vector<Vertex> vertices, std::vector<uint32_t> indices);
		MeshSource(std::vector<ColoredVertex> vertices, std::vector<uint32_t> indices);
		MeshSource(std::vector<AnimatedVertex> vertices, std::vector<uint32_t> indices);

		virtual AssetType GetAssetType() const override { return AssetType::MeshSource; }

		const std::vector<AnimatedVertex>& GetAnimatedVertices() const { return m_AnimatedVertices; }
		const std::vector<Vertex>&		   GetVertices() const { return m_StaticVertices; }
		const std::vector<uint32_t>&	   GetIndices() const { return m_Indices; }
		const std::vector<Triangle>&	   GetTriangles() const { return m_Triangles; }
		
		const std::unordered_map<std::string, uint32_t> GetBoneMapping() const { return m_BoneMapping; }
		const std::vector<BoneInfo>						GetBoneInfo() const { return m_BoneInfo; }
		const std::string& GetSourceFilePath()     const { return m_SourceFilePath; }
		const glm::mat4&   GetInverseTransform()   const { return m_InverseTransform; }
		const glm::mat4&   GetSubmeshTransform()   const { return m_SubmeshTransform; }
		const AABB&		   GetSubmeshBoundingBox() const { return m_SubmeshBoundingBox; }

		const aiScene*	   GetScene()			 const { return m_Scene; }
		bool			   IsAnimated()			 const { return m_IsAnimated; }

		Ref<VertexBuffer>   GetVertexBuffer() const { return m_VertexBuffer; }
		Ref<IndexBuffer>    GetIndexBuffer()  const { return m_IndexBuffer; }

		static AssetType	GetStaticType() { return AssetType::MeshSource; }

	private:	
		void loadFromScene(const aiScene* scene);
		void loadSkeleton(const aiScene* scene);
		void loadMeshes(const aiScene* scene);
		void loadBoneInfo(const aiScene* scene);
		void setupTriangles();
		void traverseNodes(aiNode* node, const glm::mat4& parentTransform);
		void updateBoundingBox(const glm::vec3& position);
	
		uint32_t findJointIndex(const std::string& name) const;
	private:
		std::string m_SourceFilePath;
		

		std::vector<AnimatedVertex> m_AnimatedVertices;
		std::vector<Vertex>			m_StaticVertices;
		std::vector<ColoredVertex>	m_StaticColoredVertices;

		std::vector<uint32_t>		m_Indices;
		std::vector<Triangle>		m_Triangles;

		ozz::unique_ptr<ozz::animation::Skeleton> m_Skeleton;
		std::unordered_map<std::string, uint32_t> m_BoneMapping;
		
		const aiScene*							  m_Scene;
		bool									  m_IsAnimated;
		
		Ref<VertexBuffer>	  m_VertexBuffer;
		Ref<IndexBuffer>	  m_IndexBuffer;

		std::vector<BoneInfo> m_BoneInfo;
		uint32_t			  m_BoneCount = 0;

		glm::mat4			  m_InverseTransform;
		// TODO: Per submesh
		glm::mat4			  m_SubmeshInverseTransform;
		glm::mat4			  m_SubmeshTransform;
		AABB				  m_SubmeshBoundingBox;
	};
}