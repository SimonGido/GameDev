#pragma once
#include "XYZ/Asset/Renderer/VoxelMeshSource.h"


namespace XYZ {
    class XYZ_API VoxelMeshCollision
    {
    public:
        struct CollisionResult
        {
            glm::vec3 CollisionNormal;
            bool      Collide;
        };

        static CollisionResult IsCollision(const glm::vec3& point, const VoxelSubmesh& submesh);

    private:

    };

}