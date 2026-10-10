# 선택 인스턴스 회전 기즈모

## 범위와 사용법

Windows의 기존 ImGui 패스에 ImGuizmo 회전 기즈모를 연결했다. 헬멧을 클릭해 선택하고 회전 고리를 왼쪽 버튼으로 드래그한다. settings 창의 Local/World로 축 기준을 고른다. 이동/크기 기즈모, 임의 pivot 편집, undo/redo, 저장, Android 터치 기즈모는 추가하지 않았다. Sponza는 기존처럼 선택할 수 없다.

기존 좌클릭 일반 드래그 회전과 앞선 worldYaw/worldPivot 전용 처리는 제거했다. 프레임별 인스턴스 버퍼와 펜스, 디스크립터 재사용, 카메라의 기존 이동 키/오른쪽 버튼 동작은 유지했다. 기즈모 조작 중에는 오른쪽 마우스 카메라 회전이 겹치지 않게 했다.

이 기록은 작성 시점의 스냅샷이다. 앞선 review-4-5-6 문서의 worldYaw 설명은 이제 과거 구현이다.

## 의존성과 빌드

공식 ImGuizmo 1.83의 ImGuizmo.h/.cpp 및 MIT LICENSE를 External/ImGuizmo에 포함했다. 커밋은 `664cf2d73864a36b2a8b5091d33fc4578c885eca`다. 새 빌드마다 다운로드하지 않는다. CMakeLists.txt에 소스 한 개를 추가했다.

기존 ImGui 1.90.5는 imgui.h를 포함하기 전에 IMGUI_DEFINE_MATH_OPERATORS가 정의되어야 한다. ImGuizmo.cpp에만 CMake 컴파일 정의를 추가했다. 라이브러리 소스 자체를 수정하거나 ImGui를 업그레이드하지 않았다. Visual Studio 프로젝트 파일을 직접 열어 빌드하는 별도 경로는 갱신하지 않았으며 CMake 빌드가 대상이다.

## 이전 동작 → 문제 → 새 흐름

이전에는 recordFrame의 마우스 이동량을 Scene::rotateSelectedObject에 넘겨 각도를 더하고, 셰이더에서 pivot 기준 회전을 계산했다.

```cpp
// 이전 핵심
GlobalScene->rotateSelectedObject(mouseDeltaX * rotationRadiansPerPixel);
// Scene 내부
model->instanceData[instanceIndex].worldYaw += angleRadians;
```

```glsl
// 이전 셰이더 핵심
positionWorld = rotateAroundWorldPivot(positionWorld, worldPivot, worldYaw);
```

기즈모도 행렬을 계산하는데 각도와 pivot으로 다시 분해해 전달하면 상태와 계산이 중복된다. 그래서 기즈모가 수정할 인스턴스 행렬 하나를 실제 렌더링에도 사용한다.

전체 흐름:

1. Run → beginFrame: 현재 슬롯의 기존 펜스 대기.
2. consumePicking: GPU 피킹 결과를 받아 Scene 선택 갱신. 기존 비동기 지연은 유지.
3. recordFrame: ImGui 시작 → settings UI → 선택 행렬에 기즈모 조작 적용.
4. 기즈모 위가 아닌 장면 클릭만 새 피킹 요청으로 처리. 기즈모 클릭으로 선택을 해제하지 않는다.
5. Model::uploadInstances(frameIndex): 수정한 행렬을 현재 슬롯에 업로드.
6. graph.render: G-buffer/PBR/그림자/피킹은 모두 같은 인스턴스 행렬로 드로우.
7. 기존 ImGuiPass가 기즈모를 포함한 UI를 장면 위에 그린다. endFrame은 기존 방식으로 제출.

## 파일별 변경

### Model.h / Model.cpp

InstanceData의 pos/rot/미사용 scale 및 worldYaw/worldPivot 대신 다음을 둔다. roughness/metallic/r/g/b는 유지했다.

```cpp
glm::mat4 transform{1.0f};
```

