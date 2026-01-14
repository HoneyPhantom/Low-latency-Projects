#pragma once
#include <vector>
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <iomanip>
using namespace std;

class LatencyRecorder {
private:
    vector<uint64_t> samples;
    uint64_t bucket_ns;
    uint64_t max_ns;

public:
    explicit LatencyRecorder(size_t reserve_count = 0 , uint64_t bucket_size_ns = 50, uint64_t max_latency_ns = 5000) : bucket_ns(bucket_size_ns), max_ns(max_latency_ns) {
        if(reserve_count) samples.reserve(reserve_count);
    }

    inline size_t size() const{
        return samples.size();
    }

    inline void add(uint64_t ns) {
        samples.push_back(ns);
    }

    void clear() {
        samples.clear();
    }

    void report(ostream& out , size_t warmup_drop = 0) {
        if(samples.empty() || warmup_drop >= size()){
            out << "No samples to report." << endl;
            return;
        }

        vector<uint64_t> v(samples.begin() + warmup_drop , samples.end());
        sort(v.begin() , v.end());
        size_t n = v.size();


        uint64_t minv = v.front();
        uint64_t maxv = v.back();

        __uint128_t sum = 0;
        for(uint64_t x : v) sum += x;
        uint64_t avg = (uint64_t)(sum / n);

        auto pct = [&](double p)->uint64_t {
            size_t idx = (size_t)(p * 0.01 * (n - 1));
            return v[idx];
        };

        uint64_t p50 = pct(50);
        uint64_t p90 = pct(90);
        uint64_t p99 = pct(99);
        uint64_t p999 = pct(99.9);

        out << "Samples: " << n << " (warmup dropped: " << warmup_drop << ")\n";
        out << "Latency (ns)\n";
        out << "  min   : " << minv << "\n";
        out << "  avg   : " << avg << "\n";
        out << "  p50   : " << p50 << "\n";
        out << "  p90   : " << p90 << "\n";
        out << "  p99   : " << p99 << "\n";
        out << "  p99.9 : " << p999 << "\n";
        out << "  max   : " << maxv << "\n\n";

        const uint64_t buckets = max_ns / bucket_ns + 1;
        vector<uint64_t> hist(buckets, 0);

        for(uint64_t x : v) {
            uint64_t bi = x / bucket_ns;
            if (bi >= buckets) bi = buckets - 1;
            hist[bi]++;
        }

        out << "Histogram (bucket = " << bucket_ns << " ns)" << endl;
        for(uint64_t i = 0; i < buckets; ++i) {
            uint64_t lo = i * bucket_ns;
            uint64_t hi = lo + bucket_ns - 1;

            out << setw(6) << lo << " - " << setw(6) << hi << " ns : " << hist[i] << endl;
        }
    }
};
