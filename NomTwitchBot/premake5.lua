project "NomTwitchBot"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++17"
	staticruntime "off"
	
	targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

	files {
		"src/**.h",
		"src/**.cpp",
	}

	includedirs{
		"%{wks.location}/BotCore/src",
		"%{IncludeDir.OpenSSL}",
		"%{IncludeDir.ImGUI}",
	}

	libdirs {
		"%{LibraryDir.OpenSSL}",
	}

	links {
		"BotCore",
	}

	filter "system:windows"
		systemversion "latest"
		system "windows"
		links {
			"ImGUI",
			"%{Library.D3D11}",
			"%{Library.DXGI}",
		}

	filter "configurations:Debug"
		defines {"NOM_DEBUG","NOM_ENABLE_ASSERTS"}
		runtime "Debug"
		symbols "on"
		links {
			"%{Library.OpenSSLDebug}",
			"%{Library.OpenSSLcryptoDebug}",
		}

	filter "configurations:Release"
		defines "NOM_RELEASE"
		runtime "Release"
		optimize "on"
		links {
			"%{Library.OpenSSL}",
			"%{Library.OpenSSLcrypto}",
		}

	filter "configurations:Dist"
		defines "NOM_DIST"
		runtime "Release"
		optimize "on"
		links {
			"%{Library.OpenSSL}",
			"%{Library.OpenSSLcrypto}",
		}