#ifndef _TASKSYS_H
#define _TASKSYS_H

#include "itasksys.h"
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

class TaskSystemSerial: public ITaskSystem {
public:
    explicit TaskSystemSerial(int num_threads);
    ~TaskSystemSerial() override;

    const char* name() override;
    void run(IRunnable* runnable, int num_total_tasks) override;
    TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks, const std::vector<TaskID>& deps) override;
    void sync() override;
};

class TaskSystemParallelSpawn: public ITaskSystem {
public:
    explicit TaskSystemParallelSpawn(int num_threads);
    ~TaskSystemParallelSpawn() override;

    const char* name() override;
    void run(IRunnable* runnable, int num_total_tasks) override;
    TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks, const std::vector<TaskID>& deps) override;
    void sync() override;

private:
    std::thread **threads;
    const int num_threads;
};

class TaskSystemParallelThreadPoolSpinning: public ITaskSystem {
public:
    explicit TaskSystemParallelThreadPoolSpinning(int num_threads);
    ~TaskSystemParallelThreadPoolSpinning() override;

    const char* name() override;
    void run(IRunnable* runnable, int num_total_tasks) override;
    TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks, const std::vector<TaskID>& deps) override;
    void sync() override;

private:
    std::thread **threads;
    const int num_threads;

    IRunnable *current_runnable;
    int current_num_total_tasks;

    std::queue<int> tasks;
    std::mutex mtx;

    std::atomic<int> tasks_completed{0};
    std::atomic<bool> stop{false};
};

class TaskSystemParallelThreadPoolSleeping: public ITaskSystem {
public:
    explicit TaskSystemParallelThreadPoolSleeping(int num_threads);
    ~TaskSystemParallelThreadPoolSleeping() override;

    const char* name() override;
    void run(IRunnable* runnable, int num_total_tasks) override;
    TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks, const std::vector<TaskID>& deps) override;
    void sync() override;

private:
    std::thread **threads;
    const int num_threads;

    std::queue<std::function<void()>> tasks;
    std::mutex mtx;

    std::condition_variable cv;
    bool stop = false;
};

#endif
