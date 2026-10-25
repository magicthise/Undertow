#include "undertow/pattern.h"

namespace undertow {

std::vector<bool> bjorklund(int64_t pulses, int64_t steps) {
    // Start with one group per hit and one per gap, then repeatedly append the
    // trailing groups onto the leading ones until at most one group is left
    // over (Toussaint, "The Euclidean Algorithm Generates Traditional Musical
    // Rhythms", 2005).
    using Group = std::vector<bool>;
    std::vector<Group> leading(static_cast<size_t>(std::max<int64_t>(pulses, 0)), Group{true});
    std::vector<Group> trailing(static_cast<size_t>(std::max<int64_t>(steps - pulses, 0)), Group{false});

    while (std::min(leading.size(), trailing.size()) > 1) {
        const size_t paired = std::min(leading.size(), trailing.size());
        std::vector<Group> merged(paired);
        for (size_t i = 0; i < paired; ++i) {
            merged[i] = leading[i];
            merged[i].insert(merged[i].end(), trailing[i].begin(), trailing[i].end());
        }
        // Whichever list had groups left over becomes the new trailing list.
        auto& longer = leading.size() > paired ? leading : trailing;
        std::vector<Group> remainder(longer.begin() + static_cast<std::ptrdiff_t>(paired), longer.end());
        leading = std::move(merged);
        trailing = std::move(remainder);
    }

    std::vector<bool> rhythm;
    for (const auto& group : leading) rhythm.insert(rhythm.end(), group.begin(), group.end());
    for (const auto& group : trailing) rhythm.insert(rhythm.end(), group.begin(), group.end());
    return rhythm;
}

}  // namespace undertow
