#include "element.h"
#include "fault-injection.h"
#include "ordered-element.h"
#include "vector.h"

#include <gtest/gtest.h>

#include <sstream>
#include <string>

template class vector<int>;
template class vector<element>;
template class vector<std::string>;
template class vector<ordered_element>;

namespace {

template <class Actual, class Expected>
void expect_eq(const Actual& actual, const Expected& expected) {
  fault_injection_disable dg;

  EXPECT_EQ(expected.size(), actual.size());

  if (!std::equal(expected.begin(), expected.end(), actual.begin(), actual.end())) {
    std::stringstream out;
    out << '{';

    bool add_comma = false;
    for (const auto& e : expected) {
      if (add_comma) {
        out << ", ";
      }
      out << e;
      add_comma = true;
    }

    out << "} != {";

    add_comma = false;
    for (const auto& e : actual) {
      if (add_comma) {
        out << ", ";
      }
      out << e;
      add_comma = true;
    }

    out << "}\n";

    ADD_FAILURE() << out.rdbuf();
  }
}

template <typename C>
class strong_exception_safety_guard {
public:
  explicit strong_exception_safety_guard(const C& c) noexcept
      : ref(c)
      , expected((fault_injection_disable{}, c)) {}

  strong_exception_safety_guard(const strong_exception_safety_guard&) = delete;

  ~strong_exception_safety_guard() {
    if (std::uncaught_exceptions() > 0) {
      expect_eq(expected, ref);
    }
  }

private:
  const C& ref;
  C expected;
};

template <>
class strong_exception_safety_guard<element> {
public:
  explicit strong_exception_safety_guard(const element& c) noexcept
      : ref(c)
      , expected((fault_injection_disable{}, c)) {}

  strong_exception_safety_guard(const strong_exception_safety_guard&) = delete;

  ~strong_exception_safety_guard() {
    if (std::uncaught_exceptions() > 0) {
      do_assertion();
    }
  }

private:
  void do_assertion() {
    fault_injection_disable dg;
    ASSERT_EQ(expected, ref);
  }

private:
  const element& ref;
  element expected;
};

class base_test : public ::testing::Test {
protected:
  void SetUp() override {
    ordered_element::insertion_order().clear();
  }

  void expect_empty_storage(const vector<element>& a) {
    instances_guard.expect_no_instances();
    EXPECT_TRUE(a.empty());
    EXPECT_EQ(0, a.size());
    EXPECT_EQ(0, a.capacity());
    EXPECT_EQ(nullptr, a.data());
  }

  element::no_new_instances_guard instances_guard;
};

class correctness_test : public base_test {};

class exception_safety_test : public base_test {};

class performance_test : public base_test {};

} // namespace

TEST_F(correctness_test, default_ctor) {
  vector<element> a;
  expect_empty_storage(a);
}

TEST_F(exception_safety_test, non_throwing_default_ctor) {
  faulty_run([] {
    try {
      vector<element> a;
    } catch (...) {
      fault_injection_disable dg;
      ADD_FAILURE() << "default constructor should not throw";
      throw;
    }
  });
}

TEST_F(correctness_test, push_back) {
  static constexpr size_t N = 5000;

  vector<element> a;
  for (size_t i = 0; i < N; ++i) {
    a.push_back(2 * i + 1);
  }

  EXPECT_EQ(N, a.size());
  EXPECT_LE(N, a.capacity());

  for (size_t i = 0; i < N; ++i) {
    ASSERT_EQ(2 * i + 1, a[i]);
  }
}

TEST_F(exception_safety_test, push_back_throw) {
  static constexpr size_t N = 10;

  faulty_run([] {
    vector<element> a;
    for (size_t i = 0; i < N; ++i) {
      element x = 2 * i + 1;
      strong_exception_safety_guard sg_1(a);
      strong_exception_safety_guard sg_2(x);
      a.push_back(x);
    }
  });
}

