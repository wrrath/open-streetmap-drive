# Vulkan overview

This is a beginner-oriented explanation of the Vulkan code used by Open StreetMap Drive. It assumes familiarity with C++ and the broad OpenGL ideas of vertex buffers, shaders, and drawing triangles.

Vulkan is explicit: work that an OpenGL driver often infers or performs on demand must be described up front. The extra objects in `VulkanRenderer` answer four questions:

1. **Where can we draw?** Instance, physical device, logical device, queue, and window surface.
2. **Which images will reach the screen?** Swapchain, image views, and framebuffers.
3. **How will a draw be processed?** Render pass, shaders, pipeline layout, and graphics pipeline.
4. **When may work run or resources be reused?** Command buffers, semaphores, and fences.

## OpenGL-to-Vulkan mental map

These comparisons are approximate, but useful when first reading the code.

| OpenGL idea | Vulkan equivalent in this project |
| --- | --- |
| Driver/context setup | `VkInstance`, `VkPhysicalDevice`, `VkDevice`, `VkQueue` |
| Default window framebuffer | A swapchain of `VkImage`s, each wrapped by an image view and framebuffer |
| Shader program plus fixed state | `VkPipelineLayout` and `VkPipeline` |
| `glBindBuffer` / `glDrawElements` | Recorded `vkCmdBind*` / `vkCmdDrawIndexed` commands |
| Uniform update for one small matrix | A Vulkan push constant |
| Implicit ordering and driver synchronization | Explicit semaphores, pipeline stage masks, and a fence |
| GLSL compiled by the driver | GLSL compiled ahead of time to SPIR-V |

Unlike OpenGL's mutable global state machine, most Vulkan render state is baked into an immutable pipeline. Changing topology, blending, or another baked state commonly means choosing a different pipeline.

## Initialization, in dependency order

The `VulkanRenderer` constructor calls small setup methods in the order below. Later objects depend on earlier ones, so reversing arbitrary steps will not work.

### 1. Window, instance, and surface

GLFW creates a window with `GLFW_NO_API`; it must not create an OpenGL context.

A `VkInstance` connects the application to the Vulkan implementation. GLFW reports the platform-specific instance extensions needed to present to its window. `glfwCreateWindowSurface()` then creates a `VkSurfaceKHR`, Vulkan's platform-neutral handle for that window.

The instance does not represent the GPU and does not execute drawing.

### 2. Physical device, logical device, and queue

A `VkPhysicalDevice` describes an installed GPU and its capabilities. The prototype takes the first enumerated device.

Queue families advertise kinds of work. `findGraphicsQueueFamily()` finds one family that can both execute graphics commands and present to the GLFW surface. `vkCreateDevice()` creates the application's logical `VkDevice`, enabling the swapchain extension, and `vkGetDeviceQueue()` retrieves one queue from that family.

A useful distinction is:

- **physical device:** capability description;
- **logical device:** the application's connection to that device;
- **queue:** where recorded GPU work is submitted.

### 3. Swapchain and image views

The swapchain owns a small set of presentable images. Instead of drawing forever into one back buffer, the application repeatedly acquires an available image, renders into it, and presents it.

The renderer asks the surface for supported formats and size limits. It prefers an sRGB BGRA format, uses FIFO presentation (always supported and similar to vsync), and asks for one more than the minimum image count when allowed.

A `VkImage` is storage. A `VkImageView` describes how a shader or framebuffer will interpret that storage. This project creates one color view for every swapchain image.

### 4. Render pass and framebuffers

The render pass declares how attachments are used during a rendering operation. This one has a single color attachment:

- clear it at the beginning;
- store the result at the end;
- transition from an undefined old layout to the presentation layout.

The subpass dependency makes color-attachment writes wait at the appropriate pipeline stage. There is no depth attachment yet.

A framebuffer pairs this render-pass description with one concrete swapchain image view. Consequently, there is one framebuffer per swapchain image.

### 5. Shaders, pipeline layout, and graphics pipeline

CMake compiles `assets/shaders/road.vert` and `road.frag` from GLSL to Vulkan's SPIR-V bytecode. The renderer briefly creates shader modules from those files while building the pipeline, then destroys the modules; the finished pipeline retains what it needs.

The road vertex format has three attributes in one interleaved binding:

```text
location 0: vec3 position
location 1: vec3 normal
location 2: vec2 UV
```

The byte stride and `offsetof` values tell Vulkan how `map::RoadVertex` in C++ corresponds to shader input locations. If that struct or the shader declarations change, the pipeline descriptions must change too.

The pipeline also fixes triangle-list assembly, viewport/scissor, filled rasterization, no culling, one sample per pixel, and color writes with blending disabled. Unlike a typical OpenGL loop, these states are not set again before every draw.

The pipeline layout describes resources visible to shaders. The only resource here is a vertex-stage push-constant range containing one model-view-projection matrix. Push constants are best thought of as a tiny, fast command-stream payload, not general buffer storage.

If compiled shaders are absent, pipeline creation is skipped. Frame rendering still clears and presents an image.

### 6. Command pool and command buffers

A command buffer stores GPU instructions; calling a `vkCmd...` function records an instruction rather than immediately executing it. A command pool owns the allocation backing those buffers and ties them to a queue family.

The renderer allocates one primary command buffer per swapchain framebuffer. It re-records the command buffer for the acquired image each frame because the camera matrix changes. The command pool was created with `VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT`, which permits this individual reset.

### 7. Synchronization objects

