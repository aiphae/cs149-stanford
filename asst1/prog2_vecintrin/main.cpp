#include <cstdio>
#include <algorithm>
#include <getopt.h>
#include <cmath>
#include "CS149intrin.h"
#include "logger.h"

using namespace std;

#define EXP_MAX 10

Logger CS149Logger;

void usage(const char *progname);
void initValue(float *values, int *exponents, float *output, float *gold, unsigned int N);
void absSerial(float *values, float *output, int N);
void absVector(float *values, float *output, int N);
void clampedExpSerial(const float *values, const int *exponents, float *output, int N);
void clampedExpVector(float *values, int *exponents, float *output, int N);
float arraySumSerial(const float *values, int N);
float arraySumVector(float *values, int N);
bool verifyResult(const float *values, const int *exponents, const float *output, const float *gold, int N);

int main(int argc, char *argv[]) {
    int N = 16;
    bool printLog = false;

    // Parse commandline options
    int opt;
    static option long_options[] = {
        {"size", 1, nullptr, 's'},
        {"log", 0, nullptr, 'l'},
        {"help", 0, nullptr, '?'},
        {nullptr, 0, nullptr, 0}
    };

    while ((opt = getopt_long(argc, argv, "s:l?", long_options, NULL)) != EOF) {
        switch (opt) {
            case 's':
                N = atoi(optarg);
                if (N <= 0) {
                    printf("Error: Workload size is set to %d (<0).\n", N);
                    return -1;
                }
                break;
            case 'l':
                printLog = true;
                break;
            case '?':
            default:
                usage(argv[0]);
                return 1;
        }
    }

    auto *values = new float[N + VECTOR_WIDTH];
    const auto exponents = new int[N + VECTOR_WIDTH];
    auto *output = new float[N + VECTOR_WIDTH];
    auto *gold = new float[N + VECTOR_WIDTH];
    initValue(values, exponents, output, gold, N);

    clampedExpSerial(values, exponents, gold, N);
    clampedExpVector(values, exponents, output, N);

    // absSerial(values, gold, N);
    // absVector(values, output, N);

    printf("\e[1;31mCLAMPED EXPONENT\e[0m (required) \n");
    const bool clampedCorrect = verifyResult(values, exponents, output, gold, N);
    if (printLog) {
        CS149Logger.printLog();
    }
    CS149Logger.printStats();

    printf("************************ Result Verification *************************\n");
    if (!clampedCorrect) {
        printf("@@@ Failed!!!\n");
    }
    else {
        printf("Passed!!!\n");
    }

    printf("\n\e[1;31mARRAY SUM\e[0m (bonus) \n");
    if (N % VECTOR_WIDTH == 0) {
        const float sumGold = arraySumSerial(values, N);
        const float sumOutput = arraySumVector(values, N);
        constexpr float epsilon = 0.1f;
        if (const bool sumCorrect = abs(sumGold - sumOutput) < epsilon * 2; !sumCorrect) {
            printf("Expected %f, got %f\n.", sumGold, sumOutput);
            printf("@@@ Failed!!!\n");
        }
        else {
            printf("Passed!!!\n");
        }
    }
    else {
        printf("Must have N %% VECTOR_WIDTH == 0 for this problem (VECTOR_WIDTH is %d)\n", VECTOR_WIDTH);
    }

    delete[] values;
    delete[] exponents;
    delete[] output;
    delete[] gold;

    return 0;
}

void usage(const char *progname) {
    printf("Usage: %s [options]\n", progname);
    printf("Program Options:\n");
    printf("  -s  --size <N>     Use workload size N (Default = 16)\n");
    printf("  -l  --log          Print vector unit execution log\n");
    printf("  -?  --help         This message\n");
}

void initValue(float *values, int *exponents, float *output, float *gold, unsigned int N) {
    for (unsigned int i = 0; i < N + VECTOR_WIDTH; i++) {
        // Random input values
        values[i] = -1.f + 4.f * static_cast<float>(rand()) / RAND_MAX;
        exponents[i] = rand() % EXP_MAX;
        output[i] = 0.f;
        gold[i] = 0.f;
    }
}

bool verifyResult(const float *values, const int *exponents, const float *output, const float *gold, const int N) {
    int incorrect = -1;
    constexpr float epsilon = 0.00001f;

    for (int i = 0; i < N + VECTOR_WIDTH; i++) {
        if (abs(output[i] - gold[i]) > epsilon) {
            incorrect = i;
            break;
        }
    }

    if (incorrect != -1) {
        if (incorrect >= N) {
            printf("You have written to out of bound value!\n");
        }

        printf("Wrong calculation at value[%d]!\n", incorrect);
        printf("value  = ");
        for (int i = 0; i < N; i++) {
            printf("%f ", values[i]);
        }
        printf("\n");

        printf("exp    = ");
        for (int i = 0; i < N; i++) {
            printf("%9d ", exponents[i]);
        }
        printf("\n");

        printf("output = ");
        for (int i = 0; i < N; i++) {
            printf("%f ", output[i]);
        }
        printf("\n");

        printf("gold   = ");
        for (int i = 0; i < N; i++) {
            printf("%f ", gold[i]);
        }
        printf("\n");

        return false;
    }

    printf("Results matched with answer!\n");
    return true;
}

// Computes the absolute value of all elements in the input array
// values, stores result in output.
void absSerial(const float *values, float *output, const int N) {
    for (int i = 0; i < N; i++) {
        if (const float x = values[i]; x < 0) {
            output[i] = -x;
        }
        else {
            output[i] = x;
        }
    }
}

