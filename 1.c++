#include <iostream>
#include <queue>
#include <vector>
#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <map>
#include <utility>

using namespace std;

class Task {
public:
    int id, priority, burstTime, arrivalTime, remainingTime;
    string name;
    void (*function)();

    Task(int i, int p, int b, int a, string n, void (*f)())
        : id(i), priority(p), burstTime(b), arrivalTime(a),
          remainingTime(b), name(n), function(f) {}

    bool operator>(const Task& other) const { return priority > other.priority; }
};

class Scheduler {
public:
    priority_queue<Task, vector<Task>, greater<Task>> taskQueue;
    vector<Task> finishedTasks;
    mutex queueMutex;
    condition_variable cv;
    bool running = true;

    void scheduleTask(Task task) {
        lock_guard<mutex> lock(queueMutex);
        taskQueue.push(task);
        cout << "Task " << task.name << " scheduled (ID: " << task.id << ", Priority: " << task.priority << ")\n";
        cv.notify_one();
    }

    Task getNextTask() {
        unique_lock<mutex> lock(queueMutex);
        cv.wait(lock, [this] { return !taskQueue.empty() || !running; });
        if (taskQueue.empty()) return Task{-1, 0, 0, 0, "NoTask", nullptr};

        Task next = taskQueue.top();
        taskQueue.pop();
        return next;
    }

    void executeTask(Task& task) {
        cout << "Executing " << task.name << " (ID: " << task.id << ")\n";
        if (task.function) task.function();
        this_thread::sleep_for(chrono::milliseconds(task.remainingTime));
        cout << "Task " << task.name << " finished\n";
        finishedTasks.push_back(task);
    }

    void run() {
        while (running) {
            Task task = getNextTask();
            if (task.id != -1)
                executeTask(task);
        }
        cout << "Scheduler stopped\n";
    }

    void stop() {
        lock_guard<mutex> lock(queueMutex);
        running = false;
        cv.notify_one();
    }
};

// Power Management Module
class PowerManager {
public:
    enum class SleepMode { ACTIVE, IDLE, SLEEP, DEEPSLEEP };
    SleepMode currentMode = SleepMode::ACTIVE;
    int cpuFrequency = 1000;
    mutex modeMutex;

    string getSleepModeString(SleepMode mode) {
        switch (mode) {
            case SleepMode::ACTIVE: return "ACTIVE";
            case SleepMode::IDLE: return "IDLE";
            case SleepMode::SLEEP: return "SLEEP";
            case SleepMode::DEEPSLEEP: return "DEEPSLEEP";
            default: return "UNKNOWN";
        }
    }

    void setCPUFrequency(int frequency) {
        lock_guard<mutex> lock(modeMutex);
        cpuFrequency = frequency;
        cout << "CPU Frequency: " << frequency << " MHz\n";
    }

    void enterSleepMode(SleepMode mode) {
        lock_guard<mutex> lock(modeMutex);
        currentMode = mode;
        cout << "Entered " << getSleepModeString(mode) << " mode\n";
        if (mode != SleepMode::ACTIVE)
            this_thread::sleep_for(chrono::milliseconds(100));
    }
};

struct Data {
    vector<char> content;
    size_t size;
    string type;

    Data(const string& d, const string& t)
        : content(d.begin(), d.end()), size(d.size()), type(t) {}
};

class DataSyncManager {
public:
    enum class Protocol { MQTT, COAP, CUSTOM };
    Protocol currentProtocol = Protocol::MQTT;

    mutex protocolMutex, bufferMutex;
    condition_variable bufferCV;
    queue<Data> dataBuffer;
    bool connected = true;

    void setProtocol(Protocol protocol) {
        lock_guard<mutex> lock(protocolMutex);
        currentProtocol = protocol;
        cout << "Protocol: " << (protocol == Protocol::MQTT ? "MQTT" :
                                  protocol == Protocol::COAP ? "CoAP" : "CUSTOM") << "\n";
    }

    void bufferData(Data data) {
        lock_guard<mutex> lock(bufferMutex);
        dataBuffer.push(data);
        cout << "Buffered data (Size: " << data.size << ")\n";
        bufferCV.notify_one();
    }

    void sendData() {
        while (true) {
            unique_lock<mutex> lock(bufferMutex);
            bufferCV.wait(lock, [this] { return !dataBuffer.empty() || !connected; });

            if (!connected && dataBuffer.empty()) break;

            if (!dataBuffer.empty()) {
                Data data = move(dataBuffer.front());
                dataBuffer.pop();
                lock.unlock();

                cout << "Sending " << data.type << " data via "
                     << (currentProtocol == Protocol::MQTT ? "MQTT" :
                         currentProtocol == Protocol::COAP ? "CoAP" : "CUSTOM") << "\n";

                this_thread::sleep_for(chrono::milliseconds(50));
            }
        }
        cout << "Data sender stopped.\n";
    }

    void setConnectionStatus(bool status) {
        lock_guard<mutex> lock(protocolMutex);
        connected = status;
        cout << "Connection " << (status ? "established" : "lost") << "\n";
        bufferCV.notify_one();
    }
};

int main() {
    Scheduler scheduler;
    thread schedulerThread(&Scheduler::run, &scheduler);

    PowerManager powerManager;
    DataSyncManager dataSyncManager;
    thread dataSenderThread(&DataSyncManager::sendData, &dataSyncManager);

    scheduler.scheduleTask(Task{1, 1, 50, 0, "Task1", [] { cout << "Executing Task1\n"; }});
    scheduler.scheduleTask(Task{2, 3, 100, 10, "Task2", [] { cout << "Executing Task2\n"; }});
    scheduler.scheduleTask(Task{3, 2, 80, 20, "Task3", [] { cout << "Executing Task3\n"; }});

    this_thread::sleep_for(chrono::milliseconds(200));

    powerManager.setCPUFrequency(500);
    powerManager.enterSleepMode(PowerManager::SleepMode::IDLE);
    this_thread::sleep_for(chrono::milliseconds(100));
    powerManager.enterSleepMode(PowerManager::SleepMode::ACTIVE);
    powerManager.setCPUFrequency(1000);

    dataSyncManager.bufferData(Data{"Sensor Data 1", "Sensor"});
    dataSyncManager.setConnectionStatus(false);
    dataSyncManager.bufferData(Data{"Sensor Data 2", "Sensor"});
    this_thread::sleep_for(chrono::milliseconds(50));
    dataSyncManager.setConnectionStatus(true);
    this_thread::sleep_for(chrono::milliseconds(100));

    scheduler.stop();
    dataSyncManager.setConnectionStatus(false);

    schedulerThread.join();
    dataSenderThread.join();

    cout << "Simulation complete." << endl;
    return 0;
}


