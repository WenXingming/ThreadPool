#include "Fnv1aHash.h"

#include <gtest/gtest.h>

#include <string>

TEST(Fnv1aHashTest, SplitUpdatesMatchSingleUpdate) {
    const std::string text = "abcdefghijklmnopqrstuvwxyz0123456789";

    Fnv1aHash oneShot;
    oneShot.update(text.data(), text.size());

    Fnv1aHash split;
    split.update(text.data(), 7);
    split.update(text.data() + 7, 11);
    split.update(text.data() + 18, text.size() - 18);

    EXPECT_EQ(oneShot.value(), split.value());
}

TEST(Fnv1aHashTest, DifferentInputsProduceDifferentHashes) {
    Fnv1aHash first;
    first.update("alpha", 5);

    Fnv1aHash second;
    second.update("bravo", 5);

    EXPECT_NE(first.value(), second.value());
}