// Implementation of absSerial() above, but it is vectorized using CS149 intrinsics.
void absVector(float *values, float *output, const int N) {
    __cs149_vec_float x;
    __cs149_vec_float result;
    __cs149_vec_float zero = _cs149_vset_float(0.f);

    // Note: Take a careful look at this loop indexing. This example
    // code is not guaranteed to work when (N % VECTOR_WIDTH) != 0.
    // Why is that the case?
    for (int i = 0; i < N; i += VECTOR_WIDTH) {
        // All ones
        __cs149_mask maskAll = _cs149_init_ones();

        // All zeros
        __cs149_mask maskIsNegative = _cs149_init_ones(0);

        // Load vector of values from contiguous memory addresses
        _cs149_vload_float(x, values + i, maskAll); // x = values[i];

        // Set mask according to predicate
        _cs149_vlt_float(maskIsNegative, x, zero, maskAll); // if (x < 0) {

        // Execute instruction using mask ("if" clause)
        _cs149_vsub_float(result, zero, x, maskIsNegative); //   output[i] = -x;

        // Inverse maskIsNegative to generate "else" mask
        __cs149_mask maskIsNotNegative = _cs149_mask_not(maskIsNegative); // } else {

        // Execute instruction ("else" clause)
        _cs149_vload_float(result, values + i, maskIsNotNegative); //   output[i] = x; }

        // Write results back to memory
        _cs149_vstore_float(output + i, result, maskAll);
    }
}

// Accepts an array of values and an array of exponents.
//
// For each element, compute values[i]^exponents[i] and clamp value to
// 9.999. Store result in output.
void clampedExpSerial(const float *values, const int *exponents, float *output, const int N) {
    for (int i = 0; i < N; i++) {
        const float x = values[i];
        if (const int y = exponents[i]; y == 0) {
            output[i] = 1.f;
        }
        else {
            float result = x;
            int count = y - 1;
            while (count > 0) {
                result *= x;
                count--;
            }
            if (result > 9.999999f) {
                result = 9.999999f;
            }
            output[i] = result;
        }
    }
}

void clampedExpVector(float *values, int *exponents, float *output, const int N) {
    __cs149_vec_float result;

    __cs149_vec_int v_zero_int = _cs149_vset_int(0);
    __cs149_vec_int v_one_int = _cs149_vset_int(1);
    __cs149_vec_float v_one_float = _cs149_vset_float(1.0f);
    __cs149_vec_float v_clamp = _cs149_vset_float(9.999999f);

    for (int i = 0; i < N; i += VECTOR_WIDTH) {
        const int elements_left = N - i;
        __cs149_mask iterMask;
        if (elements_left < VECTOR_WIDTH) {
             iterMask = _cs149_init_ones(elements_left);
        }
        else {
             iterMask = _cs149_init_ones();
        }

        __cs149_vec_float x;
        __cs149_vec_int y;
        _cs149_vload_float(x, values + i, iterMask);
        _cs149_vload_int(y, exponents + i, iterMask);

        __cs149_mask m_is_zero;
        _cs149_veq_int(m_is_zero, y, v_zero_int, iterMask);
        _cs149_vmove_float(result, v_one_float, m_is_zero);

        __cs149_mask m_is_not_zero = _cs149_mask_not(m_is_zero);
        m_is_not_zero = _cs149_mask_and(m_is_not_zero, iterMask);
        _cs149_vmove_float(result, x, m_is_not_zero);

        __cs149_vec_int v_count;
        _cs149_vsub_int(v_count, y, v_one_int, iterMask);

        __cs149_mask m_active;
        _cs149_vgt_int(m_active, v_count, v_zero_int, iterMask);

        while (_cs149_cntbits(m_active) > 0) {
            _cs149_vmult_float(result, result, x, m_active);
            _cs149_vsub_int(v_count, v_count, v_one_int, m_active);
            _cs149_vgt_int(m_active, v_count, v_zero_int, m_active);
        }

        __cs149_mask m_greater;
        _cs149_vgt_float(m_greater, result, v_clamp, iterMask);
        _cs149_vset_float(result, 9.999999, m_greater);

        _cs149_vstore_float(output + i, result, iterMask);
    }
}

// Returns the sum of all elements in values.
float arraySumSerial(const float *values, const int N) {
    float sum = 0;
    for (int i = 0; i < N; i++) {
        sum += values[i];
    }
    return sum;
}

// Returns the sum of all elements in values.
float arraySumVector(float *values, const int N) {
    float sum = 0.0f;

    __cs149_vec_float v_sum = _cs149_vset_float(0.f);
    __cs149_vec_float v_zero_float = _cs149_vset_float(0.f);
    __cs149_mask maskAll = _cs149_init_ones();
    __cs149_vec_float x;

    for (int i = 0; i < N; i += VECTOR_WIDTH) {
        const int elements_left = N - i;
        __cs149_mask iterMask;
        if (elements_left < VECTOR_WIDTH) {
            iterMask = _cs149_init_ones(elements_left);
        }
        else {
            iterMask = _cs149_init_ones();
        }

        _cs149_vmove_float(x, v_zero_float, maskAll);
        _cs149_vload_float(x, values + i, iterMask);
        _cs149_vadd_float(v_sum, v_sum, x, maskAll);
    }

    float temp_array[VECTOR_WIDTH];
    _cs149_vstore_float(temp_array, v_sum, maskAll);

    for (const float i : temp_array) {
        sum += i;
    }

    return sum;
}