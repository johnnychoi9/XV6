#ifndef LIFE_H_
#define LIFE_H_

#include <algorithm>
#include <iosfwd>
#include <vector>

/* I strongly recommend against replacing this with bool, because
 * modifying a vector<bool> from multiple threads may not safe even
 * when modifying a vector<unsigned char> would be.
 *
 * See also http://en.cppreference.com/w/cpp/container/vector_bool
 */
typedef unsigned char LifeCell;

/*
 * Holds the state of a matrix of life cells.
 *
 * Each cell is represented by a LifeCell which is either 0 or 1.
 *
 * To produce the simulation, multiple of these are used since we need
 * the current state to produce the next state.
 */
class LifeBoard {
private:
    int width_;
    int height_;
    std::vector<LifeCell> cells_;

public:
    LifeBoard(int width, int height) : width_(width), height_(height), cells_(width * height) {}

    LifeBoard() : width_(0), height_(0) {}

    LifeBoard(const LifeBoard &other) :
        width_(other.width_), height_(other.height_),
        cells_(other.cells_) {}
                        

    LifeCell& at(int x, int y) {
        return cells_[y * width_ + x];
    }

    LifeCell const& at(int x, int y) const {
        return cells_[y * width_ + x];
    }
    
    int width() const  { return width_; }
    int height() const { return height_; }

    bool operator==(const LifeBoard &other) {
        return width_ == other.width_ && cells_ == other.cells_;
    }

    bool operator!=(const LifeBoard &other) {
        return !operator==(other);
    }

    friend void swap(LifeBoard&, LifeBoard&);
    friend std::ostream& operator<<(std::ostream&, LifeBoard const&);
    friend std::istream& operator>>(std::istream&, LifeBoard&);
};
    
extern std::ostream& operator<<(std::ostream&, LifeBoard const&);
extern std::istream& operator>>(std::istream&, LifeBoard&);

inline void swap(LifeBoard &first, LifeBoard &second) {
    using std::swap;
    swap(first.cells_, second.cells_);
    swap(first.width_, second.width_);
    swap(first.height_, second.height_);
}

/*
 * Simulates the specified number of steps by the Game of Life rules,
 * updating the LifeBoard state to have the new state.
 *
 * Two versions: a serial version and a parallel version you must implement.
 */ 
extern void simulate_life_serial(LifeBoard &state, int steps);
extern void simulate_life_parallel(int threads, LifeBoard &state, int steps);

#endif
