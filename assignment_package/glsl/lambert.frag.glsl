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

uniform vec3 u_FogColor;
uniform float u_FogDensity;

out vec4 out_Col; // This is the final output color that you will see on your
// screen for the pixel that is currently being processed.

uniform sampler2D shadowMap;
in vec4 fragPosLightSpace;

float ShadowCalculation(vec4 fragPosLightSpace) {
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    float closestDepth = texture(shadowMap, projCoords.xy).r;
    float currentDepth = projCoords.z;

    float bias = 0.005;
    float shadow = (currentDepth - bias) > closestDepth ? 0.5 : 1.0;

    return shadow;
}

void main()
{
    vec2 uv = fs_UV;

    if (fs_IsAnimating > 0.5) {
        float speed = 80.0;
        uv.x += mod(u_Time, speed / 16.0) / speed;
    }
    vec4 diffuseColor = texture(u_Texture, uv);

    // Calculate the diffuse term for Lambert shading
    float diffuseTerm = dot(normalize(fs_Nor), normalize(fs_LightVec));
    // Avoid negative lighting values
    diffuseTerm = clamp(diffuseTerm, 0, 1);

    float ambientTerm = 0.2;
    float lightIntensity = diffuseTerm + ambientTerm;   //Add a small float value to the color multiplier
    //to simulate ambient lighting. This ensures that faces that are not
    //lit by our point light are not completely black.

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

    float shadow = ShadowCalculation(fragPosLightSpace);
    vec3 litColor = diffuseColor.rgb * lightIntensity * shadow;
    vec3 finalColor = mix(u_FogColor, litColor, fogFactor);

    out_Col = vec4(finalColor, diffuseColor.a);

    vec4 shadowColor = texture(shadowMap, fragPosLightSpace.xy);
    out_Col = vec4(vec3(shadowColor.r), 1.0);

    out_Col = vec4(finalColor * lightIntensity, diffuseColor.a);

    // Compute final shaded color
    // out_Col = vec4(diffuseColor.rgb * lightIntensity, diffuseColor.a);
}
