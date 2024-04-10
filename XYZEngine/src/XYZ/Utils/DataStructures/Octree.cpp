#include "stdafx.h"
#include "Octree.h"

#include "XYZ/Debug/Profiler.h"

#include <queue>

namespace XYZ {
	static bool VoxelInsideNode(const glm::ivec3& voxel, const VoxelOctreeNode& node)
	{
		return voxel.x >= node.X
			&& voxel.y >= node.Y
			&& voxel.z >= node.Z
			&& voxel.x < node.X + node.Size
			&& voxel.y < node.Y + node.Size
			&& voxel.z < node.Z + node.Size;
	}

	static uint32_t Index3D(uint32_t x, uint32_t y, uint32_t z, uint32_t width, uint32_t height)
	{
		return x + width * (y + height * z);
	}


	Octree::Octree(const AABB& aabb, uint32_t maxDepth)
		:
		m_MaxDepth(maxDepth)
	{
		auto& root = m_Nodes.emplace_back();
		root.BoundingBox = aabb;
	}
	void Octree::Initialize(const AABB& aabb, uint32_t maxDepth)
	{
		m_MaxDepth = maxDepth;
		m_Nodes.clear();
		auto& root = m_Nodes.emplace_back();
		root.BoundingBox = aabb;
	}
	void Octree::InsertData(const AABB& aabb, int32_t data)
	{
		XYZ_PROFILE_FUNC("Octree::InsertData");
		constexpr float resizeConstant = 1.5f;
		OctreeNode* root = &m_Nodes[0];
		if (!root->BoundingBox.Contains(aabb))
		{
			AABB newAABB;
			newAABB.Min.x = std::min(root->BoundingBox.Min.x, aabb.Min.x) * resizeConstant;
			newAABB.Min.y = std::min(root->BoundingBox.Min.y, aabb.Min.y) * resizeConstant;
			newAABB.Min.z = std::min(root->BoundingBox.Min.z, aabb.Min.z) * resizeConstant;

			newAABB.Max.x = std::max(root->BoundingBox.Max.x, aabb.Max.x) * resizeConstant;
			newAABB.Max.y = std::max(root->BoundingBox.Max.y, aabb.Max.y) * resizeConstant;
			newAABB.Max.z = std::max(root->BoundingBox.Max.z, aabb.Max.z) * resizeConstant;

			Octree newOctree = resize(newAABB, *this);
			m_Nodes = std::move(newOctree.m_Nodes);
		}

		std::stack<int32_t> nodesToIterate;
		nodesToIterate.push(0); // Start from root

		while (!nodesToIterate.empty())
		{
			int32_t nodeIndex = nodesToIterate.top();
			nodesToIterate.pop();

			OctreeNode* node = &m_Nodes[nodeIndex];
			XYZ_ASSERT(node->Depth <= m_MaxDepth, "Wrong depth");

			bool canInsert = true;
			if (node->IsLeaf && node->Depth != m_MaxDepth)
			{
				splitNode(nodeIndex);
			}

			node = &m_Nodes[nodeIndex];
			if (!node->IsLeaf)
			{
				for (int32_t childIndex : node->Children)
				{
					OctreeNode* childNode = &m_Nodes[childIndex];
					if (childNode->BoundingBox.Contains(aabb))
					{
						nodesToIterate.push(childIndex);
						canInsert = false;
						break;
					}
				}
			}
			if (canInsert)
			{
				node->Data.push_back({ data, aabb });
				break;
			}
		}
	}

