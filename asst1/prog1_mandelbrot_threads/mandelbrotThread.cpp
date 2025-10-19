#include <cstdio>
#include <thread>
#include <cstdlib>
#include "CycleTimer.h"

struct WorkerArgs {
    float x0, x1;
    float y0, y1;
    int width;
    int height;
    int maxIterations;
    int* output;
    int threadId;
    int numThreads;
};

extern void mandelbrotSerial(
    float x0, float y0, float x1, float y1,
    int width, int height,
    int startRow, int numRows,
    int maxIterations,
    int output[]
);

// Thread entrypoint
void workerThreadStart(const WorkerArgs *const args) {
    const int startRow = args->threadId;
    const int rowStep = args->numThreads;

    mandelbrotSerial(
        args->x0, args->y0, args->x1, args->y1,
        args->width, args->height,
        startRow, rowStep,
        args->maxIterations, args->output
    );

    printf("Hello world from thread %d\n", args->threadId);
}

// Multithreaded implementation of mandelbrot set image generation.
// Threads of execution are created by spawning std::threads.
void mandelbrotThread(
    const int numThreads,
    const float x0, const float y0, const float x1, const float y1,
    const int width, const int height,
    const int maxIterations, int output[]
) {
    static constexpr int MAX_THREADS = 32;

    if (numThreads > MAX_THREADS) {
        fprintf(stderr, "Error: Max allowed threads is %d\n", MAX_THREADS);
        exit(1);
    }

    // Creates thread objects that do not yet represent a thread
    std::thread workers[MAX_THREADS];
    WorkerArgs args[MAX_THREADS];

    for (int i = 0; i < numThreads; i++) {
        args[i].x0 = x0;
        args[i].y0 = y0;
        args[i].x1 = x1;
        args[i].y1 = y1;
        args[i].width = width;
        args[i].height = height;
        args[i].maxIterations = maxIterations;
        args[i].numThreads = numThreads;
        args[i].output = output;
      
        args[i].threadId = i;
    }

    // Spawn the worker threads. Note that only numThreads - 1 std::threads
    // are created, and the main application thread is used as a worker as well.
    for (int i = 1; i < numThreads; i++) {
        workers[i] = std::thread(workerThreadStart, &args[i]);
    }
    
    workerThreadStart(&args[0]);

    // Join worker threads
    for (int i = 1; i < numThreads; i++) {
        workers[i].join();
    }
}
