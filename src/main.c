#include <raylib.h>
#include <raymath.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/inotify.h>
#include <unistd.h>
#include "game.h"
#define MAX_DIFF 441.67 // sqrt(255^2 + 255^2 + 255^2)

#define EVENT_SIZE (sizeof(struct inotify_event))
#define BUF_LEN (1024 * (EVENT_SIZE + 16))

#define FLAG_IMPLEMENTATION
#include "flag.h"


RenderTexture2D diff_text;

Texture texture1;
Texture texture2;
Image image1;
Image image2;

void usage(FILE *stream) {
  fprintf(stream, "Usage: ./image_quilt [OPTIONS] [--] input_files\n");
  fprintf(stream, "OPTIONS:\n");
  flag_print_options(stream);
}

int main(int argc, char *argv[]) {

  char **image_path = flag_str("image_name", NULL, "filepath of image to load");

  if (!flag_parse(argc, argv)) {
    usage(stderr);
    flag_print_error(stderr);
    exit(1);
  }

  const int screenWidth = 800;
  const int screenHeight = 600;

  InitWindow(screenWidth, screenHeight, "Image quilt demo");

  // plan, load 2 images
  // right click moves one
  // left click moves the other

  if (*image_path != NULL) {
    image1 = LoadImage(*image_path);
    image2 = LoadImage(*image_path);
  } else {
    image1 = LoadImage("rainbow.jpg");
    image2 = LoadImage("rainbow.jpg");
  }

  ImageFormat(&image1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
  ImageFormat(&image2, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

  // new width = / 2 of screen width
  // new height keeps ratio
  double ratio = (double)(image1.height) / (double)(image1.width);

  ImageResizeNN(&image1, screenWidth / 2, ((float)screenWidth / 2) * ratio);
  ImageResizeNN(&image2, screenWidth / 2, ((float)screenWidth / 2) * ratio);

  texture1 = LoadTextureFromImage(image1);
  texture2 = LoadTextureFromImage(image2);

  // 3rd image/texture that is the color difference between the images
  // updated when those images move
  Vector2 max_vec = Vector2Max((Vector2){image1.width, image1.height},
                               (Vector2){image2.width, image2.height});
  RenderTexture2D diff_text = LoadRenderTexture(max_vec.x, max_vec.y);


  game_state state = {
    .texture1 = texture1,
    .texture2 = texture2,
    .image1 = image1,
    .image2 = image2,
    .diff_text = diff_text,
    .dist = EUCLIDEAN_DIST,
    .image1_loc = {0,0},
    .image2_loc = {image1.width,0},
    .image_overlap = {0},
    .camera = {.zoom = 1.0f}
  };

  bool run = true;
  void *handle = NULL;
  void (*render)(bool *, char *, game_state *);

  int inotify_fd, watch_fd;
  char buffer[BUF_LEN];

  // Initialize inotify
  inotify_fd = inotify_init();
  if (inotify_fd < 0) {
    perror("inotify_init");
    exit(EXIT_FAILURE);
  }

  // Set inotify file descriptor to non-blocking mode
  int flags = fcntl(inotify_fd, F_GETFL, 0);
  fcntl(inotify_fd, F_SETFL, flags | O_NONBLOCK);

  // Watch a specific file
  watch_fd = inotify_add_watch(inotify_fd, "./libgame.so",
                               IN_ATTRIB | IN_MODIFY | IN_CREATE | IN_DELETE);

  if (watch_fd < 0) {
    perror("inotify_add_watch");
    exit(EXIT_FAILURE);
  }
  else{
    printf("added libgame.so to watch\n");
  }

  printf("Starting game loop\n");
  char error_buf[1024 * 4];
  char *errors = NULL; // read file to get errors if compile failed

  FILE *error_file = NULL;
  do {
    // check if file changed if so close handle
    int length = read(inotify_fd, buffer, BUF_LEN);

    // Process events
    int i = 0;
    while (i < length) {
      struct inotify_event *event = (struct inotify_event *)&buffer[i];

      printf("File event\n");
      if (event->mask & IN_MODIFY) {
        printf("File './libgame.so' was modified.\n");
        if (handle) {
          dlclose(handle);
        }
        handle = NULL;
      }
      if (event->mask & IN_CREATE) {
        printf("File 'libgame.so' was created.\n");
        if (handle) {
          dlclose(handle);
        }
        handle = NULL;
      }
      if (event->mask & IN_DELETE) {
        printf("File 'libgame.so' was deleted.\n");
        if (handle) {
          dlclose(handle);
        }
        handle = NULL;
      }
      if (event->mask & IN_ATTRIB) {
        printf("File 'libgame.so' was changed\n");
        if (handle) {
          dlclose(handle);
        }
        handle = NULL;
      }
      if (event->mask & IN_IGNORED) {
        printf("File 'libgame.so' was removed from inotify adding back\n");
        inotify_add_watch(inotify_fd, "./libgame.so",
                          IN_ATTRIB | IN_MODIFY | IN_CREATE | IN_DELETE);
      }

      i += EVENT_SIZE + event->len;
    }

    while (!handle) {
      printf("Opening lib\n");
      handle = dlopen("./libgame.so", RTLD_LAZY);
      if(!handle){
        fprintf(stderr, "dlopen failed: %s\n", dlerror());
      }
      if (handle) {
        printf("Reloaded the lib\n");
        render = dlsym(handle, "render");

        errors = NULL;
        error_file = fopen("error.log", "r");
        fflush(error_file);
        if (fseek(error_file, 0, SEEK_SET) != 0) {
          perror("fseek failed");
        }
        memset(error_buf,0,1024*4);
        int res = fread(error_buf, 1, 1024 * 4, error_file);
        printf("read error file at got len %d\n", res);
        if (res != 0) {
          errors = error_buf;
          printf("got errors %s\n",errors);
        }
        fclose(error_file);
      }
    }
    render(&run, errors,&state);
  } while (run);

  return EXIT_SUCCESS;
}
