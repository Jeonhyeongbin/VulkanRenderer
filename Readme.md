# Vulkan PBR Renderer

Android용 Vulkan 기반 Deferred PBR 렌더러입니다.

- Deferred Rendering (2-Subpass, G-Buffer)
- PBR (Physically Based Rendering) + IBL
- Sponza 씬 + DamagedHelmet 모델
- KTX 큐브맵 스카이박스
- Shadow Map
- MSAA 4x

---

## Android 빌드 및 실행

### 요구 사항

| 항목 | 버전 |
|---|---|
| Android Studio | Hedgehog (2023.1.1) 이상 |
| Android NDK | r25c 이상 |
| CMake | 3.22.1 이상 |
| 대상 기기 | Android 7.0 (API 24) 이상, **Vulkan 1.0 지원 필수** |
| 아키텍처 | arm64-v8a |

> Vulkan 지원 여부 확인: [Vulkan Caps Viewer](https://play.google.com/store/apps/details?id=de.saschawillems.vulkancapsviewer) 앱으로 확인

---

### 1단계 — 저장소 클론

```bash
git clone https://github.com/<your-username>/VulkanRenderer.git
cd VulkanRenderer
```

---

### 2단계 — Android Studio에서 열기

1. Android Studio 실행
2. **File → Open** 선택
3. 클론한 폴더 안의 **`android/`** 폴더를 선택하여 열기
   (`VulkanRenderer/android/` — `build.gradle`이 있는 폴더)

---

### 3단계 — NDK & CMake 설치

Android Studio 상단 메뉴:

```
Tools → SDK Manager → SDK Tools 탭
```

아래 항목을 체크하고 **Apply**:

- [x] **NDK (Side by side)** — r25c 이상
- [x] **CMake** — 3.22.1 이상

---

### 4단계 — 기기 연결 및 개발자 모드 활성화

1. 핸드폰 **설정 → 휴대전화 정보 → 빌드 번호** 를 7번 탭 → 개발자 모드 활성화
2. **설정 → 개발자 옵션 → USB 디버깅** 켜기
3. USB 케이블로 PC와 연결
4. 핸드폰에서 **"USB 디버깅 허용"** 팝업 → 허용

Android Studio 상단 기기 선택 드롭다운에 연결된 기기가 표시되면 준비 완료.

---

### 5단계 — 빌드 & 설치

Android Studio 상단의 **Run** 버튼 클릭 (또는 `Shift + F10`)

> 첫 빌드 시 Gradle이 CMake로 C++ 소스를 컴파일하므로 **5~10분** 소요될 수 있습니다.

빌드가 완료되면 APK가 자동으로 기기에 설치되고 앱이 실행됩니다.

---

### 조작 방법

| 제스처 | 동작 |
|---|---|
| 오른쪽 손가락 드래그 | 카메라 회전 |
| 왼쪽 손가락 드래그 | 카메라 이동 (전후좌우) |
| 두 손가락 핀치 | 줌 인/아웃 |

---

### 트러블슈팅

**`local.properties` 파일이 없다는 에러**
Android Studio가 자동으로 생성합니다. SDK 경로를 직접 지정하려면:
```
# android/local.properties 직접 생성
sdk.dir=C\:\\Users\\<사용자명>\\AppData\\Local\\Android\\Sdk   # Windows
sdk.dir=/Users/<사용자명>/Library/Android/sdk                   # macOS
```

**NDK를 찾을 수 없다는 에러**
`Tools → SDK Manager → SDK Tools`에서 NDK 설치 여부 확인

**기기가 인식되지 않음**
USB 드라이버 재설치 또는 케이블 교체, USB 디버깅 재확인

**앱 설치 후 즉시 크래시**
기기가 Vulkan을 지원하지 않을 수 있습니다. Vulkan Caps Viewer로 지원 여부 확인

---

## 프로젝트 구조

```
VulkanRenderer/
├── Project1/                   # C++ 소스 코드
│   ├── *.cpp / *.h             # 렌더러 소스
│   ├── shaders/                # GLSL 소스 + 컴파일된 SPV
│   └── External/               # 서드파티 라이브러리 (GLM, ImGui, tinygltf 등)
│
└── android/                    # Android 프로젝트
    ├── app/
    │   ├── build.gradle        # 빌드 설정 (minSdk 24, compileSdk 34)
    │   └── src/main/
    │       ├── AndroidManifest.xml
    │       ├── CMakeLists.txt  # NDK CMake 빌드 스크립트
    │       ├── assets/         # 모델, 텍스처, 셰이더 SPV
    │       └── jniLibs/        # libktx.so (arm64-v8a, x86_64)
    ├── build.gradle
    └── settings.gradle
```

---

## 사용 라이브러리

| 라이브러리 | 용도 |
|---|---|
| [Vulkan](https://www.vulkan.org/) | 그래픽스 API |
| [GLM](https://github.com/g-truc/glm) | 수학 라이브러리 |
| [tinygltf](https://github.com/syoyo/tinygltf) | GLTF 모델 로딩 |
| [KTX-Software](https://github.com/KhronosGroup/KTX-Software) | KTX 텍스처 로딩 |
| [Dear ImGui](https://github.com/ocornut/imgui) | GUI (PC 전용) |
| [stb_image](https://github.com/nothings/stb) | 이미지 디코딩 |
