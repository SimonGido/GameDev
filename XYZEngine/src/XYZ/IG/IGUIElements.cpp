#include "stdafx.h"
#include "IGUIElements.h"
#include "XYZ/Core/KeyCodes.h"

namespace XYZ {
	namespace Helper {
		static bool Collide(const glm::vec2& pos, const glm::vec2& size, const glm::vec2& point)
		{
			return (pos.x + size.x > point.x &&
				pos.x		   < point.x&&
				pos.y + size.y >  point.y &&
				pos.y < point.y);
		}

		static uint32_t FindNumCharacters(const char* source, float maxWidth, Ref<Font> font)
		{
			if (!source)
				return 0;

			float xCursor = 0.0f;
			uint32_t counter = 0;
			while (source[counter] != '\0')
			{
				auto& character = font->GetCharacter(source[counter]);
				if (xCursor + (float)character.XAdvance >= maxWidth)
					break;

				xCursor += character.XAdvance;
				counter++;
			}
			return counter;
		}
	}

	IGWindow::IGWindow(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color)
		:
		IGElement(position, size, color)
	{
		ElementType = IGElementType::Window;
	}

	bool IGWindow::OnLeftClick(const glm::vec2& mousePosition, bool& handled)
	{
		glm::vec2 absolutePosition = GetAbsolutePosition();
		if (!handled && Helper::Collide(absolutePosition, Size, mousePosition))
		{
			ReturnType = IGReturnType::Clicked;
			glm::vec2 minButtonPos = { absolutePosition.x + Size.x - IGWindow::PanelHeight, absolutePosition.y };
			if (absolutePosition.y + IGWindow::PanelHeight >= mousePosition.y)
			{
				if (Helper::Collide(minButtonPos, { IGWindow::PanelHeight, IGWindow::PanelHeight }, mousePosition))
				{
					Flags ^= IGWindow::Flags::Collapsed;
					ActiveChildren = !IS_SET(Flags, IGWindow::Flags::Collapsed);
				}
				else
				{
					Flags |= IGWindow::Flags::Moved;
					Flags &= ~Docked;
				}
				handled = true;
			}
			else
			{
				if (mousePosition.x < absolutePosition.x + 5.0f) // Left resize
				{
					Flags |= LeftResize;
					handled = true;
				}
				else if (mousePosition.x > absolutePosition.x + Size.x - 5.0f) // Right resize
				{
					Flags |= RightResize;
					handled = true;
				}
				if (mousePosition.y > absolutePosition.y + Size.y - 5.0f) // Bottom
				{
					Flags |= BottomResize;
					handled = true;
				}
			}
		}
		return handled;
	}

	bool IGWindow::OnLeftRelease(const glm::vec2& mousePosition, bool& handled)
	{
		if (IS_SET(Flags, (Moved | LeftResize | RightResize | BottomResize)))
		{
			handled = true;
		}
		Flags &= ~Moved;
		Flags &= ~(LeftResize | RightResize | BottomResize);
		if (Helper::Collide(GetAbsolutePosition(), Size, mousePosition))
		{
			ReturnType = IGReturnType::Released;
			return true;
		}
		return false;
	}

	bool IGWindow::OnMouseMove(const glm::vec2& mousePosition, bool& handled)
	{
		if (Helper::Collide(GetAbsolutePosition(), Size, mousePosition))
		{
			ReturnType = IGReturnType::Hoovered;
			return true;
		}
		return false;
	}

	glm::vec2 IGWindow::GenerateQuads(IGMesh& mesh, IGRenderData& renderData)
	{
		IGMeshFactoryData data = { renderData.SubTextures[IGRenderData::Window], this, &mesh, &renderData };
		return IGMeshFactory::GenerateUI<IGWindow>(Label.c_str(), glm::vec4(1.0f), data);
	}

	void IGWindow::HandleActions(const glm::vec2& mousePosition,const glm::vec2& mouseDiff, bool& handled)
	{
		if (IS_SET(Flags, IGWindow::Moved))
		{
			Position = mouseDiff;
			handled = true;
		}
		else if (IS_SET(Flags, IGWindow::LeftResize))
		{
			Size.x = GetAbsolutePosition().x + Size.x - mousePosition.x;
			Position.x = mousePosition.x;
			handled = true;
			if (ResizeCallback)
				ResizeCallback(Size);
		}
		else if (IS_SET(Flags, IGWindow::RightResize))
		{
			Size.x = mousePosition.x - GetAbsolutePosition().x;
			handled = true;
			if (ResizeCallback)
				ResizeCallback(Size);
		}

		if (IS_SET(Flags, IGWindow::BottomResize))
		{
			Size.y = mousePosition.y - GetAbsolutePosition().y;
			handled = true;
			if (ResizeCallback)
				ResizeCallback(Size);
		}
	}

