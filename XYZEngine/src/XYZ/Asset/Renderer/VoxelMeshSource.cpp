#include "stdafx.h"
#include "VoxelMeshSource.h"

#include "XYZ/Utils/Random.h"

#include <ogt_vox.h>

namespace XYZ {


	static AABB VoxelModelToAABB(const glm::mat4& transform, uint32_t width, uint32_t height, uint32_t depth, float voxelSize)
	{
		AABB result;
		result.Min = glm::vec3(0.0f);
		result.Max = glm::vec3(width, height, depth) * voxelSize;

		result = result.TransformAABB(transform);
		return result;
	}
	static glm::mat4 VoxMat4ToGLM(const ogt_vox_transform& voxTransform)
	{
		glm::mat4 result;
		memcpy(&result, &voxTransform, sizeof(glm::mat4));
		return result;
	}
	static uint32_t Index3D(uint32_t x, uint32_t y, uint32_t z, uint32_t width, uint32_t height)
	{
		return x + width * (y + height * z);
	}

	static bool IsBlockUniform(const std::vector<uint8_t>& arr, const glm::uvec3& start, const glm::uvec3& end, uint32_t width, uint32_t height)
	{
		const uint32_t oldIndex = Index3D(start.x, start.y, start.z, width, height);
		const uint8_t oldColorIndex = arr[oldIndex];
		for (uint32_t x = start.x; x < end.x; ++x)
		{
			for (uint32_t y = start.y; y < end.y; ++y)
			{
				for (uint32_t z = start.z; z < end.z; ++z)
				{
					const uint32_t newIndex = Index3D(x, y, z, width, height);
					const uint8_t newColorIndex = arr[newIndex];
					if (newColorIndex != oldColorIndex)
						return false;
				}
			}
		}
		return true;
	}

	static void LoadSubmeshModel(VoxelSubmesh& submesh, const ogt_vox_model* voxModel, const std::array<VoxelColor, 256>& colorPallete)
	{
		submesh.ColorIndices.resize(voxModel->size_x * voxModel->size_y * voxModel->size_z);
		submesh.Width = voxModel->size_x;
		submesh.Height = voxModel->size_y;
		submesh.Depth = voxModel->size_z;
		submesh.IsOpaque = true;

		for (uint32_t x = 0; x < voxModel->size_x; ++x)
		{
			for (uint32_t y = 0; y < voxModel->size_y; ++y)
			{
				for (uint32_t z = 0; z < voxModel->size_z; ++z)
				{
					const uint32_t index = Index3D(x, y, z, voxModel->size_x, voxModel->size_y);
					submesh.ColorIndices[index] = voxModel->voxel_data[index];
					const uint8_t colorIndex = submesh.ColorIndices[index];
					if (colorPallete[colorIndex].A < 255)
						submesh.IsOpaque = false;
				}
			}
		}
	}

	static void LoadVoxelAnimation(VoxelTransformAnimation& anim, const ogt_vox_instance& voxInstance)
	{
		anim.Loop = voxInstance.transform_anim.loop;
		anim.Transforms.resize(voxInstance.transform_anim.num_keyframes);
		for (uint32_t i = 0; i < voxInstance.transform_anim.num_keyframes; ++i)
		{
			const uint32_t frameIndex = voxInstance.transform_anim.keyframes[i].frame_index;
			anim.Transforms[frameIndex] = VoxMat4ToGLM(voxInstance.transform_anim.keyframes[i].transform);
		}
	}

	static void LoadVoxelModelAnimation(VoxelModelAnimation& anim, const ogt_vox_instance& voxInstance)
	{
		anim.Loop = voxInstance.model_anim.loop;
		anim.SubmeshIndices.resize(voxInstance.model_anim.num_keyframes);
		for (uint32_t i = 0; i < voxInstance.model_anim.num_keyframes; ++i)
		{
			const uint32_t frameIndex = voxInstance.model_anim.keyframes[i].frame_index;
			anim.SubmeshIndices[frameIndex] = voxInstance.model_anim.keyframes[i].model_index;
		}
	}


