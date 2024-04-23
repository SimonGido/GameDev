#include "stdafx.h"
#include "MeshSource.h"
#include "XYZ/Asset/AssimpImporter.h"
#include "XYZ/Asset/AssimpLog.h"
#include "XYZ/Utils/Math/Math.h"

#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/Importer.hpp>
#include <assimp/DefaultLogger.hpp>
#include <assimp/LogStream.hpp>

#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <ozz/animation/offline/skeleton_builder.h>

#include <ozz/animation/offline/animation_builder.h>
#include <ozz/animation/runtime/animation.h>
#include <ozz/animation/runtime/local_to_model_job.h>
#include <ozz/animation/runtime/sampling_job.h>
#include <ozz/base/span.h>


namespace XYZ {
	namespace Utils {

		glm::mat4 Mat4FromAssimpMat4(const aiMatrix4x4& matrix)
		{
			glm::mat4 result;
			//the a,b,c,d in assimp is the row ; the 1,2,3,4 is the column
			result[0][0] = matrix.a1; result[1][0] = matrix.a2; result[2][0] = matrix.a3; result[3][0] = matrix.a4;
			result[0][1] = matrix.b1; result[1][1] = matrix.b2; result[2][1] = matrix.b3; result[3][1] = matrix.b4;
			result[0][2] = matrix.c1; result[1][2] = matrix.c2; result[2][2] = matrix.c3; result[3][2] = matrix.c4;
			result[0][3] = matrix.d1; result[1][3] = matrix.d2; result[2][3] = matrix.d3; result[3][3] = matrix.d4;
			return result;
		}
		ozz::math::Float4x4 Float4x4FromMat4(const glm::mat4& mat)
		{
			ozz::math::Float4x4 result;
			result.cols[0] = ozz::math::simd_float4::LoadPtrU(glm::value_ptr(mat[0]));
			result.cols[1] = ozz::math::simd_float4::LoadPtrU(glm::value_ptr(mat[1]));
			result.cols[2] = ozz::math::simd_float4::LoadPtrU(glm::value_ptr(mat[2]));
			result.cols[3] = ozz::math::simd_float4::LoadPtrU(glm::value_ptr(mat[3]));
			return result;
		}

		ozz::math::Float4x4 Float4x4FromAIMatrix4x4(const aiMatrix4x4& matrix)
		{
			ozz::math::Float4x4 result;
			result.cols[0] = ozz::math::simd_float4::Load(matrix.a1, matrix.b1, matrix.c1, matrix.d1);
			result.cols[1] = ozz::math::simd_float4::Load(matrix.a2, matrix.b2, matrix.c2, matrix.d2);
			result.cols[2] = ozz::math::simd_float4::Load(matrix.a3, matrix.b3, matrix.c3, matrix.d3);
			result.cols[3] = ozz::math::simd_float4::Load(matrix.a4, matrix.b4, matrix.c4, matrix.d4);
			return result;
		}
	}

	static const uint32_t s_MeshImportFlags =
		aiProcess_CalcTangentSpace |        // Create binormals/tangents just in case
		aiProcess_Triangulate |             // Make sure we're triangles
		aiProcess_SortByPType |             // Split meshes by primitive type
		aiProcess_GenNormals |              // Make sure we have legit normals
		aiProcess_GenUVCoords |             // Convert UVs if required 
		aiProcess_OptimizeMeshes |          // Batch draws where possible
		aiProcess_JoinIdenticalVertices |
		aiProcess_GlobalScale |             // e.g. convert cm to m for fbx import (and other formats where cm is native)
		aiProcess_ValidateDataStructure;    // Validation