The prototype creates:

- `imageAvailableSemaphore_`: signaled when the presentation system has released the acquired image;
- `renderFinishedSemaphore_`: signaled when rendering submitted to the queue is complete;
- `inFlightFence_`: signaled when the CPU may safely reuse this frame's resources.

Semaphores order GPU/presentation operations. A fence is observable by the CPU. The fence starts signaled so the very first frame does not wait forever.

## Mesh upload and Vulkan memory

`Game` passes a CPU-side `RoadMesh` to `setRoadMesh()`. The renderer creates a vertex buffer and an index buffer. Vulkan buffer creation and memory allocation are separate operations:

1. `vkCreateBuffer()` creates the buffer object and states its size/usage.
2. `vkGetBufferMemoryRequirements()` asks what backing memory it can use.
3. `findMemoryType()` chooses a compatible heap/type with requested properties.
4. `vkAllocateMemory()` allocates that backing memory.
5. `vkBindBufferMemory()` connects the two objects.
6. `vkMapMemory()` exposes the memory to the CPU so `memcpy` can upload data.

The requested memory is host-visible and host-coherent. **Host-visible** means the CPU can map it. **Host-coherent** means explicit cache flushing is unnecessary for these writes. This is simple but not necessarily the fastest GPU memory; staging into device-local buffers is a common later optimization.

Indices avoid repeating vertices and are drawn as 32-bit unsigned values by `vkCmdDrawIndexed()`.

## One frame, step by step

`drawFrame()` performs this sequence:

```text
CPU waits for previous frame's fence
             |
acquire swapchain image --signals--> imageAvailable semaphore
             |
record commands for that image
             |
submit to graphics queue
  waits: imageAvailable at color-output stage
  runs:  command buffer
  signals: renderFinished semaphore + inFlight fence
             |
present waits for renderFinished
```

More precisely:

1. **Wait and reset the fence.** There is only one frame in flight, so this serializes CPU frame submission and makes command-buffer reuse straightforward.
2. **Acquire an image.** `vkAcquireNextImageKHR()` returns the index selecting the command buffer and framebuffer for this frame.
3. **Record commands.** The renderer resets that command buffer, begins it, begins the render pass, optionally draws the road, then ends both.
4. **Submit.** `vkQueueSubmit()` waits on image availability at the color-output stage, executes the command buffer, and signals both completion objects.
5. **Present.** `vkQueuePresentKHR()` waits for rendering to finish before displaying the same swapchain image.

The semaphore wait stage is important: it says the submitted work must not access the acquired image as a color attachment before acquisition has completed.

## Commands recorded for the road draw

Before recording, CPU code computes the camera:

- heading creates a forward vector;
- the eye sits above and behind the vehicle;
- `glm::lookAt` creates the view matrix;
- `glm::perspective` creates a Vulkan depth-range projection because CMake defines `GLM_FORCE_DEPTH_ZERO_TO_ONE`;
- the projection's Y value is negated to account for Vulkan's framebuffer coordinate convention with this GLM setup.

Inside the render pass the command buffer:

1. clears the image to sky blue;
2. binds the road graphics pipeline;
3. writes `projection * view` as a push constant (the road model transform is identity);
4. binds the vertex and index buffers;
5. records one indexed draw call.

The vertex shader transforms positions to clip space and forwards UVs. The fragment shader procedurally chooses asphalt/paint color from the UV; no texture or descriptor set is used.

## Object lifetime and destruction

Vulkan generally requires an object to outlive objects or submitted work that depend on it. The destructor first calls `vkDeviceWaitIdle()`, then destroys synchronization and mesh resources, swapchain-related resources, pipeline objects, the command pool, and the device. The surface and instance outlive the device; the GLFW window outlives the surface.

One subtle rule visible in the code is that a buffer and its `VkDeviceMemory` are separate handles: destroy the buffer before freeing memory bound to it.

## Important current omissions

The current renderer is a teaching prototype, not yet a robust Vulkan framework:

- **No swapchain recreation.** Resize, minimize, `VK_ERROR_OUT_OF_DATE_KHR`, and some present results are not handled. A production loop must rebuild all swapchain-size/format-dependent objects.
- **No validation layers/debug messenger.** Install and enable these during development to catch invalid usage and lifetime errors.
- **One frame in flight.** This is simple, but prevents CPU preparation from overlapping multiple GPU frames.
- **No depth attachment/test.** Road triangles lie on one plane; a richer 3D scene needs depth buffering.
- **Minimal capability checks.** Device extensions, queue support, formats, and return codes should be validated more comprehensively.
- **No RAII wrappers.** Exceptions during partial construction can currently leave already-created C/GLFW/Vulkan resources undisposed because the full object's destructor will not run.

Keeping these limitations explicit is important: Vulkan code that works on one machine or while a window stays fixed is not automatically complete.

## Suggested reading order in the code

1. `VulkanRenderer.hpp` — inventory the handles and setup functions.
2. The `VulkanRenderer` constructor — see creation order.
3. `createSwapchain()`, `createRenderPass()`, and `createGraphicsPipeline()` — understand the render target and fixed state.
4. `setRoadMesh()` — follow CPU data into Vulkan memory.
5. `recordCommandBuffer()` — see the actual draw instructions.
6. `drawFrame()` — see synchronization, submission, and presentation.
7. The destructor — compare destruction order with creation order.

For how this renderer connects to maps, physics, and the game loop, see [ARCHITECTURE.md](ARCHITECTURE.md).
