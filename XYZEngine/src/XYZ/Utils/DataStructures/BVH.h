#pragma once
#include "XYZ/Utils/Math/AABB.h"
#include "XYZ/Utils/Math/Math.h"


namespace XYZ {

	struct BVHNode
	{
		static constexpr int32_t Invalid = -1;

		AABB	AABB;
		int32_t Depth = 0;
		int32_t Data = Invalid;
		int32_t Parent = Invalid;
		int32_t Left = Invalid;
		int32_t Right = Invalid;
	};

	struct BVHConstructData
	{
		AABB AABB;
		int32_t Data;
	};

	class BVH
	{
	public:
		void Construct(const std::vector<BVHConstructData>& constructData);
		void ConstructTest(std::vector<BVHConstructData>& constructData);
		void Clear() { m_Nodes.clear(); }

		void Traverse(const std::function<void(const BVHNode&, bool)>& action) const;

		const std::vector<BVHNode>& GetNodes() const { return m_Nodes; }
	private:
		int32_t construct(int32_t parent, int32_t depth, std::vector<BVHConstructData>& constructData, size_t start, size_t end);
	private:
		std::vector<BVHNode> m_Nodes;
	};
}