# L1A — The GPU Framebuffer, Double Buffering, and V-Sync

Before you write a single line of OpenGL or Vulkan code, you need to understand what you are actually talking to. The GPU is not a general-purpose compute device you just hand pixels to. It has its own memory, its own bus, its own timing contracts with the monitor. Every real-time graphics API — OpenGL, Vulkan, Metal, DirectX — is fundamentally a protocol for managing those resources and coordinating those timings. If you treat the API as magic, you will never understand why things go wrong, why performance collapses, or why competitive games ship with vsync off.

This section covers the physical memory where your rendered frame lives, what that memory contains, what happens when the monitor tries to read it while you are still writing to it, and how the industry solved that problem. These are not historical curiosities. They are the concepts behind Vulkan's present modes, Unreal Engine's frame sync settings, and the vsync toggle every game ships.

---

## 1. GPU Memory vs CPU Memory

### Why They Are Separate

Open your laptop right now. There is one pool of RAM — your DRAM sticks — and your CPU reads and writes to it. You probably think of memory as one thing. On a discrete GPU (the kind in a desktop or a laptop with a dedicated graphics card), this is not true. The GPU has its own separate memory chips soldered directly to the graphics card: GDDR6, GDDR6X, or HBM depending on the generation.

The reason is bandwidth. The CPU needs memory that handles many small random accesses — cache misses, heap allocations, stack frames. DRAM optimized for that is DDR5, with a 64-bit wide bus running at maybe 50-80 GB/s of total bandwidth shared across all CPU cores.

The GPU is doing something completely different. It is running thousands of threads in parallel, all of which need to read texture data and write pixel data at the same time, all to different memory addresses. If those threads had to share the CPU's memory bus, they would serialize. The whole pipeline collapses.

GDDR6 solves this with a physically wider bus (256-bit or 384-bit wide on high-end cards) and higher clock speeds. An RTX 4090 has 1008 GB/s of memory bandwidth to its GDDR6X. Compare that to a CPU's 80 GB/s. This is not a 2x improvement — it is more than 10x. This bandwidth is why the GPU can process millions of fragments per frame without stalling.

```
CPU side                         GPU side
---------                        ---------
[CPU Cores]                      [GPU Shader Cores x thousands]
     |                                  |
[L1/L2/L3 Cache]                 [L1/L2 GPU Cache]
     |                                  |
[DDR5 DRAM]                      [GDDR6X VRAM]
  ~80 GB/s                         ~1000 GB/s
     |                                  |
     +----------[PCIe Bus]-------------+
                  ~64 GB/s
```

The PCIe bus connects the two. PCIe 4.0 x16 (what most modern cards use) runs at about 32 GB/s in each direction — 64 GB/s bidirectional. Fast in absolute terms, but a hard bottleneck if you are constantly moving large amounts of data back and forth. This is why one of the golden rules of GPU programming is: get your data to the GPU once, keep it there, and do not copy it back unless you have to.

### The Apple M-Series Exception

Apple M1/M2/M3/M4 chips use what Apple calls "unified memory." The CPU and GPU share the same physical DRAM package. This is not the same as the GPU losing its dedicated memory — it is a packaging decision. Instead of DDR5 on the motherboard and GDDR6 on a discrete card connected by PCIe, everything is on the same die or package running on very wide memory buses (up to 400+ GB/s on M2 Ultra).

What "unified" actually means in practice: there is one physical DRAM pool, but the GPU driver still allocates heaps within it and manages them separately. Textures and framebuffers still live in GPU-accessible memory regions. The big difference is that you eliminate the PCIe transfer overhead entirely — if a texture lives in unified memory, the GPU can read it without a copy. This is why M-series chips can drive surprisingly complex workloads with no discrete GPU: the bandwidth is high enough and the latency low enough that the separation penalty disappears.

