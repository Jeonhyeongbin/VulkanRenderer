# VulkanRenderer
- vulkan api를 사용한 미니 렌더러입니다.


# Feature
```
![title](Images/image.png)
```
- Instancing
- PBR


## 구조

```
---
title: Animal example
---
classDiagram
    BaseRenderSystem-- PBRRenderSystem
    BaseRenderSystem-- PointLightRenderSystem
    BaseRenderSystem-- SkyBoxRenderSystem
   
    class BaseRenderSystem{
          +pipelinelayoutCreate()
          +virtual pipelineCreate() abstract
    }
    class PBRRenderSystem{
        pbr.vert
        pbr.frag 
RenderGameObject()
    }
    class PointLightRenderSystem{
        pointLight.vert
        pointLight.frag
RenderPointLight()
    }
    class SkyBoxRenderSystem{
        skybox.vert
        skybox.frag
        RenderSky()
    }
    class Renderer{
  +std::unique_ptr<SwapChain> 
  +std::vector<CommandBuffer> 
    }
PBRRenderSystem--Renderer
PointLightRenderSystem--Renderer
SkyBoxRenderSystem--Renderer


```
