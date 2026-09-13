# Skybox pass

`SkyboxPass` replaces `SkyBoxRenderSystem`. Like `PBRPass`, it only declares
shaders, resources and a draw callback. It owns no Device, pipeline, descriptor,
model or output image. Existing `skybox.vert/frag` are the canonical shaders.

## Composition

Create a cube once with `scene.createCube()`; do not attach a legacy
`noTexturePipeline` to this cube. Import the environment texture with a cube image
view, sampler and its actual previous access state. Keep these resources alive
until GPU execution completes.

```cpp
// After the current frame slot's fence completes:
graph.clear();
pbr.addPass(graph, pbrResources, drawScene);
skybox.addPass(graph, {environment, pbrResources.color, pbrResources.depth},
    [cube](VkCommandBuffer cmd, const rg::RenderPass& pass,
           const rg::GraphResources& resources)
    {
        cube->bind(cmd);
        // Scene::createCube has no material nodes or legacy pipeline.
        cube->draw(cmd, resources.pipeline(pass.pipelineHandle).layout, 0);
    });
// Add presentation after skybox, then prepare/render the graph.
```

The caller supplies `context.viewDescriptorSets` (set 1, GlobalUbo), as for PBR.
The graph reflects the shader interface and supplies the environment through
`.read()` (set 2, binding 0), including its sampled-image barrier.
Do not bind a separate pipeline or environment descriptor in the callback.

Color and depth use LOAD, not CLEAR. With conventional depth (clear 1, compare
LESS_OR_EQUAL), `clip.xyww` puts the cube at depth 1, filling only background.
Depth testing is enabled and depth writes are disabled. Reversed-Z is not supported
by this shader. Opaque geometry must initialize both attachments first.
The graph uses read/write attachment synchronization for depth, conservatively
covering depth test, load and store even for a pipeline with depth writes disabled.

On resize, wait for GPU usage to finish and recreate the frame graph/images with
the new extent, then pass the new IDs to the same stateless SkyboxPass.

## Migration status

The old class and build references are removed. Application construction now uses
SkyboxPass; the complete application frame composition is still pending alongside
the remaining legacy render systems. Tests cover PBR + skybox declarations,
attachment load operations, cache reuse, vk_sync masks and real shader reflection.
This does not constitute a full application build or GPU visual verification.
