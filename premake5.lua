workspace "VulkanEngine"
    architecture "x64"
    configurations {
        "Debug",
        "Release"
    }
    includedirs {
        "Libraries/include"
    }
    libdirs {
        "Libraries/lib"
    }
    linkoptions {
    }

outputdir = "%{cfg.buildcfg}-%{cfg.architecture}"
vulkan_sdk = "D:/VulkanSDK"

project "AssetLib"
    location "AssetLib"
    kind "StaticLib"
    language "C++"
    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("obj/" .. outputdir .. "/%{prj.name}")
    
    files {
        "%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp"
    }
    
    links {
        "lz4"
    }
    
    filter "system:windows"
        cppdialect "C++17"
		systemversion "latest"
        
    filter "configurations:Debug"
        defines "DEBUG"
        symbols "On"
        
    filter "configurations:Release"
        defines "NDEBUG"
        optimize "On"
        flags {"LinkTimeOptimization"}

project "AssetConverter"
    location "AssetConverter"
    kind "ConsoleApp"
    language "C++"
    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("obj/" .. outputdir .. "/%{prj.name}")
    
    files {
        "%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp"
    }
    includedirs {
        "Libraries/include/vulkan",
        "Libraries/include/json",
        "Libraries/include/stb",
        "AssetLib/src"
    }
    
    links {
        "AssetLib"
    }
    
    filter "system:windows"
        cppdialect "C++17"
		systemversion "latest"
        
    filter "configurations:Debug"
        defines "DEBUG"
        symbols "On"
        
    filter "configurations:Release"
        defines "NDEBUG"
        optimize "On"
        flags {"LinkTimeOptimization"}
        
project "VulkanEngine"
    location "VulkanEngine"
    kind "ConsoleApp"
    language "C++"
    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("obj/" .. outputdir .. "/%{prj.name}")
    
    files {
        "%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp",
        "%{prj.name}/Shaders/**.vert",
        "%{prj.name}/Shaders/**.frag",
        "Libraries/include/imgui/**.cpp",
        "Libraries/include/fmt/**.cc",
        "Libraries/include/spirv_reflect/**.c"
    }
    includedirs {
        "Libraries/include/vulkan",
        "AssetLib/src",
        "VulkanEngine/src",
        "VulkanEngine/src/Renderer"
    }
    
    links {
        "AssetLib",
        "glfw3",
        "vulkan-1"
    }
    
    filter "system:windows"
        cppdialect "C++17"
		systemversion "latest"
        defines {
            "_WIN32"
        }
		postbuildcommands {
			("for %%f in (%{prj.location}Shaders\\*.vert %{prj.location}Shaders\\*.frag) do " .. vulkan_sdk .. "/Bin/glslangValidator.exe -V -o %%f.spv %%f")
		}
        
    filter "configurations:Debug"
        defines "DEBUG"
        symbols "On"
        
    filter "configurations:Release"
        defines "NDEBUG"
        optimize "On"
        flags {"LinkTimeOptimization"}
        