TEST_F(correctness_test, push_back_from_self) {
  static constexpr size_t N = 500;

  vector<element> a;
  a.push_back(42);
  for (size_t i = 1; i < N; ++i) {
    a.push_back(a[0]);
  }

  EXPECT_EQ(N, a.size());
  EXPECT_LE(N, a.capacity());

  for (size_t i = 0; i < N; ++i) {
    ASSERT_EQ(42, a[i]);
  }
}

TEST_F(exception_safety_test, push_backe_from_self_throw) {
  static constexpr size_t N = 10;

  faulty_run([] {
    vector<element> a;
    a.push_back(42);
    for (size_t i = 1; i < N; ++i) {
      strong_exception_safety_guard sg(a);
      a.push_back(a[0]);
    }
  });
}

TEST_F(correctness_test, push_back_reallocation) {
  static constexpr size_t N = 500;

  vector<element> a;
  a.reserve(N);
  for (size_t i = 0; i < N; ++i) {
    a.push_back(2 * i + 1);
  }

  element x = N;
  element::reset_counters();
  a.push_back(x);
  ASSERT_EQ(N + 1, element::get_copy_counter());
}

TEST_F(correctness_test, push_back_reallocation_noexcept) {
  static constexpr size_t N = 500;

  vector<element_with_non_throwing_move> a;
  a.reserve(N);
  for (size_t i = 0; i < N; ++i) {
    a.push_back(2 * i + 1);
  }

  element_with_non_throwing_move x = N;
  element::reset_counters();
  a.push_back(x);
  ASSERT_LE(element::get_copy_counter(), 501);
}

TEST_F(correctness_test, subscripting) {
  static constexpr size_t N = 500;

  vector<element> a;
  for (size_t i = 0; i < N; ++i) {
    a.push_back(2 * i + 1);
  }

  for (size_t i = 0; i < N; ++i) {
    ASSERT_EQ(2 * i + 1, a[i]);
  }

  const vector<element>& ca = a;

  for (size_t i = 0; i < N; ++i) {
    ASSERT_EQ(2 * i + 1, ca[i]);
  }
}

TEST_F(correctness_test, data) {
  static constexpr size_t N = 500;

  vector<element> a;
  for (size_t i = 0; i < N; ++i) {
    a.push_back(2 * i + 1);
  }

  {
    element* ptr = a.data();
    for (size_t i = 0; i < N; ++i) {
      ASSERT_EQ(2 * i + 1, ptr[i]);
    }
  }

  {
    const element* cptr = std::as_const(a).data();
    for (size_t i = 0; i < N; ++i) {
      ASSERT_EQ(2 * i + 1, cptr[i]);
    }
  }
}

TEST_F(correctness_test, front_back) {
  static constexpr size_t N = 500;
  vector<element> a;
  for (size_t i = 0; i < N; ++i) {
    a.push_back(2 * i + 1);
  }

  EXPECT_EQ(1, a.front());
  EXPECT_EQ(1, std::as_const(a).front());

  EXPECT_EQ(&a[0], &a.front());
  EXPECT_EQ(&a[0], &std::as_const(a).front());

  EXPECT_EQ(2 * N - 1, a.back());
  EXPECT_EQ(2 * N - 1, std::as_const(a).back());

  EXPECT_EQ(&a[N - 1], &a.back());
  EXPECT_EQ(&a[N - 1], &std::as_const(a).back());
}

TEST_F(correctness_test, reserve) {
  static constexpr size_t N = 500, M = 100, K = 5000;

  vector<element> a;
  a.reserve(N);
  EXPECT_EQ(0, a.size());
  EXPECT_EQ(N, a.capacity());

  for (size_t i = 0; i < M; ++i) {
    a.push_back(2 * i + 1);
  }
  EXPECT_EQ(M, a.size());
  EXPECT_EQ(N, a.capacity());

  for (size_t i = 0; i < M; ++i) {
    ASSERT_EQ(2 * i + 1, a[i]);
  }

  a.reserve(K);
  EXPECT_EQ(M, a.size());
  EXPECT_EQ(K, a.capacity());

  for (size_t i = 0; i < M; ++i) {
    ASSERT_EQ(2 * i + 1, a[i]);
  }
}

