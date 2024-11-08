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