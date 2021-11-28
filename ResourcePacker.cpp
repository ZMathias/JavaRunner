#include <fstream>
#include <Windows.h>
#include "resource.h"

const std::pair<char*, DWORD> get_resource(const int resource_name, const int resource_type)
{
	std::pair<char*, DWORD> final_data;
	HMODULE h_module = nullptr;
	GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, reinterpret_cast<LPCTSTR>(get_resource), &h_module);

	const HRSRC h_resource = FindResource(h_module, MAKEINTRESOURCE(resource_name), MAKEINTRESOURCE(resource_type));
	if (h_resource == nullptr) return {0, 0};

	const HGLOBAL h_pre_data = LoadResource(h_module, h_resource);
	if (h_pre_data == nullptr) return {0, 0};

	final_data.second = SizeofResource(h_module, h_resource);
	final_data.first = static_cast<char*>(LockResource(h_pre_data));
	return final_data;
}

int main()
{
	std::ofstream file("payload.exe", std::ios_base::binary);
	const auto data = get_resource(PAYLOAD, EXECUTABLE);
	file.write(data.first, data.second);
}