	VoxelMeshSource::VoxelMeshSource(const std::string& filepath)
		:
		m_Filepath(filepath),
		m_NumVoxels(0)
	{
		std::ifstream output(m_Filepath, std::ios::binary);
		std::vector<uint8_t> data(std::istreambuf_iterator<char>(output), {});
		auto scene = ogt_vox_read_scene_with_flags(data.data(), data.size(), k_read_scene_flags_keyframes);

		memcpy(m_ColorPallete.data(), scene->palette.color, m_ColorPallete.size() * sizeof(VoxelColor));
		for (uint32_t i = 0; i < scene->num_models; ++i)
		{
			VoxelSubmesh& submesh = m_Submeshes.emplace_back();
			LoadSubmeshModel(submesh, scene->models[i], m_ColorPallete);

			m_NumVoxels += static_cast<uint32_t>(submesh.ColorIndices.size());
		}

		for (uint32_t i = 0; i < scene->num_instances; ++i)
		{
			VoxelInstance& instance = m_Instances.emplace_back();
			instance.SubmeshIndex = scene->instances[i].model_index;
			const auto& submesh = m_Submeshes[instance.SubmeshIndex];
			
			glm::vec3 centerTranslation = -glm::vec3(
				static_cast<float>(submesh.Width) / 2.0f * submesh.VoxelSize,
				static_cast<float>(submesh.Height) / 2.0f * submesh.VoxelSize,
				static_cast<float>(submesh.Depth) / 2.0f * submesh.VoxelSize
			);
			
			instance.Transform = VoxMat4ToGLM(scene->instances[i].transform) * glm::translate(glm::mat4(1.0f), centerTranslation);
			AABB::Union(m_AABB, VoxelModelToAABB(instance.Transform, submesh.Width, submesh.Height, submesh.Depth, submesh.VoxelSize));

			LoadVoxelAnimation(instance.TransformAnimation, scene->instances[i]);
			LoadVoxelModelAnimation(instance.ModelAnimation, scene->instances[i]);
		}
		ogt_vox_destroy_scene(scene);
	}
	int64_t VoxelSubmesh::Compress(uint32_t scale)
	{
		VoxelSubmesh copy = std::move(*this);
		CompressScale = scale;
		Width = Math::RoundUp(Width, scale) / scale;
		Height = Math::RoundUp(Height, scale) / scale;
		Depth = Math::RoundUp(Depth, scale) / scale;
		VoxelSize = VoxelSize * scale;
		Compressed = true;

		CompressedCells.resize(Width * Height * Depth);
		std::vector<uint8_t> compressedColorIndices;

		uint32_t voxelOffset = 0;
		for (uint32_t cx = 0; cx < Width; ++cx)
		{
			for (uint32_t cy = 0; cy < Height; ++cy)
			{
				for (uint32_t cz = 0; cz < Depth; ++cz)
				{
					const uint32_t cIndex = Index3D(cx, cy, cz, Width, Height);
					CompressedCell& cell = CompressedCells[cIndex];

					const uint32_t xStart = cx * scale;
					const uint32_t yStart = cy * scale;
					const uint32_t zStart = cz * scale;
				
					const uint32_t xEnd = std::min(xStart + scale, copy.Width);
					const uint32_t yEnd = std::min(yStart + scale, copy.Height);
					const uint32_t zEnd = std::min(zStart + scale, copy.Depth);

					const bool isUniform = IsBlockUniform(copy.ColorIndices, { xStart, yStart, zStart }, { xEnd, yEnd, zEnd }, copy.Width, copy.Height);
					if (isUniform)
					{
						cell.VoxelCount = 1;
						compressedColorIndices.push_back(copy.ColorIndices[Index3D(xStart, yStart, zStart, copy.Width, copy.Height)]);
					}
					else
					{
						const uint32_t offset = static_cast<uint32_t>(compressedColorIndices.size());
						cell.VoxelCount = scale * scale * scale;
						compressedColorIndices.resize(offset + cell.VoxelCount);
						uint8_t* cellColorIndices = &compressedColorIndices[offset];
						for (uint32_t x = xStart; x < xEnd; ++x)
						{
							for (uint32_t y = yStart; y < yEnd; ++y)
							{
								for (uint32_t z = zStart; z < zEnd; ++z)
								{
									const uint32_t index = Index3D(x, y, z, copy.Width, copy.Height);
									const uint32_t insertIndex = Index3D(x - xStart, y - yStart, z - zStart, scale, scale);
									const uint8_t colorIndex = copy.ColorIndices[index];
									cellColorIndices[insertIndex] = colorIndex;
								}
							}
						}
					}	
					cell.VoxelOffset = voxelOffset;
					voxelOffset += cell.VoxelCount;
				}
			}
		}

		const int64_t savedSpace = copy.ColorIndices.size() - compressedColorIndices.size();
		ColorIndices = std::move(compressedColorIndices);
		return savedSpace;
	}

