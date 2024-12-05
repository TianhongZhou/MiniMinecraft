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

out vec4 fs_Pos;
out vec4 fs_Nor;            // The array of normals that has been transformed by u_ModelInvTr. This is implicitly passed to the fragment shader.
out vec4 fs_LightVec;       // The direction in which our virtual light lies, relative to each vertex. This is implicitly passed to the fragment shader.
out vec4 fs_Col;            // The color of each vertex. This is implicitly passed to the fragment shader.
out vec2 fs_UV;
out float fs_IsAnimating;

uniform vec3 u_LightDir;  // The direction of our virtual light, which is used to compute the shading of
                                        // the geometry in the fragment shader.

uniform mat4 u_LightSpaceMatrix;
out vec4 fragPosLightSpace;

void main()
{
    // fs_Pos = vs_Pos;
    fs_Col = vs_Col;                         // Pass the vertex colors to the fragment shader for interpolation
    fs_UV = vs_UV.xy;
    fs_IsAnimating = vs_UV.z;

    fragPosLightSpace = u_LightSpaceMatrix * vs_Pos;

    mat3 invTranspose = mat3(u_ModelInvTr);
    fs_Nor = vs_Nor;

    vec4 modelposition = u_Model * vs_Pos;   // Temporarily store the transformed vertex positions for use below

    fs_LightVec = normalize(vec4(u_LightDir, 0.0));  // Compute the direction in which the light source lies

    fs_Pos = u_ViewProj * modelposition;

    gl_Position = u_ViewProj * modelposition;// gl_Position is a built-in variable of OpenGL which is
                                             // used to render the final positions of the geometry's vertices
}
