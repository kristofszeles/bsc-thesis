#pragma once

#include <string>

const std::string vertexShader = R"glsl(
#version 130

in vec3 vs_in_pos;
in vec3 vs_in_norm;
in vec2 vs_in_tex;

out vec3 vs_out_pos;
out vec3 vs_out_norm;
out vec2 vs_out_tex;
out float opacity;

uniform mat4 mvp;
uniform mat4 model;
uniform mat4 modelIT;
uniform vec3 playerPos;
uniform int renderDistance;

void main() {
    gl_Position = mvp * vec4(vs_in_pos, 1);
    vs_out_pos = (model * vec4(vs_in_pos, 1)).xyz;
    vs_out_norm = (modelIT * vec4(vs_in_norm, 0)).xyz;
    vs_out_tex = vec2(vs_in_tex.x, 1 - vs_in_tex.y);
    float distance = length(playerPos - vs_out_pos);
    opacity = clamp(distance / renderDistance, 0.0, 1.0);
}
)glsl";

const std::string fragmentShader1 = R"glsl(
#version 130

in vec3 vs_out_pos;
in vec3 vs_out_norm;
in vec2 vs_out_tex;
in float opacity;

out vec4 fs_out_col;

uniform sampler2D texImage;

void main() {
    vec3 light_pos = vec3(0, 50, 0);
    vec3 light_color = vec3(0.6, 0.6, 0.6);
    vec3 normal = normalize(vs_out_norm); // calculate unit vector
    vec3 light_direction = normalize(light_pos - vs_out_pos); // calculate light direction
    // ambient light
    vec3 ambient = 0.6 * light_color;
    // diffuse light
    float di = clamp(dot(light_direction, normal), 0.0, 1.0); // diffuse
    vec3 diffuse = di * light_color * 0.4; // calculate diffuse component
    
    vec4 textureColor = texture(texImage, vs_out_tex);
    fs_out_col = 2 * vec4(ambient + diffuse, 1.0) * vec4(1.0, 1.0, 1.0, 1.0 - opacity) * textureColor;
}
)glsl";

const std::string fragmentShader2 = R"glsl(
#version 130

in vec3 vs_out_pos;
in vec3 vs_out_norm;
in vec2 vs_out_tex;

out vec4 fs_out_col;

uniform sampler2D texImage;

void main() {
    fs_out_col = texture(texImage, vs_out_tex);
}
)glsl";