#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <algorithm>

void writePPMImage(
    const int* data,
    const int width,
    const int height,
    const char *filename,
    const int maxIterations
) {
    FILE *fp = fopen(filename, "wb");

    // Write ppm header
    fprintf(fp, "P6\n");
    fprintf(fp, "%d %d\n", width, height);
    fprintf(fp, "255\n");

    for (int i = 0; i < width * height; ++i) {
        // Clamp iteration count for this pixel, then scale the value
        // to 0-1 range. Raise resulting value to a power (< 1) to
        // increase brightness of low iteration count
        // pixels. a.k.a. Make things look cooler.
        const float mapped = std::pow(
            std::min(static_cast<float>(maxIterations),
            static_cast<float>(data[i])) / 256.f, .5f
        );

        // Convert back into 0-255 range, 8-bit channels
        const auto result = static_cast<unsigned char>(255.f * mapped);
        for (int j = 0; j < 3; ++j) {
            fputc(result, fp);
        }
    }

    fclose(fp);
    printf("Wrote image file %s\n", filename);
}
