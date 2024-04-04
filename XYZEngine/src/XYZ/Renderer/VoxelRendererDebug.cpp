#include "stdafx.h"
#include "VoxelRendererDebug.h"

#include "Renderer.h"
#include "XYZ/Asset/Renderer/VoxelMeshSource.h"
#include "XYZ/Utils/Math/Math.h"

namespace XYZ {
	namespace Utils {

		static const float EPSILON = 0.01f;

		static int CalculateNumberOfSteps(const Ray& ray, float tMin, float tMax, float voxelSize)
		{
			glm::vec3 rayStart = ray.Origin + ray.Direction * (tMin - EPSILON);
			glm::vec3 rayEnd = ray.Origin + ray.Direction * (tMax - EPSILON);
			glm::ivec3 startVoxel = glm::ivec3(floor(rayStart / voxelSize));
			glm::ivec3 endVoxel = glm::ivec3(floor(rayEnd / voxelSize));

			glm::ivec3 diff = abs(endVoxel - startVoxel);
			return diff.x + diff.y + diff.z;
		}
		static uint32_t Index3D(uint32_t x, uint32_t y, uint32_t z, uint32_t width, uint32_t height)
		{
			return x + width * (y + height * z);
		}

		static uint32_t Index3D(const glm::ivec3& index, uint32_t width, uint32_t height)
		{
			return Index3D(index.x, index.y, index.z, width, height);
		}

		static bool IsValidVoxel(const glm::ivec3& voxel, uint32_t width, uint32_t height, uint32_t depth)
		{
			return ((voxel.x < width && voxel.x >= 0)
				&& (voxel.y < height && voxel.y >= 0)
				&& (voxel.z < depth && voxel.z >= 0));
		}

		static AABB ModelAABB(const VoxelSubmesh& submesh)
		{
			AABB result;
			result.Min = glm::vec3(0.0);
			result.Max = glm::vec3(submesh.Width, submesh.Height, submesh.Depth) * submesh.VoxelSize;
			return result;
		}

		static AABB VoxelAABB(glm::ivec3 voxel, float voxelSize)
		{
			AABB result;
			result.Min = glm::vec3(voxel.x * voxelSize, voxel.y * voxelSize, voxel.z * voxelSize);
			result.Max = result.Min + voxelSize;
			return result;
		}

		static float VoxelDistanceFromRay(const glm::vec3& origin, const glm::vec3& direction, const glm::ivec3& voxel, float voxelSize)
		{
			AABB aabb = VoxelAABB(voxel, voxelSize);
			if (Math::PointInBox(origin, aabb.Min, aabb.Max))
				return 0.0;

			float dist;
			Ray ray(origin, direction);
			if (ray.IntersectsAABB(aabb, dist))
				return dist;

			return FLT_MAX;
		}

		static const glm::vec4 VoxelToColor(VoxelColor voxel)
		{
			glm::vec4 color;
			color.x = voxel.R / 255.0f;
			color.y = voxel.G / 255.0f;
			color.z = voxel.B / 255.0f;
			color.w = voxel.A / 255.0f;

			return color;
		}

		static glm::vec3 VoxelPosition(const glm::ivec3& voxel, float voxelSize)
		{
			return glm::vec3(voxel.x * voxelSize, voxel.y * voxelSize, voxel.z * voxelSize);
		}

		static glm::vec4 BlendColors(const glm::vec4& colorA, const glm::vec4& colorB)
		{
			return colorA + colorB * colorB.a * (1.0f - colorA.a);
		}