이 행렬이 인스턴스의 월드 배치 기준이다. rootModelMatrix는 초기 배치 입력이고, updateInstances에서 다음처럼 초기 행렬로 반영한다.

```cpp
auto transform = glm::translate(rootModelMatrix, positions[i]);
transform = glm::rotate(transform, rotations[i].x, glm::vec3(1, 0, 0));
transform = glm::rotate(transform, rotations[i].y, glm::vec3(0, 1, 0));
instanceData[i].transform = glm::rotate(transform, rotations[i].z, glm::vec3(0, 0, 1));
```

현재 Scene의 초기 회전 입력은 모두 0이라 헬멧 자세가 달라지지 않는다. Sponza의 rootModelMatrix 이동도 초기 행렬에 포함되어 유지된다. 초기화 API의 비영 회전은 이제 이름대로 X/Y/Z 라디안 회전이며, 옛 GLSL의 rot.x=Z라는 비표준 대응은 쓰지 않는다. updateInstances를 다시 호출하면 해당 인스턴스 변환을 초기 입력으로 재설정한다. rootModelMatrix만 사후 변경해도 이미 생성된 인스턴스 행렬이 자동 변경되지는 않는다.

drawGraphGeometry / drawGraphAlphaGeometry의 glTF 노드 순회는 단위행렬에서 시작하도록 바꿨다. rootModelMatrix를 인스턴스 행렬과 push.model 양쪽에 두 번 적용하지 않기 위해서다. push.model에는 glTF 노드 계층 변환만 남는다. 큐브 등 기본 bind()/draw() 경로는 변경하지 않았다.

### Scene.h / Scene.cpp

rotateSelectedObject 대신 selectedTransform을 제공한다. 기존 selectedObject 검증을 통과한 오브젝트가 가리키는 모델에서 `objectId - firstid` 인스턴스를 구한다.

```cpp
const auto instanceIndex = *selected - model->firstid;
return &model->instanceData[instanceIndex].transform;
```

선택 없음/인덱스 오류는 nullptr를 반환한다. 이 포인터는 Scene이나 기즈모에 따로 저장하지 않고 그 프레임의 즉시 편집에서만 사용한다. 소유권은 Model의 instanceData에 남는다.

### JHBApplication.h / JHBApplication.cpp

기존 ImGui NewFrame 이후 ImGuizmo::BeginFrame을 호출한다. UI 설정값 gizmoLocal 하나만 추가했다. 이는 변환 상태 복사본이 아니라 사용자가 고른 축 모드다. 재시작하면 Local로 돌아간다.

```cpp
if (auto *transform = GlobalScene->selectedTransform())
{
    ImGuizmo::Manipulate(glm::value_ptr(window.getCamera()->getView()),
                         glm::value_ptr(window.getCamera()->getProjection()),
                         ImGuizmo::ROTATE,
                         gizmoLocal ? ImGuizmo::LOCAL : ImGuizmo::WORLD,
                         glm::value_ptr(*transform));
}
```

위는 설정/입력 가드 부분을 생략한 핵심 코드다. 인자는 카메라 view, projection, 회전 작업, 축 모드, 읽고 수정할 행렬 주소 순서다. 행렬을 각도로 분해하지 않는다. Local은 편집하는 인스턴스 좌표축이고 World는 고정된 월드 좌표축이다. glTF 내부 개별 노드의 축을 직접 편집하는 기능은 아니다.

기즈모는 ImGui background draw list에 그려서 settings 창이 그 위를 덮는다. SetRect에는 ImGui 논리 화면 크기를 전달하고 기존 framebuffer scale 처리는 ImGui 렌더러에 맡긴다. 장면은 음수 높이 Vulkan viewport를 사용하므로 위아래 화면 대응은 ImGuizmo와 같고 카메라 투영을 추가 반전하지 않는다.

