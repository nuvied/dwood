#version 330 



in vec2 fragTexCoord;
// in vec4 fragColor;

out vec4 finalColor;

// uniform vec2 lightPos;
#define MAX_LIGHTS 8

struct Light
{
    float intensity;
    vec2 pos;
    float radius;
    vec3 color;
};
uniform int light_count;
uniform Light lights[MAX_LIGHTS];
uniform sampler2D tex;
uniform vec2 renderSize;
uniform float baseDarkness;

void main()
{
    vec2 uv = vec2(gl_FragCoord.x / renderSize.x,
                   1.0 - (gl_FragCoord.y / renderSize.y));
    vec2 aspectScale = vec2(renderSize.x / renderSize.y, 1.0);

    vec3 light = vec3(0.0);
    for(int i = 0; i < MAX_LIGHTS; i++)
    {
        if(i >= light_count)break;

        vec2 lightUV = lights[i].pos / renderSize;
        vec2 correctUv = (uv - lightUV) * aspectScale;

        float dist = length(correctUv);

        if(dist < lights[i].radius){
           
            float attenuation = 1.0 - (dist / lights[i].radius);
            light += lights[i].color * attenuation * attenuation * lights[i].intensity;
        }
        
       // light += smoothstep(0.0,1.0,dist);
        
        
    }
    // float dist = length(correctUv);
    // //float alpha = 1.0/(1.0+dist * dist * 0.01);
    // float alpha = smoothstep(0.0,.4,dist);

    // float color = smoothstep(.3,0.0, dist);
    light = clamp(light, 0.0, 1.0);

    // Darkness with transparent holes near lights
    float brightness = max(light.r, max(light.g, light.b));

    vec4 texColor = texture(tex, fragTexCoord);
    
    finalColor = vec4(light * 0.2, max(baseDarkness - brightness, 0.0));

}