		static RaymarchState CreateRaymarchState(const Ray& ray, float tMin, const glm::ivec3& step, const glm::ivec3& maxSteps, float voxelSize, const glm::ivec3& decompressedVoxelOffset)
		{
			RaymarchState state;
			const glm::vec3 rayStart = ray.Origin + ray.Direction * tMin;
			const glm::ivec3 decompressedCurrentVoxel = glm::ivec3(floor(rayStart / voxelSize));

			state.Distance = tMin;
			state.CurrentVoxel = decompressedCurrentVoxel - decompressedVoxelOffset;
			state.MaxSteps = maxSteps;
			state.DecompressedVoxelOffset = decompressedVoxelOffset;
			glm::vec3 next_boundary = glm::vec3(
				float((step.x > 0) ? decompressedCurrentVoxel.x + 1 : decompressedCurrentVoxel.x) * voxelSize,
				float((step.y > 0) ? decompressedCurrentVoxel.y + 1 : decompressedCurrentVoxel.y) * voxelSize,
				float((step.z > 0) ? decompressedCurrentVoxel.z + 1 : decompressedCurrentVoxel.z) * voxelSize
			);

			state.Max = tMin + (next_boundary - rayStart) / ray.Direction; // we will move along the axis with the smallest value
			return state;
		}

		static void PerformStep(RaymarchState& state, const glm::ivec3& step, const glm::vec3& delta)
		{
			if (state.Max.x < state.Max.y && state.Max.x < state.Max.z)
			{
				state.Distance = state.Max.x;
				state.Max.x += delta.x;
				state.CurrentVoxel.x += step.x;
				state.MaxSteps.x--;
			}
			else if (state.Max.y < state.Max.z)
			{
				state.Distance = state.Max.y;
				state.Max.y += delta.y;
				state.CurrentVoxel.y += step.y;
				state.MaxSteps.y--;
			}
			else
			{
				state.Distance = state.Max.z;
				state.Max.z += delta.z;
				state.CurrentVoxel.z += step.z;
				state.MaxSteps.z--;
			}
		}

		static float GetNextDistance(const RaymarchState& state, const glm::ivec3& step, const glm::vec3& delta)
		{
			if (state.Max.x < state.Max.y && state.Max.x < state.Max.z)
			{
				return state.Max.x;
			}
			else if (state.Max.y < state.Max.z)
			{
				return state.Max.y;

			}
			else
			{
				return state.Max.z;
			}
		}

		static Ray CreateRay(
			const glm::vec3& origin,
			const glm::mat4& inverseModelSpace,
			const glm::mat4& inverseProjection,
			const glm::mat4& inverseView,
			glm::vec2 coords,
			glm::vec2 viewportSize
		)
		{
			Ray ray = Ray::Zero();
			coords.x /= viewportSize.x;
			coords.y /= viewportSize.y;
			coords = coords * 2.0f - 1.0f; // -1 -> 1

			glm::vec4 target = inverseProjection * glm::vec4(coords.x, -coords.y, 1, 1);

			ray.Origin = (inverseModelSpace * glm::vec4(origin, 1.0));
			glm::vec3 targetXYZ = { target.x, target.y, target.z };
			ray.Direction = glm::vec3(inverseModelSpace * inverseView * glm::vec4(glm::normalize(targetXYZ / target.w), 0)); // World space
			ray.Direction = glm::normalize(ray.Direction);
			return ray;
		}



		static bool ResolveRayModelIntersection(glm::vec3& origin, const glm::vec3& direction, float& dist, const VoxelSubmesh& submesh)
		{
			AABB aabb = ModelAABB(submesh);
			bool result = Math::PointInBox(origin, aabb.Min, aabb.Max);
			dist = 0.0;
			if (!result)
			{
				// Check if we are intersecting with grid
				Ray ray(origin, direction);
				result = ray.IntersectsAABB(aabb, dist);
				origin = origin + direction * (dist - EPSILON); // Move origin to first intersection
			}
			return result;
		}


	}


	VoxelRendererDebug::VoxelRendererDebug(
		Ref<PrimaryRenderCommandBuffer> commandBuffer,
		Ref<UniformBufferSet> uniformBufferSet
	)
		:
		m_CommandBuffer(commandBuffer),
		m_UniformBufferSet(uniformBufferSet),
		c_BoundingBoxColor(1.0f, 1.0f, 0.0f, 1.0f)
	{
		m_ViewportSize = { 1280, 720 };
		m_Renderer = Ref<Renderer2D>::Create(Renderer2DConfiguration{ commandBuffer, uniformBufferSet });
		createRenderPass();
		createPipeline();
	}

