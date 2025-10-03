#version 330

//precision mediump float;

in vec2 fragTexCoord;
in vec4 fragColor;


out vec4 finalColor;
uniform vec2 lightPos;

uniform sampler2D tex;

void main()
{
    vec2 uv = gl_FragCoord.xy / vec2(1024.0, 512.0);
    vec2 lightUV = lightPos / vec2(1024.0, 512.0);

    vec2 correctedUV = vec2((uv.x - lightUV.x) * 2.0, (1.0 - uv.y - lightUV.y));

    float dist = length(correctedUV);
    float alpha = smoothstep(0.2, 0.3, dist);

    vec4 texColor = texture(tex, fragTexCoord);

    finalColor = vec4(texColor.rgb, texColor.a -alpha);
}