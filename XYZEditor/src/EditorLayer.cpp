#include "EditorLayer.h"

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

#include <stb_image/stb_image.h>

namespace XYZ {

	static glm::vec2 MouseToWorld(const glm::vec2& point, const glm::vec2& windowSize)
	{
		glm::vec2 offset = { windowSize.x / 2,windowSize.y / 2 };
		return { point.x - offset.x, offset.y - point.y };
	}
	static bool Collide(const glm::vec2& pos, const glm::vec2& size, const glm::vec2& point)
	{
		return (pos.x + size.x > point.x &&
			pos.x		   < point.x&&
			pos.y + size.y >  point.y &&
			pos.y < point.y);
	}
	static bool HasExtension(const std::string& path, const char* extension)
	{
		auto lastDot = path.rfind('.');
		auto count = path.size() - lastDot;

		std::string_view view(path.c_str() + lastDot + 1, count);

		if (!view.compare(0, view.size() - 1, extension))
			return true;
		return false;
	}
	
	
	EditorLayer::EditorLayer()
	{
	}

	EditorLayer::~EditorLayer()
	{
	}

	void EditorLayer::OnAttach()
	{
		int width, height, channels;
		stbi_set_flip_vertically_on_load(1);
		uint8_t* pixels = (uint8_t*)stbi_load("Assets/Textures/Gui/Prohibited.png", &width, &height, &channels, 0);
		m_ProhibitedCursor = Application::Get().GetWindow().CreateCustomCursor(pixels, width, height, width / 2.0f, height / 2.0f);
		

		Renderer::Init();
		NativeScriptEngine::Init();

		NativeScriptEngine::SetOnReloadCallback([this] {
			//auto storage = m_Scene->GetECS().GetComponentStorage<NativeScriptComponent>();
			//for (int i = 0; i < storage->Size(); ++i)
			//{
			//	(*storage)[i].ScriptableEntity = (ScriptableEntity*)NativeScriptEngine::CreateScriptObject((*storage)[i].ScriptObjectName);
			//	if ((*storage)[i].ScriptableEntity)
			//	{
			//		(*storage)[i].ScriptableEntity->Entity = m_StoredEntitiesWithScript[i];
			//		(*storage)[i].ScriptableEntity->OnCreate();
			//	}
			//}
		});

		NativeScriptEngine::SetOnRecompileCallback([this]() {		
			//auto storage = m_Scene->GetECS().GetComponentStorage<NativeScriptComponent>();
			//for (int i = 0; i < storage->Size(); ++i)
			//{
			//	if ((*storage)[i].ScriptableEntity)
			//	{
			//		m_StoredEntitiesWithScript.push_back((*storage)[i].ScriptableEntity->Entity);
			//	}
			//}
		});
		

		uint32_t windowWidth = Application::Get().GetWindow().GetWidth();
		uint32_t windowHeight = Application::Get().GetWindow().GetHeight();
		m_Scene = m_AssetManager.GetAsset<Scene>("Assets/Scenes/scene.xyz");
		m_Scene->SetViewportSize(windowWidth, windowHeight);
		SceneManager::Get().SetActive(m_Scene);

		m_Material = m_AssetManager.GetAsset<Material>("Assets/Materials/material.mat");
		m_Material->SetFlags(XYZ::RenderFlags::TransparentFlag);

		m_TestEntity = m_Scene->GetEntity(2);
		m_SpriteRenderer = &m_TestEntity.GetComponent<SpriteRenderer>();
		m_Transform = &m_TestEntity.GetComponent<TransformComponent>();

		m_CharacterTexture = Texture2D::Create(XYZ::TextureWrap::Clamp, TextureParam::Nearest, TextureParam::Nearest, "Assets/Textures/player_sprite.png");
		m_CharacterSubTexture = Ref<SubTexture2D>::Create(m_CharacterTexture, glm::vec2(0, 0), glm::vec2(m_CharacterTexture->GetWidth() / 8, m_CharacterTexture->GetHeight() / 3));
		m_CharacterSubTexture2 = Ref<SubTexture2D>::Create(m_CharacterTexture, glm::vec2(1, 2), glm::vec2(m_CharacterTexture->GetWidth() / 8, m_CharacterTexture->GetHeight() / 3));
		m_CharacterSubTexture3 = Ref<SubTexture2D>::Create(m_CharacterTexture, glm::vec2(2, 2), glm::vec2(m_CharacterTexture->GetWidth() / 8, m_CharacterTexture->GetHeight() / 3));	

		auto backgroundTexture = m_AssetManager.GetAsset<Texture2D>("Assets/Textures/Backgroundfield.png");

		///////////////////////////////////////////////////////////

		uint32_t count = 5000;
		auto computeMat = Ref<Material>::Create(Shader::Create("Assets/Shaders/Particle/ParticleComputeShader.glsl"));
		auto renderMat = Ref<Material>::Create(Shader::Create("Assets/Shaders/Particle/ParticleShader.glsl"));
		
		renderMat->Set("u_Texture", Texture2D::Create(XYZ::TextureWrap::Clamp, TextureParam::Nearest, TextureParam::Nearest, "Assets/Textures/flame.png"), 0);
		renderMat->Set("u_Texture", Texture2D::Create(XYZ::TextureWrap::Clamp, TextureParam::Nearest, TextureParam::Nearest, "Assets/Textures/flame_emission.png"), 1);
	
		computeMat->SetRoutine("blueColor");	
	
		m_Particle = &m_TestEntity.AddComponent<ParticleComponent>(ParticleComponent());
		m_Particle->RenderMaterial = Ref<MaterialInstance>::Create(renderMat);
		m_Particle->ComputeMaterial = Ref<MaterialInstance>::Create(computeMat);
		m_Particle->ParticleEffect = Ref<ParticleEffect>::Create(ParticleEffectConfiguration(count, 2.0f), ParticleLayoutConfiguration());
		
		m_Particle->ComputeMaterial->Set("u_Speed", 0.2f);
		m_Particle->ComputeMaterial->Set("u_Gravity", 0.0f);
		m_Particle->ComputeMaterial->Set("u_Loop", (int)1);
		m_Particle->ComputeMaterial->Set("u_NumberRows", 8.0f);
		m_Particle->ComputeMaterial->Set("u_NumberColumns", 8.0f);
		m_Particle->ComputeMaterial->Set("u_ParticlesInExistence", 0.0f);
		m_Particle->ComputeMaterial->Set("u_Time", 0.0f);

		
		std::random_device rd;
		std::mt19937 rng(rd());
		std::uniform_real_distribution<> dist(-1, 1);

		m_Vertices = new XYZ::ParticleVertex[count];
		m_Data = new XYZ::ParticleInformation[count];

		for (int i = 0; i < count; ++i)
		{
			m_Vertices[i].Position = glm::vec4(0.008f * i, 0.0f, 0.0f, 0.0f);
			m_Vertices[i].Color = glm::vec4(0.3, 0.5, 1.0, 1);
			m_Vertices[i].Rotation = 0.0f;
			m_Vertices[i].TexCoordOffset = glm::vec2(0);

			m_Data[i].DefaultPosition.x = m_Vertices[i].Position.x;
			m_Data[i].DefaultPosition.y = m_Vertices[i].Position.y;
			m_Data[i].ColorBegin = m_Vertices[i].Color;
			m_Data[i].ColorEnd = m_Vertices[i].Color;
			m_Data[i].SizeBegin = 1.0f;
			m_Data[i].SizeEnd = 0.1f;
			m_Data[i].Rotation = dist(rng) * 15;
			m_Data[i].Velocity = glm::vec2(0.0f, fabs(dist(rng)));
			m_Data[i].DefaultVelocity = m_Data[i].Velocity;
			m_Data[i].LifeTime = fabs(dist(rng)) + 4;
		}
		m_Particle->ParticleEffect->SetParticlesRange(m_Vertices, m_Data, 0, count);
		///////////////////////////////////////////////////////////

		Ref<Font> font = Ref<Font>::Create(14, "Assets/Fonts/arial.ttf");
		auto texture = Texture2D::Create(XYZ::TextureWrap::Clamp, TextureParam::Linear, TextureParam::Nearest, "Assets/Textures/Gui/TexturePack_Dark.png");
		auto material = Ref<Material>::Create(Shader::Create("Assets/Shaders/GuiShader.glsl"));
		material->Set("u_Texture", texture, 0);
		material->Set("u_Texture", font->GetTexture(), 1);

		float divisor = 8.0f;
		GuiSpecification specs;
		specs.Material = material;
		specs.Font = font;
		specs.SubTexture[GuiSpecification::BUTTON] = Ref<SubTexture2D>::Create(texture, glm::vec2(0,0), glm::vec2((float)texture->GetWidth() / divisor, (float)texture->GetHeight() / divisor));
		specs.SubTexture[GuiSpecification::CHECKBOX_CHECKED] = Ref<SubTexture2D>::Create(texture, glm::vec2(1, 1), glm::vec2((float)texture->GetWidth() / divisor, (float)texture->GetHeight() / divisor));
		specs.SubTexture[GuiSpecification::CHECKBOX_UNCHECKED] = Ref<SubTexture2D>::Create(texture, glm::vec2(0, 1), glm::vec2((float)texture->GetWidth() / divisor, (float)texture->GetHeight() / divisor));
		m_GuiContext = Application::Get().GetGuiLayer()->CreateContext(&m_ECS, specs);		
		

		uint32_t canvas = m_GuiContext->CreateCanvas(CanvasSpecification(
				CanvasRenderMode::ScreenSpace,
				glm::vec3(0.0f),
				glm::vec2(windowWidth, windowHeight),
				glm::vec4(0.0f)
		));
		for (int i = 0; i < 10; ++i)
		{
			uint32_t editorEntity = m_GuiContext->CreateButton(canvas,
				ButtonSpecification{
					"Button",
					glm::vec3(i * 50,i * 50, 0.0f),
					glm::vec2(50.0f,50.0f),
					glm::vec4(1.0f,1.0f,1.0f,1.0f),
					glm::vec4(0.4f, 1.0f, 0.8f, 1.0f),
					glm::vec4(1.0f, 0.5f, 0.8f, 1.0f)
			});

			m_ECS.GetComponent<Button>(editorEntity).RegisterCallback<ClickEvent>(Hook(&EditorLayer::onButtonClickTest, this));
			m_EditorEntities.push_back(editorEntity);
		}

		m_ECS.GetComponent<RectTransform>(m_EditorEntities[6]).Position = glm::vec3(100.0f, 0.0f, 0.0f);
		m_GuiContext->SetParent(m_EditorEntities[6], m_EditorEntities[7]);
		
		for (int i = 0; i < 10; ++i)
		{
			uint32_t editorEntity = m_GuiContext->CreateCheckbox(canvas,
				CheckboxSpecification{
					"Checkbox",
					glm::vec3(i * 70, -70.0f, 0.0f),
					glm::vec2(50.0f,50.0f),
					glm::vec4(1.0f,1.0f,1.0f,1.0f),
					glm::vec4(1.0f, 0.5f, 0.8f, 1.0f)
				});

			m_ECS.GetComponent<Checkbox>(editorEntity).RegisterCallback<CheckedEvent>(Hook(&EditorLayer::onCheckboxCheckedTest, this));
			m_EditorEntities.push_back(editorEntity);
		}
		for (int i = 0; i < 10; ++i)
		{
			uint32_t editorEntity = m_GuiContext->CreateText(canvas,
				TextSpecification{
					TextAlignment::Center,
					"Checkbox",
					glm::vec3(i * 70, -140.0f, 0.0f),
					glm::vec2(50.0f,50.0f),
					glm::vec4(1.0f, 0.5f, 0.8f, 1.0f)
				});

			m_EditorEntities.push_back(editorEntity);
		}

		auto& rectTransform = m_ECS.GetComponent<RectTransform>(m_EditorEntities.back());
		auto& canvasRenderer = m_ECS.GetComponent<CanvasRenderer>(m_EditorEntities.back());
		auto& text = m_ECS.GetComponent<Text>(m_EditorEntities.back());
		rectTransform.Size.x += 50;
		
		rectTransform.Execute<CanvasRendererRebuildEvent>(CanvasRendererRebuildEvent(
			&canvasRenderer, &rectTransform, TextCanvasRendererRebuild(&text)
		));
	}	



