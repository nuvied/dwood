#version 330

//precision mediump float;



out vec4 finalColor;
uniform vec2 lensePos;


void main()
{
    vec2 uv = gl_FragCoord.xy / vec2(1024.0, 512.0);
    vec2 lightUV = lensePos / vec2(1024.0, 512.0);

    vec2 correctedUV = vec2((uv.x - lightUV.x) * 2.0, (1.0 - uv.y - lightUV.y));

    float dist = length(correctedUV);

    vec2 shineUV = vec2(correctedUV.x, correctedUV.y + 0.15);

    float color_dist = length(shineUV);

    float color = smoothstep(1.0,0.0, color_dist);

    float a = smoothstep(.2, .4, dist);
    float alpha = (a + color);
    alpha = clamp(alpha,0.0,1.0);


    //vec3 addedColor = vec3(texColor.r + color.r, texColor.g + color.g, texColor.b+color.b);
     vec3 c = vec3(0.0,0.1,0.2) - color * 0.5;

     finalColor = vec4(c, a - 0.55);
}