TEST_F(correctness_test, reserve_superfluous) {
  static constexpr size_t N = 5000, M = 100, K = 500;

  vector<element> a;
  a.reserve(N);
  ASSERT_EQ(0, a.size());
  ASSERT_EQ(N, a.capacity());

  for (size_t i = 0; i < M; ++i) {
    a.push_back(2 * i + 1);
  }
  ASSERT_EQ(M, a.size());
  ASSERT_EQ(N, a.capacity());

  for (size_t i = 0; i < M; ++i) {
    ASSERT_EQ(2 * i + 1, a[i]);
  }

  element* old_data = a.data();

  a.reserve(K);
  EXPECT_EQ(M, a.size());
  EXPECT_EQ(N, a.capacity());
  EXPECT_EQ(old_data, a.data());

  for (size_t i = 0; i < M; ++i) {
    ASSERT_EQ(2 * i + 1, a[i]);
  }
}

TEST_F(correctness_test, reserve_empty) {
  vector<element> a;
  a.reserve(0);
  expect_empty_storage(a);
}

TEST_F(exception_safety_test, reserve_throw) {
  static constexpr size_t N = 10;

  faulty_run([] {
    fault_injection_disable dg;
    vector<element> a;
    a.reserve(N);

    for (size_t i = 0; i < N; ++i) {
      a.push_back(2 * i + 1);
    }
    dg.reset();

    strong_exception_safety_guard sg(a);
    a.reserve(N + 1);
  });
}

TEST_F(correctness_test, reserve_noexcept) {
  static constexpr size_t N = 500, M = 100, K = 5000;

  vector<element_with_non_throwing_move> a;
  a.reserve(N);
  EXPECT_EQ(0, a.size());
  EXPECT_EQ(N, a.capacity());

  for (size_t i = 0; i < M; ++i) {
    a.push_back(2 * i + 1);
  }
  EXPECT_EQ(M, a.size());
  EXPECT_EQ(N, a.capacity());

  for (size_t i = 0; i < M; ++i) {
    ASSERT_EQ(2 * i + 1, a[i]);
  }

  element::reset_counters();
  a.reserve(K);
  ASSERT_LE(element::get_copy_counter(), 100);

  EXPECT_EQ(M, a.size());
  EXPECT_EQ(K, a.capacity());

  for (size_t i = 0; i < M; ++i) {
    ASSERT_EQ(2 * i + 1, a[i]);
  }
}

TEST_F(correctness_test, shrink_to_fit) {
  static constexpr size_t N = 500, M = 100;

  vector<element> a;
  a.reserve(N);
  ASSERT_EQ(0, a.size());
  ASSERT_EQ(N, a.capacity());

  for (size_t i = 0; i < M; ++i) {
    a.push_back(2 * i + 1);
  }
  ASSERT_EQ(M, a.size());
  ASSERT_EQ(N, a.capacity());

  for (size_t i = 0; i < M; ++i) {
    ASSERT_EQ(2 * i + 1, a[i]);
  }

  a.shrink_to_fit();
  EXPECT_EQ(M, a.size());
  EXPECT_EQ(M, a.capacity());

  for (size_t i = 0; i < M; ++i) {
    ASSERT_EQ(2 * i + 1, a[i]);
  }
}

TEST_F(correctness_test, shrink_to_fit_superfluous) {
  static constexpr size_t N = 500;

  vector<element> a;
  a.reserve(N);
  ASSERT_EQ(0, a.size());
  ASSERT_EQ(N, a.capacity());

  for (size_t i = 0; i < N; ++i) {
    a.push_back(2 * i + 1);
  }
  ASSERT_EQ(N, a.size());

  size_t old_capacity = a.capacity();
  element* old_data = a.data();

  a.shrink_to_fit();
  EXPECT_EQ(N, a.size());
  EXPECT_EQ(old_capacity, a.capacity());
  EXPECT_EQ(old_data, a.data());
}

TEST_F(correctness_test, shrink_to_fit_empty) {
  vector<element> a;
  a.shrink_to_fit();
  expect_empty_storage(a);
}