	void EditorLayer::OnDetach()
	{
		NativeScriptEngine::Shutdown();
		Renderer::Shutdown();
	}
	void EditorLayer::OnUpdate(Timestep ts)
	{
		NativeScriptEngine::Update(ts);
		m_EditorCamera.OnUpdate(ts);
		m_Scene->OnUpdate(ts);
		m_Scene->OnRenderEditor(m_EditorCamera);
	}
	void EditorLayer::OnEvent(Event& event)
	{			
		EventDispatcher dispatcher(event);
		dispatcher.Dispatch<MouseButtonPressEvent>(Hook(&EditorLayer::onMouseButtonPress, this));
		dispatcher.Dispatch<MouseButtonReleaseEvent>(Hook(&EditorLayer::onMouseButtonRelease, this));	
		m_EditorCamera.OnEvent(event);
	}

	
	bool EditorLayer::onMouseButtonPress(MouseButtonPressEvent& event)
	{	
		return false;
	}
	bool EditorLayer::onMouseButtonRelease(MouseButtonReleaseEvent& event)
	{
		return false;
	}
	bool EditorLayer::onButtonClickTest(ClickEvent& event)
	{
		std::cout << "Clicked" << std::endl;
		return true;
	}

	bool EditorLayer::onCheckboxCheckedTest(CheckedEvent& event)
	{
		std::cout << "Checked" << std::endl;
		return false;
	}

}