#include <raylib.h>
#include <raymath.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#define MAX_DIFF 441.67 // sqrt(255^2 + 255^2 + 255^2)

#define FLAG_IMPLEMENTATION
#include "flag.h"

typedef enum { EUCLIDEAN_DIST, REDMEAN_DIST } dist_enum;

uint64_t get_current_ms() {

  struct timespec time;
  clock_gettime(CLOCK_MONOTONIC_RAW, &time);
  uint64_t ms_timestamp = (time.tv_sec) * 1000 + (time.tv_nsec) / 1000000;
  return ms_timestamp;
}

float euclid_dist(Vector3 color1, Vector3 color2) {
  float diff = Vector3Distance(color1, color2);
  diff /= MAX_DIFF;
  return diff;
}

float redmean_diff(Vector3 color1, Vector3 color2) {
  // Calculate the average red value (redmean)
  double redmean = 0.5f * (color1.x + color2.x);

  // Calculate the absolute differences between color components (keeping them
  // as floats)
  double red_diff = fabs(color1.x - color2.x);
  double green_diff = fabs(color1.y - color2.y);
  double blue_diff = fabs(color1.z - color2.z);

  double weighted_red_diff = (2 + redmean / 256.0f) * red_diff * red_diff;

  double weighted_green_diff = 4.0f * green_diff * green_diff;
  double weighted_blue_diff =
      (2.0f + (255.0f - redmean) / 256.0f) * blue_diff * blue_diff;

  double color_diff_interior =
      (weighted_red_diff + weighted_green_diff + weighted_blue_diff);

  double color_diff = sqrt(color_diff_interior);

  return color_diff / 255;
}

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
  Image image1;
  Image image2;
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

  Texture texture1 = LoadTextureFromImage(image1);
  Texture texture2 = LoadTextureFromImage(image2);

  // 3rd image/texture that is the color difference between the images
  // updated when those images move
  Vector2 max_vec = Vector2Max((Vector2){image1.width, image1.height},
                               (Vector2){image2.width, image2.height});
  RenderTexture2D diff_text = LoadRenderTexture(screenWidth, screenHeight);

  Vector2 image1_loc = {0, 0};
  Vector2 image2_loc = {image1.width, 0};
  bool need_update_diff = true;
  Rectangle image_overlap = {0};
  Color *image1_colors = LoadImageColors(image1);
  Color *image2_colors = LoadImageColors(image2);
  dist_enum dist = EUCLIDEAN_DIST;

  while (!WindowShouldClose()) {
    BeginDrawing();
    ClearBackground(RAYWHITE);
    DrawTextureV(texture1, image1_loc, WHITE);
    DrawTextureV(texture2, image2_loc, WHITE);

    // Translate based on mouse right click
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
      Vector2 delta = GetMouseDelta();
      image1_loc = Vector2Add(image1_loc, delta);
      if (!Vector2Equals(delta, (Vector2){0, 0})) {
        // something changed
        need_update_diff = true;
      }
    }

    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
      Vector2 delta = GetMouseDelta();
      image2_loc = Vector2Add(image2_loc, delta);
      if (!Vector2Equals(delta, (Vector2){0, 0})) {
        // something changed
        need_update_diff = true;
      }
    }

    if (IsKeyPressed(KEY_X)) {
      switch (dist) {
      case EUCLIDEAN_DIST:
        dist = REDMEAN_DIST;
        break;
      case REDMEAN_DIST:
        dist = EUCLIDEAN_DIST;
        break;
      }
      need_update_diff = true;
    }

    if (need_update_diff) {
      BeginTextureMode(diff_text);
      // drawing to render texture, diff
      // find overlap rectangle
      Rectangle image_1_rect = {image1_loc.x, image1_loc.y, image1.width,
                                image1.height};

      Rectangle image_2_rect = {image2_loc.x, image2_loc.y, image2.width,
                                image2.height};
      image_overlap = GetCollisionRec(image_1_rect, image_2_rect);

      printf("Got overlap rect %f,%f,%f,%f\n", image_overlap.x, image_overlap.y,
             image_overlap.width, image_overlap.height);
      ClearBackground(BLANK);
      uint64_t start = get_current_ms();
      for (int j = image_overlap.y; j < image_overlap.y + image_overlap.height;
           j++) {
        for (int i = image_overlap.x; i < image_overlap.x + image_overlap.width;
             i++) {
          // get color of both at current location
          Color image1_pixel =
              image1_colors[image1.width * (j - (int)image1_loc.y) +
                            (i - (int)image1_loc.x)];

          Color image2_pixel =
              image2_colors[image2.width * (j - (int)image2_loc.y) +
                            (i - (int)image2_loc.x)];
          Vector3 image1_color_vec = {image1_pixel.r, image1_pixel.g,
                                      image1_pixel.b};

          Vector3 image2_color_vec = {image2_pixel.r, image2_pixel.g,
                                      image2_pixel.b};

          float diff = 0;
          switch (dist) {
          case EUCLIDEAN_DIST:
            diff = euclid_dist(image1_color_vec, image2_color_vec);
            break;
          case REDMEAN_DIST:
            diff = redmean_diff(image1_color_vec, image2_color_vec);
            break;
          }

          DrawPixel(i, j, (Color){255 * diff, 255 * diff, 255 * diff, 255});
        }
      }
      uint64_t stop = get_current_ms();
      printf("making image diff mask was took %ld\n", stop - start);

      EndTextureMode();
      need_update_diff = false;
    }

    // DrawRectanglePro(image_overlap, (Vector2){0, 0}, 0, RED);

    DrawTextureRec(diff_text.texture,
                   (Rectangle){0, 0, (float)diff_text.texture.width,
                               (float)-diff_text.texture.height},
                   (Vector2){0, 0}, WHITE);
    EndDrawing();
  }

  return EXIT_SUCCESS;
}
