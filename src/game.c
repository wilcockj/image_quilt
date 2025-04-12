#include "game.h"
#include <limits.h>
#include <raylib.h>
#include <raymath.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

Color *image1_colors;
Color *image2_colors;
bool need_update_diff;

#define MAX_DIFF 441.67 // sqrt(255^2 + 255^2 + 255^2)

void DrawBestPath(Texture diff_text, Rectangle image_overlap) {

  // UNFORMATED
  Image full_image = LoadImageFromTexture(diff_text);

  // get image of just the overlap part
  Image diff_image = ImageFromImage(full_image, image_overlap);
  Color *diff_colors = LoadImageColors(diff_image);
  int width = (int)image_overlap.width;
  int height = (int)image_overlap.height;
  int32_t *prev =
      calloc(sizeof(int32_t), image_overlap.width * image_overlap.height);
  uint16_t *dist =
      calloc(sizeof(uint16_t), image_overlap.width * image_overlap.height);

  // will set to -1 when removed
  int32_t *queue =
      calloc(sizeof(int32_t), image_overlap.width * image_overlap.height);
  uint16_t queue_len = image_overlap.width * image_overlap.height;

  for (int i = 0; i < width * height; i++) {
    prev[i] = -1;
    int x = i % width;
    int y = i / width;
    if (i < width) {
      dist[i] = diff_colors[(width - x) + (y - height) * width].r;
    } else {
      dist[i] = UINT16_MAX;
    }
    queue[i] = i;
  }

  while (queue_len) {
    uint16_t min_distance = UINT16_MAX;
    int16_t min_index = -1;
    for (uint32_t i = 0; i < queue_len; i++) {
      if (dist[queue[i]] < min_distance) {
        min_index = i;
        min_distance = dist[queue[i]];
      }
    }

    // If reached the last row, stop early
    int current = queue[min_index];
    if (current >= width * (height - 1))
      break;

    // Remove current from queue
    queue[min_index] = queue[--queue_len];

    // Neighbors: top-left, top, top-right, left, right, bottom-left, bottom,
    // bottom-right
    int x = current % width;
    int y = current / width;

    for (int dy = -1; dy <= 1; dy++) {
      for (int dx = -1; dx <= 1; dx++) {
        if (dx == 0 && dy == 0)
          continue;

        int nx = x + dx;
        int ny = y + dy;

        if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
          int neighbor = ny * width + nx;
          uint16_t alt = dist[current] + diff_colors[neighbor].r;
          if (alt < dist[neighbor]) {
            dist[neighbor] = alt;
            prev[neighbor] = current;
          }
        }
      }
    }
  }
  printf("visited %d pixels\n", (width * height) - queue_len);

  // Find the end of the best path in the last row
  int best_end = -1;
  uint16_t best_cost = UINT16_MAX;
  for (int x = 0; x < width; x++) {
    int i = (height - 1) * width + x;
    if (dist[i] < best_cost) {
      best_cost = dist[i];
      best_end = i;
    }
  }

  // Draw the best path backwards
  int current = best_end;
  while (current != -1) {
    int x = current % width;
    int y = current / width;
    // printf("drawing pixel at %d,%d\n", x, y);
    DrawPixel(x, y, RED); // Or use any color you want
    current = prev[current];
  }

  free(dist);
  free(prev);
  free(queue);

  UnloadImageColors(diff_colors);
  UnloadImage(diff_image);
  UnloadImage(full_image);
}

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

bool AreRectsEqual(Rectangle rect1, Rectangle rect2) {
  return rect1.x == rect2.x && rect1.y == rect2.y &&
         rect1.width == rect2.width && rect1.height == rect2.height;
}

