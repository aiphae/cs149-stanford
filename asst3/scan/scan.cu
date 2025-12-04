#include <cstdio>

#include <cuda.h>
#include <cuda_runtime.h>
#include <driver_functions.h>
#include <thrust/scan.h>
#include <thrust/device_ptr.h>
#include <thrust/device_malloc.h>
#include <thrust/device_free.h>

#include "CycleTimer.h"

#define THREADS_PER_BLOCK 256

// Helper function to round an integer up to the next power of 2
static inline int nextPow2(int n) {
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n++;
    return n;
}

__global__
void upsweep_kernel(int* data, int two_d, int N) {
    const int tid = blockIdx.x * blockDim.x + threadIdx.x;
    const size_t i = tid * two_d * 2;
    if (i < N) {
        const int left = i + two_d - 1;
        const int right = left + two_d;
        data[right] += data[left];
    }
}

__global__
void downsweep_kernel(int* data, int two_d, int N) {
    const int tid = blockIdx.x * blockDim.x + threadIdx.x;
    const size_t i = tid * two_d * 2;
    if (i < N) {
        const int left = i + two_d - 1;
        const int right = left + two_d;
        const int t = data[left];
        data[left] = data[right];
        data[right] += t;
    }
}

__global__
void set_zero_kernel(int* data, int index) {
    data[index] = 0;
}

void exclusive_scan(int* input, int N, int* result) {
    const int length = nextPow2(N);

    cudaMemcpy(result, input, N * sizeof(int), cudaMemcpyDeviceToDevice);

    // Pad the rest with zeros if necessary
    if (length > N) {
        cudaMemset(result + N, 0, (length - N) * sizeof(int));
    }

    // Up-sweep
    for (int two_d = 1; two_d < length; two_d *= 2) {
        const int num_threads = length / (two_d * 2);
        const int blocks = (num_threads + THREADS_PER_BLOCK - 1) / THREADS_PER_BLOCK;

        upsweep_kernel<<<blocks, THREADS_PER_BLOCK>>>(result, two_d, length);
    }

    // Check for errors after upsweep (sync needed to catch execution errors)
    cudaDeviceSynchronize();

    // Set root to zero
    set_zero_kernel<<<1, 1>>>(result, length - 1);

    // Down-sweep
    for (int two_d = length / 2; two_d >= 1; two_d /= 2) {
        const int num_threads = length / (two_d * 2);
        const int blocks = (num_threads + THREADS_PER_BLOCK - 1) / THREADS_PER_BLOCK;

        downsweep_kernel<<<blocks, THREADS_PER_BLOCK>>>(result, two_d, length);
    }

    cudaDeviceSynchronize();
}

// This function is a timing wrapper around the student's
// implementation of scan - it copies the input to the GPU
// and times the invocation of the exclusive_scan() function
// above. Students should not modify it.
double cudaScan(const int* in_array, const int* end, int* result_array) {
    int* device_result;
    int* device_input;
    const int N = static_cast<int>(end - in_array);

    // This code rounds the arrays provided to exclusive_scan up
    // to a power of 2, but elements after the end of the original
    // input are left uninitialized and not checked for correctness.
    //
    // Student implementations of exclusive_scan may assume an array's
    // allocated length is a power of 2 for simplicity. This will
    // result in extra work on non-power-of-2 inputs, but it's worth
    // the simplicity of a power of two only solution.

    const int rounded_length = nextPow2(static_cast<int>(end - in_array));
    
    cudaMalloc((void**) &device_result, sizeof(int) * rounded_length);
    cudaMalloc((void**) &device_input, sizeof(int) * rounded_length);

    // For convenience, both the input and output vectors on the
    // device are initialized to the input values. This means that
    // students are free to implement an in-place scan on the result
    // vector if desired.  If you do this, you will need to keep this
    // in mind when calling exclusive_scan from find_repeats.
    cudaMemcpy(device_input, in_array, (end - in_array) * sizeof(int), cudaMemcpyHostToDevice);
    cudaMemcpy(device_result, in_array, (end - in_array) * sizeof(int), cudaMemcpyHostToDevice);

    const double startTime = CycleTimer::currentSeconds();

    exclusive_scan(device_input, N, device_result);

    // Wait for completion
    cudaDeviceSynchronize();
    const double endTime = CycleTimer::currentSeconds();
       
    cudaMemcpy(result_array, device_result, (end - in_array) * sizeof(int), cudaMemcpyDeviceToHost);

    return endTime - startTime;
}

