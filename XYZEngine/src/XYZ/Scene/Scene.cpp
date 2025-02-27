#include "stdafx.h"
#include "Scene.h"

#include "XYZ/Core/Input.h"
#include "XYZ/Core/Application.h"

#include "XYZ/Renderer/Renderer.h"
#include "XYZ/Renderer/Renderer2D.h"
#include "XYZ/Renderer/SceneRenderer.h"
#include "XYZ/Renderer/Renderer2D.h"
#include "XYZ/Renderer/MeshFactory.h"

#include "XYZ/Asset/AssetManager.h"

#include "XYZ/Script/ScriptEngine.h"
#include "XYZ/Debug/Profiler.h"
#include "XYZ/Utils/Math/Math.h"
#include "XYZ/Utils/Random.h"

#include "Prefab.h"
#include "SceneEntity.h"
#include "Components.h"

#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <glm/common.hpp>
#include <glm/gtc/random.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>


namespace XYZ {

	namespace Utils {
		static ozz::math::Float4x4 Float4x4FromMat4(const glm::mat4& mat)
		{
			ozz::math::Float4x4 result;
			result.cols[0] = ozz::math::simd_float4::LoadPtrU(glm::value_ptr(mat[0]));
			result.cols[1] = ozz::math::simd_float4::LoadPtrU(glm::value_ptr(mat[1]));
			result.cols[2] = ozz::math::simd_float4::LoadPtrU(glm::value_ptr(mat[2]));
			result.cols[3] = ozz::math::simd_float4::LoadPtrU(glm::value_ptr(mat[3]));
			return result;
		}

		template<typename T>
		static void Clone(const entt::registry& src, entt::registry& dst)
		{
			auto view = src.view<const T>();
			for (auto entity : view)
			{
				auto& component = dst.emplace<T>(entity);
				component = view.get<const T>(entity);
			}
		}

		template <typename ...Args>
		static void CloneAll(const entt::registry& src, entt::registry& dst)
		{
			(Clone<Args>(src, dst), ...);
		}

		template<typename T>
		static void CopyComponentIfExists(const entt::registry& src, entt::registry& dst, entt::entity srcEntity, entt::entity dstEntity)
		{
			if (src.any_of<const T>(srcEntity))
			{
				const auto& srcComponent = src.get<const T>(srcEntity);

				dst.emplace_or_replace<T>(dstEntity, srcComponent);
			}
		}

		template <typename ...Args>
		static void CopyComponentsIfExist(const entt::registry& src, entt::registry& dst, entt::entity srcEntity, entt::entity dstEntity)
		{
			(CopyComponentIfExists<Args>(src, dst, srcEntity, dstEntity), ...);
		}

		// TODO: do not use storages but something like ReplicationQueue<T> ?
		template <typename T>
		static std::future<bool> ReplicationCallback(entt::storage<T>& storage)
		{
			ThreadPool& pool = Application::Get().GetThreadPool();
			return pool.SubmitJob([]() {

				return true;
				});
		}

		template <typename T>
		static std::future<bool> Replicate(entt::registry& registry)
		{
			auto& storage = registry.storage<T>();
			return ReplicationCallback(storage);
		}

		template <typename ...Args>
		static void ReplicateAllAndWait(entt::registry& registry)
		{
			std::array<std::future<bool>, sizeof...(Args)> futures;
			futures[index++] = { Replicate<Args>(registry)... };
			for (auto& future : futures)
				future.wait();
		}


		static void CloneRegistry(const entt::registry& src, entt::registry& dst, bool clearDestination = true)
		{
			if (clearDestination)
				dst = entt::registry();

			dst.assign(src.data(), src.data() + src.size(), src.released()); // Copy entities

			CloneAll<XYZ_COMPONENTS>(src, dst);
		}

		static void CopyRegistry(const entt::registry& src, entt::registry& dst, bool clearDestination = false)
		{
			if (clearDestination)
				dst = entt::registry();

			dst.each([&src, &dst](entt::entity srcEntity) {
				entt::entity dstEntity = dst.create();
				CopyComponentsIfExist<XYZ_COMPONENTS>(src, dst, srcEntity, dstEntity);
				});
		}
	}
	
	static entt::registry s_CopyRegistry;


	static std::vector<ParticleEmitterGPU> s_ParticleEmitters;

	Scene::Scene(const std::string& name, const GUID& guid)
		:
		m_PhysicsWorld({ 0.0f, -9.8f }),
		m_PhysicsEntityBuffer(nullptr),
		m_Name(name),
		m_State(SceneState::Edit),
		m_ViewportWidth(0),
		m_ViewportHeight(0),
		m_CameraEntity(entt::null),
		m_SelectedEntity(entt::null)