TEST_F(exception_safety_test, shrink_to_fit_throw) {
  static constexpr size_t N = 10;

  faulty_run([] {
    fault_injection_disable dg;
    vector<element> a;
    a.reserve(N * 2);

    for (size_t i = 0; i < N; ++i) {
      a.push_back(2 * i + 1);
    }
    dg.reset();

    strong_exception_safety_guard sg(a);
    a.shrink_to_fit();
  });
}

TEST_F(correctness_test, shrink_to_fit_noexcept) {
  static constexpr size_t N = 500, M = 100;

  vector<element_with_non_throwing_move> a;
  a.reserve(N);
  ASSERT_EQ(0, a.size());
  ASSERT_EQ(N, a.capacity());

  for (size_t i = 0; i < M; ++i) {
    a.push_back(2 * i + 1);
  }
  ASSERT_EQ(M, a.size());
  ASSERT_EQ(N, a.capacity());

  for (size_t i = 0; i < M; ++i) {
    ASSERT_EQ(2 * i + 1, a[i]);
  }

  element::reset_counters();
  a.shrink_to_fit();
  ASSERT_LE(element::get_copy_counter(), 100);

  EXPECT_EQ(M, a.size());
  EXPECT_EQ(M, a.capacity());

  for (size_t i = 0; i < M; ++i) {
    ASSERT_EQ(2 * i + 1, a[i]);
  }
}

TEST_F(correctness_test, clear) {
  static constexpr size_t N = 500;

  vector<element> a;
  for (size_t i = 0; i < N; ++i) {
    a.push_back(2 * i + 1);
  }
  ASSERT_EQ(N, a.size());

  size_t old_capacity = a.capacity();
  element* old_data = a.data();

  a.clear();
  instances_guard.expect_no_instances();
  EXPECT_TRUE(a.empty());
  EXPECT_EQ(0, a.size());
  EXPECT_EQ(old_capacity, a.capacity());
  EXPECT_EQ(old_data, a.data());
}

TEST_F(exception_safety_test, non_throwing_clear) {
  faulty_run([] {
    fault_injection_disable dg;
    vector<element> a;
    for (size_t i = 0; i < 10; ++i) {
      a.push_back(2 * i + 1);
    }
    dg.reset();
    try {
      a.clear();
    } catch (...) {
      fault_injection_disable dg_2;
      ADD_FAILURE() << "clear() should not throw";
      throw;
    }
  });
}

TEST_F(correctness_test, copy_ctor) {
  static constexpr size_t N = 500;

  vector<element> a;
  for (size_t i = 0; i < N; ++i) {
    a.push_back(2 * i + 1);
  }

  vector<element> b = a;
  EXPECT_EQ(a.size(), b.size());
  EXPECT_EQ(a.size(), b.capacity());
  EXPECT_NE(a.data(), b.data());

  for (size_t i = 0; i < N; ++i) {
    ASSERT_EQ(2 * i + 1, b[i]);
  }
}

TEST_F(correctness_test, move_ctor) {
  static constexpr size_t N = 500;

  vector<element> a;
  for (size_t i = 0; i < N; ++i) {
    a.push_back(2 * i + 1);
  }

  element* a_data = a.data();

  element::reset_counters();
  vector<element> b = std::move(a);
  ASSERT_EQ(0, element::get_copy_counter());

  EXPECT_EQ(N, b.size());
  EXPECT_LE(N, b.capacity());
  EXPECT_EQ(a_data, b.data());
  EXPECT_NE(a.data(), b.data());

  for (size_t i = 0; i < N; ++i) {
    ASSERT_EQ(2 * i + 1, b[i]);
  }
}

TEST_F(performance_test, move_ctor) {
  static constexpr size_t N = 8'000;

  vector<vector<int>> a;
  for (size_t i = 0; i < N; ++i) {
    vector<int> b;
    for (size_t j = 0; j < N; ++j) {
      b.push_back(2 * i + 3 * j);
    }
    a.push_back(std::move(b));
  }

  for (size_t i = 0; i < N; ++i) {
    for (size_t j = 0; j < N; ++j) {
      ASSERT_EQ(2 * i + 3 * j, a[i][j]);
    }
  }
}