	void VoxelRendererDebug::BeginScene(const glm::mat4& inverseViewMatrix, const glm::mat4& inverseProjection, const glm::vec3& cameraPosition)
	{
		if (UpdateCamera)
		{
			m_InverseViewMatrix = inverseViewMatrix;
			m_InverseProjection = inverseProjection;
			m_CameraPosition = cameraPosition;
		}
		m_DebugLines.clear();
		updateViewportsize();
	}
	void VoxelRendererDebug::EndScene(Ref<Image2D> image)
	{
		m_ColorMaterial->SetImage("u_ColorTexture", image);
		render();
	}
	void VoxelRendererDebug::RaymarchSubmesh(uint32_t submeshIndex, const AABB& boundingBox, const glm::mat4& transform, const Ref<VoxelMesh>& mesh, const glm::vec2& coords)
	{
		m_Transform = transform;
		const auto& submesh = mesh->GetSubmeshes()[submeshIndex];
		Ray ray = Utils::CreateRay(m_CameraPosition, glm::inverse(transform), m_InverseProjection, m_InverseViewMatrix, coords, m_ViewportSize);
		AABB modelAABB = Utils::ModelAABB(submesh);

		submitAABB(modelAABB.Min, modelAABB.Max, c_BoundingBoxColor);

		Ray resolvedRay = ray;
		float dist = 0.0f;
		if (Utils::ResolveRayModelIntersection(resolvedRay.Origin, resolvedRay.Direction, dist, submesh))
		{
			submitRay(ray, dist, glm::vec4(1.0f));
			float distanceTraveled = 0.0f;
			if (submesh.CompressedCells.size() != 0)
				distanceTraveled = raymarchCompressed(ray, glm::vec4(0, 0, 0, 0), dist, submesh, FLT_MAX, mesh->GetColorPallete());
			else
				distanceTraveled = rayMarchSteps(ray, glm::vec4(0, 0, 0, 0), dist, submesh.Width, submesh.Height, submesh.Depth, 0, submesh, FLT_MAX, { submesh.Width, submesh.Height, submesh.Depth }, { 0,0,0 }, mesh->GetColorPallete()).Distance;

			m_DebugLines.push_back({
				ray.Origin,
				ray.Origin + ray.Direction * distanceTraveled,
				glm::vec4(1.0f, 0.0f, 1.0f, 1.0f)
				});
			//submitRay(resolvedRay, 3.0f, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
		}
	}
	void VoxelRendererDebug::SetViewportSize(uint32_t width, uint32_t height)
	{
		if (m_ViewportSize.x != width || m_ViewportSize.y != height)
		{
			m_ViewportSize = glm::ivec2(width, height);
			m_ViewportSizeChanged = true;
		}
	}
	void VoxelRendererDebug::render()
	{
		Renderer::BeginRenderPass(
			m_CommandBuffer,
			m_ColorRenderPass,
			false,
			true
		);

		renderColor();
		renderLines();

		Renderer::EndRenderPass(m_CommandBuffer);
	}
	void VoxelRendererDebug::renderColor()
	{
		Renderer::BindPipeline(
			m_CommandBuffer,
			m_ColorPipeline,
			m_UniformBufferSet,
			nullptr,
			m_ColorMaterial
		);

		Renderer::SubmitFullscreenQuad(m_CommandBuffer, m_ColorPipeline, m_ColorMaterialInstance);
	}
	void VoxelRendererDebug::renderLines()
	{
		Renderer::BindPipeline(
			m_CommandBuffer,
			m_LinePipeline,
			m_UniformBufferSet,
			nullptr,
			m_LineMaterial
		);

		m_Renderer->BeginScene(glm::inverse(m_InverseViewMatrix));

		int count = std::min(Renderer2D::GetMaxLines() - 100, static_cast<uint32_t>(m_DebugLines.size()));
		for (int i = 0; i < count; i++)
		{
			const auto& line = m_DebugLines[i];
			m_Renderer->SubmitLine(line.P0, line.P1, line.Color);
		}

		m_Renderer->FlushLines(m_LinePipeline, m_LineMaterialInstance, false, m_Transform);
		m_Renderer->EndScene();
	}
	void VoxelRendererDebug::createRenderPass()
	{
		FramebufferSpecification framebufferSpec;
		framebufferSpec.Attachments = {
				FramebufferTextureSpecification(ImageFormat::RGBA32F)
		};
		framebufferSpec.Samples = 1;
		framebufferSpec.ClearOnLoad = false;
		framebufferSpec.ClearColor = { 0.0f, 0.0f, 0.0f, 0.0f };

		Ref<Framebuffer> framebuffer = Framebuffer::Create(framebufferSpec);

		RenderPassSpecification renderPassSpec;
		renderPassSpec.TargetFramebuffer = framebuffer;
		m_ColorRenderPass = RenderPass::Create(renderPassSpec);
	}
	void VoxelRendererDebug::createPipeline()
	{
		{ // Line pipeline
			Ref<Shader> shader = Shader::Create("Resources/Shaders/Voxel/VoxelDebugLineShader.glsl");
			m_LineMaterial = Material::Create(shader);
			m_LineMaterialInstance = m_LineMaterial->CreateMaterialInstance();

			PipelineSpecification spec;
			spec.RenderPass = m_ColorRenderPass;
			spec.Shader = m_LineMaterial->GetShader();
			spec.Topology = PrimitiveTopology::Lines;
			spec.DepthTest = true;
			spec.DepthWrite = true;
			m_LinePipeline = Pipeline::Create(spec);
		}
		{
			// Color pipeline
			Ref<Shader> shader = Shader::Create("Resources/Shaders/Voxel/VoxelDebugColorShader.glsl");
			m_ColorMaterial = Material::Create(shader);
			m_ColorMaterialInstance = m_ColorMaterial->CreateMaterialInstance();

			PipelineSpecification spec;
			spec.RenderPass = m_ColorRenderPass;
			spec.Shader = m_ColorMaterial->GetShader();
			spec.Topology = PrimitiveTopology::Triangles;
			spec.DepthTest = true;
			spec.DepthWrite = true;
			m_ColorPipeline = Pipeline::Create(spec);
		}
	}
	void VoxelRendererDebug::updateViewportsize()
	{
		if (m_ViewportSizeChanged)
		{
			const uint32_t width = (uint32_t)m_ViewportSize.x;
			const uint32_t height = (uint32_t)m_ViewportSize.y;

			m_ColorRenderPass->GetSpecification().TargetFramebuffer->Resize(width, height);
			m_ViewportSizeChanged = false;
		}
	}
	void VoxelRendererDebug::submitAABB(const glm::vec3& min, const glm::vec3& max, const glm::vec4& color)
	{
		glm::vec3 topFrontLeft = { min.x, max.y, min.z };
		glm::vec3 topFrontRight = { max.x, max.y, min.z };
		glm::vec3 bottomFrontLeft = { min.x, min.y, min.z };
		glm::vec3 bottomFrontRight = { max.x, min.y, min.z };


		glm::vec3 topBackLeft = { min.x, max.y, max.z };
		glm::vec3 topBackRight = { max.x, max.y, max.z };
		glm::vec3 bottomBackLeft = { min.x, min.y, max.z };
		glm::vec3 bottomBackRight = { max.x, min.y, max.z };


		// Front
		m_DebugLines.push_back({ topFrontLeft, topFrontRight, color });
		m_DebugLines.push_back({ topFrontRight, bottomFrontRight, color });
		m_DebugLines.push_back({ bottomFrontRight, bottomFrontLeft, color });
		m_DebugLines.push_back({ bottomFrontLeft, topFrontLeft, color });


		//Back
		m_DebugLines.push_back({ topBackLeft, topBackRight, color });
		m_DebugLines.push_back({ topBackRight, bottomBackRight, color });
		m_DebugLines.push_back({ bottomBackRight, bottomBackLeft, color });
		m_DebugLines.push_back({ bottomBackLeft, topBackLeft, color });

		// Sides
		m_DebugLines.push_back({ topFrontLeft, topBackLeft, color });
		m_DebugLines.push_back({ topFrontRight, topBackRight, color });
		m_DebugLines.push_back({ bottomFrontLeft, bottomBackLeft, color });
		m_DebugLines.push_back({ bottomFrontRight, bottomBackRight, color });
	}
	void VoxelRendererDebug::submitRay(const Ray& ray, float size, const glm::vec4& color)
	{
		m_DebugLines.push_back({ ray.Origin, ray.Origin + ray.Direction * size, color });
	}
	void VoxelRendererDebug::submitCompressedVoxels(const VoxelSubmesh& submesh, const std::array<VoxelColor, 256>& collorPallete)
	{
		for (uint32_t cx = 0; cx < submesh.Width; ++cx)
		{
			for (uint32_t cy = 0; cy < submesh.Height; ++cy)
			{
				for (uint32_t cz = 0; cz < submesh.Depth; ++cz)
				{
					const uint32_t cIndex = Utils::Index3D(cx, cy, cz, submesh.Width, submesh.Height);
					const VoxelSubmesh::CompressedCell& cell = submesh.CompressedCells[cIndex];
					if (cell.VoxelCount == 1)
					{
						AABB cellAABB = Utils::VoxelAABB({ cx, cy, cz }, submesh.VoxelSize);
						uint8_t colorIndex = submesh.ColorIndices[cell.VoxelOffset];
						if (colorIndex == 0)
							continue;

						auto& color = collorPallete[colorIndex];
						submitAABB(cellAABB.Min, cellAABB.Max, Utils::VoxelToColor(color));
					}
				}
			}
		}
	}
	void VoxelRendererDebug::submitVoxelCell(const VoxelSubmesh& submesh, const std::array<VoxelColor, 256>& collorPallete, const glm::ivec3& cellCoord)
	{
		const uint32_t cIndex = Utils::Index3D(cellCoord.x, cellCoord.y, cellCoord.z, submesh.Width, submesh.Height);
		const VoxelSubmesh::CompressedCell& cell = submesh.CompressedCells[cIndex];
		if (cell.VoxelCount == 1)
		{
			AABB cellAABB = Utils::VoxelAABB(cellCoord, submesh.VoxelSize);
			uint8_t colorIndex = submesh.ColorIndices[cell.VoxelOffset];
			if (colorIndex == 0)
				return;

			auto& color = collorPallete[colorIndex];
			submitAABB(cellAABB.Min, cellAABB.Max, Utils::VoxelToColor(color));
		}
		else
		{
			const uint32_t xStart = cellCoord.x * submesh.CompressScale;
			const uint32_t yStart = cellCoord.y * submesh.CompressScale;
			const uint32_t zStart = cellCoord.z * submesh.CompressScale;

			const uint32_t xEnd = std::min(xStart + submesh.CompressScale, submesh.Width * submesh.CompressScale);
			const uint32_t yEnd = std::min(yStart + submesh.CompressScale, submesh.Height * submesh.CompressScale);
			const uint32_t zEnd = std::min(zStart + submesh.CompressScale, submesh.Depth * submesh.CompressScale);

			for (uint32_t x = xStart; x < xEnd; ++x)
			{
				for (uint32_t y = yStart; y < yEnd; ++y)
				{
					for (uint32_t z = zStart; z < zEnd; ++z)
					{
						AABB voxelAABB = Utils::VoxelAABB({ x, y, z }, submesh.VoxelSize / submesh.CompressScale);
						const uint32_t index = Utils::Index3D(x - xStart, y - yStart, z - zStart, submesh.CompressScale, submesh.CompressScale) + cell.VoxelOffset;
						const uint8_t colorIndex = submesh.ColorIndices[index];
						if (colorIndex == 0)
							continue;

						auto& color = collorPallete[colorIndex];

						submitAABB(voxelAABB.Min, voxelAABB.Max, Utils::VoxelToColor(color));
					}
				}
			}
		}
	}


