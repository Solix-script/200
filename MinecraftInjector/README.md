# MinecraftInjector

This Visual Studio project builds a DLL that can be injected into Minecraft Java Edition (1.21.4) to display an overlay.

## Setup

1. **Clone Dependencies**  
   - [MinHook](https://github.com/TsudaKageyu/minhook) → `Dependencies/MinHook`  
   - [ImGui](https://github.com/ocornut/imgui) → `Dependencies/ImGui`

2. **Open Project**  
   Launch `MinecraftInjector.vcxproj` in Visual Studio (x64, Debug/Release).

3. **Build**  
   Build the DLL. Output will be in `Bin\<Configuration>\`.

4. **Inject**  
   Use your preferred DLL injector to load `MinecraftInjector.dll` into the Minecraft process.

Enjoy your overlay!