TEST_F(correctness_test, copy_assignment_operator) {
  static constexpr size_t N = 500;

  vector<element> a;
  for (size_t i = 0; i < N; ++i) {
    a.push_back(2 * i + 1);
  }

  vector<element> b;
  b = a;
  EXPECT_EQ(a.size(), b.size());
  EXPECT_EQ(a.size(), b.capacity());
  EXPECT_NE(a.data(), b.data());

  vector<element> c;
  c.push_back(42);
  c = a;
  EXPECT_EQ(a.size(), c.size());
  EXPECT_EQ(a.size(), c.capacity());
  EXPECT_NE(a.data(), c.data());

  for (size_t i = 0; i < N; ++i) {
    ASSERT_EQ(2 * i + 1, a[i]);
    ASSERT_EQ(2 * i + 1, b[i]);
    ASSERT_EQ(2 * i + 1, c[i]);
  }
}

TEST_F(correctness_test, move_assignment_operator_to_empty) {
  static constexpr size_t N = 500;

  vector<element> a;
  for (size_t i = 0; i < N; ++i) {
    a.push_back(2 * i + 1);
  }

  element* a_data = a.data();

  element::reset_counters();
  vector<element> b;
  b = std::move(a);
  ASSERT_EQ(0, element::get_copy_counter());

  EXPECT_EQ(N, b.size());
  EXPECT_LE(N, b.capacity());
  EXPECT_EQ(a_data, b.data());
  EXPECT_NE(a.data(), b.data());

  for (size_t i = 0; i < N; ++i) {
    ASSERT_EQ(2 * i + 1, b[i]);
  }
}

TEST_F(correctness_test, move_assignment_operator_to_non_empty) {
  static constexpr size_t N = 500;

  vector<element> a;
  for (size_t i = 0; i < N; ++i) {
    a.push_back(2 * i + 1);
  }

  element* a_data = a.data();

  vector<element> b;
  b.push_back(42);

  element::reset_counters();
  b = std::move(a);
  ASSERT_EQ(0, element::get_copy_counter());

  EXPECT_EQ(N, b.size());
  EXPECT_LE(N, b.capacity());
  EXPECT_EQ(a_data, b.data());
  EXPECT_NE(a.data(), b.data());

  for (size_t i = 0; i < N; ++i) {
    ASSERT_EQ(2 * i + 1, b[i]);
  }
}

TEST_F(correctness_test, self_copy_assignment) {
  static constexpr size_t N = 500;

  vector<element> a;
  for (size_t i = 0; i < N; ++i) {
    a.push_back(2 * i + 1);
  }

  size_t old_capacity = a.capacity();
  element* old_data = a.data();

  element::reset_counters();
  a = a;
  ASSERT_EQ(0, element::get_copy_counter());
  ASSERT_EQ(0, element::get_copy_counter());

  EXPECT_EQ(N, a.size());
  EXPECT_EQ(old_capacity, a.capacity());
  EXPECT_EQ(old_data, a.data());

  for (size_t i = 0; i < N; ++i) {
    ASSERT_EQ(2 * i + 1, a[i]);
  }
}

TEST_F(correctness_test, self_move_assignment) {
  static constexpr size_t N = 500;

  vector<element> a;
  for (size_t i = 0; i < N; ++i) {
    a.push_back(2 * i + 1);
  }

  size_t old_capacity = a.capacity();
  element* old_data = a.data();

  element::reset_counters();
  a = std::move(a);
  ASSERT_EQ(0, element::get_copy_counter());
  ASSERT_EQ(0, element::get_copy_counter());

  EXPECT_EQ(N, a.size());
  EXPECT_EQ(old_capacity, a.capacity());
  EXPECT_EQ(old_data, a.data());

  for (size_t i = 0; i < N; ++i) {
    ASSERT_EQ(2 * i + 1, a[i]);
  }
}

TEST_F(performance_test, move_assignment) {
  static constexpr size_t N = 8'000;

  vector<vector<int>> a;
  for (size_t i = 0; i < N; ++i) {
    vector<int> b;
    for (size_t j = 0; j < N; ++j) {
      b.push_back(2 * i + 3 * j);
    }
    a.push_back({});
    a.back() = std::move(b);
  }

  for (size_t i = 0; i < N; ++i) {
    for (size_t j = 0; j < N; ++j) {
      ASSERT_EQ(2 * i + 3 * j, a[i][j]);
    }
  }
}