	void VoxelRendererDebug::rayMarch(const Ray& ray, Utils::RaymarchState& state, const glm::vec3& delta, const glm::ivec3& step, uint32_t width, uint32_t height, uint32_t depth, uint32_t voxelOffset, const VoxelSubmesh& model, float currentDistance, const std::array<VoxelColor, 256>& colorPallete)
	{
		state.Hit = false;
		state.Color = glm::vec4(0, 0, 0, 0);

		float voxelSize = model.VoxelSize / model.CompressScale;

		while (state.MaxSteps.x >= 0 && state.MaxSteps.y >= 0 && state.MaxSteps.z >= 0)
		{
			if (Utils::IsValidVoxel(state.CurrentVoxel, width, height, depth))
			{
				state.Distance = Utils::VoxelDistanceFromRay(ray.Origin, ray.Direction, state.CurrentVoxel + state.DecompressedVoxelOffset, voxelSize);
				if (state.Distance > currentDistance)
					break;

				uint32_t voxelIndex = Utils::Index3D(state.CurrentVoxel, width, height) + voxelOffset;
				uint8_t colorIndex = model.ColorIndices[voxelIndex];
				VoxelColor voxel = colorPallete[colorIndex];
				if (colorIndex != 0)
				{
					state.Color = Utils::VoxelToColor(voxel);
					state.Hit = true;
					break;
				}
			}
			PerformStep(state, step, delta);
		}
	}

