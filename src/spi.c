#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <dirent.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"


#define WINDOW_WIDTH 1024
#define WINDOW_HEIGHT 600

#ifndef ELIX_FILE_PATH_LENGTH
	#define ELIX_FILE_PATH_LENGTH 768
#endif

#ifndef ELIX_FILE_NAME_LENGTH
	#define ELIX_FILE_NAME_LENGTH 256
#endif

#ifndef nullptr
	#define nullptr NULL
#endif

typedef struct spi_input_t {
	uint8_t mouse_buttons[5];
	float mouse_x, mouse_y;
	float wheel_x, wheel_y;
	bool wheel_x_active, wheel_y_active;

} SPI_Input;

char temp_dir[ELIX_FILE_PATH_LENGTH] = {0};
char buffer_path[ELIX_FILE_PATH_LENGTH] = {0};
char current_path[ELIX_FILE_PATH_LENGTH] = {0};

SDL_Window * window = NULL;
SDL_Renderer *renderer = NULL;
SDL_Texture * image_texture = nullptr, * preview_texture = nullptr;
SDL_FRect image_dimension = {0.0f, 0.0f, 1360.0, 1920.0};
SPI_Input input = {0};

char ** current_directory_list = nullptr;
int current_directory_count = 0;
int current_directory_index = 0;
int current_directory_limit = 10;


char ** current_file_list = nullptr;
int current_file_count = 0;
int current_file_index = 0;

uint32_t event_dir_id = 0;
uint32_t event_change_id = 0;
uint32_t event_remove_id = 0;

size_t elix_cstring_length(const char * string, uint8_t include_terminator ) {
	if (string) {
		size_t c = 0;
		while(*string++) {
			++c;
		}
		return c + include_terminator;
	}
	return 0;
}

bool elix_cstring_has_suffix( const char * str, const char * suffix) {
	size_t str_length = elix_cstring_length(str, 0);
	size_t suffix_length = elix_cstring_length(suffix, 0);
	size_t offset = 0;

	if ( str_length < suffix_length )
		return false;

	offset = str_length - suffix_length;
	for (size_t c = 0; c < suffix_length;  c++ ) {
		if ( str[offset + c] != suffix[c] )
			return false;
	}
	return true;
}

size_t elix_cstring_append( char * str, const size_t len, const char * text, const size_t text_len) {
	size_t length = elix_cstring_length(str, 0);
	// TODO: Switch to memcpy
	for (size_t c = 0;length < len && c < text_len; length++, c++) {
		str[length] = text[c];
	}
	str[length] = 0;
	return length;
}

SDL_Texture * SPI_LoadImage(const char * path) {
	SDL_Texture * texture = nullptr;
	int width, height, channels = 3;
	unsigned char * data = stbi_load(path, &width, &height, &channels, 4);

	if ( data ) {
		image_dimension.w = width;
		image_dimension.h = height;
		texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, width, height);
		if ( texture ) {
			SDL_UpdateTexture(texture, NULL, data, width * 4);
		}
		stbi_image_free(data);
	}
	return texture;
}

int string_sort(const void * a, const void * b) {
	return strcmp(*(const char**)a, *(const char**)b);
}

#include <errno.h>
char **SPI_GlobDirectory(const char *path, const char *pattern, SDL_GlobFlags flags, int *count)
{
	char ** directory = nullptr;

	int index = 0;
	int dir_count = 0;

	struct dirent * entity = nullptr;
	DIR * current_directory = current_directory = opendir(path);

	if ( !current_directory ) {
		SDL_Log("DIR Error %s", strerror(errno));
		SDL_Log("Path: %s", path);
		*count = 0;
		return directory;
	}

	while ((entity = readdir(current_directory)) ) {
		if ( entity->d_name[0] == '.' && (entity->d_name[1] == '.'|| entity->d_name[1]== 0)) {

		} else if ( elix_cstring_has_suffix(entity->d_name, "-DELETED") ) {
		
		} else {
			dir_count++;
		}
	}
	
	directory = calloc(dir_count, sizeof(char*));
	rewinddir(current_directory);

	while ( (entity = readdir(current_directory)) ) {
		if ( entity->d_name[0] == '.' && (entity->d_name[1] == '.'|| entity->d_name[1]== 0)){

		} else if ( elix_cstring_has_suffix(entity->d_name, "-DELETED") ) {
		} else {
			size_t filename_size = strlen(entity->d_name)+1;
			if ( filename_size < 256 ) {
				directory[index] = calloc(filename_size, sizeof(char));
				strncpy(directory[index], entity->d_name, filename_size);
				
				index++;
			} else {
				SDL_Log("Filename too long: %s\n", entity->d_name);
			}
		}
	}

	closedir(current_directory);

	SDL_qsort(directory, dir_count, sizeof(char*), string_sort);

	*count = dir_count;
	return directory;
}