ImGuizmo 자체도 hover/drag 때 WantCaptureMouse를 요청한다. 이를 그대로 기즈모 Enable 조건으로 쓰면 hover 다음 프레임에 자기 자신을 비활성화한다. 따라서 실제 ImGui 창 hover와 활성 UI 항목으로 입력 충돌을 구분한다. 이미 기즈모를 잡았다면 드래그를 계속 처리하고, IsOver/IsUsing 동안 새 피킹을 요청하지 않는다. 오른쪽 카메라 드래그나 리사이즈 때는 기즈모 편집을 비활성화한다.

### PipelineDesc.cpp / 관련 셰이더

GLSL mat4는 vec4 열 네 개를 차지한다. binding 1의 location 5,6,7,8을 transform의 네 열로 연결한다. 각 열 offset은 offsetof(transform)+열번호*sizeof(glm::vec4)이며 stride는 sizeof(InstanceData)다. 재질 입력은 location 9~13으로 옮겼다. binding 0의 기본 정점 입력 0~4는 그대로다.

G-buffer/PBR/그림자/피킹의 핵심 변환은 모두 같다.

```glsl
layout(location = 5) in mat4 instanceTransform;
// push.model은 glTF 노드 계층 변환
vec4 worldPosition = instanceTransform * push.model * vec4(position, 1.0);
```

노멀/탄젠트에도 같은 결합 행렬을 사용한다. 기존 노멀 계산 방식 자체는 유지했다. worldYaw/worldPivot 및 rotateWorldY/rotateAroundWorldPivot은 제거했다. instanceRotation.glsl의 옛 로컬 회전 함수는 이번 active 경로에서 더 이상 사용하지 않지만 기존 파일/빌드 의존성은 남겼다. 사용하지 않는 과거 deferedoffscreen.vert 등은 새 인스턴스 ABI로 이식하지 않았다.

## 검증과 제한

- Windows CMake Debug 빌드 및 셰이더 컴파일 성공. 변경된 네 SPIR-V 파일도 갱신했다. 기존 CMake/project 및 컴파일·링크 경고는 별개로 남는다.
- Android NDK aarch64-linux-android24: Model.cpp, Scene.cpp, PipelineDesc.cpp, JHBApplication.cpp 문법·타입 검사 성공. Android 기즈모 UI 추가나 APK/기기 실행은 아니다.
- build/gizmo-check의 임시 비GPU 검사: 실제 ImGui/ImGuizmo와 이 프로젝트의 LH/0..1 projection을 사용해 회전 고리 hover→누르기→이동→해제를 입력했다. Local/World 모두 행렬 회전 변경, translation 보존, 버튼 해제 후 IsUsing 해제를 확인했다. 이 검사는 제품 실행 옵션이나 정식 빌드 타겟에 추가하지 않았다.
- 임시 검사에서 hover 캡처 충돌을 재현하고 애플리케이션 가드를 수정한 뒤 통과했다.
- Y축 고리를 정확히 옆면에서 보는 카메라 조건은 이동 입력에 회전값이 변하지 않는 경우가 재현되었다. 비스듬한 시점에서는 Local/World Y축 검사 모두 통과했다. 해당 경우 카메라를 위/옆으로 이동해야 하며 라이브러리의 특수 보정은 추가하지 않았다. 기본 카메라/모델 원점 높이가 같으므로 Y 고리를 잡기 어렵다면 시점을 바꿔야 한다.
- 기존 GraphTests의 입력 항목 검사만 새 ABI(14개, mat4 열 5~8)에 맞췄다. 전체 GraphStructure 테스트는 이번 턴에 재실행하지 않았다. 이전 기록의 invalid commandBuffer 테스트 문제 해결을 주장하지 않는다.
- 실제 Vulkan 렌더러 창에서의 기즈모 표시, HiDPI, 회전 후 피킹/그림자 일치, 반복 리사이즈는 아직 검증하지 않았다.
- 회전만 제공한다. 중심은 인스턴스 원점이며 메시 무게중심 자동 계산은 없다. 장면 재로딩 시 초기 행렬로 돌아가며 편집 내용 영구 저장은 없다.
