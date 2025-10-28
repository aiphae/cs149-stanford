#include "tasksys.h"

// =================================================================
// SERIAL IMPLEMENTATION
// =================================================================

const char* TaskSystemSerial::name() {
    return "Serial";
}

TaskSystemSerial::TaskSystemSerial(const int num_threads) : ITaskSystem(num_threads) {}

TaskSystemSerial::~TaskSystemSerial() {}

void TaskSystemSerial::run(IRunnable* runnable, const int num_total_tasks) {
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }
}

TaskID TaskSystemSerial::runAsyncWithDeps(
    IRunnable* runnable, int num_total_tasks, const std::vector<TaskID>& deps
) {
    return 0;
}

void TaskSystemSerial::sync() {}

// =================================================================
// PARALLEL TASK SYSTEM IMPLEMENTATION
// =================================================================

const char* TaskSystemParallelSpawn::name() {
    return "Parallel + Always Spawn";
}

TaskSystemParallelSpawn::TaskSystemParallelSpawn(const int num_threads)
    : ITaskSystem(num_threads)
    , num_threads(num_threads)
{
    threads = new std::thread *[num_threads];
}

TaskSystemParallelSpawn::~TaskSystemParallelSpawn() {
    delete[] threads;
}

void TaskSystemParallelSpawn::run(IRunnable* runnable, const int num_total_tasks) {
    const int tasks_per_thread = (num_total_tasks + num_threads - 1) / num_threads;

    for (int i = 0; i < num_threads; i++) {
        const int start_task = i * tasks_per_thread;
        const int end_task = std::min((i + 1) * tasks_per_thread, num_total_tasks);

        threads[i] = new std::thread([runnable, start_task, end_task, num_total_tasks] {
            for (int j = start_task; j < end_task; ++j) {
                runnable->runTask(j, num_total_tasks);
            }
        });
    }

    for (int i = 0; i < num_threads; ++i) {
        threads[i]->join();
        delete threads[i];
        threads[i] = nullptr;
    }
}

TaskID TaskSystemParallelSpawn::runAsyncWithDeps(
    IRunnable* runnable, int num_total_tasks, const std::vector<TaskID>& deps
) {
    return 0;
}

void TaskSystemParallelSpawn::sync() {}

// =================================================================
// PARALLEL THREAD POOL SPINNING TASK SYSTEM IMPLEMENTATION
// =================================================================

const char* TaskSystemParallelThreadPoolSpinning::name() {
    return "Parallel + Thread Pool + Spin";
}

TaskSystemParallelThreadPoolSpinning::TaskSystemParallelThreadPoolSpinning(const int num_threads)
    : ITaskSystem(num_threads)
    , num_threads(num_threads)
{
    threads = new std::thread *[num_threads];

    for (int i = 0; i < num_threads; ++i) {
        threads[i] = new std::thread([this] {
            while (!stop.load()) {
                int task_id = -1;

                {
                    std::unique_lock<std::mutex> lock(mtx);
                    if (!tasks.empty()) {
                        task_id = tasks.front();
                        tasks.pop();
                    }
                }

                if (task_id != -1) {
                    current_runnable->runTask(task_id, current_num_total_tasks);
                    tasks_completed.fetch_add(1);
                }
            }
        });
    }
}

TaskSystemParallelThreadPoolSpinning::~TaskSystemParallelThreadPoolSpinning() {
    stop.store(true);

    for (int i = 0; i < num_threads; ++i) {
        if (threads[i]->joinable()) {
            threads[i]->join();
        }
        delete threads[i];
    }
    delete[] threads;
}

void TaskSystemParallelThreadPoolSpinning::run(IRunnable* runnable, const int num_total_tasks) {
    current_runnable = runnable;
    current_num_total_tasks = num_total_tasks;
    tasks_completed.store(0);

    {
        std::unique_lock<std::mutex> lock(mtx);
        for (int i = 0; i < num_total_tasks; ++i) {
            tasks.push(i);
        }
    }

    while (tasks_completed.load() < num_total_tasks) {
    }
}

TaskID TaskSystemParallelThreadPoolSpinning::runAsyncWithDeps(
    IRunnable* runnable, int num_total_tasks, const std::vector<TaskID>& deps
) {
    // You do not need to implement this method.
    return 0;
}

void TaskSystemParallelThreadPoolSpinning::sync() {
    // You do not need to implement this method.
    return;
}

// =================================================================
// PARALLEL THREAD POOL SLEEPING TASK SYSTEM IMPLEMENTATION
// =================================================================

const char* TaskSystemParallelThreadPoolSleeping::name() {
    return "Parallel + Thread Pool + Sleep";
}

TaskSystemParallelThreadPoolSleeping::TaskSystemParallelThreadPoolSleeping(const int num_threads)
    : ITaskSystem(num_threads)
    , num_threads(num_threads)
{
    threads = new std::thread *[num_threads];
    for (int i = 0; i < num_threads; ++i) {
        threads[i] = new std::thread([this] {
            while (true) {
                std::function<void()> task_to_run;
                {
                    std::unique_lock<std::mutex> lock(mtx);

                    cv.wait(lock, [this] {
                        return stop || !tasks.empty();
                    });

                    if (stop && tasks.empty()) {
                        return;
                    }

                    if (tasks.empty()) {
                        continue;
                    }

                    task_to_run = tasks.front();
                    tasks.pop();
                }

                task_to_run();
            }
        });
    }
}

TaskSystemParallelThreadPoolSleeping::~TaskSystemParallelThreadPoolSleeping() {
    {
        std::unique_lock<std::mutex> lock(mtx);
        stop = true;
    }
    cv.notify_all();

    for (int i = 0; i < num_threads; ++i) {
        if (threads[i]->joinable()) {
            threads[i]->join();
        }
        delete threads[i];
    }
    delete[] threads;
}

void TaskSystemParallelThreadPoolSleeping::run(IRunnable* runnable, const int num_total_tasks) {
    for (int i = 0; i < num_total_tasks; i++) {
        auto work_item = [runnable, i, num_total_tasks] {
            runnable->runTask(i, num_total_tasks);
        };

        std::unique_lock<std::mutex> lock(mtx);
        tasks.push(work_item);
        lock.unlock();

        cv.notify_one();
    }
}

TaskID TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                    const std::vector<TaskID>& deps) {


    //
    // TODO: CS149 students will implement this method in Part B.
    //

    return 0;
}

void TaskSystemParallelThreadPoolSleeping::sync() {

    //
    // TODO: CS149 students will modify the implementation of this method in Part B.
    //

    return;
}
