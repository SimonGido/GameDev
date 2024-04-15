#include "stdafx.h"
#include "SpaceColonization.h"

#include "XYZ/Utils/Random.h"
#include "XYZ/Utils/Algorithms/Raymarch.h"

#include <glm/ext/scalar_constants.hpp>

namespace XYZ {

	static uint32_t Index3D(const glm::ivec3& index, uint32_t width, uint32_t height)
	{
		return index.x + width * (index.y + height * index.z);
	}


	SpaceColonization::SpaceColonization(const SCInitializer& initializer)
		:
		m_BranchLength(initializer.BranchLength),
		m_KillRange(initializer.KillRange),
		m_AttractionRange(initializer.AttractionRange)
	{
		m_Attractors.resize(initializer.AttractorsCount);
		for (uint32_t i = 0; i < initializer.AttractorsCount; i++)
		{
			float theta = 2.0 * glm::pi<float>() * RandomNumber(0.0f, 1.0f);      // Random angle around the vertical axis
			float phi = acos(2.0 * RandomNumber(0.0f, 1.0f) - 1.0);   // Random angle from the vertical axis

			// Convert spherical coordinates to Cartesian coordinates
			m_Attractors[i].x = initializer.AttractorsCenter.x + RandomNumber(0.0f, initializer.AttractorsRadius) * sin(phi) * cos(theta);
			m_Attractors[i].y = initializer.AttractorsCenter.y + RandomNumber(0.0f, initializer.AttractorsRadius) * sin(phi) * sin(theta);
			m_Attractors[i].z = initializer.AttractorsCenter.z + RandomNumber(0.0f, initializer.AttractorsRadius) * cos(phi);
		}
		m_RootBranch = new SCBranch();
		m_RootBranch->Start = initializer.RootPosition;
		m_RootBranch->End = initializer.RootPosition + glm::vec3(0.0f, initializer.BranchLength, 0.0f);
		m_RootBranch->Direction = glm::normalize(glm::vec3(0.0f, 1.0f, 0.0f) + randomGrowVector(m_GrowSize));
		m_RootBranch->Parent = nullptr;
		m_Branches.push_back(m_RootBranch);
		m_Extremities.push_back(m_RootBranch);
	}
	void SpaceColonization::Grow()
	{
		// we remove the attractors in kill range
		for (int32_t i = m_Attractors.size() - 1; i >= 0; i--) 
		{
			for (SCBranch* branch : m_Branches)
			{
				if (glm::distance(branch->End, m_Attractors[i]) < m_KillRange) 
				{
					m_Attractors.erase(m_Attractors.begin() + i);
					break;
				}
			}
		}

		if (!m_Attractors.empty())
		{
			// we clear the active attractors
			m_ActiveAttractors.clear();
			for (SCBranch* branch : m_Branches)
			{
				branch->Attractors.clear();
			}

			// each attractor is associated to its closest branch, if in attraction range
			uint32_t attractorIndex = 0;
			for (const glm::vec3& attractor : m_Attractors) 
			{
				float min = std::numeric_limits<float>::max();
				SCBranch* closest = nullptr; // will store the closest branch
				for (SCBranch* branch : m_Branches) 
				{
					float distance = glm::distance(branch->End, attractor);
					if (distance < m_AttractionRange && distance < min)
					{
						min = distance;
						closest = branch;
					}
				}

				// if a branch has been found, we add the attractor to the branch
				if (closest != nullptr) 
				{
					closest->Attractors.push_back(attractor);
					m_ActiveAttractors.push_back(attractorIndex);
				}
				attractorIndex++;
			}


			// if at least an attraction point has been found, we want our tree to grow towards it
			if (!m_ActiveAttractors.empty()) 
			{
				// because new extremities will be set here, we clear the current ones
				m_Extremities.clear();

				// new branches will be added here
				std::vector<SCBranch*> newBranches;
				for (SCBranch* branch : m_Branches) 
				{
					// if the branch has attraction points, we grow towards them
					if (!branch->Attractors.empty())
					{
						// we compute the direction of the new branch
						glm::vec3 dir(0, 0, 0);
						for (const glm::vec3& attr : branch->Attractors) 
						{
							dir += glm::normalize(attr - branch->End);
						}
						dir /= branch->Attractors.size();
						// random growth
						dir += randomGrowVector(m_GrowSize);
						dir = glm::normalize(dir);

						// our new branch grows in the correct direction
						SCBranch* newBranch = new SCBranch();
						newBranch->Start = branch->End;
						newBranch->End = branch->End + dir * m_BranchLength;
						newBranch->Direction = dir;
						newBranch->Parent = branch;


						newBranch->DistanceFromRoot = branch->DistanceFromRoot + 1;
						branch->Children.push_back(newBranch);
						newBranches.push_back(newBranch);
						m_Extremities.push_back(newBranch);
					}
					else 
					{
						// if no attraction points, we only check if the branch is an extremity
						if (branch->Children.empty()) 
						{
							m_Extremities.push_back(branch);
						}
					}
				}

				// we merge the new branches with the previous ones
				for (auto newBranch : newBranches)
					m_Branches.push_back(newBranch);
			}
			else 
			{
				// we grow the extremities of the tree
				for (uint32_t i = 0; i < m_Extremities.size(); ++i) 
				{
					SCBranch* extremity = m_Extremities[i];
					// the new branch starts where the extremity ends
					glm::vec3 start = extremity->End;
					// we add randomness to the direction
					glm::vec3 dir = extremity->Direction + randomGrowVector(m_GrowSize);
					// we add the direction multiplied by the branch length to get the end point
					glm::vec3 end = extremity->End + dir * m_BranchLength;
					// a new branch can be created with the same direction as its parent
					SCBranch* newBranch = new SCBranch();
					newBranch->Start = start;
					newBranch->End = end;
					newBranch->Direction = dir;
					newBranch->Parent = extremity;

					// the current extrimity has a new child
					extremity->Children.push_back(newBranch);

					// let's add the new branch to the list and set it as the new extremity 
					m_Branches.push_back(newBranch);
					
					m_Extremities[i] = newBranch;
				}
			}
		}
	}
	void SpaceColonization::Grow(std::vector<uint8_t>& voxels, uint32_t width, uint32_t height, uint32_t depth, float voxelSize, int32_t radius)
	{
		if (m_Finished)
			return;

		Grow();

		const uint8_t Grass = 3;
		const uint32_t Wood = 4;

		for (const SCBranch * branch : m_Branches)
		{
			Ray ray(branch->Start, branch->Direction);
			Raymarch raymarch(ray, width, height, depth, voxelSize);
			
			glm::ivec3 currentVoxel = raymarch.GetCurrentVoxel();
			const glm::vec3 startVoxelPosition = glm::vec3(currentVoxel) * voxelSize;
			glm::vec3 currentVoxelPosition = glm::vec3(currentVoxel) * voxelSize;
			while (glm::distance(currentVoxelPosition, startVoxelPosition) < m_BranchLength)
			{
				for (int32_t x = -radius; x < radius; x++)
				{
					for (int32_t z = -radius; z < radius; z++)
					{
						// Check if inside radius
						if ((x * x) + (z * z) <= radius * radius)
						{
							glm::ivec3 voxel = currentVoxel + glm::ivec3(x, 0, z);
							const uint32_t index = Index3D(voxel, width, height);
							if (index < voxels.size())
								voxels[index] = Wood;
						}
					}
				}
	
				raymarch.Step();
				currentVoxel = raymarch.GetCurrentVoxel();
				currentVoxelPosition = glm::vec3(currentVoxel) * voxelSize;
			}
		}
		if (m_Attractors.empty())
		{
			m_Finished = true;
			for (const SCBranch* branch : m_Branches)
			{
				if (branch->Children.empty())
				{
					glm::ivec3 voxel = branch->End / voxelSize;
					glm::ivec3 elipsoid(RandomNumber(6u, 15u), RandomNumber(3u, 15u), RandomNumber(6u, 15u));
					int32_t max = std::max(elipsoid.x, std::max(elipsoid.y, elipsoid.z));
					for (int32_t x = -max * 2; x < max * 2; x++)
					{
						for (int32_t y = -max * 2; y < max * 2; y++)
						{
							for (int32_t z = -max * 2; z < max * 2; z++)
							{
								float elX = std::pow((static_cast<float>(x) / elipsoid.x), 2);
								float elY = std::pow((static_cast<float>(y) / elipsoid.y), 2);
								float elZ = std::pow((static_cast<float>(z) / elipsoid.z), 2);

								if (elX + elY + elZ <= 1.0f)
								{
									glm::ivec3 sphereVoxel = voxel + glm::ivec3(x, y, z);
									const uint32_t index = Index3D(sphereVoxel, width, height);
									if (index < voxels.size())
										voxels[index] = Grass;
								}
							}
						}
					}
				}
			}
		}
	}

	void SpaceColonization::VoxelizeAttractors(std::vector<uint8_t>& voxels, uint32_t width, uint32_t height, uint32_t depth, float voxelSize)
	{
		for (const auto& attractor : m_Attractors)
		{
			glm::ivec3 voxel(attractor / voxelSize);
			const uint32_t index = Index3D(voxel, width, height);
			if (index < voxels.size())
				voxels[index] = 10;
		}
	}
	glm::vec3 SpaceColonization::randomGrowVector(float growSize)
	{
		float alpha = RandomNumber(0.0f, glm::pi<float>());
		float theta = RandomNumber(0.0f, glm::pi<float>() * 2.0f);

		glm::vec3 point(
			cos(theta) * sin(alpha),
			sin(theta) * sin(alpha),
			cos(alpha)
		);

		return point * growSize;
	}

}