	bool Octree::TryInsert(const AABB& aabb, int32_t data, const Math::Frustum& frustum)
	{
		constexpr float resizeConstant = 1.5f;
		OctreeNode* root = &m_Nodes[0];
		if (!root->BoundingBox.Contains(aabb))
		{
			AABB newAABB;
			newAABB.Min.x = std::min(root->BoundingBox.Min.x, aabb.Min.x) * resizeConstant;
			newAABB.Min.y = std::min(root->BoundingBox.Min.y, aabb.Min.y) * resizeConstant;
			newAABB.Min.z = std::min(root->BoundingBox.Min.z, aabb.Min.z) * resizeConstant;

			newAABB.Max.x = std::max(root->BoundingBox.Max.x, aabb.Max.x) * resizeConstant;
			newAABB.Max.y = std::max(root->BoundingBox.Max.y, aabb.Max.y) * resizeConstant;
			newAABB.Max.z = std::max(root->BoundingBox.Max.z, aabb.Max.z) * resizeConstant;

			Octree newOctree = resize(newAABB, *this);
			m_Nodes = std::move(newOctree.m_Nodes);
		}

		std::stack<int32_t> nodesToIterate;
		nodesToIterate.push(0); // Start from root

		while (!nodesToIterate.empty())
		{
			int32_t nodeIndex = nodesToIterate.top();
			nodesToIterate.pop();

			OctreeNode* node = &m_Nodes[nodeIndex];
			if (!node->BoundingBox.InsideFrustum(frustum))
				return false;

			XYZ_ASSERT(node->Depth <= m_MaxDepth, "Wrong depth");

			bool canInsert = true;
			if (node->IsLeaf && node->Depth != m_MaxDepth)
			{
				splitNode(nodeIndex);
			}

			node = &m_Nodes[nodeIndex];
			if (!node->IsLeaf)
			{
				for (int32_t childIndex : node->Children)
				{
					OctreeNode* childNode = &m_Nodes[childIndex];
					if (childNode->BoundingBox.Contains(aabb))
					{
						nodesToIterate.push(childIndex);
						canInsert = false;
						break;
					}
				}
			}
			if (canInsert)
			{
				node->Data.push_back({ data, aabb });
				break;
			}
		}
		return true;
	}

	void Octree::Clear()
	{
		for (auto& node : m_Nodes)
		{
			node.Data.clear();
		}
	}

	int32_t Octree::FindNode(const glm::vec3& position)
	{
		std::stack<int32_t> nodesToIterate;
		nodesToIterate.push(0); // Start from root

		while (!nodesToIterate.empty())
		{
			int32_t nodeIndex = nodesToIterate.top();
			nodesToIterate.pop();

			OctreeNode* node = &m_Nodes[nodeIndex];

			bool canReturn = true;
			if (!node->IsLeaf)
			{
				for (int32_t childIndex : node->Children)
				{
					OctreeNode* childNode = &m_Nodes[childIndex];

					if (Math::PointInBox(position, childNode->BoundingBox.Min, childNode->BoundingBox.Max))
					{
						nodesToIterate.push(childIndex);
						canReturn = false;
						break;
					}
				}
			}
			if (canReturn)
			{
				if (Math::PointInBox(position, node->BoundingBox.Min, node->BoundingBox.Max))
					return nodeIndex;
			}
		}
		return 0;
	}

	void Octree::SortNodeDistance(int32_t nodeIndex, const glm::vec3& position)
	{
		OctreeNode* node = &m_Nodes[nodeIndex];
		if (!node->IsLeaf)
		{
			std::sort(&node->Children[0], &node->Children[0] + 8, [this, position](int32_t indexA, int32_t indexB) {

				OctreeNode* aChild = &m_Nodes[indexA];
				OctreeNode* bChild = &m_Nodes[indexB];

				return aChild->BoundingBox.Distance(position) < bChild->BoundingBox.Distance(position);
			});
		}
	}

	void Octree::SortNodeDistanceInverse(int32_t nodeIndex, const glm::vec3& position)
	{
		OctreeNode* node = &m_Nodes[nodeIndex];
		if (!node->IsLeaf)
		{
			std::sort(&node->Children[0], &node->Children[0] + 8, [this, position](int32_t indexA, int32_t indexB) {

				OctreeNode* aChild = &m_Nodes[indexA];
				OctreeNode* bChild = &m_Nodes[indexB];

				return aChild->BoundingBox.Distance(position) > bChild->BoundingBox.Distance(position);
			});
		}
	}