void SPI_Switch(int direction) {

	if ( current_file_list == nullptr || current_file_count == 0 ) {
		return;
	}

	if ( current_file_index + direction < 0 ) {
		//current_file_index = current_file_count - 1;
		current_file_index = current_file_index;
	} else if ( current_file_index + direction >= current_file_count ) {
		//current_file_index = 0;
		current_file_index = current_file_index;
	} else {
		current_file_index += direction;
	}


	char full_path[ELIX_FILE_PATH_LENGTH] = {0};
	elix_cstring_append(full_path, ELIX_FILE_PATH_LENGTH, current_path, strlen(current_path));
	elix_cstring_append(full_path, ELIX_FILE_PATH_LENGTH, "/", 1);
	elix_cstring_append(full_path, ELIX_FILE_PATH_LENGTH, current_file_list[current_file_index], strlen(current_file_list[current_file_index]));
	if ( image_texture ) {
		SDL_DestroyTexture(image_texture);
	}

	image_texture = SPI_LoadImage(full_path);
}

void SPI_EnterDirectory(const char * path) {
	if ( current_file_list ) {
		for (int i = 0; i < current_file_count; i++) {
			free(current_file_list[i]);
		}
		free(current_file_list);
	}
	memset(current_path, 0, ELIX_FILE_PATH_LENGTH);
	elix_cstring_append(current_path, ELIX_FILE_PATH_LENGTH, path, strlen(path));
	//elix_cstring_append(current_path, ELIX_FILE_PATH_LENGTH, "/", 1);

	current_file_list = SPI_GlobDirectory(current_path, ".png", SDL_GLOB_CASEINSENSITIVE, &current_file_count);
	current_file_index = 0;

	if ( current_file_count > 0 ) {
		SPI_Switch(0);
	}
}

void SPI_ListDirectory(const char * path) {
	if ( current_directory_list ) {
		for (int i = 0; i < current_directory_count; i++) {
			free(current_directory_list[i]);
		}
		free(current_directory_list);
	}

	current_directory_list = SPI_GlobDirectory(path, nullptr, SDL_GLOB_CASEINSENSITIVE, &current_directory_count);


}
SDL_Texture * SPI_LoadPreview(const char * path) {
	int file_count = 0;
	char ** file_list = SPI_GlobDirectory(path, ".png", SDL_GLOB_CASEINSENSITIVE, &file_count);
	if (file_count == 0 || file_list == nullptr) {
		return nullptr;
	}
	
	//Note: path points to buffer_path so lets bypass 'const'
	elix_cstring_append(buffer_path, ELIX_FILE_PATH_LENGTH, file_list[0], strlen(file_list[0]));

	SDL_Texture * texture = nullptr;
	int width, height, channels = 3;
	unsigned char * data = stbi_load(buffer_path, &width, &height, &channels, 4);

	if ( data ) {
		image_dimension.w = width;
		image_dimension.h = height;
		texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, width, height);
		if ( texture ) {
			SDL_UpdateTexture(texture, NULL, data, width * 4);
		}
		stbi_image_free(data);
	}
	SDL_free(file_list);
	return texture;
}
void SPI_DestroyPreview() {
	if (preview_texture)
	{
		SDL_DestroyTexture(preview_texture);
		preview_texture = nullptr;
	}
}