For your learning, this matters when you use Metal on macOS. The `MTLBuffer` and `MTLTexture` objects you allocate live in this unified pool. On discrete GPUs, the same data would require an explicit upload from CPU RAM to VRAM. Understanding this difference is what lets you profile and optimize memory bandwidth correctly on different hardware.

---

## 2. The Framebuffer as GPU Memory

### What It Actually Is

"Framebuffer" is a term that sounds special, but it is just a name for a region of GPU memory that your render output gets written into and that the display subsystem reads from to drive the monitor.

Think of the framebuffer as a large flat array. Each element of the array represents one pixel on the screen. The GPU writes to this array as fragments pass through the fragment shader and the output merger. The display controller reads from it in order, row by row, to produce the signal the monitor displays.

A single pixel in a typical 32-bit color framebuffer contains:

```
struct Pixel {
    uint8_t r;      // 1 byte: red channel [0, 255]
    uint8_t g;      // 1 byte: green channel
    uint8_t b;      // 1 byte: blue channel
    uint8_t a;      // 1 byte: alpha channel
};                  // 4 bytes total per pixel
```

The depth and stencil values for that pixel are stored in a separate buffer (or a packed format, depending on the hardware and API), but conceptually they belong to the same pixel's data.

### Memory Layout

For a 1920x1080 framebuffer, pixels are stored linearly in memory, row-major:

```
Address 0:          Pixel (0, 0)    ← top-left corner
Address 4:          Pixel (1, 0)
Address 8:          Pixel (2, 0)
...
Address 7676:       Pixel (1919, 0) ← end of row 0
Address 7680:       Pixel (0, 1)    ← start of row 1
...
Address 8294396:    Pixel (1919, 1079) ← bottom-right corner
```

Each row is `1920 × 4 = 7,680 bytes`. All 1080 rows: `7,680 × 1080 = 8,294,400 bytes ≈ 8 MB`.

This is just the RGBA color buffer. The framebuffer in a typical render pass also includes:

**Depth buffer (Z-buffer):** One float per pixel, usually 24-bit or 32-bit.

```
1920 × 1080 × 4 bytes (32-bit float depth) = 8,294,400 bytes ≈ 8 MB
```

Or with packed depth-stencil format (`D24_UNORM_S8_UINT`): 3 bytes for depth + 1 byte for stencil = 4 bytes per pixel, same total size.

**Stencil buffer:** 8-bit integer per pixel, often packed with depth above.

```
Total framebuffer (color + depth/stencil) at 1920×1080:
  Color buffer:         ~8 MB
  Depth buffer:         ~8 MB
  (stencil packed in)
  ──────────────────────────
  Total per frame:     ~16 MB
```

With double buffering (explained next section), you have two of these, so ~32 MB of VRAM just for the framebuffers at 1080p. At 4K (3840×2160):

```
3840 × 2160 × 4 bytes = 33,177,600 bytes ≈ 32 MB per buffer
Two buffers (double buffering) ≈ 64 MB
Add depth: ≈ 128 MB total for framebuffers alone
```

This is a small fraction of VRAM on a modern card (8-24 GB), but it is a real fixed cost that exists before you load a single texture or mesh.

---

## 3. Buffer Types and Their Purposes

### The Color Buffer

The color buffer stores the RGBA value of each pixel as it will appear on screen. The GPU's fragment shader writes to it. The display controller reads from it.

**Bit depth options:**

- **8-bit per channel (24-bit color + 8-bit alpha):** The historical standard. Each channel is a `uint8` — 256 levels per channel, 16.7 million distinct colors. This is what you get when you specify `GL_RGBA8` or `VK_FORMAT_B8G8R8A8_UNORM`. More than enough for standard dynamic range (SDR) output.

- **10-bit per channel (HDR10):** Each channel is a 10-bit unsigned integer — 1024 levels per channel. Required for HDR output on modern TVs and monitors. 4 bytes per pixel is maintained by using 10+10+10+2 packing (two bits wasted on alpha or used for alpha precision). Unreal Engine's HDR output path writes to 10-bit buffers.

