#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <iostream>
#include <random>
#include <chrono>
using namespace std;

template<size_t W>
class StreamStats {
private:
    size_t count = 0;
    double mean = 0.0;
    double M2 = 0.0;
    double mini = 0.0;
    double maxi = 0.0;
    
    double window[W] = {0.0};
    size_t wpos = 0;
    size_t wcount = 0;
    double wsum = 0.0;
public:
    void push(double x){
        if(++count > 1){
            double delta1 = x - mean;
            mean += delta1 / (count + 1);
            double delta2 = x - mean;
            M2 += delta1 * delta2;
            mini = min(mini , x);
            maxi = max(maxi , x);
        }
        else mean = mini = maxi = x;

        if(count < W) ++wcount;
        else wsum -= window[wpos];
        window[wpos] = x;
        wsum += x;
        wpos = (wpos + 1) % W;
    }

    size_t samples() const{ return count; }
    double get_mean() const{ return mean; }
    double get_variance() const {
        return (count > 1) ? (M2 / (count - 1)) : 0.0;
    }
    double get_min() const { return mini; }
    double get_max() const { return maxi; }
    double get_window_avg() const {
        return wcount ? (wsum / wcount) : 0.0;
    }
};

int main() {
    StreamStats<100> stats;

    std::mt19937 rng(123);
    std::normal_distribution<double> dist(100.0, 5.0);

    const int N = 1'000'000;

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < N; ++i) {
        double x = dist(rng);
        stats.push(x);
    }

    auto end = std::chrono::steady_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    std::cout << "Samples: " << stats.samples() << "\n";
    std::cout << "Mean: " << stats.get_mean() << "\n";
    std::cout << "Variance: " << stats.get_variance() << "\n";
    std::cout << "Min: " << stats.get_min() << "\n";
    std::cout << "Max: " << stats.get_max() << "\n";
    std::cout << "Rolling Avg(100): " << stats.get_window_avg() << "\n";
    std::cout << "Time per sample: " << (double)ns / N << " ns\n";
}