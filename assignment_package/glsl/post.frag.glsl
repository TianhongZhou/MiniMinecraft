#version 150

uniform sampler2D u_Texture;
uniform int u_Water;

in vec2 fs_UV;

out vec4 out_Col;

void main()
{
    vec3 col = texture(u_Texture, fs_UV).xyz;

    if (u_Water == 1) {
        col.z = 1.0;
    } else if (u_Water == 0) {
        col.x = 1.0;
    }

    out_Col = vec4(col, 1.0);
}
