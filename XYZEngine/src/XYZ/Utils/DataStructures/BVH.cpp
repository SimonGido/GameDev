#include "stdafx.h"
#include "BVH.h"



namespace XYZ {

	static size_t FindClosestToLast(const std::vector<BVHNode>& nodes)
	{
		size_t result = 0;
		if (nodes.size() == 0)
			return result;

		float minDistance = FLT_MAX;
		size_t lastIndex = nodes.size() - 1;

		for (size_t i = 0; i < nodes.size() - 1; i++)
		{
			float newDistance = glm::distance(nodes[lastIndex].AABB.GetCenter(), nodes[i].AABB.GetCenter());
			if (newDistance < minDistance)
			{
				minDistance = newDistance;
				result = i;
			}
		}
		return result;
	}

	

	static size_t FindClosestToStart(const std::vector<BVHNode>& nodes, const std::vector<size_t>& indices)
	{
		size_t result = 0;
		if (nodes.size() == 0)
			return result;

		float minDistance = FLT_MAX;
	
		for (size_t i = 1; i < indices.size(); i++)
		{
			float newDistance = glm::distance(nodes[indices[0]].AABB.GetCenter(), nodes[indices[i]].AABB.GetCenter());
			if (newDistance < minDistance)
			{
				minDistance = newDistance;
				result = i;
			}
		}
		return result;
	}


	static size_t FindMinArea(const std::vector<BVHNode>& nodes, const std::vector<size_t>& indices)
	{
		size_t result = 0;
		if (nodes.size() == 0)
			return result;

		float minArea = FLT_MAX;

		const BVHNode& startNode = nodes[indices[0]];

		for (size_t i = 1; i < indices.size(); i++)
		{
			const BVHNode& mergeNode = nodes[indices[i]];

			AABB mergedAABB = AABB::Union(startNode.AABB, mergeNode.AABB);
			float newArea = mergedAABB.CalculateArea();
			if (newArea < minArea)
			{
				minArea = newArea;
				result = i;
			}
		}
		return result;
	}


	void BVH::Construct(const std::vector<BVHConstructData>& constructData)
	{
		m_Nodes.clear();
		if (constructData.size() == 0)
			return;

		std::vector<size_t> downNodeIndices;
		for (size_t i = 0; i < constructData.size(); i++)
		{
			const auto& data = constructData[i];
			m_Nodes.push_back({ data.AABB, 0, data.Data });
			downNodeIndices.push_back(i);
		}
		std::vector<size_t> topNodeIndices;
		do
		{
			size_t leftDownIndex = 0;
			size_t rightDownIndex = FindMinArea(m_Nodes, downNodeIndices);

			size_t leftIndex = downNodeIndices[leftDownIndex];
			size_t rightIndex = downNodeIndices[rightDownIndex];

			BVHNode& leftNode = m_Nodes[leftIndex];
			BVHNode& rightNode = m_Nodes[rightIndex];

			if (downNodeIndices.size() == 1) // Only one downNode is left
			{
				downNodeIndices = std::move(topNodeIndices); // Top nodes are now down nodes
				downNodeIndices.push_back(leftIndex); // Include one unpaired down node
				continue;
			}

			BVHNode parentNode;
			parentNode.AABB = AABB::Union(leftNode.AABB, rightNode.AABB);

			parentNode.Left = leftIndex;
			parentNode.Right = rightIndex;
			leftNode.Parent = m_Nodes.size();
			rightNode.Parent = m_Nodes.size();

			m_Nodes.push_back(parentNode); // Push new created parent

			topNodeIndices.push_back(m_Nodes.size() - 1); // Push created parent as top down index
			downNodeIndices.erase(downNodeIndices.begin() + rightDownIndex); // First remove from back
			downNodeIndices.erase(downNodeIndices.begin() + leftDownIndex); // Remove from front ( leftDownIndex is 0 )

			if (downNodeIndices.size() == 0) // We traverse all downNodes
			{
				downNodeIndices = std::move(topNodeIndices);
			}

		} while (downNodeIndices.size() != 1 || topNodeIndices.size() != 0);

		std::stack<size_t> nodeIndices;
		nodeIndices.push(m_Nodes.size() - 1);
		while (!nodeIndices.empty())
		{
			size_t index = nodeIndices.top();
			nodeIndices.pop();

			auto& node = m_Nodes[index];
			bool hasLeftChild = node.Left != BVHNode::Invalid;
			bool hasRightChild = node.Right != BVHNode::Invalid;
			bool hasParent = node.Parent != BVHNode::Invalid;

			if (hasParent)
			{
				auto& parent = m_Nodes[node.Parent];
				node.Depth = parent.Depth + 1;
			}

			if (hasLeftChild)
			{
				nodeIndices.push(node.Left);
			}
			if (hasRightChild)
			{
				nodeIndices.push(node.Right);
			}
		}
	}
}