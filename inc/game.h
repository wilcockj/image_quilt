#pragma once
#include <raylib.h>


typedef enum { EUCLIDEAN_DIST, REDMEAN_DIST } dist_enum;

typedef struct{
    Texture texture1;
    Texture texture2;
    Image image1;
    Image image2;
    RenderTexture2D diff_text;
    dist_enum dist;
    Vector2 image1_loc;
    Vector2 image2_loc;
    Rectangle image_overlap;
    Camera2D camera;
} game_state;
