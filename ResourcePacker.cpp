// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

#include <iostream>
#include <string>
#include <fstream>
#include <Windows.h>
#include <zip.h>
#include "resource.h"

// usual and unusual cleanup state defines
#define CLEANUP_USUAL 0
#define CLEANUP_UNUSUAL 1


constexpr std::string_view archive_name{"java.zip"};

//global StdOut buffer handle
HANDLE hConsole;

std::pair<char*, DWORD> get_resource(const int resource_name, const int resource_type)
{
	std::pair<char*, DWORD> final_data;
	HMODULE h_module = nullptr;
	GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, reinterpret_cast<LPCTSTR>(get_resource), &h_module);

	HRSRC h_resource = FindResource(h_module, MAKEINTRESOURCE(resource_name), MAKEINTRESOURCE(resource_type));
	if (h_resource == nullptr)
	{
		throw std::runtime_error("Error while finding resource: " + std::to_string(GetLastError()));
	}

	const HGLOBAL h_pre_data = LoadResource(h_module, h_resource);
	if (h_pre_data == nullptr) return {nullptr, 0};

	final_data.second = SizeofResource(h_module, h_resource);
	final_data.first = static_cast<char*>(LockResource(h_pre_data));
	return final_data;
}

void LogInfo(std::ostream& ostr, const std::string_view& str)
{
	ostr << "[";
	SetConsoleTextAttribute(hConsole, FOREGROUND_BLUE);
	ostr << "INFO";
	SetConsoleTextAttribute(hConsole, 15);
	ostr << "] " << str;
}

void LogWarning(std::ostream& ostr, const std::string_view& str)
{
	ostr << "[";
	SetConsoleTextAttribute(hConsole, 14 | FOREGROUND_INTENSITY);
	ostr << "WARN";
	SetConsoleTextAttribute(hConsole, 15);
	ostr << "] " << str;
}

void LogError(std::ostream& ostr, const std::string_view& str)
{
	ostr << "[";
	SetConsoleTextAttribute(hConsole, FOREGROUND_RED);
	ostr << "ERROR";
	SetConsoleTextAttribute(hConsole, 15);
	ostr << "] " << str;
}

bool ExtractFile(const std::string_view file_name)
{
	// Open the zip file and check for errors
	int err{};
	zip_t* zip = zip_open(file_name.data(), ZIP_RDONLY, &err);

	// Check for errors
	if (err != 0)
	{
		zip_close(zip);
		throw std::runtime_error("Failed to open zip file with error: " + std::to_string(err) + "\n");
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
	
	zip_discard(zip);
	return EXIT_SUCCESS;
}

BOOL SpawnJavaProcess(char* args)
{
	STARTUPINFOA startupInfo{};
	PROCESS_INFORMATION processInfo{};

	ZeroMemory(&startupInfo, sizeof startupInfo);
	startupInfo.cb = sizeof startupInfo;
	ZeroMemory(&processInfo, sizeof processInfo);

	const BOOL result = CreateProcessA(
		"jdk\\bin\\java.exe",
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
	// Check if successful
	if (result == 0) throw std::runtime_error("Failed to spawn java process with error: " + std::to_string(GetLastError()) + ".\n");

	LogInfo(std::clog, "Successfully spawned java process. Waiting until it finishes...\n");

	// use processInfo to wait for the process to finish
	WaitForSingleObject(processInfo.hProcess, INFINITE);

	CloseHandle(processInfo.hProcess);
	CloseHandle(processInfo.hThread);
	return EXIT_SUCCESS;
}

BOOL SetJDKAttributes(const DWORD& attributes)
{
	if (SetFileAttributesA("jdk", attributes) == 0)
	{
		LogWarning(std::clog, "Failed to set JDK attributes.\n");
		return FALSE;
	}
	return TRUE;
}

/*BOOL SetJAVAHOME(const char* path)
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
}*/

std::string FindJar()
{
	WIN32_FIND_DATAA find_data{};
	HANDLE file_handle = FindFirstFileA("*.jar", &find_data);
	if (INVALID_HANDLE_VALUE == file_handle)
	{
		return {};
	}
	CloseHandle(file_handle);
	return find_data.cFileName;
}

void DumpJDK()
{
	std::ofstream java_file(archive_name.data(), std::ios_base::binary);
	const auto text = get_resource(DEMO_TEXT, IDR_TEXT_FILE1);
	java_file.write(text.first, text.second);
	java_file.close();
}

void Cleanup(const unsigned int& state)
{
	if (state == CLEANUP_UNUSUAL && GetFileAttributesA(archive_name.data()) != INVALID_FILE_ATTRIBUTES)
	{
		if (DeleteFileA(archive_name.data()) == 0) LogWarning(std::clog, "Partial cleanup: java archive was not found. (" + std::to_string(GetLastError()) + ")\n");
	}

	if (GetFileAttributesA("jdk") != INVALID_FILE_ATTRIBUTES)
	{
		SHFILEOPSTRUCTA file_op{};

		file_op.wFunc = FO_DELETE;
		file_op.pFrom = "jdk\0";
		file_op.fFlags = FOF_SILENT | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_NOCONFIRMMKDIR;


		const DWORD error = SHFileOperationA(&file_op);
		if (error != 0)
		{
			LogWarning(std::clog, "Encountered an error while deleting the JDK: " + std::to_string(error) + ".\n");
			SetJDKAttributes(FILE_ATTRIBUTE_NORMAL);
		}
	}
	else LogWarning(std::clog, "Partial cleanup: JDK was not found.\n");
}


int main(int argc, char* argv[])
{
	try
	{
		hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

		//print out all arguments
		for (int i{}; i < argc; ++i)
		{
			std::cout << argv[i] << '\n';
		}

		// Check if the jdk folder exists
		if (GetFileAttributesA("jdk") == INVALID_FILE_ATTRIBUTES)
		{
			DumpJDK(); 
			if (!ExtractFile(archive_name)) LogInfo(std::clog, "Successfully extracted the portable java runtime.\n");
			if (DeleteFileA(archive_name.data()) == 0) LogWarning(std::clog, "Error deleting java.zip: " + std::to_string(GetLastError()) + '\n');
		}

		SetJDKAttributes(FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);

		//std::string progr_args{ " -jar " };
		std::string progr_args{" -Xmx2048M -Xms512M -Dspring.profiles.active=dev -Dspring.config.name=application,environment -Dspring.config.additional-location=config\\ -Dlogging.config=config\\logback.xml -jar "};

		const std::string jar_name = FindJar();

		if (jar_name.empty()) throw std::runtime_error("Failed to find jar file.\n");

		progr_args += jar_name + " ";

		if (argc >= 2)
		{
			const std::string tmp = argv[1];
			progr_args += tmp.substr(1, std::string::npos) + " ";
		}

		for (int i = 2; i < argc; ++i)
		{
			progr_args += argv[i];
			progr_args += ' ';
		}

		const auto args = new char[progr_args.size()]{};
		std::ranges::copy(progr_args, args);
		args[progr_args.size()] = '\0';

		LogInfo(std::clog, "Attempting to launch " + jar_name + "...\n");

		//spawns the java process and waits for it to finish
		SpawnJavaProcess(args);

		LogInfo(std::clog, "Detected thread exit signal. Cleaning up and exiting...\n");

		Cleanup(CLEANUP_USUAL);
	}
	catch (const std::exception& e)
	{
		LogError(std::clog, e.what());
		LogInfo(std::clog, "Cleaning up and exiting...\n");
		Cleanup(CLEANUP_UNUSUAL);
	}
}
