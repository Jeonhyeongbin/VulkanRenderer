# VulkanRenderer
- vulkan api를 사용한 미니 렌더러입니다.

# 세팅
## 필수 환경

- Windows 10/11 (64-bit)
- Visual Studio 2022 (C++ 데스크톱 개발 워크로드 포함)
- [Vulkan SDK](https://vulkan.lunarg.com/sdk/home)
- [CMake 3.15 이상](https://cmake.org/download/)

---

## 빌드 방법

```bash
cd Project1
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Debug
```

빌드 결과물은 `Project1/build/bin/Debug/` 에 생성됩니다.

---

## 실행 방법

exe를 더블클릭하지 말고 **반드시 Project1 폴더에서 실행**해야 합니다.

```bash
cd Project1
.\build\bin\Debug\VulkanRenderer.exe
```



# Feature
![실행 결과](./image.png)
- Instancing
- PBR-IBL
- Mouse-Picking
- Deferred-rendering (using subpass)
- Omnidirectional-shadowMapping
- KTX(Khronos Texture) texture storage Format을 사용하였습니다.
- gtlf 2.0 format with tinygltf [tinygltf](https://github.com/syoyo/tinygltf).

# InProgress
- GPUT based Culling and LOD
- SSAO


# 조작법
- 마우스로 회전 및 wasd키로 이동 및 마우스휠로 줌인 줌아웃.
- 헬멧을 클릭하여 회전을 할 수 있습니다 

