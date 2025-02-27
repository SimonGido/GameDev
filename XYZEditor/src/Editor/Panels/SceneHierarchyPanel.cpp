#include "stdafx.h"
#include "SceneHierarchyPanel.h"

#include "XYZ/Core/Input.h"
#include "XYZ/Core/Application.h"

#include "XYZ/Scene/Prefab.h"
#include "XYZ/Scene/SceneEntity.h"
#include "XYZ/Scene/Components.h"

#include "XYZ/Asset/AssetManager.h"

#include "Editor/Event/EditorEvents.h"

#include <imgui.h>

namespace XYZ {
    namespace Utils {
        static bool Collide(const glm::vec2& pos, const glm::vec2& size, const glm::vec2& point)
        {
            return (pos.x + size.x > point.x &&
                pos.x		   < point.x&&
                pos.y + size.y >  point.y &&
                pos.y < point.y);
        }
      
    }
    namespace Editor {
        SceneHierarchyPanel::SceneHierarchyPanel(std::string name)
            :
            EditorPanel(std::move(name))
        {
        }
        SceneHierarchyPanel::~SceneHierarchyPanel()
        {
        }
        void SceneHierarchyPanel::OnImGuiRender(bool& open)
        {
            if (ImGui::Begin("Scene Hierarchy", &open))
            {
                if (m_Context.Raw())
                {                  
                    drawEntityNode(SceneEntity(m_Context->m_SceneEntity, m_Context.Raw()), true);
                    if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && ImGui::IsWindowHovered())
                    {
                        m_Context->SetSelectedEntity(entt::null);
                        Application::Get().OnEvent(EntitySelectedEvent(SceneEntity()));
                    }
                    if (ImGui::BeginPopupContextWindow(0, 1, false))
                    {
                        if (ImGui::MenuItem("Create Empty Entity"))
                            m_Context->CreateEntity("Empty Entity", GUID());
                    
                        ImGui::EndPopup();
                    }                   
                }
            }
            ImGui::End();
        }

        bool SceneHierarchyPanel::OnEvent(Event& e)
        {
            return false;
        }

        void SceneHierarchyPanel::SetSceneContext(const Ref<Scene>& scene)
        {
            m_Context = scene;
        }
    

        void SceneHierarchyPanel::drawEntityNode(const SceneEntity& entity, bool defaultOpen)
        {
            const auto* tag = &entity.GetComponent<SceneTagComponent>().Name;
            const auto* rel = &entity.GetComponent<Relationship>();

            ImGuiTreeNodeFlags flags = (m_Context->GetSelectedEntity() == entity ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow;
            flags |= ImGuiTreeNodeFlags_SpanAvailWidth;
           
            if (defaultOpen)
                flags |= ImGuiTreeNodeFlags_DefaultOpen;
            if (rel->GetFirstChild() == entt::null)
                flags |= ImGuiTreeNodeFlags_Leaf;

            const bool opened = ImGui::TreeNodeEx((void*)(uint64_t)(uint32_t)entity, flags, tag->c_str());
            dragAndDrop(entity);
            
            if (ImGui::IsItemClicked())
            {
                m_Context->SetSelectedEntity(entity.ID());
                Application::Get().OnEvent(EntitySelectedEvent(entity));
            }
            
            bool entityDeleted = false;
            if (ImGui::BeginPopupContextItem())
            {
                if (ImGui::MenuItem("Create Empty Entity"))
                {
                    m_Context->CreateEntity("Empty Entity", entity, GUID());
                }
                if (ImGui::MenuItem("Delete Entity"))
                {
                    entityDeleted = true;
                }
                if (ImGui::MenuItem("Create Prefab"))
                {
                    Ref<Prefab> prefab = AssetManager::CreateAsset<Prefab>(*tag + ".prefab", "Assets/Prefabs");
                    prefab->Create(entity);
                    AssetManager::Serialize(prefab->GetHandle());
                }
                ImGui::EndPopup();
            }
           
            // it might be invalidated
            tag = &entity.GetComponent<SceneTagComponent>().Name;
            rel = &entity.GetComponent<Relationship>();
            if (opened)
            {
                if (rel->GetFirstChild() != entt::null)
                    drawEntityNode(SceneEntity(rel->GetFirstChild(), m_Context.Raw()), false);
                ImGui::TreePop();
            }

            // it might be invalidated
            tag = &entity.GetComponent<SceneTagComponent>().Name;
            rel = &entity.GetComponent<Relationship>();
            if (rel->GetNextSibling() != entt::null)
                drawEntityNode(SceneEntity(rel->GetNextSibling(), m_Context.Raw()), false);

            if (entityDeleted)
            {
                m_Context->DestroyEntity(entity);
                Application::Get().OnEvent(EntitySelectedEvent(SceneEntity()));
            }
        }
        void SceneHierarchyPanel::dragAndDrop(const SceneEntity& entity)
        {
            if (ImGui::BeginDragDropSource())
            {
                entt::entity id = entity.ID();
                ImGui::SetDragDropPayload("ENTITY_DRAG", (void*)&id, sizeof(entt::entity), ImGuiCond_Always);
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_DRAG", 0))
                {
                    const entt::entity child = *(entt::entity*)payload->Data;
                    if (child != entity.ID())
                    {
                        const auto& parentTransform = m_Context->m_Registry.get<TransformComponent>(entity.ID());
                        auto& transform = m_Context->m_Registry.get<TransformComponent>(child);
                      
                        transform.DecomposeTransform(transform->WorldTransform);      
                        Relationship::SetupRelation(entity.ID(), child, m_Context->m_Registry);
                        transform.DecomposeTransform(glm::inverse(parentTransform->WorldTransform) * transform.GetLocalTransform());
                    }
                }
                ImGui::EndDragDropTarget();
            }
        }
    }
}