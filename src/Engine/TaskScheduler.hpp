#pragma once
#include <mutex>
#include <vector>
#include <functional>
#include <string>
#include <chrono>
#include <algorithm>
#include <deque>

namespace Nova {
    struct Job {
        std::string name;
        std::function<void(double)> callback;
        int priority;      // Lower numbers run first
        double frequency;  // 0 for every frame, or Hz (e.g. 60.0)
        double lastRunTime = 0.0;
    };

    class TaskScheduler {
    public:
        void AddJob(Job job) {
            jobs.push_back(job);
            // Sort by priority so the order is deterministic
            std::sort(jobs.begin(), jobs.end(), [](const Job& a, const Job& b) {
                return a.priority < b.priority;
            });
        }

        void Step() {
            auto now = std::chrono::high_resolution_clock::now();
            double currentTime = std::chrono::duration<double>(now.time_since_epoch()).count();

            for (auto& job : jobs) {
                double deltaTime = currentTime - job.lastRunTime;

                if (job.frequency <= 0 || deltaTime >= (1.0 / job.frequency)) {
                    job.callback(deltaTime);
                    job.lastRunTime = currentTime;
                }
            }
        }

        // Marshalling, allows us to run code that HAS to run on the main thread, on the main thread.
        // Marshalling: Submit a function to be run on the main thread
        void ExecuteOnMainThread(std::function<void()> task) {
            std::lock_guard<std::mutex> lock(mtx);
            mainThreadQueue.push_back(task);
        }

        // Drain the queue (Must be called by the main thread!)
        void ProcessMainThreadTasks() {
            std::deque<std::function<void()>> toProcess;

            // Scope the lock so we don't hold it while executing the tasks
            {
                std::lock_guard<std::mutex> lock(mtx);
                std::swap(toProcess, mainThreadQueue);
            }

            for (auto& task : toProcess) {
                task();
            }
        }

    private:
        std::mutex mtx;
        std::deque<std::function<void()>> mainThreadQueue;
        std::vector<Job> jobs;
    };
}
