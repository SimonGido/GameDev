#pragma once
#include "XYZ/Asset/Renderer/VoxelMeshSource.h"


namespace XYZ {
    class XYZ_API VoxelMeshCollision
    {
    public:
        struct CollisionResult
        {
            uint32_t  VoxelIndex = 0;
            uint32_t  CellIndex = 0;
            bool      Collide = false;
        };

        static CollisionResult IsCollision(const glm::vec3& point, const VoxelSubmesh& submesh, const glm::mat4& transform);

    private:

    };

}