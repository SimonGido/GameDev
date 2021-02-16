#pragma once
#include "PhysicsShape.h"

#include <glm/glm.hpp>

namespace XYZ {

	struct Fixture
	{
		PhysicsShape* Shape;
		float Density;
	};

	class PhysicsBody
	{
	public:
		PhysicsBody(const glm::vec2& position, float angle, uint32_t id);
		
		void SetDensity(float density) { m_Density = density; recalculateMass(); }

		const glm::vec2& GetPosition() const { return m_Position; }
		const glm::vec2& GetVelocity() const { return m_Velocity; }
		float GetMass() const { return m_Mass;  }
		float GetAngle() const { return m_Angle; }
		uint32_t GetID() const { return m_ID; }
		const PhysicsShape* GetShape() const { return m_Shape; }



		enum class Type { Dynamic, Kinematic, Static };


		Type m_Type = Type::Dynamic;
		

		glm::vec2 m_OldPosition;
		glm::vec2 m_Position;
		float	  m_Angle;

		glm::vec2 m_Forces = glm::vec2(0.0f);
		glm::vec2 m_Velocity = glm::vec2(0.0f);
		float     m_AngularVelocity = 0.0f;
		float	  m_Mass = 0.0f;
		float     m_Friction = 1.0f;
		float	  m_Restitution = 1.0f;


	private:
		void recalculateMass();
		
	private:
		float m_Density = 1.0f;
		
		const uint32_t m_ID;

		PhysicsShape* m_Shape;
		friend class PhysicsWorld;
	};
}