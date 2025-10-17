#pragma once
#include "pch.h"

#include <vector>
#include <functional>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <chrono>

namespace UTaskLibrary
{
    enum class ExecutionMode
    {
        MainThread,
        SeparateThread
    };

    struct Task
    {
        float ExecutionTime;
        std::function<void()> Callback;
        bool bExecuted = false;
    };

    struct Queue
    {
        std::vector<Task> Tasks;
        float StartTime = 0.0f;
        bool bInitialized = false;
        ExecutionMode Mode = ExecutionMode::MainThread;
        
        std::unique_ptr<std::thread> WorkerThread;
        std::condition_variable TaskCV;
        std::atomic<bool> bShouldStop{false};
    };

    void Initialize(const std::string& QueueName, float CurrentTime, ExecutionMode Mode = ExecutionMode::MainThread);
    void QueueTask(const std::string& QueueName, float CurrentTime, float DelaySeconds, std::function<void()> Callback);
    void ProcessTasks(const std::string& QueueName, float CurrentTime);
    void Clear(const std::string& QueueName);
    bool IsInitialized(const std::string& QueueName);
    void Shutdown(const std::string& QueueName);
}