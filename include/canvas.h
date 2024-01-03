#ifndef CANVAS_H
#define CANVAS_H

#include <stdint.h>

struct Canvas {
    int width;
    int height;
    int scale;
    uint8_t *buffer;
};

void InitCanvas(struct Canvas *canvas, int width, int height);
void DeinitCanvas(struct Canvas *canvas);

void SetPixel(struct Canvas *canvas, int x, int y, uint8_t value);
void DrawCanvas(struct Canvas *canvas);
void ClearCanvas(struct Canvas *canvas);

#endif // !CANVAS_H
