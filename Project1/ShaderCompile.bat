cd /d "%~dp0"
%VULKAN_SDK%\Bin\glslc.exe .\shaders\pbr.vert -o .\shaders\pbr.vert.spv
%VULKAN_SDK%\Bin\glslc.exe .\shaders\pbr.frag -o .\shaders\pbr.frag.spv
%VULKAN_SDK%\Bin\glslc.exe .\shaders\pbrnotexture.frag -o .\shaders\pbrnotexture.frag.spv
%VULKAN_SDK%\Bin\glslc.exe .\shaders\point_light.vert -o .\shaders\point_light.vert.spv
%VULKAN_SDK%\Bin\glslc.exe .\shaders\point_light.frag -o .\shaders\point_light.frag.spv
%VULKAN_SDK%\Bin\glslc.exe .\shaders\skybox.vert -o .\shaders\skybox.vert.spv
%VULKAN_SDK%\Bin\glslc.exe .\shaders\skybox.frag -o .\shaders\skybox.frag.spv
%VULKAN_SDK%\Bin\glslc.exe .\shaders\filtercube.vert -o .\shaders\filtercube.vert.spv
%VULKAN_SDK%\Bin\glslc.exe .\shaders\irradiancecube.frag -o .\shaders\irradiancecube.frag.spv
%VULKAN_SDK%\Bin\glslc.exe .\shaders\prefilterenvmap.frag -o .\shaders\prefilterenvmap.frag.spv
%VULKAN_SDK%\Bin\glslc.exe .\shaders\genbrdflut.vert -o .\shaders\genbrdflut.vert.spv
%VULKAN_SDK%\Bin\glslc.exe .\shaders\genbrdflut.frag -o .\shaders\genbrdflut.frag.spv
%VULKAN_SDK%\Bin\glslc.exe .\shaders\picking.frag -o .\shaders\picking.frag.spv
%VULKAN_SDK%\Bin\glslc.exe .\shaders\shadowOffscreen.vert -o .\shaders\shadowOffscreen.vert.spv
%VULKAN_SDK%\Bin\glslc.exe .\shaders\shadowOffscreen.frag -o .\shaders\shadowOffscreen.frag.spv
%VULKAN_SDK%\Bin\glslc.exe .\shaders\deferedoffscreen.vert -o .\shaders\deferedoffscreen.vert.spv
%VULKAN_SDK%\Bin\glslc.exe .\shaders\deferedoffscreen.frag -o .\shaders\deferedoffscreen.frag.spv
%VULKAN_SDK%\Bin\glslc.exe .\shaders\deferedPBR.vert -o .\shaders\deferedPBR.vert.spv
%VULKAN_SDK%\Bin\glslc.exe .\shaders\deferedPBR.frag -o .\shaders\deferedPBR.frag.spv
%VULKAN_SDK%\Bin\glslc.exe .\shaders\deferedoffscreenNotexture.frag -o .\shaders\deferedoffscreenNotexture.frag.spv
%VULKAN_SDK%\Bin\glslc.exe .\shaders\deferedoffscreenSkybox.vert -o .\shaders\deferedoffscreenSkybox.vert.spv
%VULKAN_SDK%\Bin\glslc.exe .\shaders\deferedoffscreenSkybox.frag -o .\shaders\deferedoffscreenSkybox.frag.spv
%VULKAN_SDK%\Bin\glslc.exe .\shaders\computeCull.comp -o .\shaders\computeCull.comp.spv
exit /b 0
