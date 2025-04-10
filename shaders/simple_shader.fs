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
};
uniform int light_count;
uniform Light lights[MAX_LIGHTS];

void main()
{
    vec2 uv = vec2(gl_FragCoord.x / 1024.0,  1.0 - (gl_FragCoord.y / 512.0));


    float light = 0.0;
    for(int i = 0; i < MAX_LIGHTS; i++)
    {
        if(i >= light_count)break;

        vec2 lightUV = lights[i].pos / vec2(1024.0,512.0);

        vec2 correctUv = vec2((uv.x - lightUV.x) * 2.0, uv.y - lightUV.y);

        float dist = length(correctUv);

        if(dist < lights[i].radius){
           
             float attenuation = 1.0 - (dist / lights[i].radius);
            light += attenuation * attenuation; // smooth falloff
        }
        
       // light += smoothstep(0.0,1.0,dist);
        
        
    }
    // float dist = length(correctUv);
    // //float alpha = 1.0/(1.0+dist * dist * 0.01);
    // float alpha = smoothstep(0.0,.4,dist);

    // float color = smoothstep(.3,0.0, dist);
   light = clamp(light, 0.0, 1.0);

    // Darkness with transparent holes near lights
    float darkness = 1.0 - light;
    finalColor = vec4(0.0, 0.0, 0.1, darkness - 0.35); // black with alpha fade
}