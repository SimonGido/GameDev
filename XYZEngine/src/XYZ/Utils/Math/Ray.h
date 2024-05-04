#pragma once
#include "XYZ/Core/Core.h"
#include "AABB.h"

#include <glm/glm.hpp>


namespace XYZ {

    struct XYZ_API Ray
    {
        glm::vec3 Origin, Direction;

        Ray() = default;

        Ray(const glm::vec3& origin, const glm::vec3& direction)
        {
            Origin = origin;
            Direction = direction;
        }

        static Ray Zero()
        {
            return { {0.0f, 0.0f, 0.0f},{0.0f, 0.0f, 0.0f} };
        }

        static Ray CastRay(glm::vec2 mouse, const glm::mat4& proj, const glm::mat4& view, const glm::vec3& pos);

        bool IntersectsAABB(const AABB& aabb, float& t) const;
        

        bool IntersectsTriangle(const glm::vec3& A, const glm::vec3& B, const glm::vec3& C, float& t) const;
       
    };

}