	IGImageWindow::IGImageWindow(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color)
		:
		IGWindow(position, size, color)
	{
	}
	
	glm::vec2 IGImageWindow::GenerateQuads(IGMesh& mesh, IGRenderData& renderData)
	{
		IGMeshFactoryData data = { SubTexture, this, &mesh, &renderData };
		return IGMeshFactory::GenerateUI<IGImageWindow>(Label.c_str(), glm::vec4(1.0f), data);
	}

	IGButton::IGButton(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color)
		:
		IGElement(position, size, color)
	{}

	bool IGButton::OnLeftClick(const glm::vec2& mousePosition, bool& handled)
	{
		if (!handled && Helper::Collide(GetAbsolutePosition(), Size, mousePosition))
		{
			handled = true;
			ReturnType = IGReturnType::Clicked;
			return true;
		}
		return false;
	}
	bool IGButton::OnMouseMove(const glm::vec2& mousePosition, bool& handled)
	{
		Color = IGRenderData::Colors[IGRenderData::DefaultColor];
		if (Helper::Collide(GetAbsolutePosition(), Size, mousePosition))
		{
			ReturnType = IGReturnType::Hoovered;
			Color = IGRenderData::Colors[IGRenderData::HooverColor];
			return true;
		}
		return false;
	}
	glm::vec2 IGButton::GenerateQuads(IGMesh& mesh, IGRenderData& renderData)
	{
		IGMeshFactoryData data = { renderData.SubTextures[IGRenderData::Button], this, &mesh, &renderData };
		return IGMeshFactory::GenerateUI<IGButton>(Label.c_str(), glm::vec4(1.0f), data);
	}

	IGCheckbox::IGCheckbox(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color)
		:
		IGElement(position, size, color)
	{
	}
	bool IGCheckbox::OnLeftClick(const glm::vec2& mousePosition, bool& handled)
	{
		if (!handled && Helper::Collide(GetAbsolutePosition(), Size, mousePosition))
		{
			ReturnType = IGReturnType::Clicked;
			Checked = !Checked;
			handled = true;
			return true;
		}
		return false;
	}
	bool IGCheckbox::OnMouseMove(const glm::vec2& mousePosition, bool& handled)
	{
		Color = IGRenderData::Colors[IGRenderData::DefaultColor];
		if (Helper::Collide(GetAbsolutePosition(), Size, mousePosition))
		{
			ReturnType = IGReturnType::Hoovered;
			Color = IGRenderData::Colors[IGRenderData::HooverColor];
			return true;
		}
		return false;
	}
	glm::vec2 IGCheckbox::GenerateQuads(IGMesh& mesh, IGRenderData& renderData)
	{
		uint32_t subTextureIndex = IGRenderData::CheckboxUnChecked;
		if (Checked)
			subTextureIndex = IGRenderData::CheckboxChecked;

		IGMeshFactoryData data = { renderData.SubTextures[subTextureIndex], this, &mesh, &renderData };
		return IGMeshFactory::GenerateUI<IGCheckbox>(Label.c_str(), glm::vec4(1.0f), data);
	}
	IGSlider::IGSlider(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color)
		:
		IGElement(position, size, color)
	{
	}
	bool IGSlider::OnLeftClick(const glm::vec2& mousePosition, bool& handled)
	{
		if (!handled && Helper::Collide(GetAbsolutePosition(), Size, mousePosition))
		{
			ReturnType = IGReturnType::Clicked;
			Modified = true;
			handled = true;
			return true;
		}
		return false;
	}
	bool IGSlider::OnLeftRelease(const glm::vec2& mousePosition, bool& handled)
	{
		Modified = false;
		return false;
	}
	bool IGSlider::OnMouseMove(const glm::vec2& mousePosition, bool& handled)
	{
		Color = IGRenderData::Colors[IGRenderData::DefaultColor];
		if (Modified)
		{
			glm::vec2 absolutePosition = GetAbsolutePosition();
			Value = (mousePosition.x - absolutePosition.x) / Size.x;
			Value = std::clamp(Value, 0.0f, 1.0f);
		}
		if (Helper::Collide(GetAbsolutePosition(), Size, mousePosition))
		{
			ReturnType = IGReturnType::Hoovered;
			Color = IGRenderData::Colors[IGRenderData::HooverColor];
			return true;
		}
	}
	glm::vec2 IGSlider::GenerateQuads(IGMesh& mesh, IGRenderData& renderData)
	{
		IGMeshFactoryData data = { renderData.SubTextures[IGRenderData::Slider], this, &mesh, &renderData };
		return IGMeshFactory::GenerateUI<IGSlider>(Label.c_str(), Color, data);
	}
	IGText::IGText(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color)
		:
		IGElement(position, size, color)
	{
	}
	bool IGText::OnLeftClick(const glm::vec2& mousePosition, bool& handled)
	{
		return false;
	}
	bool IGText::OnMouseMove(const glm::vec2& mousePosition, bool& handled)
	{
		Color = IGRenderData::Colors[IGRenderData::DefaultColor];
		if (Helper::Collide(GetAbsolutePosition(), Size, mousePosition))
		{
			ReturnType = IGReturnType::Hoovered;
			Color = IGRenderData::Colors[IGRenderData::HooverColor];
			return true;
		}
		return false;
	}
	glm::vec2 IGText::GenerateQuads(IGMesh& mesh, IGRenderData& renderData)
	{
		IGMeshFactoryData data = { nullptr, this, &mesh, &renderData };
		return IGMeshFactory::GenerateUI<IGText>(Text.c_str(), Color, data);
	}

