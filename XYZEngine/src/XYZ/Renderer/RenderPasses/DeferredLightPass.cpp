#include "stdafx.h"
#include "DeferredLightPass.h"
#include "XYZ/Utils/Math/Math.h"

#include "XYZ/Scene/Components.h"

namespace XYZ {

	void DeferredLightPass::Init(const DeferredLightPassConfiguration& config, const Ref<RenderCommandBuffer>& commandBuffer)
	{
		m_RenderPass = config.Pass;
		m_Shader = config.Shader;
		createPipeline();

		m_LightStorageBufferSet = StorageBufferSet::Create(Renderer::GetConfiguration().FramesInFlight);
		m_LightStorageBufferSet->Create(sc_MaxNumberOfLights * sizeof(PointLight), 0, 1);
		m_LightStorageBufferSet->Create(sc_MaxNumberOfLights * sizeof(SpotLight), 0, 2);
	}

	void DeferredLightPass::PreSubmit(Ref<Scene> scene)
	{
		XYZ_PROFILE_FUNC("DeferredLightPass::PreSubmit");
		m_PointLights.clear();
		m_SpotLights.clear();

		auto& registry = scene->GetRegistry();

		// Spot lights
		auto spotLight2DView = registry.view<TransformComponent, SpotLight2D>();
		for (auto entity : spotLight2DView)
		{
			// Render previous frame data
			auto& [transform, light] = spotLight2DView.get<TransformComponent, SpotLight2D>(entity);
			auto [trans, rot, scale] = transform.GetWorldComponents();
			
			SpotLight spotLight{
				glm::vec4(light.Color, 1.0f),
				glm::vec2(trans),
				light.Radius,
				light.Intensity,
				light.InnerAngle,
				light.OuterAngle
			};
			m_SpotLights.push_back(spotLight);
		}

		// Point Lights
		auto pointLight2DView = registry.view<TransformComponent, PointLight2D>();
		for (auto entity : pointLight2DView)
		{
			auto& [transform, light] = pointLight2DView.get<TransformComponent, PointLight2D>(entity);
			auto [trans, rot, scale] = transform.GetWorldComponents();
			
			PointLight pointLight{
				glm::vec4(light.Color, 1.0f),
				glm::vec2(trans),
				light.Radius,
				light.Intensity
			};
			m_PointLights.push_back(pointLight);
		}

		// Particle Lights
		auto particleView = registry.view<TransformComponent, ParticleRenderer, ParticleComponent>();
		for (auto entity : particleView)
		{
			auto& [transform, renderer, particleComponent] = particleView.get<TransformComponent, ParticleRenderer, ParticleComponent>(entity);
		
			auto& renderData = particleComponent.System->GetRenderData();
		
			for (const auto& lightData : renderData.LightData)
			{
				glm::mat4 lightTransform = glm::translate(transform.WorldTransform, lightData.Position);
				glm::vec3 worldLightPos = Math::TransformToTranslation(lightTransform);
		
				PointLight pointLight{
					glm::vec4(lightData.Color, 1.0f),
					glm::vec2(worldLightPos),
					lightData.Radius,
					lightData.Intensity
				};
				m_PointLights.push_back(pointLight);
			}
		}

		if (m_SpotLights.size() > sc_MaxNumberOfLights)
			m_SpotLights.resize(sc_MaxNumberOfLights);

		if (m_PointLights.size() > sc_MaxNumberOfLights)
			m_PointLights.resize(sc_MaxNumberOfLights);


		constexpr uint32_t countOffset = 16;

		ByteBuffer spotLightBuffer;
		spotLightBuffer.Allocate(countOffset + (m_SpotLights.size() * sizeof(SpotLight)));
		spotLightBuffer.Write(m_SpotLights.size(), 0);
		spotLightBuffer.Write(m_SpotLights.data(), m_SpotLights.size() * sizeof(SpotLight), countOffset);

		ByteBuffer pointLightBuffer;
		pointLightBuffer.Allocate(countOffset + (m_PointLights.size() * sizeof(PointLight)));
		pointLightBuffer.Write(m_PointLights.size(), 0);
		pointLightBuffer.Write(m_PointLights.data(), m_PointLights.size() * sizeof(PointLight), countOffset);

		Ref<StorageBufferSet> instance = m_LightStorageBufferSet;
		Renderer::Submit([
			instance,
				pointLightBuffer,
				spotLightBuffer
		]() mutable
			{
				const uint32_t frame = Renderer::GetCurrentFrame();
				instance->Get(1, 0, frame)->RT_Update(pointLightBuffer.Data, pointLightBuffer.Size);
				instance->Get(2, 0, frame)->RT_Update(spotLightBuffer.Data, spotLightBuffer.Size);

				// TODO: recycle
				pointLightBuffer.Destroy();
				spotLightBuffer.Destroy();
			});
	}

	void DeferredLightPass::Submit(
		const Ref<RenderCommandBuffer>& commandBuffer,
		const Ref<Image2D>& colorImage,
		const Ref<Image2D>& positionImage
	)
	{
		XYZ_PROFILE_FUNC("DeferredLightPass::Submit");
		Renderer::BeginRenderPass(commandBuffer, m_RenderPass, true);
	
		m_Material->SetImageArray("u_Texture", colorImage, 0);
		m_Material->SetImageArray("u_Texture", positionImage, 1);


		Renderer::BindPipeline(
			commandBuffer,
			m_Pipeline,
			m_CameraBufferSet,
			m_LightStorageBufferSet,
			m_Material
		);

		Renderer::SubmitFullscreenQuad(commandBuffer, m_Pipeline, m_MaterialInstance);
		Renderer::EndRenderPass(commandBuffer);
	}

	void DeferredLightPass::createPipeline()
	{
		PipelineSpecification specification;
		specification.Shader = m_Shader;
		specification.Layouts = m_Shader->GetLayouts();
		specification.RenderPass = m_RenderPass;
		specification.Topology = PrimitiveTopology::Triangles;
		specification.DepthWrite = false;
		m_Pipeline = Pipeline::Create(specification);
		m_Material = Material::Create(m_Shader);
		m_MaterialInstance = Ref<XYZ::MaterialInstance>::Create(m_Material);
	}
}
