project "BotCore"
	kind "StaticLib"
	language "C++"
	cppdialect "C++17"
	staticruntime "off"
	
	targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

	pchheader "nompch.h"
	pchsource "src/nompch.cpp"

	files {
		"src/**.h",
		"src/**.cpp",
	}

	defines {
	}

	includedirs{
		"src",
		"%{IncludeDir.OpenSSL}",
		"%{IncludeDir.ImGUI}",
	}

	libdirs {
		"%{LibraryDir.OpenSSL}",
	}

	links {
		"%{Library.D3D11}",
	}
	
	flags {"NoPCH"}

	filter "system:windows"
		systemversion "latest"

		defines {
            "NOM_PLATFORM_WINDOWS",
		}

		links {
            "%{Library.WinSock}",
			"%{Library.D3D11}",
			"%{Library.DXGI}",
		}

	filter "configurations:Debug"
		defines "NOM_DEBUG"
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