#ifdef WIN32
#include <SDL.h>
#undef main
#else
#include <SDL2/SDL.h>
#endif

#include <GL/glew.h>

#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <random>
#include <stdexcept>
#include <string_view>
#include <vector>

#define GLM_FORCE_SWIZZLE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/scalar_constants.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "obj_parser.hpp"
#include "stb_image.h"

std::string to_string(std::string_view str) {
	return std::string(str.begin(), str.end());
}

void sdl2_fail(std::string_view message) {
	throw std::runtime_error(to_string(message) + SDL_GetError());
}

void glew_fail(std::string_view message, GLenum error) {
	throw std::runtime_error(
		to_string(message) +
		reinterpret_cast<const char *>(glewGetErrorString(error)));
}

std::string read_shader(const std::string &file) {
	std::ifstream stream(file);
	if (!stream.is_open()) {
		throw std::runtime_error("Can't open shader file: " + file);
	}
	std::stringstream buffer;
	buffer << stream.rdbuf();
	stream.close();
	return buffer.str();
}

GLuint create_shader(GLenum type, const char *source) {
	GLuint result = glCreateShader(type);
	glShaderSource(result, 1, &source, nullptr);
	glCompileShader(result);
	GLint status;
	glGetShaderiv(result, GL_COMPILE_STATUS, &status);
	if (status != GL_TRUE) {
		GLint info_log_length;
		glGetShaderiv(result, GL_INFO_LOG_LENGTH, &info_log_length);
		std::string info_log(info_log_length, '\0');
		glGetShaderInfoLog(result, info_log.size(), nullptr, info_log.data());
		throw std::runtime_error("Shader compilation failed: " + info_log);
	}
	return result;
}

template <typename... Shaders> GLuint create_program(Shaders... shaders) {
	GLuint result = glCreateProgram();
	(glAttachShader(result, shaders), ...);
	glLinkProgram(result);

	GLint status;
	glGetProgramiv(result, GL_LINK_STATUS, &status);
	if (status != GL_TRUE) {
		GLint info_log_length;
		glGetProgramiv(result, GL_INFO_LOG_LENGTH, &info_log_length);
		std::string info_log(info_log_length, '\0');
		glGetProgramInfoLog(result, info_log.size(), nullptr, info_log.data());
		throw std::runtime_error("Program linkage failed: " + info_log);
	}

	return result;
}

