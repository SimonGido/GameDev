#pragma once
#include "XYZ/Core/Ref/Ref.h"
#include "XYZ/Core/Ref/WeakRef.h"
#include "XYZ/Core/Timestep.h"

#include "XYZ/Event/Event.h"
#include "XYZ/Renderer/Camera.h"

#include "XYZ/Physics/ContactListener.h"
#include "XYZ/Physics/PhysicsWorld2D.h"

#include "XYZ/Utils/DataStructures/ThreadPass.h"

#include "SceneCamera.h"
#include "GPUScene.h"

#include <entt/entt.hpp>

#include <box2d/box2d.h>

namespace XYZ {

    enum class SceneState
    {
        Play,
        Edit,
        Pause
    };


    class Renderer2D;
    class SceneRenderer;
    class SceneEntity;

    namespace Editor {
        class SceneHierarchyPanel;
    }


    struct PointLight3D
    {
        glm::vec3 Position = { 0.0f, 0.0f, 0.0f };
        float	  Multiplier = 0.0f;
        glm::vec3 Radiance = { 0.0f, 0.0f, 0.0f };
        float	  MinRadius = 0.001f;
        float	  Radius = 25.0f;
        float	  Falloff = 1.f;
        float	  SourceSize = 0.1f;
        bool	  CastsShadows = true;
        char	  Padding[3]{ 0, 0, 0 };
    };
    
    struct DirectionalLight
    {
        glm::vec3 Direction;
        float Padding{ 0.0f };
        glm::vec3 Radiance;
        float     Multiplier;
    };


    struct PointLight2D
    {
        glm::vec4 Color;
        glm::vec2 Position;
        float	  Radius;
        float	  Intensity;
    };

    struct SpotLight2D
    {
        glm::vec4 Color;
        glm::vec2 Position;
        float	  Radius;
        float	  Intensity;
        float	  InnerAngle;
        float	  OuterAngle;

        float Alignment[2];
    };

    struct LightEnvironment
    {
        std::vector<DirectionalLight> DirectionalLights;
        std::vector<PointLight3D>     PointLights3D;
        std::vector<PointLight2D>     PointLights2D;
        std::vector<SpotLight2D>      SpotLights2D;
    };

    class XYZ_API Scene : public Asset
    {
    public:
        Scene(const std::string& name, const GUID& guid = GUID());
        ~Scene();

        SceneEntity CreateEntity(const std::string& name, const GUID& guid = GUID());
        SceneEntity CreateEntity(const std::string& name, SceneEntity parent, const GUID& guid = GUID());
   

        void DestroyEntity(SceneEntity entity);
        void SetState(SceneState state) { m_State = state; }
        void SetViewportSize(uint32_t width, uint32_t height);
        void SetSelectedEntity(entt::entity ent) { m_SelectedEntity = ent; }

        void OnPlay();
        void OnStop();
        void OnUpdate(Timestep ts);
        void OnRender(Ref<SceneRenderer> sceneRenderer);
        void OnUpdateEditor(Timestep ts);
        void OnRenderEditor(Ref<SceneRenderer> sceneRenderer, const glm::mat4& viewProjection, const glm::mat4& view, const glm::mat4& projection);

        
        void OnImGuiRender();

        SceneEntity GetEntityByName(const std::string& name);
        SceneEntity GetEntityByGUID(const GUID& guid);
        SceneEntity GetSceneEntity();
        SceneEntity GetSelectedEntity();

        const entt::registry& GetRegistry() const { return m_Registry; }

        inline SceneState               GetState() const { return m_State; }
        inline const GUID&              GetUUID() const { return m_UUID; }
        inline const std::string&       GetName() const { return m_Name; }
        inline const LightEnvironment&  GetLightEnvironment() const { return m_LightEnvironment; }

        virtual AssetType GetAssetType() const override { return AssetType::Scene; }
        static AssetType GetStaticType() { return AssetType::Scene; }


        void CreateParticleTest();
    private:
        void onScriptComponentConstruct(entt::registry& reg, entt::entity ent);
        void onScriptComponentDestruct(entt::registry& reg, entt::entity ent);
  

        void updateScripts(Timestep ts);
        void updateHierarchy();      
        void updateHierarchyAsync();
        void updateSubHierarchy(entt::entity parent);


        void updateAnimationView(Timestep ts);
        void updateAnimationViewAsync(Timestep ts);

        void updateParticleView(Timestep ts);
        void updateGPUParticleView(Timestep ts);
        void updateRigidBody2DView();

       
        void setupPhysics();
        void setupLightEnvironment();

    private:
        PhysicsWorld2D      m_PhysicsWorld;
        ContactListener     m_ContactListener;
        SceneEntity*        m_PhysicsEntityBuffer;
        LightEnvironment    m_LightEnvironment;
        GPUScene            m_GPUScene;

        entt::registry      m_Registry;
        GUID                m_UUID;
        entt::entity        m_SceneEntity;

        std::string m_Name;
        SceneState  m_State;

        entt::entity m_SelectedEntity;
        entt::entity m_CameraEntity;


        uint32_t m_ViewportWidth;
        uint32_t m_ViewportHeight;

        std::shared_mutex m_ScriptMutex;
        
        bool  m_UpdateAnimationAsync = false;
        bool  m_UpdateHierarchyAsync = false;

        friend SceneRenderer;
        friend class SceneIntersection;
        friend class SceneEntity;
        friend class SceneSerializer;
        friend class ScriptEngine;
        friend class Prefab;
        friend class Editor::SceneHierarchyPanel;

    };
}