	{
		m_SceneEntity = m_Registry.create();
		m_Registry.emplace<Relationship>(m_SceneEntity);
		m_Registry.emplace<IDComponent>(m_SceneEntity, guid);
		m_Registry.emplace<TransformComponent>(m_SceneEntity);
		m_Registry.emplace<SceneTagComponent>(m_SceneEntity, name);	

		b2World& physicsWorld = m_PhysicsWorld.GetWorld();
		physicsWorld.SetContactListener(&m_ContactListener);

		m_Registry.on_construct<ScriptComponent>().connect<&Scene::onScriptComponentConstruct>(this);
		m_Registry.on_destroy<ScriptComponent>().connect<&Scene::onScriptComponentDestruct>(this);
	
		

		// Temporary

		ParticleSystemLayout inputLayout("Input", {
			{"StartPosition", VariableType{"vec4", 16}},
			{"StartColor",	  VariableType{"vec4", 16}},
			{"StartRotation", VariableType{"vec4", 16}},
			{"StartScale",	  VariableType{"vec4", 16}},
			{"StartVelocity", VariableType{"vec4", 16}},
			{"EndColor",	  VariableType{"vec4", 16}},
			{"EndRotation",   VariableType{"vec4", 16}},
			{"EndScale",	  VariableType{"vec4", 16}},
			{"EndVelocity",   VariableType{"vec4", 16}},
			{"VelocityCurve", VariableType{"vec2[4]", 32}},
			{"Position",	  VariableType{"vec4", 16}},
			{"LifeTime",	  VariableType{"float", 4}},
			{"LifeRemaining", VariableType{"float", 4}}
			});

		ParticleEmitterGPU emitter(inputLayout.GetStride());
		emitter.EmissionRate = 10.0f;

		emitter.EmitterModules.push_back(Ref<BoxParticleEmitterModuleGPU>::Create(inputLayout.GetStride(), std::vector<uint32_t>{ inputLayout.GetVariableOffset("StartPosition") }));
		emitter.EmitterModules.push_back(Ref<SpawnParticleEmitterModuleGPU>::Create(inputLayout.GetStride(), std::vector<uint32_t>{ inputLayout.GetVariableOffset("LifeTime") }));
		emitter.EmitterModules.push_back(Ref<TestParticleEmitterModuleGPU>::Create(inputLayout.GetStride(), std::vector<uint32_t>{ inputLayout.GetVariableOffset("StartColor") }));
		s_ParticleEmitters.push_back(emitter);
	}


	Scene::~Scene()
	{
		m_Registry.on_construct<ScriptComponent>().disconnect<&Scene::onScriptComponentConstruct>(this);
		m_Registry.on_destroy<ScriptComponent>().disconnect<&Scene::onScriptComponentDestruct>(this);
	}

	SceneEntity Scene::CreateEntity(const std::string& name, const GUID& guid)
	{
		XYZ_PROFILE_FUNC("Scene::CreateEntity");
		entt::entity id = m_Registry.create();
		SceneEntity entity(id, this);

		entity.EmplaceComponent<IDComponent>(guid);
		entity.EmplaceComponent<Relationship>();
		entity.EmplaceComponent<SceneTagComponent>(name);
		entity.EmplaceComponent<TransformComponent>(glm::vec3(0.0f, 0.0f, 0.0f));

		auto& sceneRelation = m_Registry.get<Relationship>(m_SceneEntity);
		Relationship::SetupRelation(m_SceneEntity, id, m_Registry);

		return entity;
	}

	SceneEntity Scene::CreateEntity(const std::string& name, SceneEntity parent, const GUID& guid)
	{
		XYZ_PROFILE_FUNC("Scene::CreateEntity Parent");
		XYZ_ASSERT(parent.m_Scene == this, "");
		const entt::entity id = m_Registry.create();
		SceneEntity entity(id, this);

		entity.EmplaceComponent<IDComponent>(guid);
		entity.EmplaceComponent<Relationship>();
		entity.EmplaceComponent<SceneTagComponent>(name);
		entity.EmplaceComponent<TransformComponent>(glm::vec3(0.0f, 0.0f, 0.0f));
		auto& sceneRelation = m_Registry.get<Relationship>(m_SceneEntity);
		Relationship::SetupRelation(parent.ID(), id, m_Registry);

		return entity;
	}

	void Scene::DestroyEntity(SceneEntity entity)
	{
		XYZ_PROFILE_FUNC("Scene::DestroyEntity");
		Relationship::RemoveRelation(entity.m_ID, m_Registry);
		if (entity.m_ID == m_SelectedEntity)
			m_SelectedEntity = entt::null;

		std::stack<entt::entity> entities;
		auto& parentRel = m_Registry.get<Relationship>(entity.m_ID);
		if (m_Registry.valid(parentRel.FirstChild))
			entities.push(parentRel.FirstChild);

		while (!entities.empty())
		{
			entt::entity tmpEntity = entities.top();
			entities.pop();
			if (tmpEntity == m_SelectedEntity)
				m_SelectedEntity = entt::null;

			const auto& rel = m_Registry.get<Relationship>(tmpEntity);
			if (m_Registry.valid(rel.FirstChild))
				entities.push(rel.FirstChild);
			if (m_Registry.valid(rel.NextSibling))
				entities.push(rel.NextSibling);

			m_Registry.destroy(tmpEntity);
		}
	    m_Registry.destroy(entity.m_ID);
	}

