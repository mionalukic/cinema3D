#version 330 core

out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D texture_diffuse1;

uniform float ambientStrength;   // npr. 0.35 ili 0.02
uniform float lightIntensity;    // npr. 1.0 ili 0.1
uniform bool cinemaDark;

void main()
{
    vec4 texColor = texture(texture_diffuse1, TexCoords);

    if (texColor.a < 0.1)
        discard;

    vec3 color = texColor.rgb;

    vec3 result;

    if (cinemaDark)
    {
        result = color * ambientStrength * 0.3;
    }
    else
    {
        result = color * (ambientStrength + lightIntensity);
    }

    FragColor = vec4(result, texColor.a);
}