void render(bool *running, char *errors, game_state *state) {
  *running = !WindowShouldClose();

  if (image1_colors == NULL) {
    printf("Getting image 1 colors\n");
    image1_colors = LoadImageColors(state->image1);
  }
  if (image2_colors == NULL) {
    printf("Getting image 2 colors\n");
    image2_colors = LoadImageColors(state->image2);
  }

  // Zoom based on mouse wheel
  float wheel = GetMouseWheelMove();
  if (wheel != 0) {
    // Get the world point that is under the mouse
    Vector2 mouseWorldPos =
        GetScreenToWorld2D(GetMousePosition(), state->camera);

    // Set the offset to where the mouse is
    state->camera.offset = GetMousePosition();

    // Set the target to match, so that the camera maps the world space point
    // under the cursor to the screen space point under the cursor at any zoom
    state->camera.target = mouseWorldPos;

    // Zoom increment
    // Uses log scaling to provide consistent zoom speed
    float scale = 0.2f * wheel;
    state->camera.zoom =
        Clamp(expf(logf(state->camera.zoom) + scale), 0.125f, 64.0f);
  }

  // Translate based on mouse right click
  if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
    Vector2 delta = GetMouseDelta();
    delta = Vector2Scale(delta, -1.0f / state->camera.zoom);
    state->camera.target = Vector2Add(state->camera.target, delta);
  }

  BeginDrawing();
  ClearBackground(RAYWHITE);

  // Translate based on mouse right click
  if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
    Vector2 delta = GetMouseDelta();
    delta = Vector2Scale(delta, 1.0f / state->camera.zoom);

    if (delta.x >= 0) {
      delta.x = ceil(delta.x);
    } else {
      delta.x = floor(delta.x);
    }
    if (delta.y >= 0) {
      delta.y = ceil(delta.y);
    } else {
      delta.y = floor(delta.y);
    }
    state->image1_loc = Vector2Add(state->image1_loc, delta);

    state->image1_loc.x = floor(state->image1_loc.x);
    state->image1_loc.y = floor(state->image1_loc.y);
    if (!Vector2Equals(delta, (Vector2){0, 0})) {
      // something changed
      need_update_diff = true;
    }
  }

  if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
    Vector2 delta = GetMouseDelta();
    delta = Vector2Scale(delta, 1.0f / state->camera.zoom);
    if (delta.x >= 0) {
      delta.x = ceil(delta.x);
    } else {
      delta.x = floor(delta.x);
    }
    if (delta.y >= 0) {
      delta.y = ceil(delta.y);
    } else {
      delta.y = floor(delta.y);
    }
    state->image2_loc = Vector2Add(state->image2_loc, delta);
    state->image2_loc.x = floor(state->image2_loc.x);
    state->image2_loc.y = floor(state->image2_loc.y);
    if (!Vector2Equals(delta, (Vector2){0, 0})) {
      // something changed
      need_update_diff = true;
    }
  }

  if (IsKeyPressed(KEY_X)) {
    switch (state->dist) {
    case EUCLIDEAN_DIST:
      state->dist = REDMEAN_DIST;
      break;
    case REDMEAN_DIST:
      state->dist = EUCLIDEAN_DIST;
      break;
    }
    need_update_diff = true;
  }

  Rectangle image_1_rect = {state->image1_loc.x, state->image1_loc.y,
                            state->image1.width, state->image1.height};

  Rectangle image_2_rect = {state->image2_loc.x, state->image2_loc.y,
                            state->image2.width, state->image2.height};
  state->image_overlap = GetCollisionRec(image_1_rect, image_2_rect);

  if (need_update_diff) {
    BeginTextureMode(state->diff_text);
    // drawing to render texture, diff
    // find overlap rectangle

    printf("Got overlap rect %f,%f,%f,%f\n", state->image_overlap.x,
           state->image_overlap.y, state->image_overlap.width,
           state->image_overlap.height);

    printf("images at %f,%f %f,%f\n", state->image1_loc.x, state->image1_loc.y,
           state->image2_loc.x, state->image2_loc.y);
    ClearBackground(BLANK);
    uint64_t start = get_current_ms();
    uint16_t pixels_drawn = 0;
    Vector2 col_topr =
        (Vector2){state->image_overlap.x, state->image_overlap.y};
    for (int j = state->image_overlap.y;
         j < state->image_overlap.y + state->image_overlap.height; j++) {
      for (int i = state->image_overlap.x;
           i < state->image_overlap.x + state->image_overlap.width; i++) {
        // get color of both at current location
        Color image1_pixel =
            image1_colors[state->image1.width * (j - (int)state->image1_loc.y) +
                          (i - (int)state->image1_loc.x)];

        Color image2_pixel =
            image2_colors[state->image2.width * (j - (int)state->image2_loc.y) +
                          (i - (int)state->image2_loc.x)];
        Vector3 image1_color_vec = {image1_pixel.r, image1_pixel.g,
                                    image1_pixel.b};

        Vector3 image2_color_vec = {image2_pixel.r, image2_pixel.g,
                                    image2_pixel.b};

        float diff = 0;
        switch (state->dist) {
        case EUCLIDEAN_DIST:
          diff = euclid_dist(image1_color_vec, image2_color_vec);
          break;
        case REDMEAN_DIST:
          diff = redmean_diff(image1_color_vec, image2_color_vec);
          break;
        }

        DrawPixel(i - col_topr.x, j - col_topr.y,
                  (Color){255 * diff, 255 * diff, 255 * diff, 255});
        pixels_drawn++;
      }
    }

    uint64_t stop = get_current_ms();
    printf("making image diff mask was took %ld at %f,%f drew %d pixels\n",
           stop - start, state->image_overlap.x, state->image_overlap.y,
           pixels_drawn);

    EndTextureMode();

    BeginTextureMode(state->diff_text);
    DrawBestPath(state->diff_text.texture, state->image_overlap);
    EndTextureMode();
    need_update_diff = false;
  }

  BeginMode2D(state->camera);
  DrawTextureV(state->texture1, state->image1_loc, WHITE);
  DrawTextureV(state->texture2, state->image2_loc, WHITE);

  DrawTextureRec(state->diff_text.texture,
                 (Rectangle){0, 0, (float)state->diff_text.texture.width,
                             (float)-state->diff_text.texture.height},
                 (Vector2){state->image_overlap.x, state->image_overlap.y},
                 WHITE);
  if (errors) {
    DrawText(errors, 0, 0, 6, RED);
  }
  EndMode2D();
  EndDrawing();
}
