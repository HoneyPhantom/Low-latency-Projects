#include <iostream>
#include <cstddef>
#include <atomic>
#include <thread>
#include <cstring>
#include <fstream>
using namespace std;

template<typename T, size_t N>
class SPSCQueue {
private:
    T buffer[N];
    atomic<size_t> head{0};
    atomic<size_t> tail{0};
public:
    bool push(const T& value){
        size_t t = tail.load(memory_order_relaxed);
        size_t next = (t + 1) % N;
        if(next == head.load(memory_order_acquire)) return false;
        buffer[t] = value;
        tail.store(next , memory_order_release);
        return true;
    }

    bool pop(T& value){
        size_t h = head.load(memory_order_relaxed);
        size_t next = (h + 1) % N;
        if(next == tail.load(memory_order_acquire)) return false;
        value = buffer[h];
        head.store(next , memory_order_release);
        return true;
    }
};

static const int MAX_MSG = 128;
static const int BATCH = 64;

struct LogMessage {
    char text[MAX_MSG];
    size_t len;
};

class AsyncLogger {
private:
    SPSCQueue<LogMessage , 4096> queue;
    atomic<bool> run{true};
    thread worker;
    ofstream out;

    void consume_loop(){
        LogMessage m;
        while(run.load(memory_order_acquire)){
            int count = 0;
            while(count < BATCH && queue.pop(m)){
                out.write(m.text , m.len + 1);
                ++count;
            }
            if(count) out.flush();
        }
        while(queue.pop(m)) out.write(m.text , m.len + 1);
        out.flush();
    }
public:
    AsyncLogger(const char* filename) : out(filename){
        worker = thread([this](){ this->consume_loop(); });
    }

    void log_async(const char* msg){
        LogMessage m{};
        m.len = min<size_t>(strlen(msg) , MAX_MSG - 1);
        memcpy(m.text , msg , m.len);
        m.text[m.len] = '\n';
        queue.push(m);
    }   

    ~AsyncLogger() {
        run.store(false , memory_order_release);
        worker.join();
        out.close();
    }
};

int main() {
    AsyncLogger logger("log.txt");
    for(int i = 0; i < 100000; ++i) logger.log_async("Hello low latency logging");
    this_thread::sleep_for(chrono::seconds(1));
    return 0;
}