- **16-bit float per channel (FP16 HDR):** Each channel is a 16-bit IEEE float — can represent values greater than 1.0, which is needed for high dynamic range intermediate rendering. Your internal render targets during a frame (before tone mapping) are almost always FP16 to preserve precision. The final output to the monitor is then tone-mapped back down to 10-bit or 8-bit.

The GPU writes to the color buffer at the output merger stage, after the fragment shader runs and after alpha blending and depth testing have been applied.

### The Depth Buffer (Z-Buffer)

Every pixel in the color buffer has a corresponding depth value — a float in [0.0, 1.0] representing how far that pixel's geometry is from the camera, after the perspective projection and perspective divide have mapped depth to the normalized range.

At the start of each frame, you clear the depth buffer to 1.0 (the far plane). As geometry is rasterized, for each fragment the GPU checks: is this fragment's depth value less than what is already stored in the depth buffer at this pixel location? If yes, the geometry is closer to the camera, so it wins — write the fragment's color to the color buffer and update the depth buffer to this fragment's depth. If no, the geometry is behind something already rendered, so discard the fragment.

This is the Z-buffer algorithm, and it is why you can render geometry in any order and still get correct hidden surface removal. The hardware runs this check in parallel for every fragment.

Why "Z buffer"? After the perspective projection transform, the depth coordinate is encoded in the z component of clip coordinates. After perspective divide, it becomes NDC z, which maps to [0,1] via the viewport transform. The name predates modern pipeline terminology — it comes from the z-axis being the depth axis in eye space. The name stuck.

**Depth precision gotcha:** Most hardware uses 24-bit fixed-point depth internally (`GL_DEPTH_COMPONENT24`), not 32-bit float, even when you specify float. Precision is non-linear — you have high precision near the camera and very little near the far plane. This is why setting your near plane to 0.01 and far plane to 100000.0 produces z-fighting artifacts on distant geometry: the depth values for objects at 90,000 units and 90,001 units may round to the same integer after quantization. The fix is to keep the near/far ratio small. This is called the "depth precision problem" and it is a real, practical issue in every 3D engine.

### The Stencil Buffer

The stencil buffer is an 8-bit unsigned integer per pixel. You can use it as a per-pixel mask: before rendering, write specific integer values to regions of the stencil buffer, then configure subsequent draw calls to only affect pixels where the stencil buffer contains (or does not contain) a specific value.

Common uses:

- **Mirror rendering:** Render the mirror geometry first, writing a 1 to the stencil buffer. Then render the reflected scene with the stencil test configured to only write fragments where stencil = 1.
- **UI clipping:** Restrict UI elements to a bounding region without GPU-expensive scissor rects.
- **Portal rendering:** Same idea as mirrors — use stencil to mask the view through a portal.
- **Deferred rendering G-buffer classification:** Tag pixels as "lit" vs "unlit" to skip expensive lighting passes on sky pixels.

The stencil buffer is not magic — it is just an 8-bit scratchpad per pixel that you control. Its usefulness comes from the fact that stencil tests happen in fixed-function hardware before the fragment shader runs, making masking extremely cheap.

### The Accumulation Buffer (Legacy)

The accumulation buffer was a legacy OpenGL feature — an extra high-precision RGBA buffer you could incrementally accumulate into over multiple passes. The idea: render the same scene multiple times with the camera slightly offset each frame, accumulate the results, divide by the number of samples — you get multi-sample anti-aliasing or depth-of-field or motion blur.

It was removed in OpenGL 3.2+ core profile. Modern techniques accomplish the same results in shaders: temporal anti-aliasing (TAA) accumulates samples across frames using the motion vector buffer; depth of field is computed in a post-processing pass using a depth sample and a bokeh blur kernel. Faster, more flexible, and shader-programmable.

---

## 4. The Tearing Problem and Double Buffering

### How a Monitor Actually Works

