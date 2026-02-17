#include <iostream>
#include <filesystem>
#include <string>
#include <cstdlib>
#include <vector>

namespace fs = std::filesystem;

int main(int argc, char* argv[])
{
	if (argc < 3 || argc > 4) {
		std::cerr << "Usage: " << argv[0] << " <input> <-dxbc | -dxil | -spirv> [output.hlsl]\n";
		return 1;
	}

	std::string mode = argv[2];
	if (mode != "-dxbc" && mode != "-dxil" && mode != "-spirv") {
		std::cerr << "Error: Invalid mode. Use -dxbc, -dxil or -spirv\n";
		return 1;
	}

	fs::path exePath = fs::absolute(argv[0]).parent_path();
	fs::path dxbc2dxil = exePath / "dxbc2dxil.exe";
	fs::path dxilSpirv = exePath / "dxil-spirv.exe";
	fs::path spirvCross = exePath / "spirv-cross.exe";

	std::vector<std::string> missingTools;
	if (mode == "-dxbc" && !fs::exists(dxbc2dxil))
		missingTools.push_back("dxbc2dxil.exe");
	if ((mode == "-dxbc" || mode == "-dxil") && !fs::exists(dxilSpirv))
		missingTools.push_back("dxil-spirv.exe");
	if (!fs::exists(spirvCross))
		missingTools.push_back("spirv-cross.exe");

	if (!missingTools.empty()) {
		std::cerr << "Error: Required tools not found in executable directory:\n";
		for (const auto& tool : missingTools) {
			std::cerr << "  - " << tool << "\n";
		}
		return 1;
	}

	fs::path inputFile = fs::absolute(argv[1]);
	fs::path outputFile;

	if (argc > 3) {
		outputFile = fs::absolute(argv[3]);
	}
	else {
		outputFile = inputFile;
		outputFile.replace_extension(".hlsl");
	}

	// chdir should be called after fs::absolute
	fs::current_path(exePath);

	try {
		int result = 0;
		fs::path tempDxil;
		fs::path tempSpv;

		if (mode == "-dxbc") {
			// DXBC -> DXIL -> SPIR-V -> HLSL
			tempDxil = inputFile;
			tempDxil.replace_extension(".dxil");

			tempSpv = inputFile;
			tempSpv.replace_extension(".spv");

			// Step 1: DXBC -> DXIL
			std::string cmd1 = "cmd /c \"\"dxbc2dxil.exe\" \"" + inputFile.string() +
				"\" -o \"" + tempDxil.string() +
				"\" -emit-bc\"";
			result = std::system(cmd1.c_str());
			if (result != 0) {
				std::cerr << "dxbc2dxil failed with exit code: " << result << std::endl;
				return 1;
			}

			// Step 2: DXIL -> SPIR-V
			std::string cmd2 = "cmd /c \"\"dxil-spirv.exe\" \"" + tempDxil.string() +
				"\" --output \"" + tempSpv.string() + 
				"\" --raw-llvm\"";
			result = std::system(cmd2.c_str());
			if (result != 0) {
				std::cerr << "dxil-spirv failed with exit code: " << result << std::endl;
				fs::remove(tempDxil);
				return 1;
			}

			// Step 3: SPIR-V -> HLSL
			std::string cmd3 = "cmd /c \"\"spirv-cross.exe\" \"" + tempSpv.string() +
				"\" --output \"" + outputFile.string() +
				"\" --hlsl --shader-model 51\"";
			result = std::system(cmd3.c_str());
			if (result != 0) {
				std::cerr << "spirv-cross failed with exit code: " << result << std::endl;
			}

			fs::remove(tempDxil);
			fs::remove(tempSpv);
		}
		else if (mode == "-dxil") {
			// DXIL -> SPIR-V -> HLSL
			tempSpv = inputFile;
			tempSpv.replace_extension(".spv");

			// Step 1: DXIL -> SPIR-V
			std::string cmd1 = "cmd /c \"\"dxil-spirv.exe\" \"" + inputFile.string() +
				"\" --output \"" + tempSpv.string() + "\"\"";
			result = std::system(cmd1.c_str());
			if (result != 0) {
				std::cerr << "dxil-spirv failed with exit code: " << result << std::endl;
				return 1;
			}

			// Step 2: SPIR-V -> HLSL
			std::string cmd2 = "cmd /c \"\"spirv-cross.exe\" \"" + tempSpv.string() +
				"\" --output \"" + outputFile.string() +
				"\" --hlsl --shader-model 60\"";
			result = std::system(cmd2.c_str());
			if (result != 0) {
				std::cerr << "spirv-cross failed with exit code: " << result << std::endl;
			}

			fs::remove(tempSpv);
		}
		else if (mode == "-spirv") {
			// SPIR-V -> HLSL
			std::string cmd = "cmd /c \"\"spirv-cross.exe\" \"" + inputFile.string() +
				"\" --output \"" + outputFile.string() +
				"\" --hlsl --shader-model 60\"";
			result = std::system(cmd.c_str());
			if (result != 0) {
				std::cerr << "spirv-cross failed with exit code: " << result << std::endl;
				return 1;
			}
		}

		if (result == 0) {
			return 0;
		}
		return 1;
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
}