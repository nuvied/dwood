#version 300 es

precision mediump float;

in vec2 fragTexCoord;
in vec4 fragColor;

out vec4 finalColor;

uniform vec2 lightPos;

void main()
{
    vec2 uv = vec2(gl_FragCoord.x / 1024.0,  1.0 - (gl_FragCoord.y / 512.0));
    vec2 lightUV = lightPos / vec2(1024.0,512.0);

    vec2 correctUv = vec2((uv.x - lightUV.x) * 2.0, uv.y - lightUV.y);
    float dist = length(correctUv);
    //float alpha = 1.0/(1.0+dist * dist * 0.01);
    float alpha = smoothstep(0.0,.4,dist);

    float color = smoothstep(.3,0.0, dist);

    finalColor = vec4(vec3(0.0,0.0,0.1) + color ,alpha + (color));
}