TEST_F(correctness_test, empty_storage) {
  vector<element> a;
  expect_empty_storage(a);

  vector<element> b = a;
  expect_empty_storage(b);

  a = b;
  expect_empty_storage(a);
}

TEST_F(correctness_test, pop_back) {
  static constexpr size_t N = 500;

  vector<element> a;
  for (size_t i = 0; i < N; ++i) {
    a.push_back(2 * i + 1);
  }

  size_t old_capacity = a.capacity();
  element* old_data = a.data();

  for (size_t i = N; i > 0; --i) {
    ASSERT_EQ(2 * i - 1, a.back());
    ASSERT_EQ(i, a.size());
    a.pop_back();
  }
  instances_guard.expect_no_instances();
  EXPECT_TRUE(a.empty());
  EXPECT_EQ(0, a.size());
  EXPECT_EQ(old_capacity, a.capacity());
  EXPECT_EQ(old_data, a.data());
}

TEST_F(correctness_test, destroy_order) {
  vector<ordered_element> a;

  a.push_back(1);
  a.push_back(2);
  a.push_back(3);
}

TEST_F(correctness_test, insert_begin) {
  static constexpr size_t N = 500;

  vector<element> a;
  for (size_t i = 0; i < N; ++i) {
    element x = 2 * i + 1;
    auto it = a.insert(std::as_const(a).begin(), x);
    ASSERT_EQ(a.begin(), it);
    ASSERT_EQ(i + 1, a.size());
  }

  for (size_t i = 0; i < N; ++i) {
    ASSERT_EQ(2 * i + 1, a.back());
    a.pop_back();
  }
  ASSERT_TRUE(a.empty());
}

TEST_F(correctness_test, insert_end) {
  static constexpr size_t N = 500;

  vector<element> a;

  for (size_t i = 0; i < N; ++i) {
    a.push_back(2 * i + 1);
  }
  ASSERT_EQ(N, a.size());

  for (size_t i = 0; i < N; ++i) {
    element x = 4 * i + 1;
    auto it = a.insert(a.end(), x);
    ASSERT_EQ(a.end() - 1, it);
    ASSERT_EQ(N + i + 1, a.size());
  }

  for (size_t i = 0; i < N; ++i) {
    ASSERT_EQ(2 * i + 1, a[i]);
  }
  for (size_t i = 0; i < N; ++i) {
    ASSERT_EQ(4 * i + 1, a[N + i]);
  }
}

TEST_F(performance_test, insert) {
  static constexpr size_t N = 8'000;

  vector<vector<int>> a;
  for (size_t i = 0; i < N; ++i) {
    a.push_back(vector<int>());
    for (size_t j = 0; j < N; ++j) {
      a.back().push_back(2 * (i + 1) + 3 * j);
    }
  }

  vector<int> temp;
  for (size_t i = 0; i < N; ++i) {
    temp.push_back(3 * i);
  }
  auto it = a.insert(a.begin(), temp);
  EXPECT_EQ(a.begin(), it);

  for (size_t i = 0; i <= N; ++i) {
    for (size_t j = 0; j < N; ++j) {
      ASSERT_EQ(2 * i + 3 * j, a[i][j]);
    }
  }
}

TEST_F(correctness_test, insert_xvalue_reallocation_noexcept) {
  static constexpr size_t N = 500, K = 7;

  vector<element_with_non_throwing_move> a;
  a.reserve(N);
  for (size_t i = 0; i < N; ++i) {
    a.push_back(2 * i + 1);
  }

  element_with_non_throwing_move x = N;
  element::reset_counters();
  a.insert(a.begin() + K, std::move(x));
  ASSERT_LE(element::get_copy_counter(), 501);
}

