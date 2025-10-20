#include <immintrin.h>
#include <cmath>

void sqrt_simd(int N,
    float initialGuess,
    float *values,
    float *output
) {
    static const __m256 v_threshold = _mm256_set1_ps(0.00001f);
    static const __m256 v_one = _mm256_set1_ps(1.0f);
    static const __m256 v_three = _mm256_set1_ps(3.0f);
    static const __m256 v_half = _mm256_set1_ps(0.5f);
    static const __m256 v_initial_guess = _mm256_set1_ps(initialGuess);
    static const __m256 m_abs = _mm256_castsi256_ps(_mm256_set1_epi32(0x7FFFFFFF));

    int i = 0;
    for (; i <= N - 8; i += 8) {
        __m256 x = _mm256_loadu_ps(values + i);

        __m256 v_guess = v_initial_guess;

        __m256 v_error = _mm256_mul_ps(_mm256_mul_ps(v_guess, x), v_guess);
        v_error = _mm256_sub_ps(v_error, v_one);

        v_error = _mm256_and_ps(v_error, m_abs);

        __m256 v_compare_mask = _mm256_cmp_ps(v_error, v_threshold, _CMP_GT_OQ);

        while (_mm256_movemask_ps(v_compare_mask) != 0) {
            __m256 v_guess_sq = _mm256_mul_ps(v_guess, v_guess);
            __m256 v_guess_cubed = _mm256_mul_ps(v_guess_sq, v_guess);
            __m256 term1 = _mm256_mul_ps(v_three, v_guess);
            __m256 term2 = _mm256_mul_ps(x, v_guess_cubed);
            __m256 v_new_guess = _mm256_mul_ps(_mm256_sub_ps(term1, term2), v_half);

            v_guess = _mm256_blendv_ps(v_guess, v_new_guess, v_compare_mask);

            v_error = _mm256_mul_ps(_mm256_mul_ps(v_guess, x), v_guess);
            v_error = _mm256_sub_ps(v_error, v_one);
            v_error = _mm256_and_ps(v_error, m_abs);

            v_compare_mask = _mm256_cmp_ps(v_error, v_threshold, _CMP_GT_OQ);
        }

        __m256 v_result = _mm256_mul_ps(x, v_guess);
        _mm256_storeu_ps(output + i, v_result);
    }

    for (; i < N; ++i) {
        float x = values[i];
        float guess = initialGuess;
        float error = std::fabs(guess * guess * x - 1.f);
        while (error > 0.00001f) {
            guess = (3.f * guess - x * guess * guess * guess) * 0.5f;
            error = std::fabs(guess * guess * x - 1.f);
        }
        output[i] = x * guess;
    }
}