#include "stdafx.h"
#include "AABB.h"



namespace XYZ {



	AABB::AABB()
		: Min(0.0f), Max(0.0f) 
	{}
	AABB::AABB(const glm::vec3& min, const glm::vec3& max)
		: Min(min), Max(max) 
	{}
	float AABB::CalculateArea() const
	{
		const glm::vec3 diff = Max - Min;
		return 2.0f * (diff.x * diff.y * diff.z + diff.z * diff.x);
	}

	float AABB::CalculateVolume() const
	{
		float volume = (Max.x - Min.x) * (Max.y - Min.y) * (Max.z - Min.z);
		return volume;
	}

	float AABB::GetPerimeter() const
	{
		const float wx = Max.x - Min.x;
		const float wy = Max.y - Min.y;
		return 2.0f * (wx + wy);
	}
	bool AABB::Contains(const AABB& aabb) const
	{
		return (aabb.Min.x >= Min.x) &&
			(aabb.Min.y >= Min.y) &&
			(aabb.Min.z >= Min.z) &&
			(aabb.Max.x <= Max.x) &&
			(aabb.Max.y <= Max.y) &&
			(aabb.Max.z <= Max.z);
	}

	bool AABB::Intersect(const AABB& aabb) const
	{
		const float d1x = aabb.Min.x - Max.x;
		const float d1y = aabb.Min.y - Max.y;
		const float d2x = Min.x - aabb.Max.x;
		const float d2y = Min.y - aabb.Max.y;

		if (d1x > 0.0f || d1y > 0.0f)
			return false;

		if (d2x > 0.0f || d2y > 0.0f)
			return false;

		return true;
	}

	static bool Within(float min, float val, float max)
	{
		return min <= val && val <= max;
	}

	bool AABB::InsideFrustum(const glm::mat4& mvp) const
	{
		glm::vec4 corners[8] = {
			{Min.x, Min.y, Min.z, 1.0}, // x y z
			{Max.x, Min.y, Min.z, 1.0}, // X y z
			{Min.x, Max.y, Min.z, 1.0}, // x Y z
			{Max.x, Max.y, Min.z, 1.0}, // X Y z

			{Min.x, Min.y, Max.z, 1.0}, // x y Z
			{Max.x, Min.y, Max.z, 1.0}, // X y Z
			{Min.x, Max.y, Max.z, 1.0}, // x Y Z
			{Max.x, Max.y, Max.z, 1.0}, // X Y Z
		};

		bool inside = false;

		for (size_t corner_idx = 0; corner_idx < 8; corner_idx++) 
		{
			// Transform vertex
			glm::vec4 corner = mvp * corners[corner_idx];
			// Check vertex against clip space bounds
			inside |= 
				Within(-corner.w, corner.x, corner.w) &&
				Within(-corner.w, corner.y, corner.w) &&
				Within(0.0f, corner.z, corner.w);
		}
		return inside;
	}

	bool AABB::IsOnPlane(const Math::Plane& plane) const
	{
		const glm::vec3 center = GetCenter();
		const glm::vec3 extents = Max - center;
		// Compute the projection interval radius of b onto L(t) = b.c + t * p.n
		const float r = extents.x * std::abs(plane.Normal.x) + extents.y * std::abs(plane.Normal.y) +
			extents.z * std::abs(plane.Normal.z);

		return -r <= plane.GetSignedDistanceToPlane(center);
	}

	bool AABB::InsideFrustum(const Math::Frustum& frustum) const
	{
		return IsOnPlane(frustum.LeftFace)
			&& IsOnPlane(frustum.RightFace)
			&& IsOnPlane(frustum.FarFace)
			&& IsOnPlane(frustum.NearFace)
			&& IsOnPlane(frustum.TopFace)
			&& IsOnPlane(frustum.BottomFace);
	}

	void AABB::Union(const AABB& aabb)
	{
		*this = AABB::Union(*this, aabb);
	}


	glm::vec3 AABB::GetCenter() const
	{
		return Min + (Max - Min) / 2.0f;
	}

	glm::vec3 AABB::ClosestPoint(const glm::vec3& startPoint) const
	{
		glm::vec3 closestPoint;

		closestPoint.x = std::max(Min.x, std::min(startPoint.x, Max.x));
		closestPoint.y = std::max(Min.y, std::min(startPoint.y, Max.y));
		closestPoint.z = std::max(Min.z, std::min(startPoint.z, Max.z));

		return closestPoint;
	}

	float AABB::Distance(const glm::vec3& point) const
	{
		glm::vec3 closestPoint = ClosestPoint(point);
		float dx = closestPoint.x - point.x;
		float dy = closestPoint.y - point.y;
		float dz = closestPoint.z - point.z;

		return std::sqrt(dx * dx + dy * dy + dz * dz);
	}

	AABB AABB::TransformAABB(const glm::mat4& transform) const
	{
		glm::vec4 corners[8] = {
			{Min.x, Min.y, Min.z, 1.0}, // x y z
			{Max.x, Min.y, Min.z, 1.0}, // X y z
			{Min.x, Max.y, Min.z, 1.0}, // x Y z
			{Max.x, Max.y, Min.z, 1.0}, // X Y z

			{Min.x, Min.y, Max.z, 1.0}, // x y Z
			{Max.x, Min.y, Max.z, 1.0}, // X y Z
			{Min.x, Max.y, Max.z, 1.0}, // x Y Z
			{Max.x, Max.y, Max.z, 1.0}, // X Y Z
		};
		
		AABB result;
		result.Min = glm::vec3(std::numeric_limits<float>::max());
		result.Max = glm::vec3(-std::numeric_limits<float>::max());

		for (uint32_t i = 0; i < 8; ++i)
		{
			corners[i] = transform * corners[i];
			result.Min.x = std::min(result.Min.x, corners[i].x);
			result.Min.y = std::min(result.Min.y, corners[i].y);
			result.Min.z = std::min(result.Min.z, corners[i].z);

			result.Max.x = std::max(result.Max.x, corners[i].x);
			result.Max.y = std::max(result.Max.y, corners[i].y);
			result.Max.z = std::max(result.Max.z, corners[i].z);
		}

		return result;
	}

	AABB AABB::Union(const AABB& a, const AABB& b)
	{
		AABB c;
		c.Min.x = std::min(a.Min.x, b.Min.x);
		c.Min.y = std::min(a.Min.y, b.Min.y);
		c.Min.z = std::min(a.Min.z, b.Min.z);

		c.Max.x = std::max(a.Max.x, b.Max.x);
		c.Max.y = std::max(a.Max.y, b.Max.y);
		c.Max.z = std::max(a.Max.z, b.Max.z);
		return c;
	}

	AABB AABB::operator+(const glm::vec2& val) const
	{
		AABB res;
		res.Min = Min + glm::vec3(val, 0.0f);
		res.Max = Max + glm::vec3(val, 0.0f);
		return res;
	}

}