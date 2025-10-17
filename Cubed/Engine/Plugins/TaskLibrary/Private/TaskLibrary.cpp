#include "pch.h"
#include "Engine/Plugins/TaskLibrary/Public/TaskLibrary.h"

static std::unordered_map<std::string, UTaskLibrary::Queue> Queues;

static void ThreadWorker(const std::string& QueueName) {
    auto it = Queues.find(QueueName);
    if (it == Queues.end()) return;
    
    auto& Queue = it->second;
    
    while (!Queue.bShouldStop) {
        float currentTime = static_cast<float>(
            std::chrono::duration<double>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count()
        ); 
        
        bool taskExecuted = false;
        
        for (auto& Task : Queue.Tasks) {
            if (!Task.bExecuted && currentTime >= Task.ExecutionTime) {
                Task.Callback();
                Task.bExecuted = true;
                taskExecuted = true;
            }
        }
        
        if (!taskExecuted) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

void UTaskLibrary::Initialize(const std::string& QueueName, float CurrentTime, ExecutionMode Mode) {
    auto& Queue = Queues[QueueName];
    if (Queue.bInitialized) return;
    
    Queue.bInitialized = true;
    Queue.StartTime = CurrentTime;
    Queue.Mode = Mode;
    Queue.bShouldStop = false;
    
    if (Mode == ExecutionMode::SeparateThread) {
        Queue.WorkerThread = std::make_unique<std::thread>(ThreadWorker, QueueName);
    }
}

void UTaskLibrary::QueueTask(const std::string& QueueName, float CurrentTime, float DelaySeconds, std::function<void()> Callback) {
    auto& Queue = Queues[QueueName];
    float ExecutionTime = CurrentTime + DelaySeconds;
    Queue.Tasks.push_back({ExecutionTime, Callback, false});
}

void UTaskLibrary::ProcessTasks(const std::string& QueueName, float CurrentTime) {
    auto it = Queues.find(QueueName);
    if (it == Queues.end() || !it->second.bInitialized) return;
    
    auto& Queue = it->second;
    
    if (Queue.Mode != ExecutionMode::MainThread) return;
    
    for (auto& Task : Queue.Tasks) {
        if (!Task.bExecuted && CurrentTime >= Task.ExecutionTime) {
            Task.Callback();
            Task.bExecuted = true;
        }
    }
}

void UTaskLibrary::Clear(const std::string& QueueName) {
    auto it = Queues.find(QueueName);
    if (it != Queues.end()) {
        it->second.Tasks.clear();
    }
}

void UTaskLibrary::Shutdown(const std::string& QueueName) {
    auto it = Queues.find(QueueName);
    if (it == Queues.end()) return;
    
    auto& Queue = it->second;
    Queue.bShouldStop = true;
    
    if (Queue.WorkerThread && Queue.WorkerThread->joinable()) {
        Queue.WorkerThread->join();
    }
    
    Queue.Tasks.clear();
    Queue.bInitialized = false;
    Queue.StartTime = 0.0f;
}

bool UTaskLibrary::IsInitialized(const std::string& QueueName) {
    auto it = Queues.find(QueueName);
    return it != Queues.end() && it->second.bInitialized;
}