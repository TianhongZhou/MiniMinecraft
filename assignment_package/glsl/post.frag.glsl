#version 150

uniform sampler2D u_Texture;
uniform int u_Water;
uniform float u_Time;
uniform vec2 u_Resolution;

in vec2 fs_UV;

out vec4 out_Col;

float random1(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.54531);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);

    float a = random1(i);
    float b = random1(i + vec2(1.0, 0.0));
    float c = random1(i + vec2(0.0, 1.0));
    float d = random1(i + vec2(1.0, 1.0));

    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

void main()
{
    float time = u_Time * 0.5;
    vec2 uv = fs_UV;

    float intensity = 0.0;
    vec3 overlayColor = vec3(1.0);

    if (u_Water == 1) {
        overlayColor = vec3(0.0, 0.5, 1.0);
        intensity = 0.01;
    } else if (u_Water == 0) {
        overlayColor = vec3(1.0, 0.3, 0.0);
        intensity = 0.02;
    }

    if (intensity > 0.0) {
        vec2 scaledUV = uv * u_Resolution * 0.5;
        float n1 = noise(scaledUV + vec2(time, 0.0));
        float n2 = noise(scaledUV + vec2(0.0, time));
        uv += intensity * vec2(n1 - 0.5, n2 - 0.5);

        float waveFreq = 5.0;
        float waveAmp = 0.01;

        uv.y += sin(uv.x * waveFreq + time * 2.0) * waveAmp;
        uv.x += cos(uv.y * (waveFreq * 0.5) + time) * waveAmp * 0.5;
    }

    vec3 sceneColor = texture(u_Texture, uv).xyz;

    float overlayStrength = (u_Water == 1 || u_Water == 0) ? 0.4 : 0.0;
    vec3 finalColor = mix(sceneColor, sceneColor * overlayColor, overlayStrength);

    out_Col = vec4(finalColor, 1.0);
}
