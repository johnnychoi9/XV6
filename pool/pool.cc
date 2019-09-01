#include "pool.h"
#include <unistd.h>
#include <iostream>
Task::Task() {
	pthread_cond_init(&task_cond, 0);
	done = 0;
	my_name = "";
}

Task::~Task() {
}

//are threads dynamically allocated?
//I have lock 
ThreadPool::ThreadPool(int num_threads) {
    pthread_mutex_init(&this->threadPool_mut, 0);
	pthread_cond_init(&this->threadPool_cond, 0);
	//pthread_mutex_init(&task_mut, 0);
	this->exit_status = 0;
    this->theThreads.resize(num_threads);
    for(int i = 0; i < num_threads; i++) {
        pthread_create(&this->theThreads[i], NULL, ThreadPool::worker, static_cast<void *>(this));
    }
}

void ThreadPool::SubmitTask(const std::string &name, Task* task) {
    pthread_mutex_lock(&this->threadPool_mut);
	pthread_mutex_init(&task->task_mut, 0);
	pthread_mutex_init(&task->task_fin, 0);
	task->my_name = name;
	this->map[name] = task;
    this->buffer.push_back(task);
    pthread_cond_signal(&this->threadPool_cond);
    pthread_mutex_unlock(&this->threadPool_mut);
}
// If the task is already completed, delete it
//
void ThreadPool::WaitForTask(const std::string &name) {
	Task * t = this->map[name];
	pthread_mutex_lock(&t->task_fin);
	while(t->done == 0){
//		std::cout<<"Waiting for task" <<name<<std::endl;
		pthread_cond_wait(&t->task_cond, &t->task_mut);
	}
	pthread_mutex_unlock(&t->task_fin);
	this->map.erase(name);
	delete t;
}
void * ThreadPool::worker(void * arg){
	return static_cast<ThreadPool*>(arg)->runThreads();
}

void * ThreadPool::runThreads(void) {
    while(!this->exit_status) {
		

			pthread_mutex_lock(&this->threadPool_mut);
			while(buffer.empty()){
				// this should be in a while loop # austin
				pthread_cond_wait(&this->threadPool_cond, &this->threadPool_mut);
				if(this->exit_status){
					pthread_mutex_unlock(&this->threadPool_mut);
			return 0;
			}
			}
			Task * t = this->buffer.front();
			this->buffer.pop_front();
			pthread_mutex_unlock(&this->threadPool_mut);
			// would not recommend deleting the task immediately
			t->Run();
			t->done = 1;
	//	std::cout << "wait " << t->my_name << std::endl;
		pthread_mutex_lock(&t->task_mut);
	//std::cout << "got " << t->my_name << std::endl;
		pthread_cond_signal(&(t->task_cond));
		pthread_mutex_unlock(&t->task_mut);
    }
    return 0;
}

void ThreadPool::Stop() {
	this->exit_status = 1;
//	std::cout<<"Stopping"<<std::endl;
    pthread_mutex_lock(&this->threadPool_mut);
//	std::cout<<"acquired" << std::endl;
	   
	pthread_cond_broadcast(&this->threadPool_cond);

    pthread_mutex_unlock(&this->threadPool_mut);
//	std::cout<<"unlocked" <<std::endl;
    for(unsigned int i = 0; i < this->theThreads.size(); i++) {
//		std::cout << "waiting for " <<i <<std::endl;
	pthread_join(this->theThreads[i], 0);
//		std::cout << "exited for " <<i <<std::endl;
    }
	//std::cout<<"stp[[ed"<<std::endl;
	// this might be a memory leak, have to iterate through and delete
    this->theThreads.clear();
}