/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
	if ( argc < 2 ) {

		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "No path given!", "Please provide a path to the directory to open.", NULL);
		return SDL_APP_FAILURE;
		//elix_cstring_append(temp_dir, ELIX_FILE_PATH_LENGTH, "/run/media/luke/NewMedia/backup/4/", 35);
	} else if ( argc >= 2 ) {
		elix_cstring_append(temp_dir, ELIX_FILE_PATH_LENGTH, argv[1], strlen(argv[1]));
	}


    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Couldn't initialize SDL!", SDL_GetError(), NULL);
        return SDL_APP_FAILURE;
    }

    if (!SDL_CreateWindowAndRenderer("SPI", WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Couldn't create window/renderer!", SDL_GetError(), NULL);
        return SDL_APP_FAILURE;
    }

	event_dir_id = event_change_id = event_remove_id = SDL_RegisterEvents(3);
	if ( event_dir_id != 0 ) {
		event_change_id += 1;
		event_remove_id += 2;
	}
	SPI_ListDirectory(temp_dir);
	
	
    return SDL_APP_CONTINUE;
}

SDL_MessageBoxData delete_dialog = {
	SDL_MESSAGEBOX_INFORMATION,
	NULL,
	"Delete",
	nullptr,
	2,
	(SDL_MessageBoxButtonData[]) {
		{SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "No"},
		{SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Yes"}
	},
	nullptr
};

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;  /* end the program, reporting success to the OS. */
    } else if (event->type == SDL_EVENT_MOUSE_WHEEL) {
		if (event->wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
			event->wheel.x *= -1;
			event->wheel.y *= -1;
		}
		if (event->wheel.x != 0.0f) {
			input.wheel_x_active = true;
			/* "positive to the right and negative to the left"  */
			input.wheel_x += event->wheel.x * 10.0f;
		}
		if (event->wheel.y != 0.0f) {
			input.wheel_y_active = true;
			/* "positive away from the user and negative towards the user" */
			input.wheel_y -= event->wheel.y * 10.0f;
		}

	} else if (event->type == SDL_EVENT_KEY_DOWN) {

	} else if (event->type == SDL_EVENT_KEY_UP) {
		if (event->key.key == SDLK_LEFT) {
			SPI_Switch(-1);
		} else if (event->key.key == SDLK_RIGHT) {
			SPI_Switch(1);
		} else if (event->key.key == SDLK_ESCAPE) {
			if ( image_texture ) {
				SDL_DestroyTexture(image_texture);
				image_texture = nullptr;
			}
		} else if (event->key.key == SDLK_DELETE) {
			if ( image_texture ) {
				int r = 0;
				delete_dialog.window = SDL_GetWindowFromEvent(event);
				delete_dialog.message = current_path;
				SDL_ShowMessageBox(&delete_dialog, &r);
				SDL_Log("Delete %d", r);
				if ( r ==1) {
					SDL_DestroyTexture(image_texture);
					image_texture = nullptr;

					memset(buffer_path, 0, ELIX_FILE_PATH_LENGTH);
					elix_cstring_append(buffer_path, ELIX_FILE_PATH_LENGTH, current_path, strlen(current_path));
					elix_cstring_append(buffer_path, ELIX_FILE_PATH_LENGTH, "-DELETED", 9);

					SDL_Log("Rename:%s\n%s", current_path, buffer_path);


					SDL_RenamePath(current_path, buffer_path);
					SPI_ListDirectory(temp_dir);
				}
			}
		}

	} else if (event->type == SDL_EVENT_DROP_BEGIN) {
		SDL_Log("Drop beginning on window %u at (%f, %f)", (unsigned int)event->drop.windowID, event->drop.x, event->drop.y);
	} else if (event->type == SDL_EVENT_DROP_COMPLETE) {

		SDL_Log("Drop complete on window %u at (%f, %f)", (unsigned int)event->drop.windowID, event->drop.x, event->drop.y);
	} else if (event->type == SDL_EVENT_DROP_FILE ) {
		SDL_Log("Dropped on window %u: %s at (%f, %f)", (unsigned int)event->drop.windowID, event->drop.data, event->drop.x, event->drop.y);
	} else if (event->type == SDL_EVENT_DROP_POSITION) {
		const float w_x = event->drop.x;
		const float w_y = event->drop.y;
		SDL_ConvertEventToRenderCoordinates(SDL_GetRenderer(SDL_GetWindowFromEvent(event)), event);
		
		SDL_Log("Drop position on window %u at (%f, %f) data = %s", (unsigned int)event->drop.windowID, w_x, w_y, event->drop.data);
	} else if (event->type == event_dir_id) {
		//SDL_Log("DIR CHANGE %s", (char*)event->user.data1);

		memset(buffer_path, 0, ELIX_FILE_PATH_LENGTH);
		elix_cstring_append(buffer_path, ELIX_FILE_PATH_LENGTH, temp_dir, strlen(temp_dir));

		elix_cstring_append(buffer_path, ELIX_FILE_PATH_LENGTH, (char*)event->user.data1, strlen((char*)event->user.data1));

		SPI_EnterDirectory(buffer_path);
	}
    return SDL_APP_CONTINUE;  /* carry on with the program! */
}



