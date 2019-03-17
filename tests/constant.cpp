#include <scat/constant.hpp>
#include <catch2/catch.hpp>

#define SIGNED int8_t, int16_t, int32_t, int64_t
#define UNSIGNED uint8_t, uint16_t, uint32_t, uint64_t

TEMPLATE_TEST_CASE("is_zero", "", SIGNED, UNSIGNED){
    constexpr auto f = scat::constant::is_zero<TestType>;

    TestType FALSE = 0;
    TestType TRUE = -1;

    REQUIRE(f(10) == FALSE);
    REQUIRE(f(-10) == FALSE);
    REQUIRE(f(0) == TRUE);
}

TEMPLATE_TEST_CASE("is_notzero", "", SIGNED, UNSIGNED){
    constexpr auto f = scat::constant::is_notzero<TestType>;

    TestType FALSE = 0;
    TestType TRUE = -1;

    REQUIRE(f(10) == TRUE);
    REQUIRE(f(-10) == TRUE);
    REQUIRE(f(0) == FALSE);
}

TEMPLATE_TEST_CASE("is_lessthan (signed)", "", SIGNED){
    constexpr auto f = scat::constant::is_lessthan<TestType>;

    TestType FALSE = 0;
    TestType TRUE = -1;

    REQUIRE(f(10, 10) == FALSE);
    REQUIRE(f(-10, -10) == FALSE);

    REQUIRE(f(10, 5) == FALSE);
    REQUIRE(f(-5, -10) == FALSE);
    REQUIRE(f(10, -5) == FALSE);
    REQUIRE(f(10, -20) == FALSE);

    REQUIRE(f(5, 10) == TRUE);
    REQUIRE(f(-10, -5) == TRUE);
    REQUIRE(f(-5, 10) == TRUE);
    REQUIRE(f(-20, 10) == TRUE);
}

TEMPLATE_TEST_CASE("is_lessthan (unsigned)", "", UNSIGNED){
    constexpr auto f = scat::constant::is_lessthan<TestType>;

    TestType FALSE = 0;
    TestType TRUE = -1;

    REQUIRE(f(5, 7) == TRUE);  // LT
    REQUIRE(f(7, 5) == FALSE); // GT
    REQUIRE(f(5, 5) == FALSE); // EQ
}

TEMPLATE_TEST_CASE("is_greaterthan", "", SIGNED, UNSIGNED){
    constexpr auto f = scat::constant::is_greaterthan<TestType>;

    TestType FALSE = 0;
    TestType TRUE = -1;

    REQUIRE(f(5, 7) == FALSE); // LT
    REQUIRE(f(7, 5) == TRUE);  // GT
    REQUIRE(f(5, 5) == FALSE); // EQ
}

TEMPLATE_TEST_CASE("is_lessthanequal", "", SIGNED, UNSIGNED){
    constexpr auto f = scat::constant::is_lessthanequal<TestType>;

    TestType FALSE = 0;
    TestType TRUE = -1;

    REQUIRE(f(5, 7) == TRUE);  // LT
    REQUIRE(f(7, 5) == FALSE); // GT
    REQUIRE(f(5, 5) == TRUE);  // EQ
}

TEMPLATE_TEST_CASE("is_greaterthanequal", "", SIGNED, UNSIGNED){
    constexpr auto f = scat::constant::is_greaterthanequal<TestType>;

    TestType FALSE = 0;
    TestType TRUE = -1;

    REQUIRE(f(5, 7) == FALSE);  // LT
    REQUIRE(f(7, 5) == TRUE);  // GT
    REQUIRE(f(5, 5) == TRUE);  // EQ
}

TEMPLATE_TEST_CASE("is_equal", "", SIGNED, UNSIGNED){
    constexpr auto f = scat::constant::is_equal<TestType>;

    TestType FALSE = 0;
    TestType TRUE = -1;

    REQUIRE(f(5, 5) == TRUE);
    REQUIRE(f(5, 7) == FALSE);
}

TEMPLATE_TEST_CASE("is_notequal", "", SIGNED, UNSIGNED){
    constexpr auto f = scat::constant::is_notequal<TestType>;

    TestType FALSE = 0;
    TestType TRUE = -1;

    REQUIRE(f(5, 5) == FALSE);
    REQUIRE(f(5, 7) == TRUE);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TEMPLATE_TEST_CASE("if_zero", "", SIGNED, UNSIGNED){
    constexpr auto f = scat::constant::if_zero<TestType>;

    REQUIRE(f(0, 1, 0) == 1);
    REQUIRE(f(1, 1, 0) == 0);
}

TEMPLATE_TEST_CASE("if_notzero", "", SIGNED, UNSIGNED){
    constexpr auto f = scat::constant::if_notzero<TestType>;

    REQUIRE(f(0, 1, 0) == 0);
    REQUIRE(f(1, 1, 0) == 1);
}

TEMPLATE_TEST_CASE("if_lessthan", "", SIGNED, UNSIGNED){
    constexpr auto f = scat::constant::if_lessthan<TestType>;

    REQUIRE(f(5, 7, 1, 0) == 1); // LT
    REQUIRE(f(7, 5, 1, 0) == 0); // GT
    REQUIRE(f(5, 5, 1, 0) == 0); // EQ
}

TEMPLATE_TEST_CASE("if_greaterthan", "", SIGNED, UNSIGNED){
    constexpr auto f = scat::constant::if_greaterthan<TestType>;

    REQUIRE(f(5, 7, 1, 0) == 0); // LT
    REQUIRE(f(7, 5, 1, 0) == 1); // GT
    REQUIRE(f(5, 5, 1, 0) == 0); // EQ
}

TEMPLATE_TEST_CASE("if_lessthanequal", "", SIGNED, UNSIGNED){
    constexpr auto f = scat::constant::if_lessthanequal<TestType>;

    REQUIRE(f(5, 7, 1, 0) == 1); // LT
    REQUIRE(f(7, 5, 1, 0) == 0); // GT
    REQUIRE(f(5, 5, 1, 0) == 1); // EQ
}

TEMPLATE_TEST_CASE("if_greaterthanequal", "", SIGNED, UNSIGNED){
    constexpr auto f = scat::constant::if_greaterthanequal<TestType>;

    REQUIRE(f(5, 7, 1, 0) == 0); // LT
    REQUIRE(f(7, 5, 1, 0) == 1); // GT
    REQUIRE(f(5, 5, 1, 0) == 1); // EQ
}

TEMPLATE_TEST_CASE("if_equal", "", SIGNED, UNSIGNED){
    constexpr auto f = scat::constant::if_equal<TestType>;

    REQUIRE(f(5, 5, 1, 0) == 1);
    REQUIRE(f(7, 5, 1, 0) == 0);
}

TEMPLATE_TEST_CASE("if_notequal", "", SIGNED, UNSIGNED){
    constexpr auto f = scat::constant::if_notequal<TestType>;

    REQUIRE(f(5, 5, 1, 0) == 0);
    REQUIRE(f(7, 5, 1, 0) == 1);
}
