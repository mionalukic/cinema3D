#version 330 core

in vec4 channelCol;
in vec2 channelTex;
in vec3 Normal;
in vec3 FragPos;

out vec4 outCol;

uniform sampler2D uTex;
uniform bool useTex;

uniform vec3 lightDir;
uniform vec3 lightColor;
uniform float lightIntensity;
uniform bool lightEnabled;
uniform float screenZ;
uniform float ambientStrength;
uniform bool useScreenFalloff;

uniform int emissive;
uniform float emissiveStrength;

void main()
{
    vec4 baseColor = useTex ? texture(uTex, channelTex) : channelCol;
    vec3 color = baseColor.rgb;

    if (emissive == 1)
    {
        outCol = vec4(color * emissiveStrength, baseColor.a);
        return;
    }

   if (lightEnabled)
    {
        vec3 N = normalize(Normal);
        vec3 L = normalize(-lightDir);

        float diff = max(dot(N, L), 0.0);

        float falloff = 1.0;

        if (useScreenFalloff)
        {
            float dist = abs(FragPos.z - screenZ);
            falloff = clamp(exp(-dist * 0.5), 0.0, 1.0);
        }


        vec3 ambient = ambientStrength * lightColor;

        vec3 diffuse = diff * lightColor * lightIntensity * falloff;

        color = color * ambient + color * diffuse;
    }


    outCol = vec4(color, baseColor.a);
}