	Utils::RaymarchResult VoxelRendererDebug::rayMarchSteps(const Ray& ray, const glm::vec4& startColor, float tMin, uint32_t width, uint32_t height, uint32_t depth, uint32_t voxelOffset, const VoxelSubmesh& model, float currentDistance, const glm::ivec3& maxSteps, const glm::ivec3& decompressedVoxelOffset, const std::array<VoxelColor, 256>& colorPallete)
	{
		Utils::RaymarchResult result;
		result.Color = startColor;
		result.Hit = false;
		result.Distance = 0.0;

		glm::ivec3 step = glm::ivec3(
			(ray.Direction.x > 0.0) ? 1 : -1,
			(ray.Direction.y > 0.0) ? 1 : -1,
			(ray.Direction.z > 0.0) ? 1 : -1
		);

		float voxelSize = model.VoxelSize / model.CompressScale;
		glm::vec3 delta = voxelSize / ray.Direction * glm::vec3(step);

		Utils::RaymarchState state = Utils::CreateRaymarchState(ray, tMin, step, maxSteps, voxelSize, decompressedVoxelOffset);

		while (state.MaxSteps.x >= 0 && state.MaxSteps.y >= 0 && state.MaxSteps.z >= 0)
		{
			// if new depth is bigger than currentDepth it means there is something in front of us
			if (state.Distance > currentDistance)
				break;

			rayMarch(ray, state, delta, step, width, height, depth, voxelOffset, model, currentDistance, colorPallete);
			if (state.Hit)
			{
				if (result.Hit == false)
				{
					result.Distance = state.Distance;
					result.Hit = true;
				}
				result.Color = Utils::BlendColors(result.Color, state.Color);
				if (result.Color.a >= 1.0)
					break;
			}
			PerformStep(state, step, delta); // Hit was not opaque we continue raymarching, perform step to get out of transparent voxel	
		}
		return result;
	}