To understand tearing, you need to understand how a monitor draws an image. On a CRT (cathode ray tube — the old bulky monitors), a physical electron beam sweeps across phosphor dots to produce light. The beam starts at the top-left, sweeps right to the end of the row, snaps back (horizontal retrace), moves down one row, sweeps right again, and so on until it reaches the bottom-right. Then it snaps all the way back to the top-left — this is the vertical retrace. At 60Hz, this entire cycle happens 60 times per second.

Modern LCD and OLED monitors do not have a beam, but the display controller still processes rows in the same order. The signal protocol (HDMI, DisplayPort) still sends pixel data scanline by scanline. The timing is identical. The concept of "when the display is reading row 500" is just as real on an LCD as it was on a CRT.

### What Tearing Looks Like

Suppose the GPU has only one framebuffer. The GPU writes frame N+1 directly into it. While the GPU is writing — say, halfway through updating the buffer — the monitor's display controller is reading the same buffer, because it needs to push data to the screen right now.

```
Frame buffer memory:

  Row 0   ──── Row 250  ─────────────────────────── Row 1079
  [  Frame N+1 written   ][       Frame N still here       ]
                          ^
                          GPU is here, writing row 251
                          
Monitor read pointer is at row 400, still reading old Frame N
```

The result: the top portion of the screen shows frame N+1 (the new frame), the bottom portion shows frame N (the old frame). The boundary is a horizontal discontinuity — the "tear." In a game where the camera pans right, the top half of the screen shows the world shifted right while the bottom half still shows the old position. It looks like the image was cut and the pieces slid horizontally.

```
SCREEN OUTPUT:
┌─────────────────────────────────┐
│  ← Frame N+1 (new frame) →     │  rows 0–430
│                                 │
│─────────────────────────────────│ ← TEAR LINE
│  ← Frame N (old frame)  →      │  rows 431–1079
│                                 │
└─────────────────────────────────┘
```

This is a correctness problem, not a performance problem. It happens even if the GPU is running fast.

### Double Buffering: The Solution

The fix is simple in concept: give the GPU two buffers instead of one.

- **Front buffer:** This is what the display controller reads. It always contains a complete, stable frame.
- **Back buffer:** This is what the GPU renders into. The display controller never touches this.

When the GPU finishes rendering a frame into the back buffer, it swaps them: the back buffer becomes the front buffer and vice versa. Now the GPU immediately starts rendering the next frame into what is now the back buffer (old front).

```
VRAM layout with double buffering:

┌──────────────────────┐         ┌──────────────────────┐
│    FRONT BUFFER      │◄────────┤   Display Controller  │
│   (complete frame)   │         │   (DAC/scan-out)      │
│                      │         └──────────────────────┘
│  pixels[0..2073599]  │                   │
└──────────────────────┘                   ▼
                                        MONITOR
┌──────────────────────┐
│    BACK BUFFER       │◄──── GPU Fragment Shader Output
│  (rendering in prog) │
│  pixels[0..2073599]  │
└──────────────────────┘
          │
          │  [GPU finishes frame]
          ▼
       SWAP BUFFERS
       (pointers exchange)
```

After the swap, the GPU writes to what was the front buffer (which is now the back buffer). The display controller reads from what was the back buffer (which is now the front buffer). The monitor never sees a partial frame.

The swap itself does not copy memory. It swaps pointers — the display controller's scan-out address register is updated to point to the new front buffer. This is nearly instantaneous (a few nanoseconds). The frames stay in place; only the labels change.

---

## 5. V-Sync: The Vertical Retrace Signal

### The Timing Problem

Double buffering eliminates tearing if you swap at the right time. The right time is when the display controller has just finished reading the last row of the current frame — right at the vertical retrace. If you swap while the display controller is mid-frame (say, at row 400), you get the same tear, just in a different place.

The monitor solves this by emitting a hardware signal: the vertical sync (vsync) signal. At the end of each frame — after the display controller finishes row 1079 and before it starts row 0 of the next frame — the monitor tells the GPU: "I am done with the current frame. You may now swap."