	int64_t VoxelSubmesh::Compress(uint32_t scale, bool& cancel)
	{
		VoxelSubmesh copy = std::move(*this);
		CompressScale = scale;
		Width = Math::RoundUp(Width, scale) / scale;
		Height = Math::RoundUp(Height, scale) / scale;
		Depth = Math::RoundUp(Depth, scale) / scale;
		VoxelSize = VoxelSize * scale;
		Compressed = true;

		CompressedCells.resize(Width * Height * Depth);
		std::vector<uint8_t> compressedColorIndices;

		uint32_t voxelOffset = 0;
		for (uint32_t cx = 0; cx < Width; ++cx)
		{
			for (uint32_t cy = 0; cy < Height; ++cy)
			{
				for (uint32_t cz = 0; cz < Depth; ++cz)
				{
					if (cancel)
						return 0;

					const uint32_t cIndex = Index3D(cx, cy, cz, Width, Height);
					CompressedCell& cell = CompressedCells[cIndex];

					const uint32_t xStart = cx * scale;
					const uint32_t yStart = cy * scale;
					const uint32_t zStart = cz * scale;

					const uint32_t xEnd = std::min(xStart + scale, copy.Width);
					const uint32_t yEnd = std::min(yStart + scale, copy.Height);
					const uint32_t zEnd = std::min(zStart + scale, copy.Depth);

					const bool isUniform = IsBlockUniform(copy.ColorIndices, { xStart, yStart, zStart }, { xEnd, yEnd, zEnd }, copy.Width, copy.Height);
					if (isUniform)
					{
						cell.VoxelCount = 1;
						compressedColorIndices.push_back(copy.ColorIndices[Index3D(xStart, yStart, zStart, copy.Width, copy.Height)]);
					}
					else
					{
						const uint32_t offset = static_cast<uint32_t>(compressedColorIndices.size());
						cell.VoxelCount = scale * scale * scale;
						compressedColorIndices.resize(offset + cell.VoxelCount);
						uint8_t* cellColorIndices = &compressedColorIndices[offset];
						for (uint32_t x = xStart; x < xEnd; ++x)
						{
							for (uint32_t y = yStart; y < yEnd; ++y)
							{
								for (uint32_t z = zStart; z < zEnd; ++z)
								{
									if (cancel)
										return 0;

									const uint32_t index = Index3D(x, y, z, copy.Width, copy.Height);
									const uint32_t insertIndex = Index3D(x - xStart, y - yStart, z - zStart, scale, scale);
									const uint8_t colorIndex = copy.ColorIndices[index];
									cellColorIndices[insertIndex] = colorIndex;
								}
							}
						}
					}
					cell.VoxelOffset = voxelOffset;
					voxelOffset += cell.VoxelCount;
				}
			}
		}

		const int64_t savedSpace = copy.ColorIndices.size() - compressedColorIndices.size();
		ColorIndices = std::move(compressedColorIndices);
		return savedSpace;
	}

