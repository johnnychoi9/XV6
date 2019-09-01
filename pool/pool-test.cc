#include "pool.h"

#include <pthread.h>
#include <atomic>
#include <set>
#include <iostream>
#include <sstream>
#include <vector>

namespace {

struct BarrierTask : Task {
    pthread_barrier_t *barrier_;
    std::atomic<int> *counter_;

    BarrierTask(pthread_barrier_t *barrier, std::atomic<int> *counter)
        : barrier_(barrier), counter_(counter) {};

    void Run() override {
        pthread_barrier_wait(barrier_);
        if (counter_) counter_->fetch_add(1);
    }
};

struct RecordIncrementTask : Task {
    std::atomic<int> *counter_;
    std::atomic<int> *destroy_counter_;
    int *record_value_;

    RecordIncrementTask(std::atomic<int> *counter, int *record_value) :
        counter_(counter), destroy_counter_(0), record_value_(record_value) {}
    RecordIncrementTask(std::atomic<int> *counter, std::atomic<int>* destroy_counter, int *record_value) :
        counter_(counter), destroy_counter_(destroy_counter), record_value_(record_value) {}

    void Run() override {
        *record_value_ = counter_->fetch_add(1);
    }

    ~RecordIncrementTask() {
        if (destroy_counter_) {
            destroy_counter_->fetch_add(1);
        }
    }
};

struct WaitForTaskTask : Task {
    ThreadPool *pool_;
    std::string to_wait_for_;
    bool *done_ptr_;

    WaitForTaskTask(ThreadPool *pool, const std::string &to_wait_for, bool* done_ptr) :
        pool_(pool), to_wait_for_(to_wait_for), done_ptr_(done_ptr) {}

    void Run() override {
        pool_->WaitForTask(to_wait_for_);
        *done_ptr_ = true;
    }
};

struct SubmitTaskTask : Task {
    ThreadPool *pool_;
    std::string new_task_name_;
    Task *new_task_;

    SubmitTaskTask(ThreadPool *pool, const std::string &new_task_name, Task* new_task) :
        pool_(pool), new_task_name_(new_task_name), new_task_(new_task) {}

    void Run() override {
        pool_->SubmitTask(new_task_name_, new_task_);
        new_task_ = 0;
    }
};

struct EmptyTask : Task {
    void Run() override {}
};

struct CheckDestructionTask : Task {
    bool *destroyed_ptr_;

    CheckDestructionTask(bool *destroyed_ptr) : destroyed_ptr_(destroyed_ptr) {}

    void Run() override {}