	MeshSource::MeshSource(const std::string& filepath)
		:
		m_SourceFilePath(filepath)
	{
		LogStream::Initialize();
		auto importer = std::make_unique<Assimp::Importer>();
		const aiScene* scene = importer->ReadFile(m_SourceFilePath, s_MeshImportFlags);
		if (!scene || !scene->HasMeshes() || scene->mNumMeshes > 1)
		{
			XYZ_CORE_ERROR("Failed to load mesh file: {0}", m_SourceFilePath);
			SetFlag(AssetFlag::Invalid);
			return;
		}
		m_Scene = scene;
		loadFromScene(scene);
	}
	MeshSource::MeshSource(const aiScene* scene, const std::string& filepath)
		:
		m_SourceFilePath(filepath)
	{
		m_Scene = scene;
		if (!scene || !scene->HasMeshes() || scene->mNumMeshes > 1)
		{
			XYZ_CORE_ERROR("Failed to load mesh file: {0}", m_SourceFilePath);
			SetFlag(AssetFlag::Invalid);
			return;
		}
		loadFromScene(scene);
	}
	MeshSource::MeshSource(std::vector<Vertex> vertices, std::vector<uint32_t> indices)
		:
		m_StaticVertices(std::move(vertices)),
		m_Indices(std::move(indices)),
		m_SubmeshTransform(1.0f),
		m_InverseTransform(1.0f),
		m_Scene(nullptr),
		m_IsAnimated(false)
	{
		m_SubmeshBoundingBox.Min = { FLT_MAX, FLT_MAX, FLT_MAX };
		m_SubmeshBoundingBox.Max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
		for (const auto& vertex : m_StaticVertices)
		{
			updateBoundingBox(vertex.Position);
		}
		setupTriangles();

		m_VertexBuffer = VertexBuffer::Create(m_StaticVertices.data(), static_cast<uint32_t>(m_StaticVertices.size() * sizeof(Vertex)));
		m_IndexBuffer = IndexBuffer::Create(m_Indices.data(), static_cast<uint32_t>(m_Indices.size()));
	}

	MeshSource::MeshSource(std::vector<ColoredVertex> vertices, std::vector<uint32_t> indices)
		:
		m_StaticColoredVertices(std::move(vertices)),
		m_Indices(std::move(indices)),
		m_SubmeshTransform(1.0f),
		m_InverseTransform(1.0f),
		m_Scene(nullptr),
		m_IsAnimated(false)
	{
		m_SubmeshBoundingBox.Min = { FLT_MAX, FLT_MAX, FLT_MAX };
		m_SubmeshBoundingBox.Max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
		for (const auto& vertex : m_StaticColoredVertices)
		{
			updateBoundingBox(vertex.Position);
		}
		setupTriangles();

		m_VertexBuffer = VertexBuffer::Create(m_StaticColoredVertices.data(), static_cast<uint32_t>(m_StaticColoredVertices.size() * sizeof(ColoredVertex)));
		m_IndexBuffer = IndexBuffer::Create(m_Indices.data(), static_cast<uint32_t>(m_Indices.size()));
	}

	MeshSource::MeshSource(std::vector<AnimatedVertex> vertices, std::vector<uint32_t> indices)
		:
		m_AnimatedVertices(std::move(vertices)),
		m_Indices(std::move(indices)),
		m_SubmeshTransform(1.0f),
		m_InverseTransform(1.0f),
		m_Scene(nullptr),
		m_IsAnimated(true)
	{
		m_SubmeshBoundingBox.Min = { FLT_MAX, FLT_MAX, FLT_MAX };
		m_SubmeshBoundingBox.Max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
		for (const auto& vertex : m_AnimatedVertices)
		{
			updateBoundingBox(vertex.Position);
		}
		setupTriangles();

		m_VertexBuffer = VertexBuffer::Create(m_AnimatedVertices.data(), static_cast<uint32_t>(m_AnimatedVertices.size() * sizeof(AnimatedVertex)));
		m_IndexBuffer = IndexBuffer::Create(m_Indices.data(), static_cast<uint32_t>(m_Indices.size()));
	}
	

