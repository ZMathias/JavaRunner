// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

#include <iostream>
#include <string>
#include <fstream>
#include <Windows.h>
#include <zip.h>
#include "resource.h"

std::pair<char*, DWORD> get_resource(const int resource_name, const int resource_type)
{
	std::pair<char*, DWORD> final_data;
	HMODULE h_module = nullptr;
	GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, reinterpret_cast<LPCTSTR>(get_resource), &h_module);

	const HRSRC h_resource = FindResource(h_module, MAKEINTRESOURCE(resource_name), MAKEINTRESOURCE(resource_type));
	if (h_resource == nullptr)
	{
		printf("error while finding resource: %lu", GetLastError());
		return {nullptr, 0};
	}

	const HGLOBAL h_pre_data = LoadResource(h_module, h_resource);
	if (h_pre_data == nullptr) return {nullptr, 0};

	final_data.second = SizeofResource(h_module, h_resource);
	final_data.first = static_cast<char*>(LockResource(h_pre_data));
	return final_data;
}

bool ExtractFile(const std::string_view file_name)
{
	// Open the zip file and check for errors
	int err{};
	zip_t* zip = zip_open(file_name.data(), ZIP_RDONLY, &err);

	// Check for errors
	if (err != 0)
	{
		std::cout << "Error opening zip file: " << err << std::endl;
		zip_close(zip);
		return EXIT_FAILURE;
	}

	const int numFiles = zip_get_num_files(zip);

	// Loop through the files and print their names and sizes
	for (int i{}; i < numFiles; ++i)
	{
		const char* fileName = zip_get_name(zip, i, 0);

		// Get the file size
		zip_stat_t stat{};
		zip_stat_init(&stat);
		zip_stat_index(zip, i, 0, &stat);
		if (stat.size == 0)
		{
			CreateDirectoryA(fileName, nullptr);
		}
		else
		{
			std::ofstream outFile(fileName, std::ios::binary);
			zip_file_t* file = zip_fopen_index(zip, i, ZIP_FL_UNCHANGED);
			// Read the file into the buffer
			char buffer[10240]{};
			zip_int64_t bytesRead;
			while ((bytesRead = zip_fread(file, buffer, sizeof buffer)) > 0)
			{
				outFile.write(buffer, bytesRead);
			}
			outFile.close();
			zip_fclose(file);
		}
	}
	/*//close the zip file and check for errors
	if (zip_close(zip) != 0)
	{
		std::cerr << "Error closing zip file\n";
		return EXIT_FAILURE;
	}*/
	zip_discard(zip);
	return EXIT_SUCCESS;
}

BOOL SpawnJavaProcess(char* args)
{
	STARTUPINFOA startupInfo{};
	PROCESS_INFORMATION processInfo{};

	ZeroMemory(&startupInfo, sizeof startupInfo);
	startupInfo.cb = sizeof(startupInfo);
	ZeroMemory(&processInfo, sizeof processInfo);

	const BOOL result = CreateProcessA(
		R"(C:\Windows\System32\cmd.exe)",
		args,
		nullptr,
		nullptr,
		FALSE,
		NORMAL_PRIORITY_CLASS,
		nullptr,
		nullptr,
		&startupInfo,
		&processInfo
	);
	CloseHandle(processInfo.hProcess);
	CloseHandle(processInfo.hThread);
	return result;
}

BOOL SetJAVAHOME(const char* path)
{
	std::string java_home_path(path);
	java_home_path += "\\jdk";

	HKEY hKey{};
	LONG error_code = RegCreateKeyEx(HKEY_LOCAL_MACHINE,
		L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment",
		0,
		nullptr,
		REG_OPTION_NON_VOLATILE,
		KEY_ALL_ACCESS,
		nullptr,
		&hKey,
		nullptr);
	if (error_code != ERROR_SUCCESS)
	{
		std::cerr << "Error creating registry key" << error_code << '\n';
		return false;
	}
	error_code = RegSetValueExA(hKey, "JAVA_HOME", 0, REG_SZ, reinterpret_cast<const BYTE*>(java_home_path.data()), static_cast<DWORD>(java_home_path.size()));
	if (ERROR_SUCCESS != error_code)
	{
		std::cerr << "Error setting registry value" << error_code << '\n';
		return false;
	}
	return true;
}

std::string findJar()
{
	WIN32_FIND_DATAA find_data{};
	if (INVALID_HANDLE_VALUE == FindFirstFileA("*.jar", &find_data))
	{
		return {};
	}
	return find_data.cFileName;
}

int main(int argc, char* argv[])
{
	//print out all arguments
	for (int i{}; i < argc; ++i)
	{
		std::cout << argv[i] << '\n';
	}

	// Check if the jdk folder exists
	if (GetFileAttributesA("jdk") == INVALID_FILE_ATTRIBUTES)
	{
		const std::string file_name("java.zip");
		std::ofstream java_file(file_name, std::ios_base::binary);
		const auto text = get_resource(DEMO_TEXT, IDR_TEXT_FILE1);
		java_file.write(text.first, text.second);
		java_file.close();

		if (!ExtractFile(file_name))
		{
			std::cout << "Successfully extracted the portable java runtime" << std::endl;
		}

		if (DeleteFile(L"java.zip") == 0)
		{
			std::cerr << "Error deleting \"java.zip\": " << GetLastError() << std::endl;
		}
	}

	findJar();

	std::string progr_args{"/k jdk\\bin\\java.exe -jar "};
	progr_args += findJar() + " ";
	for (int i = 1; i < argc; ++i)
	{
		progr_args += argv[i];
		progr_args += ' ';
	}
	char* args = new char[progr_args.size() + 1];
	memcpy(args, progr_args.c_str(), progr_args.size() + 1);
	args[progr_args.size()] = '\0';

	SpawnJavaProcess(args);
	putchar('\n');
}