	void Scene::OnPlay()
	{
		// Find Camera	
		auto cameraView = m_Registry.view<CameraComponent>();
		if (!cameraView.empty())
		{
			m_CameraEntity = cameraView.front();
			m_Registry.get<CameraComponent>(m_CameraEntity).Camera.SetViewportSize(m_ViewportWidth, m_ViewportHeight);
		}
		else
		{
			XYZ_CORE_ERROR("No camera found in the scene");
			m_State = SceneState::Edit;
			return;
		}

		setupPhysics();

		// Copy stored values to runtime
		auto scriptView = m_Registry.view<ScriptComponent>();
		for (auto entity : scriptView)
		{
			ScriptComponent& script = scriptView.get<ScriptComponent>(entity);
			SceneEntity sceneEntity(entity, this);
			ScriptEngine::CopyPublicFieldsToRuntime(sceneEntity);
			ScriptEngine::OnCreateEntity(sceneEntity);
		}
	
		// Reset particles
		auto particleView = m_Registry.view<ParticleComponent>();
		for (auto entity : particleView)
		{
			particleView.get<ParticleComponent>(entity).GetSystem()->Reset();
		}
		Utils::CloneRegistry(m_Registry, s_CopyRegistry);
	}

	void Scene::OnStop()
	{
		Utils::CloneRegistry(s_CopyRegistry, m_Registry);
		s_CopyRegistry = entt::registry();
		{
			b2World& physicsWorld = m_PhysicsWorld.GetWorld();
			auto rigidBodyView = m_Registry.view<RigidBody2DComponent>();
			for (const auto entity : rigidBodyView)
			{
				auto& body = rigidBodyView.get<RigidBody2DComponent>(entity);
				physicsWorld.DestroyBody(static_cast<b2Body*>(body.RuntimeBody));
			}
		}

		auto scriptView = m_Registry.view<ScriptComponent>();
		for (auto entity : scriptView)
		{
			ScriptComponent& script = scriptView.get<ScriptComponent>(entity);
			SceneEntity sceneEntity(entity, this);
			ScriptEngine::OnDestroyEntity(sceneEntity);
		}

		auto particleView = m_Registry.view<ParticleComponent>();
		for (auto entity : particleView)
		{
			particleView.get<ParticleComponent>(entity).GetSystem()->Reset();
		}

		delete[]m_PhysicsEntityBuffer;
		m_PhysicsEntityBuffer = nullptr;

		m_SelectedEntity = entt::null;
	}

	void Scene::OnUpdate(Timestep ts)
	{
		XYZ_PROFILE_FUNC("Scene::OnUpdate");
		m_PhysicsWorld.Step(ts);

		updateRigidBody2DView();
		
		updateParticleView(ts);
		updateGPUParticleView(ts);

		if (m_UpdateAnimationAsync)
			updateAnimationViewAsync(ts);
		else
			updateAnimationView(ts);

		updateScripts(ts);
		updateHierarchy();
		m_GPUScene.OnUpdate(ts);
	}

	void Scene::OnRender(Ref<SceneRenderer> sceneRenderer)
	{
		XYZ_PROFILE_FUNC("Scene::OnRender");
		// 3D part here

		///////////////
		SceneEntity cameraEntity(m_CameraEntity, this);
		SceneRendererCamera renderCamera;
		auto& cameraComponent = cameraEntity.GetComponent<CameraComponent>();
		auto& cameraTransform = cameraEntity.GetComponent<TransformComponent>();
		renderCamera.Camera = cameraComponent.Camera;
		renderCamera.ViewMatrix = glm::inverse(cameraTransform->WorldTransform);
		auto [translation, rotation, scale] = cameraTransform.GetWorldComponents();
		renderCamera.ViewPosition = translation;

		setupLightEnvironment();
		sceneRenderer->GetOptions().ShowGrid = false;
		sceneRenderer->SetViewportSize(m_ViewportWidth, m_ViewportHeight);
		sceneRenderer->BeginScene(renderCamera);


		auto spriteView = m_Registry.view<TransformComponent, SpriteRenderer>();
		for (auto entity : spriteView)
		{
			auto& [transform, spriteRenderer] = spriteView.get<TransformComponent, SpriteRenderer>(entity);
			sceneRenderer->SubmitSprite(spriteRenderer.Material.Value(), spriteRenderer.SubTexture.Value(), spriteRenderer.Color, transform->WorldTransform);
		}
		auto meshView = m_Registry.view<TransformComponent, MeshComponent>();
		for (auto entity : meshView)
		{
			auto& [transform, meshComponent] = meshView.get<TransformComponent, MeshComponent>(entity);
			sceneRenderer->SubmitMesh(meshComponent.Mesh.Value(), meshComponent.MaterialAsset.Value(), transform->WorldTransform, meshComponent.OverrideMaterial);
		}

		auto animMeshView = m_Registry.view<TransformComponent, AnimatedMeshComponent>();
		for (auto entity : animMeshView)
		{
			auto& [transform, meshComponent] = animMeshView.get<TransformComponent, AnimatedMeshComponent>(entity);
			meshComponent.BoneTransforms.resize(meshComponent.BoneEntities.size());
			for (size_t i = 0; i < meshComponent.BoneEntities.size(); ++i)
			{
				const entt::entity boneEntity = meshComponent.BoneEntities[i];
				meshComponent.BoneTransforms[i] = Utils::Float4x4FromMat4(m_Registry.get<TransformComponent>(boneEntity)->WorldTransform);
			}
			Ref<MeshSource> meshSource = meshComponent.Mesh->GetMeshSource();
			sceneRenderer->SubmitMesh(meshComponent.Mesh.Value(), meshComponent.MaterialAsset.Value(), meshSource->GetSubmeshTransform(), meshComponent.BoneTransforms, meshComponent.OverrideMaterial);
		}

		auto particleView = m_Registry.view<TransformComponent, ParticleRenderer, ParticleComponent>();
		for (auto entity : particleView)
		{
			auto& [transform, renderer, particleComponent] = particleView.get<TransformComponent, ParticleRenderer, ParticleComponent>(entity);

			auto& renderData = particleComponent.GetSystem()->GetRenderData();
		
			sceneRenderer->SubmitMesh(
				renderer.Mesh.Value(), renderer.MaterialAsset.Value(),
				renderData.ParticleData.data(),
				renderData.ParticleCount,
				sizeof(ParticleRenderData),
				renderer.OverrideMaterial
			);
		}
		m_GPUScene.OnRender(this, sceneRenderer);
		sceneRenderer->EndScene();
	}