TEST_F(correctness_test, erase) {
  static constexpr size_t N = 500;

  for (size_t i = 0; i < N; ++i) {
    vector<element> a;
    for (size_t j = 0; j < N; ++j) {
      a.push_back(2 * j + 1);
    }

    size_t old_capacity = a.capacity();
    element* old_data = a.data();

    auto it = a.erase(std::as_const(a).begin() + i);
    ASSERT_EQ(a.begin() + i, it);
    ASSERT_EQ(N - 1, a.size());
    ASSERT_EQ(old_capacity, a.capacity());
    ASSERT_EQ(old_data, a.data());

    for (size_t j = 0; j < i; ++j) {
      ASSERT_EQ(2 * j + 1, a[j]);
    }
    for (size_t j = i; j < N - 1; ++j) {
      ASSERT_EQ(2 * (j + 1) + 1, a[j]);
    }
  }
}

TEST_F(correctness_test, erase_begin) {
  static constexpr size_t N = 500;

  vector<element> a;
  for (size_t i = 0; i < N * 2; ++i) {
    a.push_back(2 * i + 1);
  }

  for (size_t i = 0; i < N; ++i) {
    auto it = a.erase(a.begin());
    ASSERT_EQ(a.begin(), it);
  }

  for (size_t i = 0; i < N; ++i) {
    ASSERT_EQ(2 * (i + N) + 1, a[i]);
  }
}

TEST_F(correctness_test, erase_end) {
  static constexpr size_t N = 500;

  vector<element> a;
  for (size_t i = 0; i < N * 2; ++i) {
    a.push_back(2 * i + 1);
  }

  for (size_t i = 0; i < N; ++i) {
    auto it = a.erase(a.end() - 1);
    ASSERT_EQ(a.end(), it);
  }

  for (size_t i = 0; i < N; ++i) {
    ASSERT_EQ(2 * i + 1, a[i]);
  }
}

TEST_F(correctness_test, erase_range_begin) {
  static constexpr size_t N = 500, K = 100;

  vector<element> a;
  for (size_t i = 0; i < N; ++i) {
    a.push_back(2 * i + 1);
  }

  size_t old_capacity = a.capacity();
  element* old_data = a.data();

  auto it = a.erase(std::as_const(a).begin(), std::as_const(a).begin() + K);
  EXPECT_EQ(a.begin(), it);
  EXPECT_EQ(N - K, a.size());
  EXPECT_EQ(old_capacity, a.capacity());
  EXPECT_EQ(old_data, a.data());

  for (size_t i = 0; i < N - K; ++i) {
    ASSERT_EQ(2 * (i + K) + 1, a[i]);
  }
}

TEST_F(correctness_test, erase_range_middle) {
  static constexpr size_t N = 500, K = 100;

  vector<element> a;

  for (size_t i = 0; i < N; ++i) {
    a.push_back(2 * i + 1);
  }

  size_t old_capacity = a.capacity();
  element* old_data = a.data();

  auto it = a.erase(a.begin() + K, a.end() - K);
  EXPECT_EQ(a.begin() + K, it);
  EXPECT_EQ(K * 2, a.size());
  EXPECT_EQ(old_capacity, a.capacity());
  EXPECT_EQ(old_data, a.data());

  for (size_t i = 0; i < K; ++i) {
    ASSERT_EQ(2 * i + 1, a[i]);
  }
  for (size_t i = 0; i < K; ++i) {
    ASSERT_EQ(2 * (i + N - K) + 1, a[i + K]);
  }
}

TEST_F(correctness_test, erase_range_end) {
  static constexpr size_t N = 500, K = 100;

  vector<element> a;

  for (size_t i = 0; i < N; ++i) {
    a.push_back(2 * i + 1);
  }

  size_t old_capacity = a.capacity();
  element* old_data = a.data();

  auto it = a.erase(a.end() - K, a.end());
  EXPECT_EQ(a.end(), it);
  EXPECT_EQ(N - K, a.size());
  EXPECT_EQ(old_capacity, a.capacity());
  EXPECT_EQ(old_data, a.data());

  for (size_t i = 0; i < N - K; ++i) {
    ASSERT_EQ(2 * i + 1, a[i]);
  }
}