	bool VoxelSubmesh::DecompressCell(uint32_t cx, uint32_t cy, uint32_t cz)
	{
		const uint32_t cIndex = Index3D(cx, cy, cz, Width, Height);
		XYZ_ASSERT(cIndex < CompressedCells.size(), "");
		CompressedCell& cell = CompressedCells[cIndex];
		if (cell.VoxelCount != 1)
			return false;

		CompressedCell origCell = cell;
		m_FreeSpace.push_back(cell);

		cell.VoxelCount = CompressScale * CompressScale * CompressScale;
		cell.VoxelOffset = static_cast<uint32_t>(ColorIndices.size());
		ColorIndices.resize(cell.VoxelOffset + cell.VoxelCount);

		uint8_t* cellColorIndices = &ColorIndices[cell.VoxelOffset];
		for (uint32_t x = 0; x < CompressScale; ++x)
		{
			for (uint32_t y = 0; y < CompressScale; ++y)
			{
				for (uint32_t z = 0; z < CompressScale; ++z)
				{
					const uint32_t index = Index3D(x, y, z,CompressScale, CompressScale);
					cellColorIndices[index] = ColorIndices[origCell.VoxelOffset];
				}
			}
		}
		return true;
	}


	VoxelSubmesh VoxelSubmesh::Compress(uint32_t scale, uint32_t width, uint32_t height, uint32_t depth, float voxelSize, const std::vector<uint8_t>& colorIndices)
	{
		VoxelSubmesh result;
		result.CompressScale = scale;
		result.Width = Math::RoundUp(width, scale) / scale;
		result.Height = Math::RoundUp(height, scale) / scale;
		result.Depth = Math::RoundUp(depth, scale) / scale;
		result.VoxelSize = voxelSize * scale;
		result.Compressed = true;
		result.CompressedCells.resize(result.Width * result.Height * result.Depth);

		uint32_t voxelOffset = 0;
		for (uint32_t cx = 0; cx < result.Width; ++cx)
		{
			for (uint32_t cy = 0; cy < result.Height; ++cy)
			{
				for (uint32_t cz = 0; cz < result.Depth; ++cz)
				{
					const uint32_t cIndex = Index3D(cx, cy, cz, result.Width, result.Height);
					CompressedCell& cell = result.CompressedCells[cIndex];

					const uint32_t xStart = cx * scale;
					const uint32_t yStart = cy * scale;
					const uint32_t zStart = cz * scale;

					const uint32_t xEnd = std::min(xStart + scale, width);
					const uint32_t yEnd = std::min(yStart + scale, height);
					const uint32_t zEnd = std::min(zStart + scale, depth);

					const bool isUniform = IsBlockUniform(colorIndices, { xStart, yStart, zStart }, { xEnd, yEnd, zEnd }, width, height);
					if (isUniform)
					{
						cell.VoxelCount = 1;
						result.ColorIndices.push_back(colorIndices[Index3D(xStart, yStart, zStart, width, height)]);
					}
					else
					{
						const uint32_t offset = static_cast<uint32_t>(result.ColorIndices.size());
						cell.VoxelCount = scale * scale * scale;
						result.ColorIndices.resize(offset + cell.VoxelCount);
						uint8_t* cellColorIndices = &result.ColorIndices[offset];
						for (uint32_t x = xStart; x < xEnd; ++x)
						{
							for (uint32_t y = yStart; y < yEnd; ++y)
							{
								for (uint32_t z = zStart; z < zEnd; ++z)
								{
									const uint32_t index = Index3D(x, y, z, width, height);
									const uint32_t insertIndex = Index3D(x - xStart, y - yStart, z - zStart, scale, scale);
									const uint8_t colorIndex = colorIndices[index];
									cellColorIndices[insertIndex] = colorIndex;
								}
							}
						}
					}
					cell.VoxelOffset = voxelOffset;
					voxelOffset += cell.VoxelCount;
				}
			}
		}
		return result;
	}
}