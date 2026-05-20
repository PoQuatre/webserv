/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   hello.cpp                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mle-flem <mle-flem@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/20 01:47:28 by mle-flem          #+#    #+#             */
/*   Updated: 2026/05/20 07:55:00 by mle-flem         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <criterion/criterion.h>

// Basic assertion tests
Test(suite_basic, test_pass)
{
    cr_assert(1 == 1, "Basic equality should pass");
}

Test(suite_basic, test_fail_example)
{
    cr_assert_eq(2 + 2, 4, "Addition should work");
}

// String tests
Test(suite_strings, test_string_eq)
{
    cr_assert_str_eq("hello", "hello", "Strings should be equal");
}

Test(suite_strings, test_string_neq)
{
    cr_assert_str_neq("hello", "world", "Strings should differ");
}

// Math tests
Test(suite_math, test_addition)
{
    int result = 3 + 4;
    cr_assert_eq(result, 7, "3 + 4 should equal 7");
}

Test(suite_math, test_float_tolerance)
{
    cr_assert_float_eq(0.1 + 0.2, 0.3, 1e-6, "Float addition within tolerance");
}

// Null / pointer tests
Test(suite_pointers, test_not_null)
{
    int x = 42;
    int *p = &x;
    cr_assert_not_null(p, "Pointer should not be null");
}

Test(suite_pointers, test_null)
{
    int *p = nullptr;
    cr_assert_null(p, "Pointer should be null");
}