### What Happens With V-Sync On

With `glfwSwapInterval(1)` in OpenGL (or `VK_PRESENT_MODE_FIFO_KHR` in Vulkan), the GPU driver waits for the vsync signal before executing the buffer swap. The sequence:

```
Timeline (60Hz monitor, vsync ON):

t=0ms    GPU starts rendering frame 1
t=14ms   GPU finishes frame 1, calls glfwSwapBuffers()
t=16.7ms Vsync signal fires ─── SWAP HAPPENS
t=16.7ms GPU starts rendering frame 2
t=30ms   GPU finishes frame 2, calls glfwSwapBuffers()
t=33.4ms Vsync signal fires ─── SWAP HAPPENS
t=33.4ms GPU starts rendering frame 3
...
```

The GPU is rendering faster than 60Hz here (14ms render time vs 16.7ms available), so it finishes, waits ~2.7ms, then swaps at vsync. Result: steady 60fps, no tearing, GPU occasionally idle.

Now what if the GPU renders slower than 60Hz — say, 20ms per frame?

```
Timeline (60Hz monitor, vsync ON, GPU slow):

t=0ms    GPU starts rendering frame 1
t=16.7ms Vsync signal fires ─── GPU NOT DONE, NO SWAP
t=20ms   GPU finishes frame 1, calls glfwSwapBuffers()
t=33.4ms Vsync signal fires ─── SWAP HAPPENS (misses 1 vsync)
t=33.4ms GPU starts rendering frame 2
t=53.4ms GPU finishes frame 2
t=66.8ms Vsync signal fires ─── SWAP HAPPENS
...
```

You missed one vsync. The frame stayed on screen for 33.4ms instead of 16.7ms. Your effective framerate dropped from 60fps to 30fps — not 50fps, not 45fps. With vsync, framerate can only fall in integer fractions of the monitor's refresh: 60, 30, 20, 15. This is the stutter problem. Your render time needs to stay consistently below 16.7ms or you pay the full vsync period penalty.

### glfwSwapInterval(0): No V-Sync

When you call `glfwSwapInterval(0)`, the driver does not wait for the vsync signal. The swap happens as soon as the GPU finishes rendering, regardless of where the monitor's scan-out pointer is. This maximally decouples GPU render time from display timing. Frames appear on screen faster — input latency drops — but tearing is possible whenever the swap happens mid-scan.

For benchmarking, you always want `glfwSwapInterval(0)` so your measured framerate reflects actual GPU throughput, not vsync quantization. For competitive games, many players accept tearing in exchange for 1-3ms lower input latency (the time between moving the mouse and seeing the result on screen). For cinematic or single-player games, tearing is unacceptable and vsync is on.

---

## 6. Triple Buffering

### The Problem With Double Buffering + V-Sync

