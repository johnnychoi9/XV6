#include <iostream>
#include <fstream>
#include <string>
#include <functional>  /* for std::function, std::bind, std::placeholders::_1, etc. */
#include "life.h"
#include "timing.h"

struct TestFunction {
    std::function<void (LifeBoard&, int)> simulate_function;
    std::string name;
};

/* _1, _2 are used below with std::bind, and their full names are
 * std::placeholders::_1 and std::placeholders::_2
 */
using namespace std::placeholders;

/*
 * This is a list of versions which are tested.
 *
 * std::bind(&foo, X, _1, _2)  creates a function bar(y, z) which calls foo(X, y, z),
 * so std::bind(&simulate_life_parallel_posix_barrier, 2, _1, _2) is roughly equivalent to
 * writing 'temp_function' and defining temp_function like:
 *
 *     void temp_function(LifeBoard &board, int iterations) {
 *          simulate_life_parallel_posix_barrier(2, board, iterations);
 *     }
 */
TestFunction test_functions[] = {
    { simulate_life_serial, "serial" },
	{ std::bind(&simulate_life_parallel, 1, _1, _2), "1 thread" },
    { std::bind(&simulate_life_parallel, 2, _1, _2), "2 threads" },
    { std::bind(&simulate_life_parallel, 4, _1, _2), "4 threads" },
    { std::bind(&simulate_life_parallel, 8, _1, _2), "8 threads" },
};

/* Given a number return a string using T, M, K to describe in a more readable way */
std::string short_cycles(uint64_t cycles) {
    const uint64_t kilo = 1000;
    const uint64_t mega = 1000 * kilo;
    const uint64_t giga = 1000 * mega;
    const uint64_t tera = 1000 * giga;
    if (cycles > tera * 10) {
        return std::to_string(cycles / tera) + " T";
    } else if (cycles > mega * 10) {
        return std::to_string(cycles / mega) + " M";
    } else if (cycles > kilo * 10) {
        return std::to_string(cycles / kilo) + " K";
    } else {
        return std::to_string(cycles);
    }
}


/* Global variable set when run_and_verify encounters an error. This allows
   us to detect errors when it's called from measure_function (in timing.cc)
 */
static bool any_errors = false, show_result_on_error = false;

void run_and_verify(LifeBoard const& start, LifeBoard const& expected, int iterations,
                    const TestFunction &function) {
    LifeBoard board{start};
    function.simulate_function(board, iterations);
    if (board != expected) {
        std::cerr << "ERROR: " << function.name << " result disagreed with expected result\n";
        if (!any_errors && show_result_on_error) {
            std::cout << "INCORRECT results from " << function.name << ": " << std::endl;
            std::cout << board;
        }
        any_errors = true;
    }
}

int main(int argc, char **argv) {
    bool show_result = false;
    bool do_timings = false;
    int iterations = 0;
    std::string input_file;
    if (argc == 4) {
        iterations = std::stoi(argv[1]);
        input_file = argv[2];
        std::string mode = argv[3];
        if (mode == "serial-result") {
            show_result = true;
        } else if (mode == "time") {
            do_timings = true;
        } else if (mode == "time-and-result") {
            show_result = true;
            show_result_on_error = true;
            do_timings = true;
        }
    }
    if (!show_result && !do_timings) {
        std::cout << "Usage:\n"
                  << "  " << argv[0] << " ITERATIONS FILENAME serial-result\n"
                  << "    Run serial computation and print out result\n"
                  << "  " << argv[0] << " ITERATIONS FILENAME time\n"
                  << "    Run all versions and compare their timings\n"
                  << "  " << argv[0] << " ITERATIONS FILENAME time-and-result\n"
                  << "    Print out result and compare timings of all versions\n"
                  << "\n"
                  << "Example: "
                  << "   " << argv[0] << " 10 ./input/make-a serial-result\n"
                  << "   [run with 10 replaced by 1, 2, 3, 4, ... to see animated example]\n"
                  << std::endl;
       return 0;
    }
    LifeBoard original_board;
    {
        std::ifstream in(input_file);
        if (!in) {
            std::cerr << "ERROR: could not open input file " << input_file << "\n";
            return 1;
        }
        in >> original_board;
        if (!in || original_board.width() == 0 || original_board.height() == 0) {
            std::cerr << "ERROR: no valid initial board found in " << input_file << "\n";
            return 1;
        }
    }
    LifeBoard board{original_board};
    simulate_life_serial(board, iterations);
    if (show_result) {
        std::cout << board;
    }
    bool should_exit_failure = false;
    if (do_timings) {
        std::cout << "---TIMING---\n";
        if (sizeof(test_functions) / sizeof(test_functions[0]) == 1) {
            std::cerr << "ERROR: modify main.cc to time more functions\n";
        }
        for (TestFunction test_function : test_functions) {
            any_errors = false;
            /* run once without timing to see if it works */
            run_and_verify(original_board, board, iterations, test_function);
            if (!any_errors) {
                /* if it does, then time it repeatedly to get a stable measurement */
                uint64_t cycles = measure_function(std::bind(&run_and_verify, original_board, board, iterations, test_function));
                if (any_errors) {
                    should_exit_failure = true;
                    std::cout << "[BROKEN] " << test_function.name << ": " << cycles << "? cycles (" << short_cycles(cycles) << "?)" << std::endl;
                } else {
                    std::cout << test_function.name << ": " << cycles << " cycles (" << short_cycles(cycles) << ")" << std::endl;
                }
            } else {
                should_exit_failure = true;
                std::cout << "[BROKEN] " << test_function.name << ": [not timed]" << std::endl;
            }
        }
    }
    return should_exit_failure ? 1 : 0;
}
