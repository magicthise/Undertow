#include "undertow/rational.h"

#include <cctype>
#include <cstdint>
#include <limits>
#include <numeric>
#include <ostream>

namespace undertow {

namespace {

constexpr int64_t kMax = std::numeric_limits<int64_t>::max();
constexpr int64_t kMin = std::numeric_limits<int64_t>::min();

[[noreturn]] void overflow() { throw RationalOverflow("time value too large for exact arithmetic"); }

// Overflow checks happen before the operation, because signed overflow is
// undefined behaviour and can't be detected afterwards.
int64_t checkedMul(int64_t a, int64_t b) {
    if (a > 0) {
        if (b > 0) {
            if (a > kMax / b) overflow();
        } else if (b < kMin / a) {
            overflow();
        }
    } else if (a < 0) {
        if (b > 0) {
            if (a < kMin / b) overflow();
        } else if (b < 0 && a < kMax / b) {
            overflow();
        }
    }
    return a * b;
}

int64_t checkedAdd(int64_t a, int64_t b) {
    if ((b > 0 && a > kMax - b) || (b < 0 && a < kMin - b)) overflow();
    return a + b;
}

int64_t checkedNegate(int64_t a) {
    if (a == kMin) overflow();
    return -a;
}

}  // namespace

Rational::Rational(int64_t numerator, int64_t denominator) {
    if (denominator == 0) throw std::domain_error("rational with zero denominator");
    // Excluding the minimum keeps every value safely negatable.
    if (numerator == kMin || denominator == kMin) overflow();
    if (denominator < 0) {
        numerator = -numerator;
        denominator = -denominator;
    }
    const int64_t divisor = std::gcd(numerator, denominator);
    num_ = numerator / divisor;
    den_ = denominator / divisor;
}

Rational Rational::parse(const std::string& text) {
    auto fail = [&]() -> Rational { throw std::invalid_argument("not a number: '" + text + "'"); };
    size_t i = 0;
    bool negative = false;
    if (i < text.size() && (text[i] == '-' || text[i] == '+')) {
        negative = text[i] == '-';
        ++i;
    }
    auto isDigit = [&](size_t at) { return at < text.size() && std::isdigit(static_cast<unsigned char>(text[at])); };
    // Digits are accumulated with overflow checks; anything past 18 digits is
    // beyond what pattern code needs.
    auto readDigits = [&](int64_t& value, int& count) {
        while (isDigit(i)) {
            value = checkedAdd(checkedMul(value, 10), text[i] - '0');
            ++count;
            ++i;
        }
    };

    int64_t whole = 0;
    int wholeDigits = 0;
    readDigits(whole, wholeDigits);

    Rational result(whole);
    if (i < text.size() && text[i] == '.') {
        ++i;
        int64_t fraction = 0;
        int fractionDigits = 0;
        readDigits(fraction, fractionDigits);
        if (fractionDigits == 0 && wholeDigits == 0) return fail();
        int64_t scale = 1;
        for (int d = 0; d < fractionDigits; ++d) scale = checkedMul(scale, 10);
        result += Rational(fraction, scale);
    } else if (i < text.size() && text[i] == '/') {
        if (wholeDigits == 0) return fail();
        ++i;
        int64_t denominator = 0;
        int denominatorDigits = 0;
        readDigits(denominator, denominatorDigits);
        if (denominatorDigits == 0 || denominator == 0) return fail();
        result = Rational(whole, denominator);
    } else if (wholeDigits == 0) {
        return fail();
    }
    if (i != text.size()) return fail();
    return negative ? -result : result;
}

int64_t Rational::floor() const { return floorDiv(num_, den_); }

int64_t Rational::ceil() const { return -floorDiv(-num_, den_); }

std::string Rational::toString() const {
    if (den_ == 1) return std::to_string(num_);
    return std::to_string(num_) + "/" + std::to_string(den_);
}

Rational Rational::operator-() const { return Rational(checkedNegate(num_), den_); }

Rational& Rational::operator+=(const Rational& other) {
    // Scale both to the least common denominator to keep intermediates small.
    const int64_t divisor = std::gcd(den_, other.den_);
    const int64_t scaleThis = other.den_ / divisor;
    const int64_t scaleOther = den_ / divisor;
    *this = Rational(checkedAdd(checkedMul(num_, scaleThis), checkedMul(other.num_, scaleOther)),
                     checkedMul(den_, scaleThis));
    return *this;
}

Rational& Rational::operator-=(const Rational& other) { return *this += -other; }

Rational& Rational::operator*=(const Rational& other) {
    // Cancel common factors across the two fractions before multiplying.
    const int64_t g1 = std::gcd(num_, other.den_);
    const int64_t g2 = std::gcd(other.num_, den_);
    *this = Rational(checkedMul(num_ / g1, other.num_ / g2), checkedMul(den_ / g2, other.den_ / g1));
    return *this;
}

Rational& Rational::operator/=(const Rational& other) {
    if (other.num_ == 0) throw std::domain_error("division by zero time");
    return *this *= Rational(other.den_, other.num_);
}

Rational operator+(Rational a, const Rational& b) { return a += b; }
Rational operator-(Rational a, const Rational& b) { return a -= b; }
Rational operator*(Rational a, const Rational& b) { return a *= b; }
Rational operator/(Rational a, const Rational& b) { return a /= b; }

bool operator==(const Rational& a, const Rational& b) { return a.num() == b.num() && a.den() == b.den(); }
bool operator!=(const Rational& a, const Rational& b) { return !(a == b); }

bool operator<(const Rational& a, const Rational& b) {
    if (a.den() == b.den()) return a.num() < b.num();
    const int64_t af = a.floor();
    const int64_t bf = b.floor();
    if (af != bf) return af < bf;
    return checkedMul(a.num(), b.den()) < checkedMul(b.num(), a.den());
}

bool operator<=(const Rational& a, const Rational& b) { return !(b < a); }
bool operator>(const Rational& a, const Rational& b) { return b < a; }
bool operator>=(const Rational& a, const Rational& b) { return !(a < b); }

std::ostream& operator<<(std::ostream& out, const Rational& value) { return out << value.toString(); }

int64_t floorDiv(int64_t a, int64_t b) {
    if (b == 0) throw std::domain_error("division by zero");
    int64_t quotient = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) --quotient;
    return quotient;
}

int64_t floorMod(int64_t a, int64_t b) {
    if (b == 0) throw std::domain_error("division by zero");
    int64_t remainder = a % b;
    if (remainder != 0 && ((remainder < 0) != (b < 0))) remainder += b;
    return remainder;
}

}  // namespace undertow