static glm::vec3 cube_vertices[]{
	{0.f, 0.f, 0.f}, {1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {1.f, 1.f, 0.f},
	{0.f, 0.f, 1.f}, {1.f, 0.f, 1.f}, {0.f, 1.f, 1.f}, {1.f, 1.f, 1.f},
};

static std::uint32_t cube_indices[]{
	// -Z
	0,
	2,
	1,
	1,
	2,
	3,
	// +Z
	4,
	5,
	6,
	6,
	5,
	7,
	// -Y
	0,
	1,
	4,
	4,
	1,
	5,
	// +Y
	2,
	6,
	3,
	3,
	6,
	7,
	// -X
	0,
	4,
	2,
	2,
	4,
	6,
	// +X
	1,
	3,
	5,
	5,
	3,
	7,
};

int main() try {
	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		sdl2_fail("SDL_Init: ");
	}

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
						SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

	SDL_Window *window = SDL_CreateWindow(
		"Graphics course practice 12", SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED, 800, 600,
		SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED);

	if (!window) {
		sdl2_fail("SDL_CreateWindow: ");
	}

	int width, height;
	SDL_GetWindowSize(window, &width, &height);

	SDL_GLContext gl_context = SDL_GL_CreateContext(window);
	if (!gl_context) {
		sdl2_fail("SDL_GL_CreateContext: ");
	}

	if (auto result = glewInit(); result != GLEW_NO_ERROR) {
		glew_fail("glewInit: ", result);
	}

	if (!GLEW_VERSION_3_3) {
		throw std::runtime_error("OpenGL 3.3 is not supported");
	}

	auto vertex_shader = create_shader(
		GL_VERTEX_SHADER, read_shader("../vertex_shader.glsl").data());
	auto fragment_shader = create_shader(
		GL_FRAGMENT_SHADER, read_shader("../fragment_shader.glsl").data());
	auto program = create_program(vertex_shader, fragment_shader);

	GLuint view_location = glGetUniformLocation(program, "view");
	GLuint projection_location = glGetUniformLocation(program, "projection");
	GLuint bbox_min_location = glGetUniformLocation(program, "bbox_min");
	GLuint bbox_max_location = glGetUniformLocation(program, "bbox_max");
	GLuint camera_position_location =
		glGetUniformLocation(program, "camera_position");
	GLuint light_direction_location =
		glGetUniformLocation(program, "light_direction");
	GLuint cloud_texture_location = glGetUniformLocation(program, "cloud_data");

	GLuint vao, vbo, ebo;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(cube_vertices), cube_vertices,
				 GL_STATIC_DRAW);

	glGenBuffers(1, &ebo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cube_indices), cube_indices,
				 GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

	const std::string project_root = PROJECT_ROOT;
	const std::string cloud_data_path = project_root + "/cloud.data";

	GLuint cloud_texture;
	glGenTextures(1, &cloud_texture);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_3D, cloud_texture);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	std::vector<char> pixels(128 * 64 * 64); // x * y * z
	std::ifstream input(cloud_data_path, std::ios::binary);
	input.read(pixels.data(), pixels.size());
	glTexImage3D(GL_TEXTURE_3D, 0, GL_R8, 128, 64, 64, 0, GL_RED,
				 GL_UNSIGNED_BYTE, pixels.data());

	const glm::vec3 cloud_bbox_min{-2.f, -1.f, -1.f};
	const glm::vec3 cloud_bbox_max{2.f, 1.f, 1.f};

	auto last_frame_start = std::chrono::high_resolution_clock::now();

	float time = 0.f;

	std::map<SDL_Keycode, bool> button_down;

	float view_angle = glm::pi<float>() / 6.f;
	float camera_distance = 3.5f;

	float camera_rotation = glm::pi<float>() / 6.f;

	bool paused = false;

	bool running = true;
	while (running) {
		for (SDL_Event event; SDL_PollEvent(&event);) {
			switch (event.type) {
			case SDL_QUIT:
				running = false;
				break;
			case SDL_WINDOWEVENT:
				switch (event.window.event) {
				case SDL_WINDOWEVENT_RESIZED:
					width = event.window.data1;
					height = event.window.data2;
					glViewport(0, 0, width, height);
					break;
				}
				break;
			case SDL_KEYDOWN:
				button_down[event.key.keysym.sym] = true;
				if (event.key.keysym.sym == SDLK_SPACE) {
					paused = !paused;
				}
				break;
			case SDL_KEYUP:
				button_down[event.key.keysym.sym] = false;
				break;
			}
		}

		if (!running) {
			break;
		}

		auto now = std::chrono::high_resolution_clock::now();
		float dt = std::chrono::duration_cast<std::chrono::duration<float>>(
					   now - last_frame_start)
					   .count();
		last_frame_start = now;

		if (!paused) {
			time += dt;
		}

		if (button_down[SDLK_UP]) {
			camera_distance -= dt;
		}
		if (button_down[SDLK_DOWN]) {
			camera_distance += dt;
		}

		if (button_down[SDLK_a]) {
			camera_rotation -= dt;
		}
		if (button_down[SDLK_d]) {
			camera_rotation += dt;
		}

		if (button_down[SDLK_w]) {
			view_angle -= dt;
		}
		if (button_down[SDLK_s]) {
			view_angle += dt;
		}

		glClearColor(0.0f, 0.0f, 0.0f, 0.f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glEnable(GL_DEPTH_TEST);

		glEnable(GL_CULL_FACE);
		glCullFace(GL_FRONT);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		float near = 0.1f;
		float far = 100.f;

		glm::mat4 model(1.f);

		glm::mat4 view(1.f);
		view = glm::translate(view, {0.f, 0.f, -camera_distance});
		view = glm::rotate(view, view_angle, {1.f, 0.f, 0.f});
		view = glm::rotate(view, camera_rotation, {0.f, 1.f, 0.f});

		glm::mat4 projection = glm::perspective(
			glm::pi<float>() / 2.f, (1.f * width) / height, near, far);

		glm::vec3 camera_position =
			(glm::inverse(view) * glm::vec4(0.f, 0.f, 0.f, 1.f)).xyz();

		glm::vec3 light_direction =
			glm::normalize(glm::vec3(std::cos(time), 1.f, std::sin(time)));

		glUseProgram(program);
		glUniformMatrix4fv(view_location, 1, GL_FALSE,
						   reinterpret_cast<float *>(&view));
		glUniformMatrix4fv(projection_location, 1, GL_FALSE,
						   reinterpret_cast<float *>(&projection));
		glUniform3fv(bbox_min_location, 1,
					 reinterpret_cast<const float *>(&cloud_bbox_min));
		glUniform3fv(bbox_max_location, 1,
					 reinterpret_cast<const float *>(&cloud_bbox_max));
		glUniform3fv(camera_position_location, 1,
					 reinterpret_cast<float *>(&camera_position));
		glUniform3fv(light_direction_location, 1,
					 reinterpret_cast<float *>(&light_direction));
		glUniform1i(cloud_texture_location, 0);

		glBindVertexArray(vao);
		glDrawElements(GL_TRIANGLES, std::size(cube_indices), GL_UNSIGNED_INT,
					   nullptr);

		SDL_GL_SwapWindow(window);
	}

	SDL_GL_DeleteContext(gl_context);
	SDL_DestroyWindow(window);
} catch (std::exception const &e) {
	std::cerr << e.what() << std::endl;
	return EXIT_FAILURE;
}