SDL_Color text_colours[] = {
	{255, 255, 255, SDL_ALPHA_OPAQUE},
	{255, 0, 0, SDL_ALPHA_OPAQUE},
	{255, 128, 128, SDL_ALPHA_OPAQUE},
	{0, 128, 128, SDL_ALPHA_OPAQUE},
};
int current_preview_index = 0;
SDL_Event event= {0};

SDL_AppResult SDL_AppIterate( void *appstate ) {
	float x = 0.0f, y = 0.0f;

	int w, h;
	float window_ratio = 1.0f;
	SDL_FRect hit_rect = {0.0f, 0.0f, 320.0f, 24.0f};

	SDL_GetWindowSize(window, &w, &h);
	SDL_GetWindowAspectRatio(window, nullptr, &window_ratio);
	//window_ratio = (float)w / (float)h;

	SDL_SetRenderDrawColor(renderer, 33, 33, 33, SDL_ALPHA_OPAQUE);
	SDL_RenderClear(renderer);


	SDL_MouseButtonFlags mouse_button = SDL_GetMouseState(&input.mouse_x, &input.mouse_y);

	if ( mouse_button & SDL_BUTTON_LMASK ) {
		input.mouse_buttons[0] = input.mouse_buttons[0] ? 2 : 1;
	} else {
		input.mouse_buttons[0] = input.mouse_buttons[0] == 2 ? 3 : 0;
	}

	
	current_directory_limit = h / 24;
	current_directory_limit -= 2;
	x = w - ( SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * 64);

	if ( image_texture ) {
		if ( preview_texture ) {
			SDL_DestroyTexture(preview_texture);
			preview_texture = nullptr;
		}
		SDL_FRect image_rect = {0.0f, 0.0f, image_dimension.w, image_dimension.h};

		float scale = (float)h / (float)image_texture->h;

		image_rect.w = image_texture->w * scale;
		image_rect.h = image_texture->h * scale;

		SDL_RenderTexture(renderer, image_texture, nullptr, &image_rect);
		SDL_SetRenderDrawColor(renderer, 20, 24, 32, SDL_ALPHA_OPAQUE); 
		SDL_RenderDebugTextFormat(renderer, w-64, y+8, "%s", current_file_list[current_file_index]);

	} else if (current_directory_count > 0) {
		if ( preview_texture ) {
			SDL_FRect image_rect = {0.0f, 0.0f, image_dimension.w, image_dimension.h};

			float scale = (float)h / (float)preview_texture->h;

			image_rect.w = preview_texture->w * scale;
			image_rect.h = preview_texture->h * scale;

			SDL_RenderTexture(renderer, preview_texture, nullptr, &image_rect);
		} 
		{
			uint8_t colour = 0;

			hit_rect.x = x;
			hit_rect.y = y;

			if ( input.mouse_x >= hit_rect.x && input.mouse_x <= hit_rect.x + hit_rect.w && input.mouse_y >= hit_rect.y && input.mouse_y <= hit_rect.y + hit_rect.h ) {
				if ( input.mouse_buttons[0] == 2 ) {
					colour = 2;
				} else if ( input.mouse_buttons[0] == 3 ) {
					colour = 3;
					
				if ( current_directory_index + -current_directory_limit < 0 ) {
					current_directory_index = 0;
				} else if ( current_directory_index + -current_directory_limit >= current_directory_count ) {
					current_directory_index = current_directory_count - current_directory_limit;
				} else {
					current_directory_index += -current_directory_limit;
				}


				} else {
					colour = 1;
				}
			}
			SDL_SetRenderDrawColor(renderer, text_colours[colour].r, text_colours[colour].g, text_colours[colour].b, text_colours[colour].a); 
			SDL_RenderDebugTextFormat(renderer, x, y+8, "%.*s (%d to %d)", 64, "Previous", current_directory_index - current_directory_limit, current_directory_index - 1);		

			y += 24.0f;
		}
		for (int i = current_directory_index; (i - current_directory_index < current_directory_limit) && (i < current_directory_count); i++) {
			uint8_t colour = 0;

			hit_rect.x = x;
			hit_rect.y = y;

			if ( input.mouse_x >= hit_rect.x && input.mouse_x <= hit_rect.x + hit_rect.w && input.mouse_y >= hit_rect.y && input.mouse_y <= hit_rect.y + hit_rect.h ) {
				if ( input.mouse_buttons[0] == 2 ) {
					colour = 2;
				} else if ( input.mouse_buttons[0] == 3 ) {
					colour = 3;
					
					event.type = event_dir_id;
					event.user.data1 = current_directory_list[i];
					SDL_PushEvent(&event);

				} else {
					colour = 1;
					if ( current_preview_index != i) {
						current_preview_index = i;
						SPI_DestroyPreview();

						memset(buffer_path, 0, ELIX_FILE_PATH_LENGTH);
						elix_cstring_append(buffer_path, ELIX_FILE_PATH_LENGTH, temp_dir, strlen(temp_dir));
						elix_cstring_append(buffer_path, ELIX_FILE_PATH_LENGTH, current_directory_list[i], SDL_strlen(current_directory_list[i]));
						elix_cstring_append(buffer_path, ELIX_FILE_PATH_LENGTH, "/", 1);
						
						preview_texture = SPI_LoadPreview(buffer_path);
					}
				}
			}

		
			SDL_SetRenderDrawColor(renderer, text_colours[colour].r, text_colours[colour].g, text_colours[colour].b, text_colours[colour].a); 
			SDL_RenderDebugTextFormat(renderer, x, y+8, "%.*s", 64, current_directory_list[i]);		

			y += 24.0f;
		}
		{
			uint8_t colour = 0;

			hit_rect.x = x;
			hit_rect.y = y;

			if ( input.mouse_x >= hit_rect.x && input.mouse_x <= hit_rect.x + hit_rect.w && input.mouse_y >= hit_rect.y && input.mouse_y <= hit_rect.y + hit_rect.h ) {
				if ( input.mouse_buttons[0] == 2 ) {
					colour = 2;
				} else if ( input.mouse_buttons[0] == 3 ) {
					colour = 3;
					if ( current_directory_index + current_directory_limit < 0 ) {
						current_directory_index = 0;
					} else if ( current_directory_index + current_directory_limit >= current_directory_count ) {
						current_directory_index = current_directory_count - current_directory_limit;
					} else {
						current_directory_index += current_directory_limit;
					}

				} else {
					colour = 1;
				}
			}
			SDL_SetRenderDrawColor(renderer, text_colours[colour].r, text_colours[colour].g, text_colours[colour].b, text_colours[colour].a); 
			SDL_RenderDebugTextFormat(renderer, x, y+8, "%.*s (%d to %d)", 64, "Next", current_directory_index + current_directory_limit, current_directory_index + current_directory_limit + current_directory_limit);			

			y += 24.0f;
		}

	}


    SDL_RenderPresent(renderer);


    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
	/* SDL will clean up the window/renderer for us. */
	SPI_DestroyPreview();
	if ( image_texture ) {
		SDL_DestroyTexture(image_texture);
		image_texture = nullptr;
	}
}
