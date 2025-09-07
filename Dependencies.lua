IncludeDir = {}
IncludeDir["OpenSSL"] = "%{wks.location}/BotCore/vendor/OpenSSL/include"
IncludeDir["ImGUI"] = "%{wks.location}/BotCore/vendor/ImGUI"

LibraryDir = {}
LibraryDir["OpenSSL"] = "%{wks.location}/BotCore/vendor/OpenSSL/lib"

Library = {}

-- Windows
Library["WinSock"] = "Ws2_32.lib"
Library["D3D11"] = "d3d11.lib"
Library["DXGI"] = "dxgi.lib"
-- OpenSSL
Library["OpenSSL"] = LibraryDir["OpenSSL"] .. "/VC/x64/MD/libssl.lib"
Library["OpenSSLcrypto"] = LibraryDir["OpenSSL"] .. "/VC/x64/MD/libcrypto.lib"
-- OpenSSL Debug
Library["OpenSSLDebug"] = LibraryDir["OpenSSL"] .. "/VC/x64/MDd/libssl.lib"
Library["OpenSSLcryptoDebug"] = LibraryDir["OpenSSL"] .. "/VC/x64/MDd/libcrypto.lib"