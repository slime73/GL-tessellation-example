/**
 * This software is in the public domain. Where that dedication is not recognized,
 * you are granted a perpetual, irrevokable license to copy and modify this file
 * as you see fit.
 *
 * Simple example of an OpenGL Tessellation Evaluation shader used to create
 * quads from point-vertices. Requires SDL and OpenGL 4.
 * Will not run outside of OS X without modifications (e.g. fetching the OpenGL
 * function pointers manually at runtime.)
 * It will look like this when run successfully: http://i.imgur.com/91drvrY.png
 **/

#include <OpenGL/gl3.h>

#include <SDL2/SDL.h>

#include <vector>
#include <cstdlib>
#include <ctime>

// Structure for per-quad vertex attributes.
struct QuadVertex
{
    GLfloat x, y;
    GLfloat size;
    GLubyte r, g, b, a;
};

static SDL_Window *window    = nullptr;
static SDL_GLContext context = nullptr;

static const size_t kRows    = 10;
static const size_t kColumns = 10;

static const float kInnerTessellationLevel = 1.0f;
static const float kOuterTessellationLevel = 1.0f;

static const char VertexShaderSource[] = R"(
#version 410 core

layout(location = 0) in vec4 inPosition;
layout(location = 1) in float inSize;
layout(location = 2) in vec4 inColor;

// Per-quad output variables.
out Quad
{
    float size;
    vec4 color;
} outQuad;

void main()
{
    outQuad.size = inSize;
    outQuad.color = inColor;

    // Pass position along to the next stage. The actual work is done in the
    // Tessellation Evaluation shader.
    gl_Position = inPosition;
}
)";

static const char TessEvaluationShaderSource[] = R"(
#version 410 core

layout(quads, equal_spacing) in;

// Per-quad input variables from the vertex shader.
in Quad
{
    float size;
    vec4 color;
} inQuad[];

out vec4 QuadColor;

void main()
{
    QuadColor = inQuad[0].color;

    // Start with the point-position passed down from the vertex shader.
    gl_Position = gl_in[0].gl_Position;

    // gl_TessCoord ranges from [0, 1] across the entire quad.
    gl_Position.xy += (gl_TessCoord.xy * 2.0 - 1.0) * inQuad[0].size;
}
)";

static const char FragmentShaderSource[] = R"(
#version 410 core

uniform vec3 ConstantColor;

in vec4 QuadColor;
out vec4 FragColor;

void main()
{
    FragColor = QuadColor + vec4(ConstantColor, 0.0);
}
)";

static GLuint CreateShader(GLenum type, const char *src)
{
    GLuint shader = glCreateShader(type);

    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint status = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

    if (status == GL_FALSE) {
        GLchar log[512] = {0};
        glGetShaderInfoLog(shader, 512, nullptr, log);

        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Shader compilation failed", (const char *) log, window);

        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

static GLuint CreateShaderProgram(const std::vector<GLuint> &shaders)
{
    GLuint program = glCreateProgram();

    for (GLuint shader : shaders) {
        glAttachShader(program, shader);
    }

    glLinkProgram(program);

    GLint status = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &status);

    if (status == GL_FALSE) {
        GLchar log[512] = {0};
        glGetProgramInfoLog(program, 512, nullptr, log);

        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Shader program link failed", (const char *) log, window);

        glDeleteProgram(program);
        return 0;
    }

    return program;
}

static bool HandleEvents()
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_WINDOWEVENT:
                if (e.window.event == SDL_WINDOWEVENT_RESIZED) {
                    glViewport(0, 0, e.window.data1, e.window.data2);
                }
                break;
            case SDL_QUIT:
                return false;
            default:
                break;
        }
    }

    return true;
}

static int CleanupSDL(int status)
{
    if (context) {
        SDL_GL_DeleteContext(context);
        context = nullptr;
    }

    if (window) {
        SDL_DestroyWindow(window);
        window = nullptr;
    }

    SDL_QuitSubSystem(SDL_WasInit(SDL_INIT_EVERYTHING));

    return status;
}

