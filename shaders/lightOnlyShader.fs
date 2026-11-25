#version 330

//precision mediump float;

in vec2 fragTexCoord;
in vec4 fragColor;


out vec4 finalColor;
uniform vec2 lightPos;
uniform vec2 renderSize;

uniform sampler2D texture0;

void main()
{
    vec2 size = renderSize.x > 0.0 && renderSize.y > 0.0 ? renderSize : vec2(1024.0, 512.0);
    vec2 uv = gl_FragCoord.xy / size;
    vec2 lightUV = lightPos / size;

    vec2 correctedUV = vec2((uv.x - lightUV.x) * 2.0, (1.0 - uv.y - lightUV.y));

    float dist = length(correctedUV);
    float alpha = smoothstep(0.2, 0.3, dist);

    vec4 texColor = texture(texture0, fragTexCoord) * fragColor;

    finalColor = vec4(texColor.rgb, clamp(texColor.a - alpha, 0.0, 1.0));
}