    ~CheckDestructionTask() {
        *destroyed_ptr_ = true;
    }
};

std::string current_test{"<none>"};
bool DEBUG = false;
int passed_subtests = 0;
int failed_subtests = 0;
int passed_tests = 0;
int failed_tests = 0;

std::vector<std::string> failed_test_groups;

void START_TEST(std::string name) {
    if (current_test != "<none>") {
        std::cerr << "test mismatch for " << name << " versus " << current_test << std::endl;
        abort();
    }
    current_test = name;
    std::cout << "STARTING test group: " << name << std::endl;
}

void END_TEST(std::string name) {
    if (current_test != name) {
        std::cerr << "test mismatch for " << name << " versus " << current_test << std::endl;
        abort();
    }
    current_test = "<none>";
    if (failed_subtests > 0) {
        ++failed_tests;
        std::cout << "FAILED test group: " << name << std::endl;
        failed_test_groups.push_back(name);
    } else {
        ++passed_tests;
        std::cout << "PASSED test group: " << name << std::endl;
    }
}

void CHECK(std::string description, bool value) {
    if (value) {
        std::cout << "PASSED: " << current_test << ": " << description << std::endl;
        ++passed_subtests;
    } else {
        std::cout << "FAILED: " << current_test << ": " << description << std::endl;
        ++failed_subtests;
    }
}

void CHECK_QUIET(std::string description, bool value) {
    if (value) {
        ++passed_subtests;
    } else {
        std::cout << "FAILED: " << current_test << ": " << description << std::endl;
        ++failed_subtests;
    }
}

std::string N(int i) {
    std::stringstream ss;
    ss << i;
    return ss.str();
}

std::string T(std::string label, int i) {
    std::stringstream ss;
    ss << label << "#" << i;
    return ss.str();
}

void submit_barrier_set(ThreadPool *pool, int thread_count, pthread_barrier_t *barrier, std::atomic<int> *check_count) {
    for (int i = 0; i < thread_count; ++i) {
        pool->SubmitTask(T("barrier", i), new BarrierTask(barrier, check_count));
    }
}

void cleanup_barrier_set(ThreadPool *pool, int thread_count, std::atomic<int> *check_count) {
    for (int i = 0; i < thread_count; ++i) {
        pool->WaitForTask(T("barrier", i));
    }
    CHECK("tasks waiting on barrier (used to make sure some tasks are submitted after other tasks run in parallel) run correct number of times", check_count->load() == thread_count);
}

void test_run_in_order(int thread_count, int count, bool submit_first, bool wait_reverse) {
    std::atomic<int> barrier_check_count{0};
    std::string description;
    {
        std::stringstream ss;
        ss << "counter task ordering with " << thread_count << " threads and " << count << " tasks";
        if (submit_first) {
             ss << ", making sure counter tasks are submitted while other tasks are running using a barrier";
        }
        if (wait_reverse) {
            ss << ", and waiting for the counter tasks in reverse order";
        }
        description = ss.str();
    }
    START_TEST(description);
    ThreadPool pool{thread_count};
    pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, NULL, thread_count+1);
    if (submit_first) {
        submit_barrier_set(&pool, thread_count, &barrier, &barrier_check_count);
    }
    std::atomic<int> counter{0};
    std::atomic<int> destroy_counter{0};
    std::vector<int> task_result(count, -1);
    for (int i = 0; i < count; ++i) {
        if (DEBUG) std::cout << "submitting  " << T("inc",i) << std::endl;
        pool.SubmitTask(T("inc", i), new RecordIncrementTask(&counter, &destroy_counter, &task_result[i]));
    }
    if (submit_first) {
        pthread_barrier_wait(&barrier);
    }
    if (wait_reverse) {
        for (int i = count - 1; i >= 0; --i) {
            pool.WaitForTask(T("inc", i));
        }
    } else {
        for (int i = 0; i < count; ++i) {
            pool.WaitForTask(T("inc", i));
        }
    }
    if (submit_first) {
        cleanup_barrier_set(&pool, thread_count, &barrier_check_count);
    }
    pool.Stop();
    CHECK("correct number of counter tasks run", counter.load() == count);
    CHECK("correct number of counter tasks deleted", destroy_counter.load() == count);
    for (int i = 0; i < count; ++i) {
        if (DEBUG) std::cout << "result[" << i << "] == " << task_result[i] << std::endl;
    }
    if (thread_count == 1) {
        for (int i = 0; i < count; ++i) {
            CHECK("task " + T("inc",i) + " assigned index " + T("",i), task_result[i] == i);
        }
    }
    std::set<int> first_wave_results(
        task_result.begin(),
        task_result.begin() + std::min(thread_count, count)
    );
    CHECK("one of first " + N(std::min(thread_count, count)) + " results is 0", first_wave_results.count(0) == 1);
    std::set<int> seen_values(task_result.begin(), task_result.end());
    for (int i = 0; i < count; ++i) {
        CHECK_QUIET("one task had index " + T("", i), seen_values.count(i) == 1);
    }
    END_TEST(description);
}

