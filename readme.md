## Milestone 1

[M1 Video](https://youtu.be/yxLdMad_Vys)

### Procedural Terrain

#### Implementation

Procedural terrain generation applies different noise functions for distinct biomes. 

For grasslands, the height calculation is: 
`(0.5f - std::abs(PerlinNoise(glm::vec2(x, z) / 70.f))) * 30.f`
Dividing by 70 reduces noise frequency, producing broader, smoother elevations that mimic real grassland slopes. 

For mountains, the height is calculated as:
`std::pow((PerlinNoise(glm::vec2(x, z) / 50.f) + 1.f) / 2.f, 3.f) * 250.f`
Dividing by 50 increases variation, and std::pow with an exponent of 3 emphasizes higher values, creating sharper peaks. The 250 scale factor adjusts the mountain heights, distinguishing them from the grassland terrain.

After calculating the height for both biomes, I have the third noise function to blend them together in order to make the transition more natural. I used Perlin noise with the uv divided by 2000 which is a super large number to calculate the `u` value. And then smoothstep the `u` to get the `t` to be used in the LERP between grassland and mountain height.

#### Difficulties

The difficulty that I encountered was how to determine which chunk belongs to which biome. I originally implemented it as randomly assigning the chunk with one of the biomes, but then the terrain looked quite strange and ugly. Then I tried to use the `t` value to determine the grassland, if `t` is smaller than 0.5, then it's grassland. This implementation looks a little bit better, but since I divided the UV with a super large number, it makes the grassland and mountain area extremely large, which is also not too realistic. Finally, I choose to determine the grassland by using the interpolated height, if the height is smaller than some threshold, then it's grassland. Otherwise, it's a mountain biome. With this implementation, the entire terrain looks much better than the others.

### Efficient Terrain Rendering and Chunking

#### Implementation

The `createChunkVBOdata()` and `createVBOdata()` functions in chunk.cpp are central to this feature. `createChunkVBOdata()` iterates through all blocks in the current chunk and stores the face data of blocks with EMPTY adjacent blocks into the index and VBO arrays. These arrays are then bound by `createVBOdata()` for rendering.

#### Difficulties

Some block faces along chunk borders were not rendering correctly. This issue stemmed from `getAdjacentBlock()` in chunk.cpp. When a border block calls `getAdjacentBlock()`, it needs to check if its adjacent block in a neighboring chunk is EMPTY to correctly determine if a face should be drawn. However, my initial implementation did not correctly identify these cases, leading to missing faces. To address this, I modified the implementation to return EMPTY for adjacent blocks in neighboring chunks regardless of their actual type. This change ensures all border faces are rendered, avoiding gaps in chunk borders.

When expanding the terrain, some neighboring chunks failed to generate while others succeeded. I traced this issue to `m_chunks`, which contained some chunk keys with nullptr values. Initially, the code only checked if a key existed in `m_chunks`, which was insufficient. By additionally verifying that the key’s value is not nullptr, I was able to ensure reliable chunk generation across neighboring regions.

### Game Engine Tick Function and Player Physics

#### Implementation

For handling key presses, I implemented a system where each detected key changes the player's acceleration. The `computePhysics()` function then calculates the resulting velocity and updates the player's position accordingly. For collision detection, I used the grid march method on the 12 vertices around the player and split the movement vector into X, Y, and Z components. This allowed me to test each axis individually, resulting in smoother collision detection. For adding and removing blocks, I also used grid march to identify the correct block for deletion or placement. The position of a new block is determined by the ray direction, and the block is added to the face intersected by the ray.

#### Difficulties

Initially, the player was only blocked by walls two blocks high but could pass through walls one block high. I discovered that when standing on the ground, the player’s actual position along the y-axis was one block higher than it should be, allowing it to pass through shorter walls. I resolved this by moving the player down by one block when standing on the ground.

After completing the milestone, I noticed the game was experiencing lag. The dT (delta time) values varied significantly, ranging from 100 to 600. When dT was large, it caused the player to "fly around" unexpectedly. For example, if the space bar was pressed at frame 0, the player's acceleration.y would increase. If a lag occurred before frame 1, dT could become quite large (e.g., 600), causing the player's velocity to be calculated as acceleration.y * 600, which was not intended. To address this, I clamped the dT values within a certain range to prevent extreme variations and maintain smoother gameplay.

## Milestone 2

[M2 Video](https://youtu.be/z-IgxoBAQag)

### Cave Systems & Post-Process

#### Implementation

##### FOR CAVE SYSTEM, NEED TO UNCOMMENT LINE 327 TO 353 IN TERRAIN.CPP AND BETTER TO COMMENT LINE 358 TO 384 FOR SMOOTH TEST

For the cave generation, I used 3D Perlin Noise to determine whether a block should be empty or solid stone. The noise value is calculated using the position of every block below the base ground level. If the noise value is negative, the block at the sampled coordinate is marked as empty; otherwise, it becomes a stone block. To create more intricate cave structures, I adjusted the coordinates by dividing them by 30, effectively increasing the frequency of the caves.

I implemented a post-processing rendering pipeline by rendering the 3D scene to a framebuffer, then rendering the framebuffer's texture onto a screen-filling quadrilateral with a dedicated post-processing shader program. This shader program dynamically adjusts the scene's appearance based on environmental conditions. For instance, if the character's head is submerged in water or lava, the shader modifies the color of the scene: the blue channel (for water) or red channel (for lava) is set to 1.0, creating a visual tint that immerses the player in the respective environment.

#### Difficulties

One of the major challenges was the computational cost of applying 3D Perlin Noise to every coordinate below the base ground. This approach significantly impacted performance, making the game laggy and difficult to play.

To address this, I implemented sparse sampling. Instead of calculating noise for every block, I sampled Perlin noise every 4 blocks and then used trilinear interpolation to estimate the noise values for the skipped blocks. This method greatly reduced the computational overhead, allowing new chunks to be generated smoothly without lag.

### Texturing and Texture Animation

#### Implementation

I implemented a feature to load textures from image files into OpenGL. Textures were passed to the GPU and associated with a sampler2D in the fragment shader. Each block type in the world was mapped to a specific region in the texture atlas, using UV coordinates scaled to the 16x16 grid. In the `lambert.frag.glsl` shader, I integrated a sampler2D to apply texture colors to block surfaces. I added a time variable to the shader, allowing for UV animation specifically for LAVA and WATER blocks. I also passed a variable from main program to shader program to identify these block types and apply offsets to their UV coordinates, giving the illusion of flowing water and molten lava.

In the `Chunk` class, I split interleaved VBO data into two separate buffers: 1. Opaque blocks, 2. Transparent blocks (e.g., WATER, ICE). This separation ensured proper rendering order, with all opaque blocks rendered first, followed by all transparent blocks. These changes are also applied to the multithreading part (`BlockTypeWorker`).

#### Difficulties

Initially, the textures of some faces on a block are assigned incorrectly, for example, up-side-down or rotated 90 degree. I changed the value in `adjacentF` in `Chunk.h` in order to rotate the face index and make the textures look correct.

### Multithreaded Terrain Generation

#### Implementation

To reduce gameplay interruptions as terrain expands, I implemented multithreading to handle terrain generation tasks off the main thread. I implemented two types of worker threads manage terrain: 1. `BlockTypeWorker`: For ungenerated 64x64 terrain zones near the player, `BlockTypeWorker` threads populate each chunk with biome-specific BlockType data, like grassland or mountain. 2. `VBOWorker`: For existing chunks lacking VBO data, `VBOWorker` threads compute the vertex and index buffers for rendering. I also used `std::mutex`, shared vectors in Terrain store chunk data for processing by worker threads: `dataToBlockTypeWorkerThreads` holds new chunks needing BlockType data. `dataFromBlockTypeWorkerThreads` holds processed chunks, which the main thread transfers to `dataToVBOWorkerThreads`. `dataFromVBOWorkerThreads` stores VBO data from `VBOWorkers` for the main thread to render. In each `tick()`, I identify nearby ungenerated zones, create new chunks, and assign them to `BlockTypeWorkers`. This approach maintains smooth gameplay as the player explores.

#### Difficulties

Initially, race conditions occurred when multiple threads accessed shared vectors simultaneously, causing inconsistent data. Adding `std::mutex` locks around each shared data structure fixed this by ensuring only one thread could access the resource at a time, though it slightly increased processing overhead.

## Milestone 3

[M3 Video](https://youtu.be/76oGmXU-TCg)

### Crosshair

#### Implementation

Implemented based on given file `worldaxes.cpp`, the crosshair is implemented by creating a small, plus-shaped set of lines at the screen center. First, I set up two line segments—one horizontal and one vertical—using a handful of vertices and uniform white color. These vertices are stored in GPU buffers (position, color, index) just like the axes. In the vertex and fragment shaders, no special transformations or texturing are required; the crosshair simply renders in normalized device coordinates over the scene. By using `GL_LINES` to draw these segments, the crosshair remains minimal, ensuring that it does not obscure gameplay elements, yet provides a stable visual reference for aiming and navigation.

### Distance fog

#### Implementation

First, I calculated the fragment position in camera space in `lambert.vert.glsl` and then pass it to the fragment shader. Using the fragment position `fs_Pos.z` in `lambert.frag.glsl` as a measure of distance from the camera, I defined a linear fog gradient that starts at a specified distance and fully envelops the scene by `endDistance`. In the fragment shader, a `fogFactor` is computed based on how far the fragment is from the camera. If the fragment's distance is below `startDistance`, no fog is applied. Between `startDistance` and `endDistance`, the fog gradually intensifies, blending the fragment's color with a specified `u_FogColor`. Past `endDistance`, the fog completely obscures the fragment. By mixing `u_FogColor` with the sampled diffuse texture color, this approach smoothly fades distant objects into the chosen atmospheric color, producing a natural sense of scale and immersion without abruptly cutting off the scene at a fixed rendering distance.

### Procedurally placed assets

#### Implementation

To add variety and interest to the generated terrain, I introduced procedural asset placement. In this snippet, I target suitable areas—well within a 16x16 chunk’s bounds and above the water level—and occasionally place a tree. Using `random1()` in `terrain.cpp` to determine both the probability of spawning and the size of the tree, I generate a trunk made of WOOD blocks and a leaf canopy of LEAF blocks placed around its top. The randomization of height and leaf radius ensures that each tree is unique, lending the biome a more natural, organic feel.

### Water waves

#### Implementation

To simulate gentle rippling water, I introduced a time-based sinusoidal displacement in the vertex shader in the lambert shaders. Each water vertex's y-position is offset using `sin(u_Time + vs_Pos.x + vs_Pos.z)`, producing subtle up-and-down motion. After adjusting vertex positions, I recalculate the normals on-the-fly by considering the derivatives of the wave function. This ensures that the surface's lighting remains correct as the geometry deforms.

In the fragment shader, if the surface is animating (e.g., a water block), I apply a time-driven UV offset to simulate flowing textures. I also re-enable the specular component of the lighting model for these animated surfaces. With this combination, the water both moves and shimmers, catching highlights in a realistic way. The end result is a dynamic, reflective body of water that integrates seamlessly into the environment, adding visual richness and depth.

#### Difficulties

In order to make the specular term more realistic, the `distortedNormal` should be matched with the wave shape. For this purpose, I calculated the gradient of the wave function `sin(u_Time + vs_Pos.x + vs_Pos.z)`, which is equal to (-df/dx, 1, -df/dz) using partial derivative. After calculating the actual gradient and applying it to the normal, the specular matched with the wave's shape.

### Post-process Camera Overlay

#### Implementation

Inside the `if (intensity > 0.0)` block of post processing shader, I apply noise-based UV perturbation and wave deformation to simulate underwater distortion. First, I scale the uv coordinates by the screen resolution to create a more pronounced effect. Using the `noise()` function, I sample two noise values that shift uv along both the x and y axes, introducing random wavy patterns. Next, I add sinusoidal and cosine functions dependent on time and position, causing gentle ripple-like oscillations. These combined transformations produce fluid, dynamic distortions reminiscent of looking through moving water, enhancing immersion when the player is submerged.