	void Scene::OnUpdateEditor(Timestep ts)
	{
		XYZ_PROFILE_FUNC("Scene::OnUpdateEditor");
		m_PhysicsWorld.Step(ts);

		if (m_UpdateHierarchyAsync)
			updateHierarchyAsync();
		else
			updateHierarchy();

		if (m_UpdateAnimationAsync)
			updateAnimationViewAsync(ts);
		else
			updateAnimationView(ts);		

		updateParticleView(ts);
		updateGPUParticleView(ts);

		m_GPUScene.OnUpdate(ts);
	}
	

	void Scene::OnRenderEditor(Ref<SceneRenderer> sceneRenderer, const glm::mat4& viewProjection, const glm::mat4& view, const glm::mat4& projection)
	{
		XYZ_PROFILE_FUNC("Scene::OnRenderEditor");
		
		
		setupLightEnvironment();
		sceneRenderer->BeginScene(viewProjection, view, projection);
	
		auto spriteView = m_Registry.view<TransformComponent, SpriteRenderer>();
		for (auto entity : spriteView)
		{
			auto& [transform, spriteRenderer] = spriteView.get<TransformComponent, SpriteRenderer>(entity);
			
			if (!spriteRenderer.Material.Valid() || !spriteRenderer.SubTexture.Valid())
				continue;
			sceneRenderer->SubmitSprite(spriteRenderer.Material.Value(), spriteRenderer.SubTexture.Value(), spriteRenderer.Color, transform->WorldTransform);
		}
		
		auto meshView = m_Registry.view<TransformComponent, MeshComponent>();
		for (auto entity : meshView)
		{
			auto& [transform, meshComponent] = meshView.get<TransformComponent, MeshComponent>(entity);
			if (!meshComponent.MaterialAsset.Valid() || !meshComponent.Mesh.Valid())
				continue;
		
			sceneRenderer->SubmitMesh(meshComponent.Mesh.Value(), meshComponent.MaterialAsset.Value(), transform->WorldTransform, meshComponent.OverrideMaterial);
		}
		
		
		auto animMeshView = m_Registry.view<TransformComponent,AnimatedMeshComponent>();
		for (auto entity : animMeshView)
		{
			auto& [transform, meshComponent] = animMeshView.get<TransformComponent,  AnimatedMeshComponent>(entity);
			if (!meshComponent.Mesh.Valid() || !meshComponent.MaterialAsset.Valid())
				continue;
			
			meshComponent.BoneTransforms.resize(meshComponent.BoneEntities.size());
			for (size_t i = 0; i < meshComponent.BoneEntities.size(); ++i)
			{
				const entt::entity boneEntity = meshComponent.BoneEntities[i];
				meshComponent.BoneTransforms[i] = Utils::Float4x4FromMat4(m_Registry.get<TransformComponent>(boneEntity)->WorldTransform);
			}
			Ref<MeshSource> meshSource = meshComponent.Mesh->GetMeshSource();
			sceneRenderer->SubmitMesh(meshComponent.Mesh.Value(), meshComponent.MaterialAsset.Value(), meshSource->GetSubmeshTransform(), meshComponent.BoneTransforms, meshComponent.OverrideMaterial);
		}

		{
			XYZ_PROFILE_FUNC("Scene::OnRenderEditor particleView");
			auto particleView = m_Registry.view<TransformComponent, ParticleRenderer, ParticleComponent>();
			for (auto entity : particleView)
			{
				auto& [transform, renderer, particleComponent] = particleView.get<TransformComponent, ParticleRenderer, ParticleComponent>(entity);
				
				if (!renderer.Mesh.Valid() || !renderer.MaterialAsset.Valid())
					continue;

				const auto& renderData = particleComponent.GetSystem()->GetRenderData();
				sceneRenderer->SubmitMesh(
					renderer.Mesh.Value(), renderer.MaterialAsset.Value(),
					renderData.ParticleData.data(),
					renderData.ParticleCount,
					sizeof(ParticleRenderData),
					renderer.OverrideMaterial
				);
			}
		}
		
		m_GPUScene.OnRender(this, sceneRenderer);
		sceneRenderer->EndScene();
	}


	void Scene::OnImGuiRender()
	{
		if (ImGui::Begin("Scene Settings"))
		{
			ImGui::Checkbox("Update Animation Async", &m_UpdateAnimationAsync);
			ImGui::Checkbox("Update Hierarchy Async", &m_UpdateHierarchyAsync);
		}
		ImGui::End();
	}