void test_wait_for_from_future_tasks(int thread_count) {
    std::atomic<int> barrier_check_count{0};
    const std::string description =
        "waiting for " + N(thread_count-1) +
        " tasks from tasks submitted later with " +
        N(thread_count) + " threads";
    START_TEST(description);
    ThreadPool pool{thread_count};
    pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, NULL, thread_count+1);
    submit_barrier_set(&pool, thread_count, &barrier, &barrier_check_count);
    bool *wait_results = new bool[thread_count - 1];
    bool *destroy_results = new bool[thread_count - 1];
    for (int i = 0; i < thread_count - 1; ++i) {
        wait_results[i] = false;
        pool.SubmitTask(T("wait", i), new WaitForTaskTask(&pool, T("empty", i), &wait_results[i]));
    }
    for (int i = 0; i < thread_count - 1; ++i) {
        pool.SubmitTask(T("empty", i), new CheckDestructionTask(&destroy_results[i]));
    }
    pthread_barrier_wait(&barrier);
    for (int i = 0; i < thread_count - 1; ++i) {
        pool.WaitForTask(T("wait", i));
    }
    cleanup_barrier_set(&pool, thread_count, &barrier_check_count);
    pool.Stop();
    for (int i = 0; i < thread_count - 1; ++i) {
        CHECK("successfully waited for task " + T("empty", i), wait_results[i]);
        CHECK("task " + T("empty", i) + " was deleted", destroy_results[i]);
    }
    delete[] wait_results;
    delete[] destroy_results;
    CHECK("tasks waiting on barrier run correct number of times",
          barrier_check_count.load() == thread_count);
    END_TEST(description);
}

void test_submit_many(int thread_count, int task_count) {
    const std::string description =
        "submitting " + N(task_count) +
        " tasks from " +
        N(thread_count) + " threads";
    START_TEST(description);
    std::atomic<int> barrier_check_count{0};
    ThreadPool pool{thread_count};
    pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, NULL, thread_count+1);
    submit_barrier_set(&pool, thread_count, &barrier, &barrier_check_count);
    std::vector<int> task_results;
    std::atomic<int> counter{0};
    task_results.resize(task_count);
    for (int i = 0; i < task_count; ++i) {
        pool.SubmitTask(
            T("submit", i),
            new SubmitTaskTask(&pool, T("incr", i), new RecordIncrementTask(&counter, &task_results[i]))
        );
    }
    pthread_barrier_wait(&barrier);
    for (int i = task_count - 1; i >= 0; --i) {
        pool.WaitForTask(T("submit", i));
        pool.WaitForTask(T("incr", i));
    }
    cleanup_barrier_set(&pool, thread_count, &barrier_check_count);
    CHECK("correct number of counter tasks (submitted from other tasks) run", counter.load() == task_count);
    if (counter.load() == task_count) {
        std::set<int> seen_values(task_results.begin(), task_results.end());
        for (int i = 0; i < task_count; ++i) {
            CHECK_QUIET("one task had index " + T("", i), seen_values.count(i) == 1);
        }
    }
    END_TEST(description);
    pool.Stop();
}

// FIXME: test Task deletion

}  // end unnamed namespace

int main(void) {
    test_run_in_order(1, 10, false, false);
    test_run_in_order(1, 10, false, true);
    test_run_in_order(1, 10, true, false);
    test_run_in_order(1, 10, true, true);
    test_run_in_order(10, 5, true, true);
    test_run_in_order(2, 10, false, false);
    test_run_in_order(2, 10, false, true);
    test_run_in_order(2, 10, true, false);
    test_run_in_order(2, 10, true, true);
    test_run_in_order(4, 40, true, true);

    test_wait_for_from_future_tasks(2);
    test_wait_for_from_future_tasks(3);

    test_submit_many(2, 10);
    test_submit_many(2, 10000);
    test_submit_many(4, 10000);

    std::cout << "--RESULT SUMMARY--\npassed " << passed_tests
              << " test groups and failed " << failed_tests << " test groups\n";
    if (failed_tests > 0) {
        std::cout << "Failed test groups:\n";
        for (auto group : failed_test_groups) {
            std::cout << "  " << group << "\n";
        }
    }
    return 0;
}
