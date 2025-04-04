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
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <vector>

#define GLM_FORCE_SWIZZLE
#define GLM_ENABLE_EXPERIMENTAL

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/scalar_constants.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "obj_parser.hpp"

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

GLuint create_program(GLuint vertex_shader, GLuint fragment_shader) {
	GLuint result = glCreateProgram();
	glAttachShader(result, vertex_shader);
	glAttachShader(result, fragment_shader);
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
		"Graphics course practice 8", SDL_WINDOWPOS_CENTERED,
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

	glClearColor(0.8f, 0.8f, 1.f, 0.f);

	auto vertex_shader = create_shader(
		GL_VERTEX_SHADER, read_shader("../vertex_shader.glsl").data());
	auto fragment_shader = create_shader(
		GL_FRAGMENT_SHADER, read_shader("../fragment_shader.glsl").data());
	auto program = create_program(vertex_shader, fragment_shader);

	GLuint model_location = glGetUniformLocation(program, "model");
	GLuint view_location = glGetUniformLocation(program, "view");
	GLuint projection_location = glGetUniformLocation(program, "projection");
	GLuint camera_position_location =
		glGetUniformLocation(program, "camera_position");
	GLuint albedo_location = glGetUniformLocation(program, "albedo");
	GLuint sun_direction_location =
		glGetUniformLocation(program, "sun_direction");
	GLuint sun_color_location = glGetUniformLocation(program, "sun_color");
	GLuint shadow_proj_location =
		glGetUniformLocation(program, "shadow_projection");

	auto vertex_shadow_map_shader = create_shader(
		GL_VERTEX_SHADER, read_shader("../vertex_shadow.glsl").data());
	auto fragment_shadow_map_shader = create_shader(
		GL_FRAGMENT_SHADER, read_shader("../fragment_shadow.glsl").data());
	auto shadow_map_program =
		create_program(vertex_shadow_map_shader, fragment_shadow_map_shader);

	GLuint debug_model_location =
		glGetUniformLocation(shadow_map_program, "model");
	GLuint shadow_projection_location =
		glGetUniformLocation(shadow_map_program, "shadow_projection");

	auto vertex_debug_shader = create_shader(
		GL_VERTEX_SHADER, read_shader("../vertex_debug.glsl").data());
	auto fragment_debug_shader = create_shader(
		GL_FRAGMENT_SHADER, read_shader("../fragment_debug.glsl").data());
	auto debug_program =
		create_program(vertex_debug_shader, fragment_debug_shader);

	int shadow_map_size = 1024;
	GLuint shadow_fbo, shadow_texture;
	glGenTextures(1, &shadow_texture);
	glBindTexture(GL_TEXTURE_2D, shadow_texture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE,
					GL_COMPARE_REF_TO_TEXTURE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, shadow_map_size,
				 shadow_map_size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

	glGenFramebuffers(1, &shadow_fbo);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, shadow_fbo);
	glFramebufferTexture(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
						 shadow_texture, 0);

	if (glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) !=
		GL_FRAMEBUFFER_COMPLETE) {
		throw std::runtime_error("Framebuffer incomplete");
	}

	GLuint debug_vao;
	glGenVertexArrays(1, &debug_vao);

	std::string project_root = PROJECT_ROOT;
	std::string scene_path = project_root + "/buddha.obj";
	obj_data scene = parse_obj(scene_path);

	GLuint scene_vao, scene_vbo, scene_ebo;
	glGenVertexArrays(1, &scene_vao);
	glBindVertexArray(scene_vao);

	glGenBuffers(1, &scene_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, scene_vbo);
	glBufferData(GL_ARRAY_BUFFER,
				 scene.vertices.size() * sizeof(scene.vertices[0]),
				 scene.vertices.data(), GL_STATIC_DRAW);

	glGenBuffers(1, &scene_ebo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, scene_ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER,
				 scene.indices.size() * sizeof(scene.indices[0]),
				 scene.indices.data(), GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(obj_data::vertex),
						  (void *)(0));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(obj_data::vertex),
						  (void *)(12));

	auto last_frame_start = std::chrono::high_resolution_clock::now();

	float time = 0.f;

	std::map<SDL_Keycode, bool> button_down;

	float camera_distance = 1.5f;
	float camera_angle = glm::pi<float>();

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
					break;
				}
				break;
			case SDL_KEYDOWN:
				button_down[event.key.keysym.sym] = true;
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
		time += dt;

		if (button_down[SDLK_UP]) {
			camera_distance -= dt;
		}
		if (button_down[SDLK_DOWN]) {
			camera_distance += dt;
		}

		if (button_down[SDLK_LEFT]) {
			camera_angle += dt;
		}
		if (button_down[SDLK_RIGHT]) {
			camera_angle -= dt;
		}

		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, shadow_fbo);
		glClear(GL_DEPTH_BUFFER_BIT);
		glViewport(0, 0, shadow_map_size, shadow_map_size);
		glClearColor(0.8f, 0.8f, 1.f, 0.f);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_FRONT);

		glm::mat4 debug_model(1.f);

		glm::vec3 sun_direction = glm::normalize(
			glm::vec3(std::sin(time * 0.5f), 2.f, std::cos(time * 0.5f)));

		glm::vec3 light_Z = -sun_direction;
		glm::vec3 sec = {1.0, 0.0, 0.0};
		if (light_Z == sec) {
			sec = {0.0, 1.0, 0.0};
		}
		glm::vec3 light_X = glm::normalize(glm::cross(light_Z, sec));
		glm::vec3 light_Y = glm::cross(light_X, light_Z);

		glm::mat4 shadow_projection =
			glm::mat4(glm::transpose(glm::mat3(light_X, light_Y, light_Z)));

		glUseProgram(shadow_map_program);
		glBindTexture(GL_TEXTURE_2D, shadow_texture);

		glUniformMatrix4fv(debug_model_location, 1, GL_FALSE,
						   reinterpret_cast<float *>(&debug_model));
		glUniformMatrix4fv(shadow_projection_location, 1, GL_FALSE,
						   reinterpret_cast<float *>(&shadow_projection));

		glBindVertexArray(scene_vao);
		glDrawElements(GL_TRIANGLES, scene.indices.size(), GL_UNSIGNED_INT,
					   nullptr);

		glViewport(0, 0, width, height);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);

		float near = 0.01f;
		float far = 1000.f;

		glm::mat4 model(1.f);

		glm::mat4 view(1.f);
		view = glm::translate(view, {0.f, 0.f, -camera_distance});
		view = glm::rotate(view, glm::pi<float>() / 6.f, {1.f, 0.f, 0.f});
		view = glm::rotate(view, camera_angle, {0.f, 1.f, 0.f});
		view = glm::translate(view, {0.f, -0.5f, 0.f});

		float aspect = (float)height / (float)width;
		glm::mat4 projection = glm::perspective(
			glm::pi<float>() / 3.f, (width * 1.f) / height, near, far);

		glm::vec3 camera_position =
			(glm::inverse(view) * glm::vec4(0.f, 0.f, 0.f, 1.f)).xyz();

		glUseProgram(program);

		glUniformMatrix4fv(model_location, 1, GL_FALSE,
						   reinterpret_cast<float *>(&model));
		glUniformMatrix4fv(view_location, 1, GL_FALSE,
						   reinterpret_cast<float *>(&view));
		glUniformMatrix4fv(projection_location, 1, GL_FALSE,
						   reinterpret_cast<float *>(&projection));
		glUniform3fv(camera_position_location, 1, (float *)(&camera_position));
		glUniform3f(albedo_location, .8f, .7f, .6f);
		glUniform3f(sun_color_location, 1.f, 1.f, 1.f);
		glUniform3fv(sun_direction_location, 1,
					 reinterpret_cast<float *>(&sun_direction));
		glUniformMatrix4fv(shadow_proj_location, 1, GL_FALSE,
						   reinterpret_cast<float *>(&shadow_projection));

		glBindVertexArray(scene_vao);
		glDrawElements(GL_TRIANGLES, scene.indices.size(), GL_UNSIGNED_INT,
					   nullptr);

		glUseProgram(debug_program);
		glDisable(GL_DEPTH_TEST);

		glBindVertexArray(debug_vao);
		glDrawArrays(GL_TRIANGLES, 0, 6);

		SDL_GL_SwapWindow(window);
	}

	SDL_GL_DeleteContext(gl_context);
	SDL_DestroyWindow(window);
} catch (std::exception const &e) {
	std::cerr << e.what() << std::endl;
	return EXIT_FAILURE;
}
