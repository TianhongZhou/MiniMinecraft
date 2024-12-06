#version 150
// ^ Change this to version 130 if you have compatibility issues

// This is a fragment shader. If you've opened this file first, please
// open and read lambert.vert.glsl before reading on.
// Unlike the vertex shader, the fragment shader actually does compute
// the shading of geometry. For every pixel in your program's output
// screen, the fragment shader is run for every bit of geometry that
// particular pixel overlaps. By implicitly interpolating the position
// data passed into the fragment shader by the vertex shader, the fragment shader
// can compute what color to apply to its pixel based on things like vertex
// position, light position, and vertex color.

uniform vec4 u_Color; // The color with which to render this instance of geometry.
uniform sampler2D u_Texture;
uniform float u_Time;

// These are the interpolated values out of the rasterizer, so you can't know
// their specific values without knowing the vertices that contributed to them
in vec4 fs_Pos;
in vec4 fs_Nor;
in vec4 fs_LightVec;
in vec4 fs_Col;
in vec2 fs_UV;
in float fs_IsAnimating;
in vec3 fs_DistortedNor;
in vec3 fs_worldPos;

uniform vec3 u_Cam;

uniform vec3 u_FogColor;
uniform float u_FogDensity;

out vec4 out_Col; // This is the final output color that you will see on your
// screen for the pixel that is currently being processed.

void main()
{
    vec2 uv = fs_UV;

    vec3 normal = normalize(fs_Nor.rgb);
    if (fs_IsAnimating > 0.5) {
        float speed = 80.0;
        uv.x += mod(u_Time, speed / 16.0) / speed;
        normal = normalize(fs_DistortedNor);
    }

    vec3 L = normalize(vec3(fs_LightVec));
    vec3 V = normalize(u_Cam - fs_worldPos);

    vec3 H = normalize(L + V);
    float diffuseTerm = max(dot(normal, L), 0.0);
    float specularTerm = pow(max(dot(normal, H), 0.0), 32.0);

    vec3 ambient = vec3(0.2);
    vec3 diffuse = vec3(1.0) * diffuseTerm;
    vec3 specular = vec3(1.0) * specularTerm;

    if (fs_IsAnimating <= 0.5) {
        specular = vec3(0.0);
    }

    vec3 lightIntensity = ambient + diffuse + specular;

    float distance = fs_Pos.z;

    float startDistance = 65.0;
    float endDistance = 120.0;

    float fogFactor = 1.0;
    if (distance > startDistance) {
        if (distance < endDistance) {
            fogFactor = 1.0 - (distance - startDistance) / (endDistance - startDistance);
        } else {
            fogFactor = 0.0;
        }
    }

    fogFactor = clamp(fogFactor, 0.0, 1.0);


    vec4 diffuseColor = texture(u_Texture, uv);
    vec3 finalColor = mix(u_FogColor, diffuseColor.rgb, fogFactor);

    out_Col = vec4(finalColor * lightIntensity, diffuseColor.a);

    // Compute final shaded color
    // out_Col = vec4(diffuseColor.rgb * lightIntensity, diffuseColor.a);
}
