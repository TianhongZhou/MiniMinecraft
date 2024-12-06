#version 150
// ^ Change this to version 130 if you have compatibility issues

//This is a vertex shader. While it is called a "shader" due to outdated conventions, this file
//is used to apply matrix transformations to the arrays of vertex data passed to it.
//Since this code is run on your GPU, each vertex is transformed simultaneously.
//If it were run on your CPU, each vertex would have to be processed in a FOR loop, one at a time.
//This simultaneous transformation allows your program to run much faster, especially when rendering
//geometry with millions of vertices.

uniform mat4 u_Model;       // The matrix that defines the transformation of the
                            // object we're rendering. In this assignment,
                            // this will be the result of traversing your scene graph.

uniform mat4 u_ModelInvTr;  // The inverse transpose of the model matrix.
                            // This allows us to transform the object's normals properly
                            // if the object has been non-uniformly scaled.

uniform mat4 u_ViewProj;    // The matrix that defines the camera's transformation.
                            // We've written a static matrix for you to use for HW2,
                            // but in HW3 you'll have to generate one yourself
uniform mat4 u_View;

uniform vec4 u_Color;       // When drawing the cube instance, we'll set our uniform color to represent different block types.

in vec4 vs_Pos;             // The array of vertex positions passed to the shader

in vec4 vs_Nor;             // The array of vertex normals passed to the shader

in vec4 vs_Col;             // The array of vertex colors passed to the shader.

in vec4 vs_UV;
uniform float u_Time;

out vec4 fs_Pos;
out vec4 fs_Nor;            // The array of normals that has been transformed by u_ModelInvTr. This is implicitly passed to the fragment shader.
out vec4 fs_LightVec;       // The direction in which our virtual light lies, relative to each vertex. This is implicitly passed to the fragment shader.
out vec4 fs_Col;            // The color of each vertex. This is implicitly passed to the fragment shader.
out vec2 fs_UV;
out float fs_IsAnimating;

out vec3 fs_DistortedNor;
out vec3 fs_worldPos;

uniform vec3 u_LightDir;  // The direction of our virtual light, which is used to compute the shading of
                                        // the geometry in the fragment shader.

void main()
{
    fs_Col = vs_Col;                         // Pass the vertex colors to the fragment shader for interpolation
    fs_UV = vs_UV.xy;
    fs_IsAnimating = vs_UV.z;

    mat3 invTranspose = mat3(u_ModelInvTr);
    fs_Nor = vs_Nor;

    float vs_y = vs_Pos.y;
    vec3 displacedPos = vec3(vs_Pos.x, vs_y, vs_Pos.z);
    vec3 distortedNor = vec3(vs_Nor);

    if (fs_IsAnimating > 0.5) {
        float wave = sin(u_Time * 0.5 + vs_Pos.x * 1.0 + vs_Pos.z * 1.0);
        vs_y += wave * 0.5;

        float wavePhase = u_Time * 0.5 + vs_Pos.x * 1.0 + vs_Pos.z * 1.0;
        float dfdx = 0.5 * cos(wavePhase);
        float dfdz = 0.5 * cos(wavePhase);

        vec3 waveGradient = vec3(-dfdx, 1.0, -dfdz);
        distortedNor = normalize(waveGradient);
    }

    displacedPos.y = vs_y;
    fs_DistortedNor = distortedNor;

    vec4 modelposition = u_Model * vec4(vs_Pos.x, vs_y, vs_Pos.z, 1.0);   // Temporarily store the transformed vertex positions for use below
    fs_worldPos = modelposition.xyz;
    fs_LightVec = normalize(vec4(u_LightDir, 0.0));  // Compute the direction in which the light source lies
    fs_Pos = u_ViewProj * modelposition;
    gl_Position = u_ViewProj * modelposition;// gl_Position is a built-in variable of OpenGL which is
                                             // used to render the final positions of the geometry's vertices
}