	void Scene::SetViewportSize(uint32_t width, uint32_t height)
	{
		m_ViewportWidth = width;
		m_ViewportHeight = height;
	}

	SceneEntity Scene::GetEntityByName(const std::string& name)
	{
		auto view = m_Registry.view<SceneTagComponent>();
		for (auto entity : view)
		{
			if (view.get<SceneTagComponent>(entity).Name == name)
				return SceneEntity(entity, this);
		}
		return SceneEntity();
	}

	SceneEntity Scene::GetEntityByGUID(const GUID& guid)
	{
		auto view = m_Registry.view<IDComponent>();
		for (auto entity : view)
		{
			if (view.get<IDComponent>(entity).ID == guid)
				return SceneEntity(entity, this);
		}
		return SceneEntity();
	}

	SceneEntity Scene::GetSceneEntity()
	{
		return SceneEntity(m_SceneEntity, this);
	}

	SceneEntity Scene::GetSelectedEntity()
	{
		return { m_SelectedEntity, this };
	}

	void Scene::onScriptComponentConstruct(entt::registry& reg, entt::entity ent)
	{
		std::unique_lock lock(m_ScriptMutex);
		ScriptEngine::CreateScriptEntityInstance({ ent, this });
	}

	void Scene::onScriptComponentDestruct(entt::registry& reg, entt::entity ent)
	{
		std::unique_lock lock(m_ScriptMutex);
		ScriptEngine::DestroyScriptEntityInstance({ ent, this });
	}



	void Scene::updateScripts(Timestep ts)
	{
		XYZ_PROFILE_FUNC("Scene::updateScripts");
		auto scriptView = m_Registry.view<ScriptComponent>();
		std::shared_lock lock(m_ScriptMutex);
		for (auto entity : scriptView)
		{
			ScriptComponent& scriptComponent = scriptView.get<ScriptComponent>(entity);
			SceneEntity sceneEntity(entity, this);
			if (!scriptComponent.ModuleName.empty())
				ScriptEngine::OnUpdateEntity(sceneEntity, ts);
		}
	}

	void Scene::updateHierarchy()
	{
		XYZ_PROFILE_FUNC("Scene::updateHierarchy");
		std::stack<entt::entity> entities;
		{
			const Relationship& relation = m_Registry.get<Relationship>(m_SceneEntity);
			if (m_Registry.valid(relation.FirstChild))
				entities.push(relation.FirstChild);
		}

		while (!entities.empty())
		{
			const entt::entity tmp = entities.top();
			entities.pop();
		
			const Relationship& relation = m_Registry.get<Relationship>(tmp);
			if (m_Registry.valid(relation.NextSibling))
				entities.push(relation.NextSibling);
			if (m_Registry.valid(relation.FirstChild))
				entities.push(relation.FirstChild);
			
			TransformComponent& transform = m_Registry.get<TransformComponent>(tmp);		
			// Every entity except m_SceneEntity must have parent
			TransformComponent& parentTransform = m_Registry.get<TransformComponent>(relation.Parent);
			
			if (parentTransform.m_Dirty || transform.m_Dirty)
				transform.GetTransform().WorldTransform = parentTransform->WorldTransform * transform.GetLocalTransform();
		}
		
		// We updated all transforms, they are no longer dirty
		auto& transformStorage = m_Registry.storage<TransformComponent>();
		for (auto& transformComponent : transformStorage)
			transformComponent.m_Dirty = false;
	}

	void Scene::updateHierarchyAsync()
	{
		XYZ_PROFILE_FUNC("Scene::updateHierarchyAsync");
		auto& threadPool = Application::Get().GetThreadPool();
		Ref<Scene> instance = this;

		const Relationship& relation = m_Registry.get<Relationship>(m_SceneEntity);
		TransformComponent& parentTransform = m_Registry.get<TransformComponent>(m_SceneEntity);

		entt::entity parent = m_SceneEntity;
		entt::entity child = relation.GetFirstChild();

		std::vector<std::future<bool>> futures;
		while (m_Registry.valid(child))
		{
			const Relationship& childRelation = m_Registry.get<Relationship>(child);

			TransformComponent& transform = m_Registry.get<TransformComponent>(child);
			if (parentTransform.m_Dirty || transform.m_Dirty)
				transform.GetTransform().WorldTransform = parentTransform->WorldTransform * transform.GetLocalTransform();

			futures.emplace_back(threadPool.SubmitJob([instance, child]() mutable {
				instance->updateSubHierarchy(child);
				return true;
			}));
			child = childRelation.GetNextSibling();
		}

		for (auto& future : futures)
			future.wait();

		// We updated all transforms, they are no longer dirty
		auto& transformStorage = m_Registry.storage<TransformComponent>();
		for (auto& transformComponent : transformStorage)
			transformComponent.m_Dirty = false;
	}

