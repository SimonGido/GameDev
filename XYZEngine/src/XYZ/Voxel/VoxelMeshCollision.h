#pragma once
#include "XYZ/Asset/Renderer/VoxelMeshSource.h"
#include "XYZ/Utils/Math/AABB.h"

namespace XYZ {
    class XYZ_API VoxelMeshCollision
    {
    public:
        struct CollisionResult
        {
            std::vector<uint32_t> VoxelIndices;
        };

        static CollisionResult IsCollision(const glm::vec3& point, const VoxelSubmesh& submesh, const glm::mat4& transform);
        static CollisionResult IsCollision(const AABB& aabb, const VoxelSubmesh& submesh, const glm::mat4& transform);

    private:
        
    };

}