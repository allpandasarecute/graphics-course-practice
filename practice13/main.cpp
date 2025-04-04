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

#include "gltf_loader.hpp"
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

int main() try {
	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		sdl2_fail("SDL_Init: ");
	}

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
						SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
	// SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 16);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

	SDL_Window *window = SDL_CreateWindow(
		"Graphics course practice 13", SDL_WINDOWPOS_CENTERED,
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

	GLuint model_location = glGetUniformLocation(program, "model");
	GLuint view_location = glGetUniformLocation(program, "view");
	GLuint projection_location = glGetUniformLocation(program, "projection");
	GLuint albedo_location = glGetUniformLocation(program, "albedo");
	GLuint color_location = glGetUniformLocation(program, "color");
	GLuint use_texture_location = glGetUniformLocation(program, "use_texture");
	GLuint light_direction_location =
		glGetUniformLocation(program, "light_direction");
	GLuint bones_location = glGetUniformLocation(program, "bones");

	const std::string project_root = PROJECT_ROOT;
	const std::string model_path = project_root + "/dancing/dancing.gltf";

	auto const input_model = load_gltf(model_path);
	GLuint vbo;
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, input_model.buffer.size(),
				 input_model.buffer.data(), GL_STATIC_DRAW);

	struct mesh {
		GLuint vao;
		gltf_model::accessor indices;
		gltf_model::material material;
	};

	auto setup_attribute = [](int index, gltf_model::accessor const &accessor,
							  bool integer = false) {
		glEnableVertexAttribArray(index);
		if (integer) {
			glVertexAttribIPointer(
				index, accessor.size, accessor.type, 0,
				reinterpret_cast<void *>(accessor.view.offset));
		} else {
			glVertexAttribPointer(
				index, accessor.size, accessor.type, GL_FALSE, 0,
				reinterpret_cast<void *>(accessor.view.offset));
		}
	};

	std::vector<mesh> meshes;
	for (auto const &mesh : input_model.meshes) {
		for (auto const &primitive : mesh.primitives) {
			auto &result = meshes.emplace_back();
			glGenVertexArrays(1, &result.vao);
			glBindVertexArray(result.vao);

			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo);
			result.indices = primitive.indices;

			setup_attribute(0, primitive.position);
			setup_attribute(1, primitive.normal);
			setup_attribute(2, primitive.texcoord);
			setup_attribute(3, primitive.joints, true);
			setup_attribute(4, primitive.weights);

			result.material = primitive.material;
		}
	}

	std::map<std::string, GLuint> textures;
	for (auto const &mesh : meshes) {
		if (!mesh.material.texture_path) {
			continue;
		}
		if (textures.contains(*mesh.material.texture_path)) {
			continue;
		}

		auto path = std::filesystem::path(model_path).parent_path() /
					*mesh.material.texture_path;

		int width, height, channels;
		auto data = stbi_load(path.c_str(), &width, &height, &channels, 4);
		assert(data);

		GLuint texture;
		glGenTextures(1, &texture);
		glBindTexture(GL_TEXTURE_2D, texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
						GL_LINEAR_MIPMAP_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA,
					 GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);

		stbi_image_free(data);

		textures[*mesh.material.texture_path] = texture;
	}

	auto last_frame_start = std::chrono::high_resolution_clock::now();

	float time = 0.f;

	std::map<SDL_Keycode, bool> button_down;

	float view_angle = 0.f;
	float camera_distance = 1.5f;

	float camera_rotation = 0.f;
	float camera_height = 1.f;

	bool paused = false;

	std::string hip_hop_animation_title = "hip-hop";
	std::string rumba_animation_title = "rumba";
	std::string flair_animation_title = "flair";

	std::string animation_title = hip_hop_animation_title;
	int last_animation_change = 0;
	float animation_change_duration = 100.f;

	std::vector<glm::vec3> last_animation_translation(input_model.bones.size());
	std::vector<glm::quat> last_animation_rotation(input_model.bones.size());
	std::vector<glm::vec3> last_animation_scaling(input_model.bones.size());

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

		if (button_down[SDLK_1]) {
			animation_title = hip_hop_animation_title;
			last_animation_change = time;
		}
		if (button_down[SDLK_2]) {
			animation_title = rumba_animation_title;
			last_animation_change = time;
		}
		if (button_down[SDLK_3]) {
			animation_title = flair_animation_title;
			last_animation_change = time;
		}

		glClearColor(0.8f, 0.8f, 1.f, 0.f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glEnable(GL_DEPTH_TEST);

		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		float near = 0.1f;
		float far = 10000.f;
		float scale = 0.75f + cos(time) * 0.25f;

		glm::mat4 model = glm::scale(glm::mat4(1.f), glm::vec3(0.009f));

		glm::mat4 view(1.f);
		view = glm::translate(view, {0.f, 0.f, -camera_distance});
		view = glm::rotate(view, view_angle, {1.f, 0.f, 0.f});
		view = glm::rotate(view, camera_rotation, {0.f, 1.f, 0.f});
		view = glm::translate(view, {0.f, -camera_height, 0.f});

		glm::mat4 projection = glm::perspective(
			glm::pi<float>() / 2.f, (1.f * width) / height, near, far);

		glm::vec3 camera_position =
			(glm::inverse(view) * glm::vec4(0.f, 0.f, 0.f, 1.f)).xyz();

		glm::vec3 light_direction = glm::normalize(glm::vec3(1.f, 2.f, 3.f));

		std::vector<glm::mat4x3> bones(input_model.bones.size());

		auto animation = input_model.animations.at(animation_title);
		float t1 = std::fmod(time, animation.max_time);
		float constant =
			(time - last_animation_change) / animation_change_duration;
		float interpolation =
			(last_animation_change != 0 && constant < 1) ? constant : 1;

		// std::cout << "interpolation = " << interpolation << std::endl;

		for (int i = 0; i < animation.bones.size(); i++) {
			auto bone = animation.bones[i];

			// auto translation = glm::translate(glm::mat4(1),
			// bone.translation(t1)); auto scaling = glm::scale(glm::mat4(1),
			// bone.scale(t1)); auto rotation = glm::toMat4(bone.rotation(t1));
			last_animation_translation[i] =
				glm::lerp(last_animation_translation[i], bone.translation(t1),
						  interpolation);
			last_animation_scaling[i] = glm::lerp(
				last_animation_scaling[i], bone.scale(t1), interpolation);
			last_animation_rotation[i] = glm::slerp(
				last_animation_rotation[i], bone.rotation(t1), interpolation);

			auto translation =
				glm::translate(glm::mat4(1), last_animation_translation[i]);
			auto scaling = glm::scale(glm::mat4(1), last_animation_scaling[i]);
			auto rotation = glm::toMat4(last_animation_rotation[i]);
			auto transform = translation * rotation * scaling;

			if (input_model.bones[i].parent != -1) {
				transform = bones[input_model.bones[i].parent] * transform;
			}
			bones[i] = transform;
		}

		for (int i = 0; i < bones.size(); i++) {
			bones[i] = bones[i] * input_model.bones[i].inverse_bind_matrix;
		}

		glUseProgram(program);
		glUniformMatrix4fv(model_location, 1, GL_FALSE,
						   reinterpret_cast<float *>(&model));
		glUniformMatrix4fv(view_location, 1, GL_FALSE,
						   reinterpret_cast<float *>(&view));
		glUniformMatrix4fv(projection_location, 1, GL_FALSE,
						   reinterpret_cast<float *>(&projection));
		glUniform3fv(light_direction_location, 1,
					 reinterpret_cast<float *>(&light_direction));
		glUniformMatrix4x3fv(bones_location, 100, GL_FALSE,
							 reinterpret_cast<float *>(&bones[0][0][0]));

		auto draw_meshes = [&](bool transparent) {
			for (auto const &mesh : meshes) {
				if (mesh.material.transparent != transparent) {
					continue;
				}

				if (mesh.material.two_sided) {
					glDisable(GL_CULL_FACE);
				} else {
					glEnable(GL_CULL_FACE);
				}

				if (transparent) {
					glEnable(GL_BLEND);
				} else {
					glDisable(GL_BLEND);
				}

				if (mesh.material.texture_path) {
					glBindTexture(GL_TEXTURE_2D,
								  textures[*mesh.material.texture_path]);
					glUniform1i(use_texture_location, 1);
				} else if (mesh.material.color) {
					glUniform1i(use_texture_location, 0);
					glUniform4fv(color_location, 1,
								 reinterpret_cast<const float *>(
									 &(*mesh.material.color)));
				} else {
					continue;
				}

				glBindVertexArray(mesh.vao);
				glDrawElements(
					GL_TRIANGLES, mesh.indices.count, mesh.indices.type,
					reinterpret_cast<void *>(mesh.indices.view.offset));
			}
		};

		draw_meshes(false);
		glDepthMask(GL_FALSE);
		draw_meshes(true);
		glDepthMask(GL_TRUE);

		SDL_GL_SwapWindow(window);
	}

	SDL_GL_DeleteContext(gl_context);
	SDL_DestroyWindow(window);
} catch (std::exception const &e) {
	std::cerr << e.what() << std::endl;
	return EXIT_FAILURE;
}