When the GPU renders faster than the vsync interval, it finishes the back buffer and then has nothing to do — it cannot swap yet (vsync hasn't fired) and it cannot overwrite the back buffer (the display controller might be mid-swap). So it sits idle for a few milliseconds every frame. At 60Hz with a GPU that can render a frame in 10ms, the GPU is idle for 6.7ms every frame — about 40% idle. This is wasted compute capacity.

More practically, if your render time is 17ms — just barely over the 16.7ms vsync interval — double buffering with vsync drops you to 30fps. Triple buffering can recover this.

### How Triple Buffering Works

Add a third buffer. Now you have:

- **Buffer A:** Currently displayed (front buffer)
- **Buffer B:** Last completed render (queued to swap at next vsync)
- **Buffer C:** GPU is rendering into this right now

When vsync fires:
1. Buffer B becomes the new front (displayed)
2. Buffer A becomes free (old front is no longer needed)
3. GPU immediately switches from Buffer C to Buffer A as its new target (or C becomes the new "pending" and A becomes "rendering" — implementations vary)

```
TRIPLE BUFFERING — BUFFER STATES OVER TIME:

              ┌────────────┬────────────┬────────────┐
              │  Buffer A  │  Buffer B  │  Buffer C  │
─────────────────────────────────────────────────────
t=0ms         │ DISPLAYED  │ completed  │ RENDERING  │
t=10ms        │ DISPLAYED  │ completed  │ done→queued│
              │            │            │            │
  (GPU starts │            │            │            │
   on free B) │ DISPLAYED  │ RENDERING  │  PENDING   │
              │            │            │            │
t=16.7ms      │            │            │            │
  vsync fires:│            │→DISPLAYED  │  PENDING   │
              │ free/render│            │            │
              │ RENDERING  │ DISPLAYED  │  pending   │
─────────────────────────────────────────────────────
```

The key insight: the GPU never has to wait. As soon as it finishes one buffer, it has another free buffer to render into immediately. At vsync, the display picks up the most recently completed buffer.

**Downside:** There is now at most 2 frames of latency (the GPU might be working on frame N+2 while frame N is displayed). For VR, where total motion-to-photon latency must be under 20ms, this extra latency is unacceptable. VR runtimes use specialized "asynchronous reprojection" techniques instead of simple triple buffering.

**VRAM cost:** 3 × 16 MB (at 1080p, color + depth) = 48 MB. Negligible on modern cards. At 4K, about 3 × 128 MB ≈ 384 MB.

---

## 7. Practical OpenGL

### glfwSwapBuffers

When you call `glfwSwapBuffers(window)`, you are not directly swapping memory. You are posting a swap command to the GPU driver's command queue. The driver translates this into a platform-level present call (WGL, GLX, or EGL depending on OS). The GPU's display controller — a separate fixed-function unit on the GPU die — receives the command and executes the swap at the appropriate time based on the swap interval setting.

This is why `glfwSwapBuffers` might block: if vsync is enabled and the driver decides to wait for the vertical retrace signal before returning, the call can stall your main thread for up to one full frame period (16.7ms at 60Hz). Some drivers do this synchronously; others insert the wait asynchronously and return immediately. This variation is why you need to profile actual frame time, not just measure CPU time between swap calls.

### Measuring Frame Time

```c
double t0 = glfwGetTime();

// ... render your scene ...

glfwSwapBuffers(window);  // may block here if vsync on

double t1 = glfwGetTime();
double frameTime = t1 - t0;  // seconds
```

With vsync on and a fast GPU, you will see `frameTime ≈ 0.01667` (16.7ms). With vsync off, you will see whatever your actual render time is — maybe 2-5ms for a simple scene. If you observe frame times jumping between 16.7ms and 33.4ms, your render time is hovering near the vsync boundary and you are experiencing vsync-induced stutter.

### Common Beginner Mistake

Calling `glFinish()` before `glfwSwapBuffers()` — this forces the CPU to wait for all previously submitted GPU commands to complete before proceeding. Combined with vsync, this double-stalls: once for `glFinish()` (GPU completion), once for vsync (display timing). Never call `glFinish()` in a render loop unless you have a specific profiling reason. The driver is allowed to pipeline and overlap GPU work; `glFinish()` destroys that overlap.

---

## 8. Modern Relevance: Vulkan Present Modes

Vulkan makes the relationship between double buffering, triple buffering, and vsync completely explicit. There is no hidden "just enable vsync" toggle — you choose a presentation mode when you create the swapchain, which is Vulkan's explicit name for the collection of framebuffers managed for display.

The relevant enum is `VkPresentModeKHR`. The four modes:

### VK_PRESENT_MODE_FIFO_KHR — V-Sync (Double Buffering)

The presentation queue is a FIFO (first in, first out). The GPU submits completed frames to the queue. At each vsync signal, the display pops one frame from the queue and displays it. If the GPU submits frames faster than vsync, they queue up and the extra ones are dropped. If slower, the previous frame is redisplayed. This is vsync. It is the only mode guaranteed to be available on all Vulkan implementations.

### VK_PRESENT_MODE_MAILBOX_KHR — Triple Buffering

The presentation queue holds exactly one frame. When the GPU submits a new completed frame, it replaces the one in the queue immediately (like a mailbox — new mail replaces old). At vsync, the display picks up whatever is currently in the queue. The GPU never waits for vsync — it submits frames as fast as it can render them. Result: the displayed frame is always the most recently completed one, with no latency buildup. This is what game engines mean by "triple buffering" in their settings menus.

### VK_PRESENT_MODE_IMMEDIATE_KHR — No V-Sync

The GPU's frame is presented to the display immediately upon submission, without waiting for vsync. Tearing possible. Maximum throughput, minimum latency. Not guaranteed to be available (some Wayland compositors on Linux do not support it).

### VK_PRESENT_MODE_FIFO_RELAXED_KHR — Relaxed V-Sync

Like FIFO, but if a vsync period passes with no new frame in the queue (GPU is running slow), the next frame is presented immediately upon arrival rather than waiting for the next vsync. Reduces stutter at low framerates at the cost of occasional tearing.

### Querying and Selecting Present Mode

```c
uint32_t modeCount;
vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &modeCount, NULL);

VkPresentModeKHR modes[modeCount];
vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &modeCount, modes);

VkPresentModeKHR chosen = VK_PRESENT_MODE_FIFO_KHR; // safe default, always available

for (uint32_t i = 0; i < modeCount; i++) {
    if (modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
        chosen = VK_PRESENT_MODE_MAILBOX_KHR; // prefer triple buffering if available
        break;
    }
}

// Use `chosen` in VkSwapchainCreateInfoKHR.presentMode
```

This is exactly what the vsync/no-vsync toggle in Unreal Engine, Unity, and every other engine is doing under the hood. The engine queries the platform for supported present modes and selects based on the user's setting. When you check "V-Sync" in Unreal's project settings, you get `FIFO_KHR`. When you uncheck it, you get `MAILBOX_KHR` if available, `IMMEDIATE_KHR` as a fallback.

---

## 9. Frame Timing and Performance

### The Input Latency Trade-Off

Input latency is the time between a physical event (you move the mouse) and the corresponding visual change appearing on your screen. It has several components:

```
Mouse hardware polling (~1ms)
    +
OS input processing (~1ms)
    +
Game engine processing + GPU render time (~5-15ms)
    +
Display pipeline (GPU→monitor signal propagation) (~1ms)
    +
Monitor pixel response time (~1-5ms)
    +
[vsync wait if enabled] (0-16.7ms)
─────────────────────────────────
Total: 8-40ms depending on settings
```

The vsync wait is the only component you can eliminate with a settings toggle. This is why competitive first-person shooter players turn vsync off — the difference between 10ms and 25ms of input latency is perceptible to trained players. The tearing is visible, but they accept it.

### Why 30fps Exists

At 60Hz, the vsync period is 16.7ms. If a game engine cannot consistently render a frame in under 16.7ms, the only stable framerate below 60fps is 30fps (2 vsync periods = 33.4ms). At 30fps, the engine has 33.4ms per frame budget. This is why "locked 30fps" can feel smoother than "fluctuating 40-50fps with vsync" — the frame duration is consistent. The brain adapts to a consistent cadence. Inconsistent frame durations (frame times varying between 20ms and 35ms) are perceived as stutter even if the average framerate is 40fps.

Cinematic games (story-heavy, slow-paced) often target 30fps to free up GPU budget for higher visual quality: more geometry, higher shadow map resolution, more expensive lighting, better anti-aliasing. The reasoning: at 30fps, you have twice the GPU time per frame as at 60fps. Spend that time on quality rather than framerate.

### 144Hz and Above

At 144Hz, the vsync period is 6.9ms. This is not just about "smoother animation" — the reduced vsync period means even with vsync on, input latency from vsync wait is at most 6.9ms instead of 16.7ms. At 240Hz: 4.2ms. This is why 144Hz and 240Hz monitors are gaming peripherals, not just a premium cosmetic upgrade. The latency reduction is real and measurable.

### Adaptive Sync: G-Sync and FreeSync

The fundamental problem with vsync is the quantization: framerate can only be multiples of `(1/refresh_rate)`. If your GPU renders at 55fps (18.2ms/frame), vsync at 60Hz drops you to 30fps — a massive, unnecessary penalty.

Adaptive sync (NVIDIA G-Sync, AMD FreeSync, VESA DisplayHDR's "Adaptive Sync") inverts the contract. Instead of the monitor running at a fixed rate and the GPU waiting for it, the monitor runs at whatever rate the GPU demands. The GPU signals the monitor when a frame is ready, and the monitor's refresh cycle adapts to match. No vsync wait, no tearing, no fixed quantization.

In Vulkan, adaptive sync is exposed via `VK_EXT_display_control` and the `VK_PRESENT_MODE_MAILBOX_KHR` mode often maps to this when the driver and display support it. In practice, engine settings labeled "G-Sync" or "FreeSync" are controlling whether the GPU signals the display for adaptive refresh or uses the traditional fixed vsync signal.

---

## Pitfalls

**Confusing glfwSwapBuffers with clearing the buffer.** `glfwSwapBuffers` swaps front and back — it does not clear anything. The old front buffer (now the new back buffer) still contains the previous frame's pixels. If you forget to call `glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)` at the start of each frame, you will render your new frame on top of the old one with the depth buffer in a partially used state.

**Assuming vsync = consistent frame time.** vsync gives you consistent frame delivery to the display, not consistent GPU render time. Your render loop still needs a CPU-side delta time measurement to handle variable game logic speed. Do not hard-code `dt = 1.0/60.0`.

**Ignoring depth buffer precision when setting near/far.** Setting `near = 0.001` and `far = 10000.0` gives a depth range ratio of 10,000,000:1. With 24-bit depth (16 million representable values), depth values beyond a few hundred units collide and z-fighting appears. A practical rule: keep the near/far ratio under 10,000:1. Set near to the smallest value your scene geometry requires.

**Calling glFinish in a render loop.** This stalls the CPU waiting for GPU completion, destroying driver pipelining. Measure GPU time with query objects (`GL_TIME_ELAPSED`) instead.

**Triple buffering on VR hardware.** VR runtimes (OpenXR, SteamVR) manage their own swap chains and present modes. You do not call `glfwSwapBuffers` in a VR render loop — you submit frames to the runtime's compositor. The runtime handles reprojection and display timing. Using standard triple buffering in a VR context adds 1-2 frames of latency, which breaks presence. Always use the runtime's APIs.

**On Apple Silicon with Xcode instruments.** Metal's Xcode frame debugger shows the "present" call timing. The GPU Timeline view shows exactly when your committed command buffer is picked up for display. The concepts here — front buffer, back buffer, present timing — are directly visible in that tool. Get comfortable reading it.

---

## Summary

The GPU has its own dedicated high-bandwidth memory for a reason — the thousands of parallel shader threads need memory bandwidth that a CPU memory bus cannot provide. The framebuffer is a concrete region of that memory: one array for color (RGBA), one for depth (float), one for stencil (uint8), together consuming ~16MB at 1080p per frame. The tearing problem exists because a single buffer is being read and written simultaneously; double buffering solves it by always giving the display controller a stable buffer and the GPU a separate work-in-progress buffer. V-Sync coordinates the swap to happen only at the end of a display cycle, guaranteeing no torn frames at the cost of framerate quantization. Triple buffering eliminates GPU idle time within that contract. Vulkan makes all of this explicit with `VkPresentModeKHR`, and every engine's vsync toggle maps directly to a choice among those modes. Understanding this layer is what separates a graphics programmer who can write shaders from one who can reason about why frames are dropping, why a game exhibits stutter, or why a VR title's latency is unacceptable.
