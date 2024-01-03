#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "raylib.h"
#include "canvas.h"

static void DrawCanvasPixel(struct Canvas *canvas, int x, int y) {
    int scale = canvas->scale;
    DrawRectangle(
        x * scale, 
        y * scale,
        scale,
        scale,
        WHITE
    );
}

void ClearCanvas(struct Canvas *canvas) {
    //memset(canvas->buffer, 0, canvas->width * canvas->height * sizeof(uint8_t));
    for (int i = 0; i < canvas->width * canvas->height; i++) {
        canvas->buffer[i] = 0;
    }
}

void InitCanvas(struct Canvas *canvas, int width, int height) {
    canvas->width = width;
    canvas->height = height;
    canvas->scale = 10;
    canvas->buffer = (uint8_t *) malloc(width * height * sizeof(uint8_t));

    if (!canvas->buffer) {
        fprintf(stderr, "Failed to allocate memory for canvas\n");
        exit(1);
    }

    ClearCanvas(canvas);
    fprintf(stderr, "Initialized canvas %dx%d\n", width, height);
}

void DeinitCanvas(struct Canvas *canvas) {
    free(canvas->buffer);
    free(canvas);
}

void SetPixel(struct Canvas *canvas, int x, int y, uint8_t value) {
    int index = y * canvas->width + x;
    canvas->buffer[index] = value;
}

void DrawCanvas(struct Canvas *canvas) {
    int length = canvas->width * canvas->height;
    for (int i = 0; i < length; i++) {
        int x = i % canvas->width; 
        int y = i / canvas->width;

        if (canvas->buffer[i] == 1) {
            DrawCanvasPixel(canvas, x, y);
        }
    }
}