	void Scene::updateSubHierarchy(entt::entity parent)
	{
		XYZ_PROFILE_FUNC("Scene::updateSubHierarchy");

		const Relationship& relation = m_Registry.get<Relationship>(parent);
		TransformComponent& parentTransform = m_Registry.get<TransformComponent>(parent);

		std::stack<entt::entity> entities;
		{
			const Relationship& relation = m_Registry.get<Relationship>(parent);
			if (m_Registry.valid(relation.FirstChild))
				entities.push(relation.FirstChild);
		}

		while (!entities.empty())
		{
			const entt::entity current = entities.top();
			entities.pop();

			const Relationship& relation = m_Registry.get<Relationship>(current);
			if (m_Registry.valid(relation.NextSibling))
				entities.push(relation.NextSibling);
			if (m_Registry.valid(relation.FirstChild))
				entities.push(relation.FirstChild);

			TransformComponent& transform = m_Registry.get<TransformComponent>(current);
			TransformComponent& parentTransform = m_Registry.get<TransformComponent>(relation.Parent);

			if (parentTransform.m_Dirty || transform.m_Dirty)
				transform.GetTransform().WorldTransform = parentTransform->WorldTransform * transform.GetLocalTransform();
		}
	}

	void Scene::updateAnimationView(Timestep ts)
	{
		XYZ_PROFILE_FUNC("Scene::updateAnimationView");
		auto animView = m_Registry.view<AnimationComponent, AnimatedMeshComponent>();
		for (auto entity : animView)
		{
			auto [anim, animMesh] = animView.get(entity);
			anim.Playing = true; // TODO: temporary
			if (anim.Playing && anim.Controller.Valid())
			{
				anim.Controller->Update(anim.AnimationTime, anim.Context);
				anim.AnimationTime += ts;
				for (size_t i = 0; i < animMesh.BoneEntities.size(); ++i)
				{
					auto& transform = m_Registry.get<TransformComponent>(animMesh.BoneEntities[i]);
					transform.GetTransform().Translation = anim.Context.LocalTranslations[i];
					transform.GetTransform().Rotation = glm::eulerAngles(anim.Context.LocalRotations[i]);
					transform.GetTransform().Scale = anim.Context.LocalScales[i];
				}
			}
		}
	}
	
	void Scene::updateAnimationViewAsync(Timestep ts)
	{
		XYZ_PROFILE_FUNC("Scene::updateAnimationViewAsync");
		Ref<Scene> instance = this;
		auto& threadPool = Application::Get().GetThreadPool();
		auto animView = m_Registry.view<AnimationComponent, AnimatedMeshComponent>();
		
		std::vector<std::future<bool>> futures;
		futures.reserve(animView.size_hint());
		
		for (auto entity : animView)
		{
			auto [anim, animMesh] = animView.get(entity);
			anim.Playing = true; // TODO: temporary
			if (anim.Playing && anim.Controller.Valid())
			{
				futures.emplace_back(threadPool.SubmitJob([instance, ts, &animation = anim, &animatedMesh = animMesh]() mutable {

					animation.Controller->Update(animation.AnimationTime, animation.Context);
					animation.AnimationTime += ts;
					for (size_t i = 0; i < animatedMesh.BoneEntities.size(); ++i)
					{
						auto& transform = instance->m_Registry.get<TransformComponent>(animatedMesh.BoneEntities[i]);
						transform.GetTransform().Translation = animation.Context.LocalTranslations[i];
						transform.GetTransform().Rotation = glm::eulerAngles(animation.Context.LocalRotations[i]);
						transform.GetTransform().Scale = animation.Context.LocalScales[i];
					}
					return true;
				}));
			}
		}
		for (auto& future : futures)
			future.wait();
	}

	void Scene::updateParticleView(Timestep ts)
	{
		XYZ_PROFILE_FUNC("Scene::updateParticleView");
		auto particleView = m_Registry.view<ParticleComponent, TransformComponent>();
		for (auto entity : particleView)
		{
			auto& [particleComponent, transformComponent] = particleView.get<ParticleComponent, TransformComponent>(entity);
			particleComponent.GetSystem()->Update(transformComponent->WorldTransform, ts);
		}
	}

	void Scene::updateGPUParticleView(Timestep ts)
	{
		XYZ_PROFILE_FUNC("Scene::updateParticleView");
		auto& particleStorage = m_Registry.storage<ParticleComponentGPU>();
		for (auto particleComponent : particleStorage)
		{
			//if (particleComponent.System->GetEmittedParticles() == particleComponent.System->GetMaxParticles())
			//	particleComponent.System->Reset();

			uint32_t emitted = 0;
			for (auto& emitter : s_ParticleEmitters)
			{
				emitted = emitter.Emit(ts);
			}
			for (auto& emitter : s_ParticleEmitters)
			{
				ParticleView view = particleComponent.System->Emit(emitted);
				emitter.Generate(emitted, view.GetData(), view.GetSize());
			}	
		}
	}

	void Scene::updateRigidBody2DView()
	{
		XYZ_PROFILE_FUNC("Scene::updateRigidBody2DView");
		auto rigidView = m_Registry.view<TransformComponent, RigidBody2DComponent>();
		for (const auto entity : rigidView)
		{
			auto [transform, rigidBody] = rigidView.get<TransformComponent, RigidBody2DComponent>(entity);
			const b2Body* body = static_cast<b2Body*>(rigidBody.RuntimeBody);
			transform.GetTransform().Translation.x = body->GetPosition().x;
			transform.GetTransform().Translation.y = body->GetPosition().y;
			transform.GetTransform().Rotation.z = body->GetAngle();
		}
	}