	void MeshSource::loadFromScene(const aiScene* scene)
	{
		m_IsAnimated = scene->mAnimations != nullptr;
		m_InverseTransform = glm::inverse(Utils::Mat4FromAssimpMat4(scene->mRootNode->mTransformation));

		m_SubmeshBoundingBox.Min = { FLT_MAX, FLT_MAX, FLT_MAX };
		m_SubmeshBoundingBox.Max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

		loadMeshes(scene);
		loadSkeleton(scene);
		traverseNodes(scene->mRootNode, glm::mat4(1.0f));
		loadBoneInfo(scene);
		setupTriangles();


		if (m_IsAnimated)
			m_VertexBuffer = VertexBuffer::Create(m_AnimatedVertices.data(), static_cast<uint32_t>(m_AnimatedVertices.size() * sizeof(AnimatedVertex)));
		else
			m_VertexBuffer = VertexBuffer::Create(m_StaticVertices.data(), static_cast<uint32_t>(m_StaticVertices.size() * sizeof(Vertex)));

		m_IndexBuffer = IndexBuffer::Create(m_Indices.data(), static_cast<uint32_t>(m_Indices.size()));
	}

	void MeshSource::loadSkeleton(const aiScene* scene)
	{
		ozz::animation::offline::RawSkeleton rawSkeleton;
		if (AssimpImporter::ExtractRawSkeleton(scene, rawSkeleton))
		{
			ozz::animation::offline::SkeletonBuilder builder;
			m_Skeleton = builder(rawSkeleton);
			if (!m_Skeleton)
				XYZ_CORE_ERROR("Failed to build runtime skeleton from file {0}", m_SourceFilePath);
		}
		else
		{
			XYZ_CORE_WARN("No skeleton in file {0}", m_SourceFilePath);
		}
	}

	void MeshSource::loadMeshes(const aiScene* scene)
	{
		for (unsigned m = 0; m < scene->mNumMeshes; m++)
		{
			aiMesh* mesh = scene->mMeshes[m];
			if (m_IsAnimated)
			{
				for (size_t i = 0; i < mesh->mNumVertices; i++)
				{
					AnimatedVertex vertex;
					vertex.Position = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
					vertex.Normal = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };
					updateBoundingBox(vertex.Position);

					if (mesh->HasTangentsAndBitangents())
					{
						vertex.Tangent = { mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z };
						vertex.Binormal = { mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z };
					}
					
					if (mesh->HasTextureCoords(0))
						vertex.TexCoord = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };

					m_AnimatedVertices.push_back(vertex);
				}
			}
			else
			{
				for (size_t i = 0; i < mesh->mNumVertices; i++)
				{
					Vertex vertex;
					vertex.Position = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
					vertex.Normal = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };
					updateBoundingBox(vertex.Position);

					if (mesh->HasTangentsAndBitangents())
					{
						vertex.Tangent = { mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z };
						vertex.Binormal = { mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z };
					}

					if (mesh->HasTextureCoords(0))
						vertex.TexCoord = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };

					m_StaticVertices.push_back(vertex);
				}
			}
			for (size_t i = 0; i < mesh->mNumFaces; i++)
			{
				XYZ_ASSERT(mesh->mFaces[i].mNumIndices == 3, "Must have 3 indices.");
				m_Indices.push_back(mesh->mFaces[i].mIndices[0]);
				m_Indices.push_back(mesh->mFaces[i].mIndices[1]);
				m_Indices.push_back(mesh->mFaces[i].mIndices[2]);
			}
		}
	}

	void MeshSource::loadBoneInfo(const aiScene* scene)
	{
		if (m_IsAnimated)
		{
			for (size_t m = 0; m < scene->mNumMeshes; m++)
			{
				aiMesh* mesh = scene->mMeshes[m];
				for (size_t i = 0; i < mesh->mNumBones; i++)
				{
					aiBone* bone = mesh->mBones[i];
					std::string boneName(bone->mName.data);
					int boneIndex = 0;

					if (m_BoneMapping.find(boneName) == m_BoneMapping.end())
					{
						// Allocate an index for a new bone
						boneIndex = m_BoneCount;
						m_BoneCount++;
						m_BoneInfo.emplace_back(
							Utils::Float4x4FromAIMatrix4x4(bone->mOffsetMatrix),
							ozz::math::Invert(Utils::Float4x4FromMat4(m_SubmeshTransform)),
							findJointIndex(boneName)
						);
						m_BoneMapping[boneName] = boneIndex;
					}
					else
					{
						boneIndex = m_BoneMapping[boneName];
					}

					for (size_t j = 0; j < bone->mNumWeights; j++)
					{
						int VertexID = bone->mWeights[j].mVertexId;
						float Weight = bone->mWeights[j].mWeight;
						m_AnimatedVertices[VertexID].AddBoneData(boneIndex, Weight);
					}
				}
			}
		}
	}
	void MeshSource::setupTriangles()
	{
		if (m_IsAnimated)
		{
			for (size_t i = 0; i < m_Indices.size(); i += 3)
			{
				const auto& v1 = m_AnimatedVertices[m_Indices[i]];
				const auto& v2 = m_AnimatedVertices[m_Indices[i + 1]];
				const auto& v3 = m_AnimatedVertices[m_Indices[i + 2]];

				m_Triangles.push_back({ v1.Position, v2.Position, v3.Position });
			}
		}
		else if (!m_StaticVertices.empty())
		{
			for (size_t i = 0; i < m_Indices.size(); i += 3)
			{
				const auto& v1 = m_StaticVertices[m_Indices[i]];
				const auto& v2 = m_StaticVertices[m_Indices[i + 1]];
				const auto& v3 = m_StaticVertices[m_Indices[i + 2]];

				m_Triangles.push_back({ v1.Position, v2.Position, v3.Position });
			}
		}
		else
		{
			for (size_t i = 0; i < m_Indices.size(); i += 3)
			{
				const auto& v1 = m_StaticColoredVertices[m_Indices[i]];
				const auto& v2 = m_StaticColoredVertices[m_Indices[i + 1]];
				const auto& v3 = m_StaticColoredVertices[m_Indices[i + 2]];

				m_Triangles.push_back({ v1.Position, v2.Position, v3.Position });
			}
		}
	}
	void MeshSource::traverseNodes(aiNode* node, const glm::mat4& parentTransform)
	{
		glm::mat4 localTransform = Utils::Mat4FromAssimpMat4(node->mTransformation);
		glm::mat4 transform = parentTransform * localTransform;
		for (uint32_t i = 0; i < node->mNumMeshes; i++)
		{
			// TODO: for every submesh
			m_SubmeshTransform = transform;
			m_SubmeshInverseTransform = glm::inverse(transform);
		}
		for (uint32_t i = 0; i < node->mNumChildren; i++)
			traverseNodes(node->mChildren[i], transform);
	}
	void MeshSource::updateBoundingBox(const glm::vec3& position)
	{
		m_SubmeshBoundingBox.Min.x = glm::min(position.x, m_SubmeshBoundingBox.Min.x);
		m_SubmeshBoundingBox.Min.y = glm::min(position.y, m_SubmeshBoundingBox.Min.y);
		m_SubmeshBoundingBox.Min.z = glm::min(position.z, m_SubmeshBoundingBox.Min.z);

		m_SubmeshBoundingBox.Max.x = glm::max(position.x, m_SubmeshBoundingBox.Max.x);
		m_SubmeshBoundingBox.Max.y = glm::max(position.y, m_SubmeshBoundingBox.Max.y);
		m_SubmeshBoundingBox.Max.z = glm::max(position.z, m_SubmeshBoundingBox.Max.z);
	}

	uint32_t MeshSource::findJointIndex(const std::string& name) const
	{
		uint32_t jointIndex = ~0;
		for (size_t j = 0; j < m_Skeleton->joint_names().size(); ++j)
		{
			if (name == m_Skeleton->joint_names()[j])
			{
				jointIndex = static_cast<int>(j);
				break;
			}
		}
		return jointIndex;
	}
	void AnimatedVertex::AddBoneData(uint32_t boneID, float weight)
	{
		for (size_t i = 0; i < 4; i++)
		{
			if (Weights[i] == 0.0)
			{
				IDs[i] = boneID;
				Weights[i] = weight;
				return;
			}
		}
		XYZ_CORE_WARN("Vertex has more than four bones/weights affecting it, extra data will be discarded (BoneID={0}, Weight={1})", boneID, weight);
	}

}