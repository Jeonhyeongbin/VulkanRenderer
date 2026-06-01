#pragma once

#ifdef ANDROID
#include <android/asset_manager.h>

// tiny_gltf.h를 여기서 include하면 TINYGLTF_IMPLEMENTATION을 정의한 Model.cpp TU에서
// 구현부가 두 번 컴파일되는 문제가 생긴다. 전방 선언만 사용한다.
// setupGltfAndroidCallbacks()를 호출하는 쪽에서는 tiny_gltf.h가 먼저 포함되어 있어야 한다.
namespace tinygltf { class TinyGLTF; }

namespace jhb {

// AAssetManager 전역 설정 (Main.cpp에서 JHBApplication 생성 전에 호출)
void setAndroidAssetManager(AAssetManager* mgr);
AAssetManager* getAndroidAssetManager();

// tinygltf 컨텍스트에 Android AAssetManager 기반 파일 콜백 등록
// ※ 이 함수를 호출하는 .cpp 파일에서 AndroidAssets.h 보다 먼저 tiny_gltf.h를 include할 것
void setupGltfAndroidCallbacks(tinygltf::TinyGLTF& context);

} // namespace jhb
#endif // ANDROID
