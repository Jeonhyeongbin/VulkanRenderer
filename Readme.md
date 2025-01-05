# VulkanRenderer
- vulkan api를 사용한 미니 렌더러입니다.

# 세팅
- vulkanSDK(https://vulkan.lunarg.com/sdk/home#windows)를 다운받아 설치 해주세요.
- 그런 후 Project1/ShaderCompile.bat를 실행하여 쉐이더를 한번 컴파일해 줘야합니다.
- vs2022에서만 빌드 및 실행이 가능합니다.

# Entry Point
- Project1/Main.cpp
```C++
int main()
{
try {
	// swapchain, framebuffer, color, depth attachment are to fixed with window size
	// every time window resize, you must create new swapcahin and others...
	jhb::JHBApplication app;
	app.Run();
}
}
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