TEST_F(correctness_test, erase_range_all) {
  static constexpr size_t N = 500;

  vector<element> a;

  for (size_t i = 0; i < N; ++i) {
    a.push_back(2 * i + 1);
  }

  size_t old_capacity = a.capacity();
  element* old_data = a.data();

  auto it = a.erase(a.begin(), a.end());
  EXPECT_EQ(a.end(), it);

  instances_guard.expect_no_instances();
  EXPECT_TRUE(a.empty());
  EXPECT_EQ(0, a.size());
  EXPECT_EQ(old_capacity, a.capacity());
  EXPECT_EQ(old_data, a.data());
}

TEST_F(performance_test, erase) {
  static constexpr size_t N = 8'000, M = 50'000, K = 100;

  vector<int> a;
  for (size_t i = 0; i < N; ++i) {
    for (size_t j = 0; j < M; ++j) {
      a.push_back(j);
    }
    auto it = a.erase(a.begin() + K, a.end() - K);
    ASSERT_EQ(a.begin() + K, it);
    ASSERT_EQ(K * 2, a.size());
    a.clear();
  }
}

TEST_F(exception_safety_test, reallocation_throw) {
  static constexpr size_t N = 10;

  faulty_run([] {
    fault_injection_disable dg;
    vector<element> a;
    a.reserve(N);
    ASSERT_EQ(N, a.capacity());

    for (size_t i = 0; i < N; ++i) {
      a.push_back(2 * i + 1);
    }
    dg.reset();

    strong_exception_safety_guard sg(a);
    a.push_back(42);
  });
}

TEST_F(exception_safety_test, copy_throw) {
  static constexpr size_t N = 10;

  faulty_run([] {
    fault_injection_disable dg;
    vector<element> a;
    a.reserve(N);
    ASSERT_EQ(N, a.capacity());

    for (size_t i = 0; i < N; ++i) {
      a.push_back(2 * i + 1);
    }
    dg.reset();

    strong_exception_safety_guard sg(a);
    [[maybe_unused]] vector<element> b(a);
  });
}

TEST_F(exception_safety_test, move_throw) {
  static constexpr size_t N = 10;

  faulty_run([] {
    fault_injection_disable dg;
    vector<element> a;
    a.reserve(N);
    ASSERT_EQ(N, a.capacity());

    for (size_t i = 0; i < N; ++i) {
      a.push_back(2 * i + 1);
    }
    dg.reset();

    strong_exception_safety_guard sg(a);
    [[maybe_unused]] vector<element> b(std::move(a));
  });
}

TEST_F(exception_safety_test, copy_assign_throw) {
  static constexpr size_t N = 10;

  faulty_run([] {
    fault_injection_disable dg;
    vector<element> a;
    a.reserve(N);

    for (size_t i = 0; i < N; ++i) {
      a.push_back(2 * i + 1);
    }

    vector<element> b;
    b.push_back(0);
    dg.reset();

    strong_exception_safety_guard sg(a);
    b = std::as_const(a);
  });
}

TEST_F(exception_safety_test, move_assign_throw) {
  static constexpr size_t N = 10;

  faulty_run([] {
    fault_injection_disable dg;
    vector<element> a;
    a.reserve(N);

    for (size_t i = 0; i < N; ++i) {
      a.push_back(2 * i + 1);
    }

    vector<element> b;
    b.push_back(0);
    dg.reset();

    strong_exception_safety_guard sg(a);
    b = std::move(a);
  });
}

TEST_F(correctness_test, member_aliases) {
  EXPECT_TRUE((std::is_same<element, vector<element>::value_type>::value));
  EXPECT_TRUE((std::is_same<element&, vector<element>::reference>::value));
  EXPECT_TRUE((std::is_same<const element&, vector<element>::const_reference>::value));
  EXPECT_TRUE((std::is_same<element*, vector<element>::pointer>::value));
  EXPECT_TRUE((std::is_same<const element*, vector<element>::const_pointer>::value));
  EXPECT_TRUE((std::is_same<element*, vector<element>::iterator>::value));
  EXPECT_TRUE((std::is_same<const element*, vector<element>::const_iterator>::value));
}
