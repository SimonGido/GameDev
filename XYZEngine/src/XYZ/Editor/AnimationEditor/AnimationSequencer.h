#pragma once
#include "SequencerLineEdit.h"

#include <ImSequencer.h>
#include <ImCurveEdit.h>

#include <imgui.h>
#include <imgui_internal.h>

namespace XYZ {
	namespace Editor {

        struct AnimationSequencer : public ImSequencer::SequenceInterface
        {
            using AddKeyCallback = std::function<void()>;

            struct SequenceItem
            {
                int               Type;
                int               FrameStart;
                int               FrameEnd;
                bool              Expanded;
                SequenceLineEdit  LineEdit;
            };

            struct SequencerItemType
            {       
                std::string              Name;
                std::vector<std::string> SubTypes;
                AddKeyCallback           Callback;
                size_t                   Height = 100;
            };


            AnimationSequencer();
            virtual int         GetFrameMin() const override { return m_FrameMin; }
            virtual int         GetFrameMax() const override { return m_FrameMax; }
            virtual int         GetItemCount() const  override { return (int)m_Items.size(); }
            virtual int         GetItemTypeCount() const override { return (int)m_SequencerItemTypes.size(); }
            virtual const char* GetItemTypeName(int typeIndex) const override { return m_SequencerItemTypes[typeIndex].Name.c_str(); }
            virtual const char* GetItemLabel(int index) const override;
            virtual void        Get(int index, int** start, int** end, int* type, unsigned int* color) override;        
            virtual void        Add(int type) override;
            virtual void        Del(int index) override { m_Items.erase(m_Items.begin() + index); }
            virtual void        Duplicate(int index) override { m_Items.push_back(m_Items[index]); }

            virtual size_t      GetCustomHeight(int index) override;
            virtual void        DoubleClick(int index) override;
            virtual void        CustomDraw(int index, ImDrawList* draw_list, const ImRect& rc, const ImRect& legendRect, const ImRect& clippingRect, const ImRect& legendClippingRect) override;
            virtual void        CustomDrawCompact(int index, ImDrawList* draw_list, const ImRect& rc, const ImRect& clippingRect) override;            
        
            
            void                AddSequencerItemType(const std::string& name, const std::initializer_list<std::string>& subTypes, AddKeyCallback callback);

 
            std::vector<SequencerItemType> m_SequencerItemTypes;
            std::vector<SequenceItem>      m_Items;
            int                            m_FrameMin;
            int                            m_FrameMax;
        };
	}
}