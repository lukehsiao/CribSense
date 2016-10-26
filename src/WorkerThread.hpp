#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <future>

template<typename R, typename... Args>
class WorkerThread {
private:
    struct item {
        std::packaged_task<R()> work;
        enum control_t { process, stop } control;

        item(std::packaged_task<R()> &&c) : work(std::move(c)), control(process) {}
        item(control_t c) : work(), control(c) {}
    };

    template<typename V>
    class async_queue {
    private:
        std::queue<V> list;
        std::mutex mutex;
        std::condition_variable cv;

    public:
        void push(V&& i) {
            std::unique_lock<std::mutex> lock(mutex, std::defer_lock);
            std::lock_guard<std::unique_lock<std::mutex>> guard(lock);
            list.push(std::forward<V>(i));
            cv.notify_one();
        }

        V pop() {
            std::unique_lock<std::mutex> lock(mutex, std::defer_lock);
            std::lock_guard<std::unique_lock<std::mutex>> guard(lock);
            while (list.empty())
                cv.wait(lock);
            V tmp = std::move(list.front());
            list.pop();
            return std::move(tmp);
        }
    };

    async_queue<item> queue;
    std::thread thread;

    void loop() {
        while (true) {
            item next = queue.pop();

            if (next.control == item::stop)
                return;

            next.work();
        }
    }

public:
    WorkerThread() : thread(std::mem_fn(&WorkerThread::loop), this) {}

    ~WorkerThread() {
        queue.push(item(item::stop));
        thread.join();
    }

    std::future<R> push(R (*fn) (Args...), Args... args) {
        item it(std::packaged_task<R()>(std::bind(fn, std::forward<Args>(args)...)));
        std::future<R> fut = it.work.get_future();

        queue.push(std::move(it));
        return std::move(fut);
    }
};
