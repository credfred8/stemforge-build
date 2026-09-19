#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

int main()
{
    constexpr int n = 343980;
    constexpr int overlap = n / 4;
    constexpr int stride = n - overlap;
    constexpr int total = n * 3 + 12345;

    std::vector<float> window(n, 1.0f);
    for (int i = 0; i < overlap; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(overlap - 1);
        window[i] = t;
        window[n - 1 - i] = t;
    }

    std::vector<float> out(total, 0.0f), weight(total, 0.0f);
    const int chunks = std::max(1, (total + stride - 1) / stride);
    for (int c = 0; c < chunks; ++c)
    {
        const int start = c * stride;
        const int end = std::min(start + n, total);
        for (int i = 0; i < end - start; ++i)
        {
            const float w = window[i];
            out[start + i] += w;
            weight[start + i] += w;
        }
    }

    float maxErr = 0.0f;
    for (int i = 1; i < total; ++i)
    {
        out[i] /= std::max(weight[i], 1.0e-8f);
        maxErr = std::max(maxErr, std::abs(out[i] - 1.0f));
    }

    std::cout << "chunks=" << chunks << " max_error=" << maxErr << "\n";
    return maxErr < 1.0e-5f ? 0 : 1;
}
