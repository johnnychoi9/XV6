#ifndef POOL_H_
#include <string>
#include <pthread.h>
#include <algorithm>
#include <list>
#include <vector>
#include <unordered_map>

class Task {
public:
    Task();
    virtual ~Task();

    virtual void Run() = 0;  // implemented by subclass
	
	std::string my_name;
	pthread_cond_t task_cond;
	pthread_mutex_t task_mut, task_fin;
	int done;
};

class ThreadPool {
public:
    ThreadPool(int num_threads);

    // Submit a task with a particular name.
    void SubmitTask(const std::string &name, Task *task);
 
    // Wait for a task by name, if it hasn't been waited for yet. Only returns after the task is completed.
    void WaitForTask(const std::string &name);

    // Stop all threads. All tasks must have been waited for before calling this.
    // You may assume that SubmitTask() is not caled after this is called.
    void Stop();
    

private:
	pthread_mutex_t threadPool_mut;
	pthread_mutex_t task_mut;
    pthread_cond_t threadPool_cond;
    std::list<Task *> buffer;
	std::unordered_map<std::string, Task *> map;
    std::vector<pthread_t> theThreads;
	int exit_status;
	
	static void * worker(void * arg);
    void * runThreads(void);
};

#endif
