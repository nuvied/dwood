#version 100 core

// Input from the vertex shader
in vec2 fragTexCoord;

// Texture sampler
uniform sampler2D texture0;

// Output color
out vec4 finalColor;

void main()
{
    // Sample the texture color
    vec4 texColor = texture(texture0, fragTexCoord);

    // Invert the color
    finalColor = vec4(1.0 - texColor.r, 1.0 - texColor.g, 1.0 - texColor.b, texColor.a);
}