#pragma once
#include "XYZ/Scene/BlueprintVariableType.h"

namespace XYZ {

	class ImGuiVariableTypeExtension
	{
		using EditFunction = std::function<bool(const char* id, std::byte* data)>;
	public:
		ImGuiVariableTypeExtension();

		void RegisterEditFunction(const std::string& name, EditFunction func);

		bool EditValue(const std::string& name, const char*id, std::byte* data);



	private:
		std::unordered_map<std::string, EditFunction> m_EditFunctions;
	};

}