// Wrapper around the Thrust library's exclusive scan function
// As above in cudaScan(), this function copies the input to the GPU
// and times only the execution of the scan itself.
//
// Students are not expected to produce implementations that achieve
// performance that is competition to the Thrust version, but it is fun to try.
double cudaScanThrust(const int* in_array, const int* end, int* result_array) {
    const int length = static_cast<int>(end - in_array);
    const thrust::device_ptr<int> d_input = thrust::device_malloc<int>(length);
    const thrust::device_ptr<int> d_output = thrust::device_malloc<int>(length);
    
    cudaMemcpy(d_input.get(), in_array, length * sizeof(int), cudaMemcpyHostToDevice);

    const double startTime = CycleTimer::currentSeconds();

    thrust::exclusive_scan(d_input, d_input + length, d_output);

    cudaDeviceSynchronize();
    const double endTime = CycleTimer::currentSeconds();
   
    cudaMemcpy(result_array, d_output.get(), length * sizeof(int), cudaMemcpyDeviceToHost);

    thrust::device_free(d_input);
    thrust::device_free(d_output);

    return endTime - startTime;
}

__global__
void mark_repeats_kernel(const int* input, int* flags, const int length) {
    const int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < length - 1) {
        if (input[i] == input[i + 1]) {
            flags[i] = 1;
        }
        else {
            flags[i] = 0;
        }
    }
    if (i == length - 1) {
        flags[i] = 0;
    }
}

__global__
void scatter_indices_kernel(const int* flags, const int* scan_results, int* output, int length) {
    const int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < length) {
        if (flags[i] == 1) {
            output[scan_results[i]] = i;
        }
    }
}

// Given an array of integers `device_input`, returns an array of all
// indices `i` for which `device_input[i] == device_input[i+1]`.
//
// Returns the total number of pairs found
int find_repeats(int* device_input, int length, int* device_output) {
    const int rounded_length = nextPow2(length);
    constexpr int threads = 256;
    const int blocks = (length + threads - 1) / threads;

    // Compute flags by comparing pairs of elements
    int* d_flags;
    cudaMalloc(&d_flags, sizeof(int) * length);
    mark_repeats_kernel<<<blocks, threads>>>(device_input, d_flags, length);
    cudaDeviceSynchronize();

    // Scan the flags
    int* d_scan_result;
    cudaMalloc(&d_scan_result, sizeof(int) * rounded_length);
    exclusive_scan(d_flags, length, d_scan_result);

    // Scatter
    scatter_indices_kernel<<<blocks, threads>>>(d_flags, d_scan_result, device_output, length);

    int last_scan, last_flag;
    cudaMemcpy(&last_scan, &d_scan_result[length - 1], sizeof(int), cudaMemcpyDeviceToHost);
    cudaMemcpy(&last_flag, &d_flags[length - 1], sizeof(int), cudaMemcpyDeviceToHost);

    cudaFree(d_flags);
    cudaFree(d_scan_result);

    return last_scan + last_flag;
}

// Timing wrapper around find_repeats. You should not modify this function.
double cudaFindRepeats(const int *input, const int length, int *output, int *output_length) {
    int *device_input;
    int *device_output;
    const int rounded_length = nextPow2(length);
    
    cudaMalloc((void**) &device_input, rounded_length * sizeof(int));
    cudaMalloc((void**) &device_output, rounded_length * sizeof(int));
    cudaMemcpy(device_input, input, length * sizeof(int), cudaMemcpyHostToDevice);

    cudaDeviceSynchronize();
    const double startTime = CycleTimer::currentSeconds();
    
    const int result = find_repeats(device_input, length, device_output);

    cudaDeviceSynchronize();
    const double endTime = CycleTimer::currentSeconds();

    // Set output count and results array
    *output_length = result;
    cudaMemcpy(output, device_output, length * sizeof(int), cudaMemcpyDeviceToHost);

    cudaFree(device_input);
    cudaFree(device_output);

    return endTime - startTime;
}

void printCudaInfo() {
    int deviceCount = 0;
    cudaError_t err = cudaGetDeviceCount(&deviceCount);

    printf("---------------------------------------------------------\n");
    printf("Found %d CUDA devices\n", deviceCount);

    for (int i = 0; i < deviceCount; i++) {
        cudaDeviceProp deviceProps;
        cudaGetDeviceProperties(&deviceProps, i);
        printf("Device %d: %s\n", i, deviceProps.name);
        printf("   SMs:        %d\n", deviceProps.multiProcessorCount);
        printf("   Global mem: %.0f MB\n", static_cast<float>(deviceProps.totalGlobalMem) / (1024 * 1024));
        printf("   CUDA Cap:   %d.%d\n", deviceProps.major, deviceProps.minor);
    }
    printf("---------------------------------------------------------\n"); 
}
