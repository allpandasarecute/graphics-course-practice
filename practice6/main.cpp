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
		"Graphics course practice 6", SDL_WINDOWPOS_CENTERED,
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

	auto dragon_vertex_shader = create_shader(
		GL_VERTEX_SHADER, read_shader("../dragon_vertex_shader.glsl").data());
	auto dragon_fragment_shader =
		create_shader(GL_FRAGMENT_SHADER,
					  read_shader("../dragon_fragment_shader.glsl").data());
	auto dragon_program =
		create_program(dragon_vertex_shader, dragon_fragment_shader);

	GLuint model_location = glGetUniformLocation(dragon_program, "model");
	GLuint view_location = glGetUniformLocation(dragon_program, "view");
	GLuint projection_location =
		glGetUniformLocation(dragon_program, "projection");

	GLuint camera_position_location =
		glGetUniformLocation(dragon_program, "camera_position");

	std::string project_root = PROJECT_ROOT;
	std::string dragon_model_path = project_root + "/dragon.obj";
	obj_data dragon = parse_obj(dragon_model_path);

	GLuint dragon_vao, dragon_vbo, dragon_ebo;
	glGenVertexArrays(1, &dragon_vao);
	glBindVertexArray(dragon_vao);

	glGenBuffers(1, &dragon_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, dragon_vbo);
	glBufferData(GL_ARRAY_BUFFER,
				 dragon.vertices.size() * sizeof(dragon.vertices[0]),
				 dragon.vertices.data(), GL_STATIC_DRAW);

	glGenBuffers(1, &dragon_ebo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, dragon_ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER,
				 dragon.indices.size() * sizeof(dragon.indices[0]),
				 dragon.indices.data(), GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(obj_data::vertex),
						  (void *)(0));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(obj_data::vertex),
						  (void *)(12));

	auto rectangle_vertex_shader = create_shader(
		GL_VERTEX_SHADER, read_shader("../rect_vertex_shader.glsl").data());
	auto rectangle_fragment_shader = create_shader(
		GL_FRAGMENT_SHADER, read_shader("../rect_fragment_shader.glsl").data());
	auto rectangle_program =
		create_program(rectangle_vertex_shader, rectangle_fragment_shader);

	GLuint center_location = glGetUniformLocation(rectangle_program, "center");
	GLuint size_location = glGetUniformLocation(rectangle_program, "size");
	GLuint render_result_location =
		glGetUniformLocation(rectangle_program, "render_result");
	GLuint mode_location = glGetUniformLocation(rectangle_program, "mode");
	GLuint time_location = glGetUniformLocation(rectangle_program, "time");

	GLuint rectangle_vao;
	glGenVertexArrays(1, &rectangle_vao);

	glActiveTexture(GL_TEXTURE0);
	GLuint texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width / 2, height / 2, 0, GL_RGBA,
				 GL_UNSIGNED_BYTE, nullptr);

	GLuint render_buffer;
	glGenRenderbuffers(1, &render_buffer);
	glBindRenderbuffer(GL_RENDERBUFFER, render_buffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width / 2,
						  height / 2);

	GLuint fbo;
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
	glFramebufferTexture(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture, 0);

	glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
							  GL_RENDERBUFFER, render_buffer);

	if (glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) !=
		GL_FRAMEBUFFER_COMPLETE) {
		std::cout << "Frame buffer is not complete\n";
		return 1;
	}

	auto last_frame_start = std::chrono::high_resolution_clock::now();

	float time = 0.f;

	std::map<SDL_Keycode, bool> button_down;

	float view_angle = 0.f;
	float camera_distance = 0.5f;
	float model_angle = glm::pi<float>() / 2.f;
	float model_scale = 1.f;

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
					glBindTexture(GL_TEXTURE_2D, texture);
					glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width / 2,
								 height / 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
								 nullptr);
					glBindRenderbuffer(GL_RENDERBUFFER, render_buffer);
					glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24,
										  width, height);
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
			model_angle -= dt;
		}
		if (button_down[SDLK_RIGHT]) {
			model_angle += dt;
		}

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		for (int i = 0; i < 4; i++) {
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
			glViewport(0, 0, width / 2, height / 2);

			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glEnable(GL_DEPTH_TEST);
			glEnable(GL_CULL_FACE);
			glClearColor((i % 2), (i / 2), 0, 1);

			float near = 0.1f;
			float far = 100.f;

			glm::mat4 model(1.f);
			model = glm::rotate(model, model_angle, {0.f, 1.f, 0.f});
			model = glm::scale(model, glm::vec3(model_scale));

			glm::mat4 view(1.f);
			glm::mat4 projection;
			float aspect = (1.f * width) / height;
			if (i == 0) {
				projection =
					glm::perspective(glm::pi<float>() / 2.f, aspect, near, far);

				view = glm::translate(view, {0.f, 0.f, -camera_distance});
				view = glm::rotate(view, view_angle, {1.f, 0.f, 0.f});
			} else if (i == 1) {
				projection = glm::ortho(-1.f * aspect, 1.f * aspect, -1.f, 1.f,
										near, far);

				view = glm::translate(view, {0.f, 0.f, -camera_distance});
			} else if (i == 2) {
				projection = glm::ortho(-1.f * aspect, 1.f * aspect, -1.f, 1.f,
										near, far);

				view = glm::translate(view, {0.f, 0.f, -camera_distance});
				view =
					glm::rotate(view, -glm::pi<float>() / 2.f, {0.f, 1.f, 0.f});
			} else {
				projection = glm::ortho(-1.f * aspect, 1.f * aspect, -1.f, 1.f,
										near, far);

				view = glm::translate(view, {0.f, 0.f, -camera_distance});
				view =
					glm::rotate(view, glm::pi<float>() / 2.f, {1.f, 0.f, 0.f});
			}

			glm::vec3 camera_position =
				(glm::inverse(view) * glm::vec4(0.f, 0.f, 0.f, 1.f)).xyz();

			glUseProgram(dragon_program);
			glUniformMatrix4fv(model_location, 1, GL_FALSE,
							   reinterpret_cast<float *>(&model));
			glUniformMatrix4fv(view_location, 1, GL_FALSE,
							   reinterpret_cast<float *>(&view));
			glUniformMatrix4fv(projection_location, 1, GL_FALSE,
							   reinterpret_cast<float *>(&projection));

			glUniform3fv(camera_position_location, 1,
						 (float *)(&camera_position));

			glBindVertexArray(dragon_vao);
			glDrawElements(GL_TRIANGLES, dragon.indices.size(), GL_UNSIGNED_INT,
						   nullptr);

			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
			glViewport(0, 0, width, height);

			glUseProgram(rectangle_program);
			glUniform2f(center_location, -0.5f + (i % 2), -0.5f + (i / 2));
			glUniform2f(size_location, 0.5f, 0.5f);
			glUniform1i(render_result_location, 0);
			glUniform1i(mode_location, i);
			glUniform1f(time_location, time);
			glBindVertexArray(rectangle_vao);
			glDrawArrays(GL_TRIANGLES, 0, 6);
		}

		SDL_GL_SwapWindow(window);
	}

	SDL_GL_DeleteContext(gl_context);
	SDL_DestroyWindow(window);
} catch (std::exception const &e) {
	std::cerr << e.what() << std::endl;
	return EXIT_FAILURE;
}
