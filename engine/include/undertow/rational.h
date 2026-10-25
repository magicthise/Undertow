#pragma once

#include <cstdint>
#include <iosfwd>
#include <stdexcept>
#include <string>

namespace undertow {

// Thrown when exact time arithmetic would not fit in 64-bit integers.
class RationalOverflow : public std::overflow_error {
public:
    using std::overflow_error::overflow_error;
};

// An exact fraction, used for all pattern time. Cycle positions have to be
// exact: floating point drifts, and drift lands events in the wrong cycle.
// Values are always stored in lowest terms with a positive denominator.
class Rational {
public:
    Rational() = default;
    // Implicit so whole numbers read naturally in pattern code: fast(2, pat).
    Rational(int64_t whole) : Rational(whole, 1) {}  // NOLINT(google-explicit-constructor)
    Rational(int64_t numerator, int64_t denominator);

    // Parses "3", "-0.25", ".5" or "3/4". Throws std::invalid_argument.
    static Rational parse(const std::string& text);

    int64_t num() const { return num_; }
    int64_t den() const { return den_; }

    // Rounds towards negative infinity, so floor(-1/2) is -1.
    int64_t floor() const;
    int64_t ceil() const;

    double toDouble() const { return static_cast<double>(num_) / static_cast<double>(den_); }
    // "3" or "3/4".
    std::string toString() const;

    Rational operator-() const;
    Rational& operator+=(const Rational& other);
    Rational& operator-=(const Rational& other);
    Rational& operator*=(const Rational& other);
    Rational& operator/=(const Rational& other);

private:
    int64_t num_ = 0;
    int64_t den_ = 1;
};

Rational operator+(Rational a, const Rational& b);
Rational operator-(Rational a, const Rational& b);
Rational operator*(Rational a, const Rational& b);
Rational operator/(Rational a, const Rational& b);

bool operator==(const Rational& a, const Rational& b);
bool operator!=(const Rational& a, const Rational& b);
bool operator<(const Rational& a, const Rational& b);
bool operator<=(const Rational& a, const Rational& b);
bool operator>(const Rational& a, const Rational& b);
bool operator>=(const Rational& a, const Rational& b);

std::ostream& operator<<(std::ostream& out, const Rational& value);

// Start of the cycle containing t.
inline Rational sam(const Rational& t) { return Rational(t.floor()); }
// Start of the cycle after the one containing t.
inline Rational nextSam(const Rational& t) { return Rational(t.floor() + 1); }
// How far into its cycle t is, from 0 up to (not including) 1.
inline Rational cyclePos(const Rational& t) { return t - sam(t); }

// Integer division and remainder rounding towards negative infinity, so cycle
// arithmetic works the same before zero as after it.
int64_t floorDiv(int64_t a, int64_t b);
int64_t floorMod(int64_t a, int64_t b);

}  // namespace undertow
