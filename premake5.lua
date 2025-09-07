include "./vendor/premake/premake_customization/solution_items.lua"
include "Dependencies.lua"

workspace "NomTwitchBot"
	architecture "x86_64"
	startproject "NomTwitchBot"
	
	configurations{
		"Debug",
		"Release",
		"Dist"
	}

	solution_items {
		".editorconfig"
	}

	flags
	{
		"MultiProcessorCompile"
	}


outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

group "Dependencies"
	include "BotCore/Vendor/ImGui"
group ""

group "Core"
	include "BotCore"
group ""

group "Tools"
    include "NomTwitchBot"
group ""

group "Misc"
group ""