	void Scene::setupPhysics()
	{
		auto rigidBodyView = m_Registry.view<RigidBody2DComponent>();
		m_PhysicsEntityBuffer = new SceneEntity[rigidBodyView.size()];
		b2World& physicsWorld = m_PhysicsWorld.GetWorld();
		const PhysicsWorld2D::Layer defaultLayer = m_PhysicsWorld.GetLayer(PhysicsWorld2D::DefaultLayer);
		b2FixtureDef fixture;
		fixture.filter.categoryBits = defaultLayer.m_CollisionMask.to_ulong();
		fixture.filter.maskBits = BIT(defaultLayer.m_ID);

		size_t counter = 0;
		for (auto ent : rigidBodyView)
		{
			SceneEntity entity(ent, this);
			auto& rigidBody = rigidBodyView.get<RigidBody2DComponent>(ent);
			const TransformComponent& transform = entity.GetComponent<TransformComponent>();
			auto [translation, rotation, scale] = transform.GetWorldComponents();
		
			b2BodyDef bodyDef;
		
			if (rigidBody.Type == RigidBody2DComponent::BodyType::Dynamic)
				bodyDef.type = b2_dynamicBody;
			else if (rigidBody.Type == RigidBody2DComponent::BodyType::Static)
				bodyDef.type = b2_staticBody;
			else if (rigidBody.Type == RigidBody2DComponent::BodyType::Kinematic)
				bodyDef.type = b2_kinematicBody;
			
			
			bodyDef.position.Set(translation.x, translation.y);
			bodyDef.angle = transform->Rotation.z;
			bodyDef.userData.pointer = reinterpret_cast<uintptr_t>(&m_PhysicsEntityBuffer[counter]);
			
			b2Body* body = physicsWorld.CreateBody(&bodyDef);
			rigidBody.RuntimeBody = body;
			
			if (entity.HasComponent<BoxCollider2DComponent>())
			{
				BoxCollider2DComponent& boxCollider = entity.GetComponent<BoxCollider2DComponent>();
				b2PolygonShape poly;
				
				poly.SetAsBox( (boxCollider.Size.x / 2.0f) * scale.x, (boxCollider.Size.y / 2.0f) * scale.x,
					   b2Vec2{ boxCollider.Offset.x, boxCollider.Offset.y }, 0.0f);
				
				fixture.shape = &poly;
				fixture.density = boxCollider.Density;
				fixture.friction = boxCollider.Friction;
				
				boxCollider.RuntimeFixture = body->CreateFixture(&fixture);
			}
			else if (entity.HasComponent<CircleCollider2DComponent>())
			{
				CircleCollider2DComponent& circleCollider = entity.GetComponent<CircleCollider2DComponent>();
				b2CircleShape circle;
		
				circle.m_radius = circleCollider.Radius * scale.x;
				circle.m_p = b2Vec2(circleCollider.Offset.x, circleCollider.Offset.y);
		
				fixture.shape = &circle;
				fixture.density  = circleCollider.Density;
				fixture.friction = circleCollider.Friction;
				circleCollider.RuntimeFixture = body->CreateFixture(&fixture);
			}
			else if (entity.HasComponent<PolygonCollider2DComponent>())
			{
				PolygonCollider2DComponent& meshCollider = entity.GetComponent<PolygonCollider2DComponent>();
				b2PolygonShape poly;
				poly.Set((const b2Vec2*)meshCollider.Vertices.data(), (int32_t)meshCollider.Vertices.size());

				fixture.shape = &poly;
				fixture.density =  meshCollider.Density;
				fixture.friction = meshCollider.Friction;
				meshCollider.RuntimeFixture = body->CreateFixture(&fixture);
			}
			else if (entity.HasComponent<ChainCollider2DComponent>())
			{
				ChainCollider2DComponent& chainCollider = entity.GetComponent<ChainCollider2DComponent>();
				
				b2ChainShape chain;
				std::vector<b2Vec2> points;
				points.reserve(chainCollider.Points.size());
				if (chainCollider.InvertNormals)
				{
					for (auto it = chainCollider.Points.rbegin(); it != chainCollider.Points.rend(); ++it)
					{
						points.push_back(b2Vec2{ it->x * scale.x, it->y * scale.y });
					}
				}
				else
				{
					for (const auto& p : chainCollider.Points)
					{
						points.push_back(b2Vec2{ p.x * scale.x, p.y * scale.y });
					}
				}
				chain.CreateChain(points.data(), (int32_t)points.size(),
					{ points[0].x,	   points[0].y },
					{ points.back().x, points.back().y });
			
				fixture.shape = &chain;
				fixture.density  = chainCollider.Density;
				fixture.friction = chainCollider.Friction;
				chainCollider.RuntimeFixture = body->CreateFixture(&fixture);
			}
			counter++;
		}
	}

