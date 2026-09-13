# Shadow pass

`ShadowPass` replaces `ShadowRenderSystem`. It declares six graph passes, in cube
layer order +X, -X, +Y, -Y, +Z, -Z. It retains the existing
`shadowOffscreen.vert/frag` names and R32F world-space distance representation.
It does not own images, pipelines, render passes, framebuffers or descriptor pools.

## Resources and composition

Create these resources outside the pass, once per in-flight frame resource set:

```cpp
rg::ImageDesc cubeDesc;
cubeDesc.width = cubeDesc.height = 1024;
cubeDesc.imageType = rg::ImageType::CubeMap;
cubeDesc.arrayLayers = 6;
cubeDesc.format = VK_FORMAT_R32_SFLOAT;
cubeDesc.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
auto cube = graph.createTexture("shadow/cube", cubeDesc);

rg::ImageDesc depthDesc;
depthDesc.width = depthDesc.height = 1024;
depthDesc.format = VK_FORMAT_D32_SFLOAT;
depthDesc.aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
depthDesc.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
auto depth = graph.createTexture("shadow/depth", depthDesc);

// Each frame, after this resource set's fence completes:
graph.clear();
shadow.addPass(graph, {cube, depth}, {lightPosition, 0.1f, 1024.0f},
    [casters](VkCommandBuffer cmd, const rg::RenderPass& pass,
              const rg::GraphResources& resources)
    {
        const auto layout = resources.pipeline(pass.pipelineHandle).layout;
        for (const auto& model : casters)
        {
            model->drawGraphGeometryOnly(cmd, layout);
        }
    });
pbrResources.shadowCube = cube;
pbr.addPass(graph, pbrResources, drawScene);
// Then skybox and presentation, followed by graph.prepare()/render().
```

Use supported R32F color/sampled-image and D32 or D16 depth-only formats.
The cube has one mip and six square layers; scratch depth has one matching layer.
The pass checks these contracts before declaring any faces. Cube views/sampler are
created by Device through graph resource creation, not by ShadowPass. The current
graph sampler uses nearest filtering/clamp-to-edge; the legacy shadow sampler
used linear filtering, so filtering quality is not claimed to be identical.
Do not share writable cube/depth images between overlapping GPU frames; graph
uniform buffers are per-frame-slot, but output TextureIds themselves are not.
Shadow resolution is independent of the swapchain extent. If changing it, wait
for GPU use to finish and recreate the affected graph/resources with new IDs.

## Draw and synchronization contract

- The graph supplies each face's view-projection/light data through `.uniforms("ubo", ...)`
  at set 3, binding 0. Reflection finds this name and the 64-byte model push constant.
- `Model::drawGraphGeometryOnly` preserves graph pipeline/descriptors, traverses
  visible glTF nodes and supports uploaded raw geometry. It only pushes the model
  matrix and draws; it does not bind material descriptors or legacy pipelines.
- Supply an instance buffer, including one zero-position instance for ordinary
  non-instanced objects. Set `rootModelMatrix` consistently with PBR. The callback
  selects shadow casters explicitly; no global object IDs or skybox exclusions are hidden in the pass.
- The shadow shader now matches PBR's `model * (position + instancePosition)`.
  Instance rotation/scale are currently unused by PBR and therefore by shadow too.
  Alpha-tested materials are not supported in this geometry-only pass yet.
- Each face writes one cube layer and clears scratch depth to 1. Empty cube pixels
  clear to maximum finite float, not zero distance, so they do not falsely occlude.
- The graph currently tracks image access for the whole image conservatively.
  Six face writes are synchronized without discarding earlier faces; a subsequent
  PBR/lighting `.read(cube)` transitions the complete cube to sampled-read access.
- Matrices use explicit right-handed Vulkan zero-to-one depth. Projection Y is
  flipped to compensate for RenderGraph's negative-height viewport and preserve
  the original cube sampling orientation.

## Status

Legacy class, direct application shadow recording and manual shadow descriptor
creation are removed. Windows/Android build references use ShadowPass.
The application frame-graph composition remains pending, as for PBR/Skybox;
the sample above is an integration contract, not a claim that the app runs.
Structural tests and real shader reflection pass. Model/ShadowPass MSVC syntax
checks pass. Full application build, Android execution and GPU validation remain unverified.
