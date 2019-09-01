#include <iostream>
#include <limits>

#include "life.h"

std::ostream& operator<<(std::ostream& out, LifeBoard const& state) {
    out << state.width() << " " << state.height() << "\n";
    for (int y = 0; y < state.height(); ++y) {
        for (int x = 0; x < state.width(); ++x) {
            if (state.at(x, y)) {
                out << '*';
            } else {
                out << '.';
            }
        }
        out << "\n";
    }
    return out;
}

std::istream& operator>>(std::istream& in, LifeBoard& state) {
    in >> state.width_ >> state.height_;
    in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    state.cells_.resize(state.width_ * state.height_);
    for (int y = 0; y < state.height(); ++y) {
        for (int x = 0; x < state.width(); ++x) {
            char c = in.get();
            state.at(x, y) = !(c == ' ' || c == '.');
        }
        in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
    return in;
}