	IGFloat::IGFloat(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color)
		:
		IGElement(position, size, color)
	{
		memset(Buffer, 0, BufferSize);
		snprintf(Buffer, sizeof(Buffer), "%f", Value);
		ModifiedIndex = 0;
		while (Buffer[ModifiedIndex] != '\0')
			ModifiedIndex++;
	}
	bool IGFloat::OnLeftClick(const glm::vec2& mousePosition, bool& handled)
	{
		Listen = false;
		Color = IGRenderData::Colors[IGRenderData::DefaultColor];
		if (!handled && Helper::Collide(GetAbsolutePosition(), Size, mousePosition))
		{
			Listen = true;
			handled = true;
			ReturnType = IGReturnType::Clicked;
			Color = IGRenderData::Colors[IGRenderData::HooverColor];
			return true;
		}
		return false;
	}
	bool IGFloat::OnMouseMove(const glm::vec2& mousePosition, bool& handled)
	{
		if (!Listen)
			Color = IGRenderData::Colors[IGRenderData::DefaultColor];
		if (Helper::Collide(GetAbsolutePosition(), Size, mousePosition))
		{
			ReturnType = IGReturnType::Hoovered;
			Color = IGRenderData::Colors[IGRenderData::HooverColor];
			return true;
		}
		return false;
	}
	bool IGFloat::OnKeyType(char character, bool& handled)
	{
		if (Listen && !handled)
		{
			handled = true;
			if (character >= toascii('0') && character <= toascii('9') || character == toascii('.'))
			{
				Buffer[ModifiedIndex++] = character;
				return true;
			}
		}
		return false;
	}
	bool IGFloat::OnKeyPress(int32_t mode, int32_t key, bool& handled)
	{
		if (Listen && !handled)
		{
			if (key == ToUnderlying(KeyCode::KEY_BACKSPACE))
			{
				if (ModifiedIndex > 0)
					ModifiedIndex--;
				Buffer[ModifiedIndex] = '\0';
				handled = true;
				return true;
			}
		}
		return false;
	}
	glm::vec2 IGFloat::GenerateQuads(IGMesh& mesh, IGRenderData& renderData)
	{
		float textWidth = Size.x - Style.Layout.LeftPadding - Style.Layout.RightPadding;
		size_t numChar = Helper::FindNumCharacters(Buffer, textWidth, renderData.Font);
		Buffer[numChar] = '\0';
		ModifiedIndex = numChar;

		IGMeshFactoryData data = {renderData.SubTextures[IGRenderData::Slider], this, &mesh, &renderData };
		return IGMeshFactory::GenerateUI<IGFloat>(Label.c_str(), Color, data);
	}

	float IGFloat::GetValue() const
	{
		return (float)atof(Buffer);
	}


	IGTree::IGTree(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color)
		:
		IGElement(position, size, color),
		Pool(sizeof(IGTreeItem)* NumberOfItemsPerBlockInPool)
	{
	}