	void Octree::splitNode(int32_t nodeIndex)
	{
		OctreeNode* parent = &m_Nodes[nodeIndex];
		parent->IsLeaf = false;
		const glm::vec3 halfSize = (parent->BoundingBox.Max - parent->BoundingBox.Min) / 2.0f;

		uint32_t childIndex = 0;
		for (uint32_t z = 0; z < 2; ++z)
		{
			for (uint32_t y = 0; y < 2; ++y)
			{
				for (uint32_t x = 0; x < 2; ++x)
				{
					parent->Children[childIndex] = static_cast<int32_t>(m_Nodes.size());
					auto& child = m_Nodes.emplace_back();
					parent = &m_Nodes[nodeIndex];

					child.BoundingBox.Min = parent->BoundingBox.Min + glm::vec3(x, y, z) * halfSize;
					child.BoundingBox.Max = child.BoundingBox.Min + halfSize;
					child.Depth = parent->Depth + 1;
					childIndex++;
				}
			}
		}
	}
	Octree Octree::resize(const AABB& aabb, const Octree& originalOctree)
	{
		Octree newOctree(aabb, originalOctree.m_MaxDepth);
		for (auto& node : originalOctree.m_Nodes)
		{
			for (auto& data : node.Data)
				newOctree.InsertData(data.BoundingBox, data.Data);
		}
		return newOctree;
	}
	VoxelOctree::VoxelOctree(const std::vector<uint8_t>& voxels, uint32_t width, uint32_t height, uint32_t depth)
		:
		m_Width(width),
		m_Height(height),
		m_Depth(depth)
	{
		VoxelOctreeNode& root = m_Nodes.emplace_back();
		uint32_t maxDimension = std::max(width, std::max(height, depth));
		root.Size = Math::RoundUp(maxDimension, 16);


		std::queue<uint32_t> nodesToIterate;
		nodesToIterate.push(0); // Start from root

		while (!nodesToIterate.empty())
		{
			int32_t nodeIndex = nodesToIterate.front();
			nodesToIterate.pop();

			bool isUniform = true;
			VoxelOctreeNode* node = &m_Nodes[nodeIndex];
			const uint8_t colorIndex = voxels[Index3D(node->X, node->Y, node->Z, width, height)];
			for (uint32_t x = node->X; x < node->X + node->Size; ++x)
			{
				for (uint32_t y = node->Y; y < node->Y + node->Size; ++y)
				{
					for (uint32_t z = node->Z; z < node->Z + node->Size; ++z)
					{
						const uint8_t newColorIndex = voxels[Index3D(x, y, z, width, height)];
						if (colorIndex != newColorIndex)
						{
							isUniform = false;
							break;
						}
					}
					if (!isUniform)
						break;
				}
				if (!isUniform)
					break;
			}
			if (!isUniform)
			{
				splitNode(nodeIndex);
				node = &m_Nodes[nodeIndex];
				for (uint32_t i = 0; i < 8; ++i)
					nodesToIterate.push(node->Children[i]);
			}
			else
			{
				node->ColorIndex = colorIndex;
			}
		}

	}


	
	VoxelOctree VoxelOctree::FromGrid(const std::vector<uint8_t>& voxels, uint32_t width, uint32_t height, uint32_t depth)
	{
		return VoxelOctree(voxels, width, height, depth);
	}

	std::vector<uint8_t> VoxelOctree::ToGrid() const
	{
		std::vector<uint8_t> result(m_Width * m_Height * m_Depth, 0);
		for (auto& node : m_Nodes)
		{
			if (node.IsLeaf)
			{
				for (uint32_t x = 0; x < node.Size; ++x)
				{
					for (uint32_t y = 0; y < node.Size; ++y)
					{
						for (uint32_t z = 0; z < node.Size; ++z)
						{
							const uint32_t index = Index3D(node.X + x, node.Y + y, node.Z + z, m_Width, m_Height);
							if (index < result.size())
								result[index] = static_cast<uint8_t>(node.ColorIndex);
						}
					}
				}
			}
		}
		return result;
	}


	void VoxelOctree::splitNode(int32_t nodeIndex)
	{
		VoxelOctreeNode* parent = &m_Nodes[nodeIndex];
		parent->IsLeaf = false;
		uint16_t halfSize = parent->Size / 2;

		uint32_t childIndex = 0;
		for (uint32_t z = 0; z < 2; ++z)
		{
			for (uint32_t y = 0; y < 2; ++y)
			{
				for (uint32_t x = 0; x < 2; ++x)
				{
					parent->Children[childIndex] = static_cast<int32_t>(m_Nodes.size());
					auto& child = m_Nodes.emplace_back();
					parent = &m_Nodes[nodeIndex];

					child.Size = halfSize;

					child.X = parent->X + x * halfSize;
					child.Y = parent->Y + y * halfSize;
					child.Z = parent->Z + z * halfSize;
					childIndex++;
				}
			}
		}
	}
}