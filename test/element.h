#pragma once

#include <set>

struct element {
  struct no_new_instances_guard;

  element() = delete;
  element(int data);
  element(const element& other);
  element(element&& other);
  ~element();

  element& operator=(const element& c);
  element& operator=(element&& c);
  operator int() const;

  static void reset_counters();
  static size_t get_copy_counter();
  static size_t get_move_counter();

private:
  void add_instance();
  void delete_instance();
  void assert_exists() const;

private:
  int data;

  static std::set<const element*> instances;
  inline static size_t copy_counter = 0;
  inline static size_t move_counter = 0;
};

struct element::no_new_instances_guard {
  no_new_instances_guard();

  no_new_instances_guard(const no_new_instances_guard&) = delete;
  no_new_instances_guard& operator=(const no_new_instances_guard&) = delete;

  ~no_new_instances_guard();

  void expect_no_instances();

private:
  std::set<const element*> old_instances;
};

struct element_with_non_throwing_move : element {
  using element::element;

  element_with_non_throwing_move(const element_with_non_throwing_move& other) noexcept = default;
  element_with_non_throwing_move(element_with_non_throwing_move&& other) noexcept = default;

  element_with_non_throwing_move& operator=(const element_with_non_throwing_move& other) = default;
  element_with_non_throwing_move& operator=(element_with_non_throwing_move&& other) noexcept = default;
};