	void Scene::setupLightEnvironment()
	{
		auto pointLights3D = m_Registry.view<PointLightComponent3D, TransformComponent>();
		m_LightEnvironment.PointLights3D.clear();
		m_LightEnvironment.PointLights2D.clear();
		m_LightEnvironment.SpotLights2D.clear();
		m_LightEnvironment.DirectionalLights.clear();

		// PointLights3D
		for (auto entity : pointLights3D)
		{
			auto [transformComponent, lightComponent] = pointLights3D.get<TransformComponent, PointLightComponent3D>(entity);
			auto [translation, rotation, scale] = transformComponent.GetWorldComponents();

			m_LightEnvironment.PointLights3D.push_back({
				translation,
				lightComponent.Intensity,
				lightComponent.Radiance,
				lightComponent.MinRadius,
				lightComponent.Radius,
				lightComponent.Falloff,
				lightComponent.LightSize,
				lightComponent.CastsShadows,
				});
		}

		auto particleView = m_Registry.view<TransformComponent, ParticleRenderer, ParticleComponent>();

		for (auto entity : particleView)
		{
			auto& [transform, renderer, particleComponent] = particleView.get<TransformComponent, ParticleRenderer, ParticleComponent>(entity);

			auto& renderData = particleComponent.GetSystem()->GetRenderData();

			for (const auto& lightData : renderData.LightData)
			{
				m_LightEnvironment.PointLights3D.push_back({
					lightData.Position,
					lightData.Intensity,
					lightData.Color,
					1.0f,
					lightData.Radius,
					1.0f,
					0.5f,
					true
				});
			}
		}
		// PointLights2D
		auto pointLight2DView = m_Registry.view<TransformComponent, PointLightComponent2D>();
		for (auto entity : pointLight2DView)
		{
			auto& [transform, light] = pointLight2DView.get<TransformComponent, PointLightComponent2D>(entity);
			auto [trans, rot, scale] = transform.GetWorldComponents();

			m_LightEnvironment.PointLights2D.push_back({
				glm::vec4(light.Color, 1.0f),
				glm::vec2(trans),
				light.Radius,
				light.Intensity
			});
		}

		// SpotLights2D
		auto spotLight2DView = m_Registry.view<TransformComponent, SpotLightComponent2D>();
		for (auto entity : spotLight2DView)
		{
			// Render previous frame data
			auto& [transform, light] = spotLight2DView.get<TransformComponent, SpotLightComponent2D>(entity);
			auto [trans, rot, scale] = transform.GetWorldComponents();

			m_LightEnvironment.SpotLights2D.push_back({
				glm::vec4(light.Color, 1.0f),
				glm::vec2(trans),
				light.Radius,
				light.Intensity,
				light.InnerAngle,
				light.OuterAngle
			});
		}


		// DirectionalLights
		auto& dirLightStorage = m_Registry.storage<DirectionalLightComponent>();
		for (auto& light : dirLightStorage)
		{
			if (m_LightEnvironment.DirectionalLights.size() == UBSceneData::MaxDirectionalLights)
				break;

			auto& dirLight = m_LightEnvironment.DirectionalLights.emplace_back();
			dirLight.Direction = light.Direction;
			dirLight.Multiplier = light.Intensity;
			dirLight.Radiance = light.Radiance;
		}
		
	}

	

	void Scene::CreateParticleTest()
	{
		auto m_ParticleSystemGPU = AssetManager::GetAsset<ParticleSystemGPU>("Resources/ParticleSystems/ParticleSystem.particle");
		auto m_ParticleCubeMesh = AssetManager::GetAsset<StaticMesh>("Resources/Meshes/Cube.mesh");

		auto m_UpdateCommandMaterial = AssetManager::GetAsset<MaterialAsset>("Resources/Materials/ParticleComputeMaterial.mat");
		auto m_ParticleMaterialGPU = AssetManager::GetAsset<MaterialAsset>("Resources/Materials/ParticleMaterialGPU.mat");
		
		Ref<Texture2D> whiteTexture = Renderer::GetDefaultResources().RendererAssets.at("WhiteTexture").As<Texture2D>();
		m_ParticleMaterialGPU->SetTexture("u_Texture", whiteTexture);

		int enabled = 1;
		m_UpdateCommandMaterial->Specialize("COLOR_OVER_LIFE", enabled);
		m_UpdateCommandMaterial->Specialize("SCALE_OVER_LIFE", enabled);
		m_UpdateCommandMaterial->Specialize("VELOCITY_OVER_LIFE", enabled);
		m_UpdateCommandMaterial->Specialize("ROTATION_OVER_LIFE", enabled);
		m_UpdateCommandMaterial->Specialize("SPAWN_LIGHTS", enabled);
	
		{
			SceneEntity entity = CreateEntity("Particle GPU Test0");
			auto& particleComponent = entity.EmplaceComponent<ParticleComponentGPU>();

			particleComponent.UpdateMaterial = m_UpdateCommandMaterial;
			particleComponent.RenderMaterial = m_ParticleMaterialGPU;
			particleComponent.Mesh = m_ParticleCubeMesh;
			particleComponent.System = m_ParticleSystemGPU;
		}
		{
			SceneEntity entity = CreateEntity("Particle GPU Test1");
			auto& particleComponent = entity.EmplaceComponent<ParticleComponentGPU>();

			particleComponent.UpdateMaterial = m_UpdateCommandMaterial;
			particleComponent.RenderMaterial = m_ParticleMaterialGPU;
			particleComponent.Mesh = m_ParticleCubeMesh;
			particleComponent.System = m_ParticleSystemGPU;
		}
	}

}