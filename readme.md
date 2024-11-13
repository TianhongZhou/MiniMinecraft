## Milestone 1

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

### Multithreaded Terrain Generation

#### Implementation

To reduce gameplay interruptions as terrain expands, I implemented multithreading to handle terrain generation tasks off the main thread. I implemented two types of worker threads manage terrain: 1. `BlockTypeWorker`: For ungenerated 64x64 terrain zones near the player, `BlockTypeWorker` threads populate each chunk with biome-specific BlockType data, like grassland or mountain. 2. `VBOWorker`: For existing chunks lacking VBO data, `VBOWorker` threads compute the vertex and index buffers for rendering. I also used `std::mutex`, shared vectors in Terrain store chunk data for processing by worker threads: `dataToBlockTypeWorkerThreads` holds new chunks needing BlockType data. `dataFromBlockTypeWorkerThreads` holds processed chunks, which the main thread transfers to `dataToVBOWorkerThreads`. `dataFromVBOWorkerThreads` stores VBO data from `VBOWorkers` for the main thread to render. In each `tick()`, I identify nearby ungenerated zones, create new chunks, and assign them to `BlockTypeWorkers`. This approach maintains smooth gameplay as the player explores.

#### Difficulties

Initially, race conditions occurred when multiple threads accessed shared vectors simultaneously, causing inconsistent data. Adding `std::mutex` locks around each shared data structure fixed this by ensuring only one thread could access the resource at a time, though it slightly increased processing overhead.