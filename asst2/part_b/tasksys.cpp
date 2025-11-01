#include "tasksys.h"


IRunnable::~IRunnable() {}

ITaskSystem::ITaskSystem(int num_threads) {}
ITaskSystem::~ITaskSystem() {}

/*
 * ================================================================
 * Serial task system implementation
 * ================================================================
 */

const char* TaskSystemSerial::name() {
    return "Serial";
}

TaskSystemSerial::TaskSystemSerial(int num_threads): ITaskSystem(num_threads) {
}

TaskSystemSerial::~TaskSystemSerial() {}

void TaskSystemSerial::run(IRunnable* runnable, int num_total_tasks) {
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }
}

TaskID TaskSystemSerial::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                          const std::vector<TaskID>& deps) {
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }

    return 0;
}

void TaskSystemSerial::sync() {
    return;
}

/*
 * ================================================================
 * Parallel Task System Implementation
 * ================================================================
 */

const char* TaskSystemParallelSpawn::name() {
    return "Parallel + Always Spawn";
}

TaskSystemParallelSpawn::TaskSystemParallelSpawn(int num_threads): ITaskSystem(num_threads) {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
}

TaskSystemParallelSpawn::~TaskSystemParallelSpawn() {}

void TaskSystemParallelSpawn::run(IRunnable* runnable, int num_total_tasks) {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }
}

TaskID TaskSystemParallelSpawn::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                 const std::vector<TaskID>& deps) {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }

    return 0;
}

void TaskSystemParallelSpawn::sync() {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
    return;
}

/*
 * ================================================================
 * Parallel Thread Pool Spinning Task System Implementation
 * ================================================================
 */

const char* TaskSystemParallelThreadPoolSpinning::name() {
    return "Parallel + Thread Pool + Spin";
}

TaskSystemParallelThreadPoolSpinning::TaskSystemParallelThreadPoolSpinning(int num_threads): ITaskSystem(num_threads) {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelThreadPoolSpinning in Part B.
}

TaskSystemParallelThreadPoolSpinning::~TaskSystemParallelThreadPoolSpinning() {}

void TaskSystemParallelThreadPoolSpinning::run(IRunnable* runnable, int num_total_tasks) {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelThreadPoolSpinning in Part B.
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }
}

TaskID TaskSystemParallelThreadPoolSpinning::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                              const std::vector<TaskID>& deps) {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelThreadPoolSpinning in Part B.
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }

    return 0;
}

void TaskSystemParallelThreadPoolSpinning::sync() {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelThreadPoolSpinning in Part B.
    return;
}

/*
 * ================================================================
 * Parallel Thread Pool Sleeping Task System Implementation
 * ================================================================
 */

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
    runAsyncWithDeps(runnable, num_total_tasks, {});
    sync();
}

TaskID TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps(
    IRunnable* runnable, const int num_total_tasks, const std::vector<TaskID>& deps
) {
    const TaskID new_id = next_task_id.fetch_add(1);
    auto group = std::make_unique<TaskGroup>(new_id, runnable, num_total_tasks);

    std::unique_lock<std::mutex> lock(graph_mtx);

    int outstanding_deps = 0;

    for (const TaskID &dep_id : deps) {
        auto it = all_groups.find(dep_id);
        if (it != all_groups.end()) {
            TaskGroup *dep_group = it->second.get();
            if (dep_group->tasks_remaining.load() > 0) {
                dep_group->dependents.push_back(new_id);
                outstanding_deps++;
            }
        }
    }

    group->outstanding_dependencies.store(outstanding_deps);
    total_incomplete_groups.fetch_add(1);

    TaskGroup *group_ptr = group.get();
    all_groups[new_id] = std::move(group);

    if (outstanding_deps == 0) {
        enqueue_tasks_for_group(group_ptr);
    }

    return new_id;
}

void TaskSystemParallelThreadPoolSleeping::enqueue_tasks_for_group(TaskGroup* group) {
    int num_tasks_added = group->num_total_tasks;

    {
        std::unique_lock<std::mutex> lock(mtx);
        for (int i = 0; i < num_tasks_added; ++i) {
            auto work_item = [this, group, i] {
                group->runnable->runTask(i, group->num_total_tasks);
                if (group->tasks_remaining.fetch_sub(1) == 1) {
                    notify_dependents_of_completion(group);
                }
            };
            tasks.push(std::move(work_item));
        }
    }


    if (num_tasks_added == 1) {
        cv.notify_one();
    }
    else {
        cv.notify_all();
    }
}

void TaskSystemParallelThreadPoolSleeping::notify_dependents_of_completion(TaskGroup* group) {
    {
        std::unique_lock<std::mutex> lock(graph_mtx);
        for (const TaskID &dep: group->dependents) {
            auto it = all_groups.find(dep);
            if (it != all_groups.end()) {
                TaskGroup *dep_group = it->second.get();
                if (dep_group->outstanding_dependencies.fetch_sub(1) == 1) {
                    enqueue_tasks_for_group(dep_group);
                }
            }
        }
    }

    if (total_incomplete_groups.fetch_sub(1) == 1) {
        std::unique_lock<std::mutex> lock(sync_mtx);
        sync_cv.notify_all();
    }
}

void TaskSystemParallelThreadPoolSleeping::sync() {
    std::unique_lock<std::mutex> lock(sync_mtx);
    sync_cv.wait(lock, [this]() {
        return total_incomplete_groups.load() == 0;
    });
}
