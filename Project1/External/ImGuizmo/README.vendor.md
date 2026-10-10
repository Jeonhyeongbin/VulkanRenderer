# ImGuizmo

- Upstream: https://github.com/CedricGuillemet/ImGuizmo
- Version: 1.83
- Commit: `664cf2d73864a36b2a8b5091d33fc4578c885eca`
- Files: ImGuizmo.h, ImGuizmo.cpp, LICENSE (MIT).
- Only the rotation widget is connected by this application; no view cube, sequencer or other widgets are used.
- Sources are vendored, so normal builds do not download dependencies.
- CMake defines IMGUI_DEFINE_MATH_OPERATORS for ImGuizmo.cpp before imgui.h is included, as required by the project's Dear ImGui 1.90.5. Upstream source is otherwise unchanged.