	bool IGTree::OnLeftClick(const glm::vec2& mousePosition, bool& handled)
	{
		Hierarchy.Traverse([&](void* parent, void* child) ->bool {
			IGTreeItem* childItem = static_cast<IGTreeItem*>(child);
			if (!handled && Helper::Collide(childItem->Position, Size, mousePosition))
			{
				childItem->Open = !childItem->Open;
				handled = true;
				return true;
			}
			return false;
		});
		return false;
	}
	bool IGTree::OnMouseMove(const glm::vec2& mousePosition, bool& handled)
	{
		Hierarchy.Traverse([&](void* parent, void* child) ->bool {
			IGTreeItem* childItem = static_cast<IGTreeItem*>(child);
			childItem->Color = IGRenderData::Colors[IGRenderData::DefaultColor];
			if (Helper::Collide(childItem->Position, Size, mousePosition))
			{
				childItem->Color = IGRenderData::Colors[IGRenderData::HooverColor];
			}
			return false;
		});
		return false;
	}
	glm::vec2 IGTree::GenerateQuads(IGMesh& mesh, IGRenderData& renderData)
	{
		IGMeshFactoryData data = { renderData.SubTextures[IGRenderData::RightArrow], this, &mesh, &renderData };
		return IGMeshFactory::GenerateUI<IGTree>(Label.c_str(), Color, data);
	}
	IGTree::IGTreeItem& IGTree::GetItem(const char* name)
	{
		auto it = NameIDMap.find(name);
		XYZ_ASSERT(it != NameIDMap.end(), "");
		
		return *static_cast<IGTreeItem*>(Hierarchy.GetData(it->second));
	}
	void IGTree::AddItem(const char* name, const char* parent, const IGTreeItem& item)
	{
		auto it = NameIDMap.find(name);
		if (it == NameIDMap.end())
		{
			IGTreeItem* ptr = Pool.Allocate<IGTreeItem>(item.Label);
			if (parent)
			{
				auto parentIt = NameIDMap.find(parent);
				if (parentIt != NameIDMap.end())
				{
					ptr->ID = Hierarchy.Insert(ptr, parentIt->second);
					NameIDMap[name] = ptr->ID;
				}
				else
				{
					XYZ_LOG_WARN("Parent item with the name: ", parent, "does not exist");
				}
			}
			else
			{
				ptr->ID = Hierarchy.Insert(ptr);
				NameIDMap[name] = ptr->ID;
			}
		}
		else
		{
			XYZ_LOG_WARN("Item with the name: ", name, "already exists");
		}
	}
	void IGTree::RemoveItem(const char* name)
	{
		auto it = NameIDMap.find(name);
		if (it != NameIDMap.end())
		{
			IGTreeItem* ptr = static_cast<IGTreeItem*>(Hierarchy.GetData(it->second));
			Pool.Deallocate(ptr);
			Hierarchy.Remove(it->second);
			NameIDMap.erase(it);
		}
		else
		{
			XYZ_LOG_WARN("Item with the name: ", name, "does not exist");
		}
	}
	IGGroup::IGGroup(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color)
		: IGElement(position, size, color)
	{
		ActiveChildren = Open;
	}
	bool IGGroup::OnLeftClick(const glm::vec2& mousePosition, bool& handled)
	{
		if (!handled && Helper::Collide(GetAbsolutePosition(), { Size.x, PanelHeight}, mousePosition))
		{
			Open = !Open;
			ActiveChildren = Open;
			handled = true;
			return true;
		}
		return false;
	}
	bool IGGroup::OnMouseMove(const glm::vec2& mousePosition, bool& handled)
	{
		return false;
	}
	glm::vec2 IGGroup::GenerateQuads(IGMesh& mesh, IGRenderData& renderData)
	{
		if (AdjustToParent && Parent)
		{
			Size.x = Parent->Size.x - Parent->Style.Layout.LeftPadding - Parent->Style.Layout.RightPadding;
		}
		ActiveChildren = Open;
		uint32_t subTextureIndex = IGRenderData::RightArrow;
		if (Open)
			subTextureIndex = IGRenderData::DownArrow;
		IGMeshFactoryData data = { renderData.SubTextures[subTextureIndex], this, &mesh, &renderData };
		return IGMeshFactory::GenerateUI<IGGroup>(Label.c_str(), Color, data);
	}
	IGSeparator::IGSeparator(const glm::vec2& position, const glm::vec2& size)
		:
		IGElement(position, size, glm::vec4(1.0f))
	{
	}
	glm::vec2 IGSeparator::GenerateQuads(IGMesh& mesh, IGRenderData& renderData)
	{
		if (AdjustToParent && Parent)
		{
			Size.x = Parent->Size.x - Parent->Style.Layout.LeftPadding - Parent->Style.Layout.RightPadding;
		}
		return Size;
	}
	
}