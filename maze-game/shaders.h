#pragma once

#include <string>

#include "gl_core.h"

// macOS: SDL defaults to a legacy OpenGL 2.1 context; only GLSL 1.20 is accepted.
// Android / Emscripten (MAZE_GLES): OpenGL ES 3.0 (WebGL 2 in the browser).
// Other desktop platforms: GLSL 1.50 with in/out (typical with GL 3.2+ contexts).

#if defined(__APPLE__)

const std::string vertexShader = R"glsl(
#version 120

attribute vec3 vs_in_pos;
attribute vec3 vs_in_norm;
attribute vec2 vs_in_tex;

varying vec3 vs_out_pos;
varying vec3 vs_out_norm;
varying vec2 vs_out_tex;
varying float opacity;

uniform mat4 mvp;
uniform mat4 model;
uniform mat4 modelIT;
uniform vec3 playerPos;
uniform int renderDistance;

void main() {
    gl_Position = mvp * vec4(vs_in_pos, 1);
    vs_out_pos = (model * vec4(vs_in_pos, 1)).xyz;
    vs_out_norm = (modelIT * vec4(vs_in_norm, 0)).xyz;
    vs_out_tex = vec2(vs_in_tex.x, 1.0 - vs_in_tex.y);
    float dist = length(playerPos - vs_out_pos);
    opacity = clamp(dist / float(renderDistance), 0.0, 1.0);
}
)glsl";

const std::string vertexShaderUnlit = R"glsl(
#version 120

attribute vec3 vs_in_pos;
attribute vec3 vs_in_norm;
attribute vec2 vs_in_tex;

varying vec2 vs_out_tex;

uniform mat4 mvp;

void main() {
    gl_Position = mvp * vec4(vs_in_pos + 0.0 * vs_in_norm, 1.0);
    vs_out_tex = vec2(vs_in_tex.x, 1.0 - vs_in_tex.y);
}
)glsl";

const std::string fragmentShader1 = R"glsl(
#version 120

varying vec3 vs_out_pos;
varying vec3 vs_out_norm;
varying vec2 vs_out_tex;
varying float opacity;

uniform sampler2D texImage;

void main() {
    vec3 light_pos = vec3(0.0, 50.0, 0.0);
    vec3 light_color = vec3(0.6, 0.6, 0.6);
    vec3 normal = normalize(vs_out_norm);
    vec3 light_direction = normalize(light_pos - vs_out_pos);
    vec3 ambient = 0.6 * light_color;
    float di = clamp(dot(light_direction, normal), 0.0, 1.0);
    vec3 diffuse = di * light_color * 0.4;

    vec4 textureColor = texture2D(texImage, vs_out_tex);
    gl_FragColor = 2.0 * vec4(ambient + diffuse, 1.0) * vec4(1.0, 1.0, 1.0, 1.0 - opacity) * textureColor;
}
)glsl";

const std::string fragmentShader2 = R"glsl(
#version 120

varying vec2 vs_out_tex;

uniform sampler2D texImage;

void main() {
    gl_FragColor = texture2D(texImage, vs_out_tex);
}
)glsl";

#elif defined(MAZE_GLES)

// WebGL requires #version to be the very first line of the source, so the raw
// literals below must open directly with it (no leading newline).
const std::string vertexShader = R"glsl(#version 300 es

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
    gl_Position = mvp * vec4(vs_in_pos, 1.0);
    vs_out_pos = (model * vec4(vs_in_pos, 1.0)).xyz;
    vs_out_norm = (modelIT * vec4(vs_in_norm, 0.0)).xyz;
    vs_out_tex = vec2(vs_in_tex.x, 1.0 - vs_in_tex.y);
    float dist = length(playerPos - vs_out_pos);
    opacity = clamp(float(renderDistance) > 0.0 ? dist / float(renderDistance) : 0.0, 0.0, 1.0);
}
)glsl";

const std::string vertexShaderUnlit = R"glsl(#version 300 es

in vec3 vs_in_pos;
in vec3 vs_in_norm;
in vec2 vs_in_tex;

out vec2 vs_out_tex;

uniform mat4 mvp;

void main() {
    gl_Position = mvp * vec4(vs_in_pos + 0.0 * vs_in_norm, 1.0);
    vs_out_tex = vec2(vs_in_tex.x, 1.0 - vs_in_tex.y);
}
)glsl";

const std::string fragmentShader1 = R"glsl(#version 300 es
precision mediump float;

in vec3 vs_out_pos;
in vec3 vs_out_norm;
in vec2 vs_out_tex;
in float opacity;

uniform sampler2D texImage;

out vec4 fs_out_col;

void main() {
    vec3 light_pos = vec3(0.0, 50.0, 0.0);
    vec3 light_color = vec3(0.6, 0.6, 0.6);
    vec3 normal = normalize(vs_out_norm);
    vec3 light_direction = normalize(light_pos - vs_out_pos);
    vec3 ambient = 0.6 * light_color;
    float di = clamp(dot(light_direction, normal), 0.0, 1.0);
    vec3 diffuse = di * light_color * 0.4;

    vec4 textureColor = texture(texImage, vs_out_tex);
    fs_out_col = 2.0 * vec4(ambient + diffuse, 1.0) * vec4(1.0, 1.0, 1.0, 1.0 - opacity) * textureColor;
}
)glsl";

const std::string fragmentShader2 = R"glsl(#version 300 es
precision mediump float;

in vec2 vs_out_tex;

uniform sampler2D texImage;

out vec4 fs_out_col;

void main() {
    fs_out_col = texture(texImage, vs_out_tex);
}
)glsl";

#else

const std::string vertexShader = R"glsl(
#version 150

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

const std::string vertexShaderUnlit = R"glsl(
#version 150

in vec3 vs_in_pos;
in vec3 vs_in_norm;
in vec2 vs_in_tex;

out vec2 vs_out_tex;

uniform mat4 mvp;

void main() {
    gl_Position = mvp * vec4(vs_in_pos + 0.0 * vs_in_norm, 1.0);
    vs_out_tex = vec2(vs_in_tex.x, 1 - vs_in_tex.y);
}
)glsl";

const std::string fragmentShader1 = R"glsl(
#version 150

in vec3 vs_out_pos;
in vec3 vs_out_norm;
in vec2 vs_out_tex;
in float opacity;

out vec4 fs_out_col;

uniform sampler2D texImage;

void main() {
    vec3 light_pos = vec3(0, 50, 0);
    vec3 light_color = vec3(0.6, 0.6, 0.6);
    vec3 normal = normalize(vs_out_norm);
    vec3 light_direction = normalize(light_pos - vs_out_pos);
    vec3 ambient = 0.6 * light_color;
    float di = clamp(dot(light_direction, normal), 0.0, 1.0);
    vec3 diffuse = di * light_color * 0.4;

    vec4 textureColor = texture(texImage, vs_out_tex);
    fs_out_col = 2 * vec4(ambient + diffuse, 1.0) * vec4(1.0, 1.0, 1.0, 1.0 - opacity) * textureColor;
}
)glsl";

const std::string fragmentShader2 = R"glsl(
#version 150

in vec2 vs_out_tex;

out vec4 fs_out_col;

uniform sampler2D texImage;

void main() {
    fs_out_col = texture(texImage, vs_out_tex);
}
)glsl";

#endif
