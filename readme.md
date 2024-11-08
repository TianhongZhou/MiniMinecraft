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

#### Difficulties

### Efficient Terrain Rendering and Chunking

#### Implementation

#### Difficulties

### Game Engine Tick Function and Player Physics

#### Implementation

#### Difficulties

Procedural Terrain

Efficient Terrain Rendering and Chunking
difficulty:
unable to show anything after the implementation of chunking. after an entire day of debugging, find that did not change the shader from instanced to lambert.

some faces not showing at the border of chunk. hard-code for all adjchunk to be null

some neighbour chunk unable to generate, some able. problem with m_chunks, it will contain some chunk key with nullptr value, need to check addition to hadChunkAt


Game Engine Tick Function and Player Physics
add aiming
add constrain for rotating

difficulty:
blocked by height 2 but not height 1. found that issues with "standing on the ground using y-axis collision", add 1 on y pos, so the player is actually from 1 to 2 instead of from 0 to 1
跳帧,add max dt