int main(int argc, char *argv[])
{
    srand((unsigned) time(nullptr));

    if (SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
        SDL_Log("Error initializing SDL: %s", SDL_GetError());
        return CleanupSDL(1);
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    window = SDL_CreateWindow("Quads", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error creating window", SDL_GetError(), nullptr);
        return CleanupSDL(1);
    }

    context = SDL_GL_CreateContext(window);
    if (!context) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error creating OpenGL 4.1 context", SDL_GetError(), window);
        return CleanupSDL(1);
    }

    std::vector<QuadVertex> quadData;
    quadData.reserve(kRows * kColumns);

    for (size_t row = 0; row < kRows; row++) {
        for (size_t column = 0; column < kColumns; column++) {
            QuadVertex quad = {};

            quad.x = (2.0f * (0.5f + column) / (float) kColumns) - 1.0f;
            quad.y = (2.0f * (0.5f + row) / (float) kRows) - 1.0f;

            quad.size = 0.05f + ((float) rand() / (float) RAND_MAX) * 0.04f;

            quad.r = GLubyte(96 + rand() % 128);
            quad.g = GLubyte(96 + rand() % 128);
            quad.b = GLubyte(96 + rand() % 128);
            quad.a = 255;

            quadData.push_back(quad);
        }
    }

    GLuint dummyVAO;
    glGenVertexArrays(1, &dummyVAO);
    glBindVertexArray(dummyVAO);

    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    glBufferData(GL_ARRAY_BUFFER, sizeof(QuadVertex) * quadData.size(), quadData.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(QuadVertex), (char *) offsetof(QuadVertex, x));
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(QuadVertex), (char *) offsetof(QuadVertex, size));
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(QuadVertex), (char *) offsetof(QuadVertex, r));

    glEnableVertexAttribArray(0); // Per-quad position.
    glEnableVertexAttribArray(1); // Per-quad size.
    glEnableVertexAttribArray(2); // Per-quad color.

    std::vector<GLuint> shaderObjects = {
        CreateShader(GL_VERTEX_SHADER, VertexShaderSource),
        CreateShader(GL_TESS_EVALUATION_SHADER, TessEvaluationShaderSource),
        CreateShader(GL_FRAGMENT_SHADER, FragmentShaderSource),
    };

    GLuint shaderProgram = CreateShaderProgram(shaderObjects);
    GLint colorLocation = glGetUniformLocation(shaderProgram, "ConstantColor");

    const GLfloat innerTessLevels[2] = {
        kInnerTessellationLevel, // inner horizontal
        kInnerTessellationLevel  // inner vertical
    };

    const GLfloat outerTessLevels[4] = {
        kOuterTessellationLevel, // outer left (vertical)
        kOuterTessellationLevel, // outer bottom (horizontal)
        kOuterTessellationLevel, // outer right (vertical)
        kOuterTessellationLevel  // outer top (horizontal)
    };

    // We can define the tessellation levels using glPatchParameter if we don't
    // have a Tessellation Control Shader stage.
    glPatchParameterfv(GL_PATCH_DEFAULT_INNER_LEVEL, innerTessLevels);
    glPatchParameterfv(GL_PATCH_DEFAULT_OUTER_LEVEL, outerTessLevels);

    glUseProgram(shaderProgram);

    while (true) {
        if (!HandleEvents()) {
            break;
        }

        glClearColor(0.0f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        // One vertex becomes one tessellated quad.
        glPatchParameteri(GL_PATCH_VERTICES, 1);

        // Draw the tessellated quads.
        glUniform3f(colorLocation, 0.0f, 0.0f, 0.0f);
        glDrawArrays(GL_PATCHES, 0, (GLsizei) (kRows * kColumns));

        // Draw the tessellated quad primitives as wireframe.
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glUniform3f(colorLocation, 1.0f, 1.0f, 1.0f);
        glDrawArrays(GL_PATCHES, 0, (GLsizei) (kRows * kColumns));
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        SDL_GL_SwapWindow(window);
    }

    for (GLuint shaderObject : shaderObjects) {
        glDeleteShader(shaderObject);
    }

    glDeleteProgram(shaderProgram);

    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &dummyVAO);

    return CleanupSDL(0);
}