	float VoxelRendererDebug::raymarchCompressed(const Ray& ray, const glm::vec4& startColor, float tMin, const VoxelSubmesh& model, float currentDistance, const std::array<VoxelColor, 256>& colorPallete)
	{
		Utils::RaymarchResult result;
		result.Hit = false;
		result.Color = startColor;
		result.Distance = 0.0;

		glm::ivec3 step = glm::ivec3(
			(ray.Direction.x > 0.0) ? 1 : -1,
			(ray.Direction.y > 0.0) ? 1 : -1,
			(ray.Direction.z > 0.0) ? 1 : -1
		);
		glm::ivec3 maxSteps = glm::ivec3(model.Width, model.Height, model.Depth);
		glm::vec3 t_delta = model.VoxelSize / ray.Direction * glm::vec3(step);
		Utils::RaymarchState state = Utils::CreateRaymarchState(ray, tMin, step, maxSteps, model.VoxelSize, glm::ivec3(0, 0, 0));

		while (state.MaxSteps.x >= 0 && state.MaxSteps.y >= 0 && state.MaxSteps.z >= 0)
		{
			if (Utils::IsValidVoxel(state.CurrentVoxel, model.Width, model.Height, model.Depth))
			{
				if (state.Distance > currentDistance)
					break;

				uint32_t cellIndex = Utils::Index3D(state.CurrentVoxel, model.Width, model.Height);
				const auto& cell = model.CompressedCells[cellIndex];
				if (cell.VoxelCount == 1) // Compressed cell
				{
					uint32_t voxelIndex = cell.VoxelOffset;
					uint32_t colorIndex = model.ColorIndices[voxelIndex];
					if (colorIndex != 0)
					{
						if (result.Hit == false)
						{
							result.Distance = state.Distance;
							result.Hit = true;
						}

						float tMin = state.Distance;
						float tMax = Utils::GetNextDistance(state, step, t_delta);

						int numSteps = Utils::CalculateNumberOfSteps(ray, tMin, tMax, model.VoxelSize / model.CompressScale);

						// TODO: calculate number of steps it would take to traverse this cell and blend color multiple times
						VoxelColor colorUINT = colorPallete[colorIndex];
						result.Color = Utils::BlendColors(result.Color, Utils::VoxelToColor(colorUINT));
						submitVoxelCell(model, colorPallete, state.CurrentVoxel);
						if (result.Color.a >= 1.0)
							break;
					}
				}
				else
				{
					// Calculates real coordinates of CurrentVoxel in decompressed model
					glm::ivec3 decompressedVoxelOffset = state.CurrentVoxel * int(model.CompressScale); // Required for proper distance calculation
					glm::vec3 newOrigin = ray.Origin + ray.Direction * (state.Distance - Utils::EPSILON); // Move origin to hit of top grid cell
					glm::vec3 voxelPosition = Utils::VoxelPosition(state.CurrentVoxel, model.VoxelSize);
					glm::vec3 cellOrigin = newOrigin - voxelPosition;


					uint32_t voxelOffset = cell.VoxelOffset;
					uint32_t width = model.CompressScale;
					uint32_t height = model.CompressScale;
					uint32_t depth = model.CompressScale;

					AABB cellAABB = Utils::VoxelAABB(state.CurrentVoxel, model.VoxelSize);
					float tMinCell = 0.0;
					ray.IntersectsAABB(cellAABB, tMinCell);

					//// Raymarch from new origin						
					Utils::RaymarchResult newResult = rayMarchSteps(ray, result.Color, tMinCell, width, height, depth, voxelOffset, model, currentDistance, { width, height, depth }, decompressedVoxelOffset, colorPallete);
					if (newResult.Hit)
					{
						submitVoxelCell(model, colorPallete, state.CurrentVoxel);
						if (result.Hit == false)
						{
							result.Distance = newResult.Distance;
							result.Hit = true;
						}
						result.Color = newResult.Color;
						if (result.Color.a >= 1.0)
							break;
					}
				}
			}
			Utils::PerformStep(state, step, t_delta);
		}
		return result.Distance;
	}
}
