#pragma once

#include <gtest/gtest.h>

#include <vector>

class ordered_element {
public:
  ordered_element(size_t val)
      : val(val) {
    insertion_order().push_back(val);
  }

  ordered_element(const ordered_element& other)
      : val(other.val) {
    other.val = 0;
  }

  ~ordered_element() {
    delete_instance();
  }

  static std::vector<size_t>& insertion_order() {
    static std::vector<size_t> insertion_order;
    return insertion_order;
  }

private:
  mutable size_t val;

  void delete_instance() {
    if (val == 0) {
      return;
    }

    size_t back = insertion_order().back();
    ASSERT_EQ(val, back) << "Elements must be destroyed in reverse order of insertion";
    insertion_order().pop_back();
  }
};
