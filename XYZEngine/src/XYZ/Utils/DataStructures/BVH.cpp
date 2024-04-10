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

	static float SplitCost(float areaLeft, float areaRight, int countLeft, int countRight, float totalArea) 
	{
		const float traversalCost = 1.0f; // Adjust this if needed
		return traversalCost + (areaLeft / totalArea) * countLeft + (areaRight / totalArea) * countRight;
	}

	struct SAHCost
	{
		size_t LeftCount = 0;
		size_t RightCount = 0;
		float Cost = std::numeric_limits<float>::infinity();
	};

	static SAHCost CalculateSahCost(std::vector<BVHConstructData>& boxes, int axis, size_t start, size_t end)
	{
		SAHCost result;
		float totalArea = 0.0f;
		size_t totalCount = end - start;

		// Calculate total surface area and count of boxes
		for (size_t i = start; i < end; i++)
			totalArea += boxes[i].AABB.CalculateArea();

		// Sort the boxes along the chosen axis
		std::sort(boxes.begin() + start, boxes.begin() + end, [axis](const BVHConstructData& a, const BVHConstructData& b) {
			if (axis == 0)
				return a.AABB.Min.x < b.AABB.Min.x;
			if (axis == 1)
				return a.AABB.Min.y < b.AABB.Min.y;
			return a.AABB.Min.z < b.AABB.Min.z;
		});

		// Calculate SAH cost for each potential split
		float areaLeft = 0.0f;
		float areaRight = totalArea;
		size_t leftCount = 0;
		size_t rightCount = totalCount;

		for (size_t i = start; i < end; ++i) 
		{
			const AABB& box = boxes[i].AABB;
			float boxArea = box.CalculateArea();

			areaLeft += boxArea;
			areaRight -= boxArea;
			
			leftCount++;
			rightCount--;

			float cost = SplitCost(areaLeft, areaRight, leftCount, rightCount, totalArea);
			if (cost < result.Cost)
			{
				result.Cost = cost;
				result.LeftCount = leftCount;
				result.RightCount = rightCount;
			}
		}

		return result;
	}

	void BVH::ConstructTest(std::vector<BVHConstructData>& constructData)
	{
		AABB rootAABB(glm::vec3(FLT_MAX), glm::vec3(-FLT_MAX));
		for (const auto& data : constructData)
			rootAABB.Union(data.AABB);

		float minCost = std::numeric_limits<float>::infinity();
		int splitAxis = 0;
		size_t leftCount = 0;
		size_t rightCount = 0;
		for (int axis = 0; axis < 3; ++axis)
		{
			SAHCost sah = CalculateSahCost(constructData, axis, 0, constructData.size());
			if (sah.Cost < minCost) 
			{
				minCost = sah.Cost;
				leftCount = sah.LeftCount;
				rightCount = sah.RightCount;
				splitAxis = axis;
			}
		}

		std::sort(constructData.begin(), constructData.end(), [splitAxis](const BVHConstructData& a, const BVHConstructData& b) {
			if (splitAxis == 0)
				return a.AABB.Min.x < b.AABB.Min.x;
			if (splitAxis == 1)
				return a.AABB.Min.y < b.AABB.Min.y;
			return a.AABB.Min.z < b.AABB.Min.z;
		});

		
		m_Nodes.emplace_back();
		m_Nodes[0].Depth = 0;
		m_Nodes[0].AABB = rootAABB;
		m_Nodes[0].Left = construct(0, 1, constructData, 0, leftCount);
		m_Nodes[0].Right = construct(0, 1, constructData, leftCount, constructData.size());
	}
	void BVH::Traverse(const std::function<void(const BVHNode&, bool)>& action) const
	{
		std::stack<int32_t> nodeIndices;
		nodeIndices.push(0);

		while (!nodeIndices.empty())
		{
			int32_t nodeIndex = nodeIndices.top();
			nodeIndices.pop();

			auto& node = m_Nodes[nodeIndex];
			if (node.Left != BVHNode::Invalid)
			{
				action(m_Nodes[node.Left], true);
				nodeIndices.push(node.Left);
			}
			if (node.Right != BVHNode::Invalid)
			{
				action(m_Nodes[node.Right], false);
				nodeIndices.push(node.Right);
			}
		}
	}
	int32_t BVH::construct(int32_t parent, int32_t depth, std::vector<BVHConstructData>& constructData, size_t start, size_t end)
	{
		if (end - start == 0)
			return BVHNode::Invalid;

		AABB nodeAABB(glm::vec3(FLT_MAX), glm::vec3(-FLT_MAX));
		for (size_t i = start; i < end; i++)
			nodeAABB.Union(constructData[i].AABB);

		int32_t result = static_cast<int32_t>(m_Nodes.size());
		m_Nodes.emplace_back();
		m_Nodes[result].Parent = parent;
		m_Nodes[result].Depth = depth;
		m_Nodes[result].AABB = nodeAABB;

		if (end - start == 1)
		{
			m_Nodes[result].Data = constructData[start].Data;
			return result;
		}

		
		float minCost = std::numeric_limits<float>::infinity();
		int splitAxis = 0;
		size_t leftCount = 0;
		size_t rightCount = 0;
		for (int axis = 0; axis < 3; ++axis)
		{
			SAHCost sah = CalculateSahCost(constructData, axis, start, end);
			if (sah.Cost < minCost)
			{
				minCost = sah.Cost;
				leftCount = sah.LeftCount;
				rightCount = sah.RightCount;
				splitAxis = axis;
			}
		}

		std::sort(constructData.begin() + start, constructData.begin() + end, [splitAxis](const BVHConstructData& a, const BVHConstructData& b) {
			if (splitAxis == 0)
				return a.AABB.Min.x < b.AABB.Min.x;
			if (splitAxis == 1)
				return a.AABB.Min.y < b.AABB.Min.y;
			return a.AABB.Min.z < b.AABB.Min.z;
		});

		m_Nodes[result].Left = construct(result, depth + 1, constructData, start, start + leftCount);
		m_Nodes[result].Right = construct(result, depth + 1, constructData, start + leftCount, end);
		return result;
	}
}