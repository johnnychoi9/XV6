#include <stdint.h>
#include <functional>

typedef uint64_t cycles_type;

extern cycles_type measure_function(std::function<void()> function);
