-- game-thick-cube build script. Shared logic lives in the ely-arcade-sdk submodule (sdk/).
dofile("../sdk/premake/ely_sdk.lua")

ely.prepare_dirs()
ely.workspace("game-thick-cube")
ely.raylib_project()
ely.sdk_project("../sdk")
ely.app_project("game-thick-cube", "../src", "../sdk")
