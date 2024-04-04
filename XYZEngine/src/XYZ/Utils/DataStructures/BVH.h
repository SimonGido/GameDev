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

		void Clear() { m_Nodes.clear(); }

		const std::vector<BVHNode>& GetNodes() const { return m_Nodes; }
	private:
	
	private:
		std::vector<BVHNode> m_Nodes;
	};
}