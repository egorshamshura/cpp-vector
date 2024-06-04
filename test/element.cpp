#include "element.h"

#include "fault-injection.h"

#include <gtest/gtest.h>

element::element(int data)
    : data(data) {
  add_instance();
}

element::element(const element& other)
    : data(other.data) {
  add_instance();
  ++copy_counter;
}

element::element(element&& other)
    : data(std::exchange(other.data, -1)) {
  add_instance();
  ++move_counter;
}

element::~element() {
  delete_instance();
}

element& element::operator=(const element& c) {
  assert_exists();
  fault_injection_point();

  ++copy_counter;
  data = c.data;
  return *this;
}

element& element::operator=(element&& c) {
  assert_exists();
  fault_injection_point();

  ++move_counter;
  data = std::exchange(c.data, -1);
  return *this;
}

element::operator int() const {
  assert_exists();
  fault_injection_point();

  return data;
}

void element::reset_counters() {
  copy_counter = 0;
  move_counter = 0;
}

size_t element::get_copy_counter() {
  return copy_counter;
}

size_t element::get_move_counter() {
  return move_counter;
}

void element::add_instance() {
  fault_injection_point();
  fault_injection_disable dg;
  auto p = instances.insert(this);
  if (!p.second) {
    std::stringstream ss;
    ss << "A new object is created at the address " << static_cast<void*>(this)
       << " while the previous object at this address was not destroyed";
    throw std::logic_error(ss.str());
  }
}

void element::delete_instance() {
  fault_injection_disable dg;
  size_t erased = instances.erase(this);
  if (erased != 1) {
    FAIL() << "Attempt of destroying non-existing object at address " << static_cast<void*>(this) << '\n';
  }
}

void element::assert_exists() const {
  fault_injection_disable dg;
  bool exists = instances.find(this) != instances.end();
  if (!exists) {
    std::stringstream ss;
    ss << "Accessing a non-existing object at address " << static_cast<const void*>(this);
    throw std::logic_error(ss.str());
  }
}

std::set<const element*> element::instances;

element::no_new_instances_guard::no_new_instances_guard()
    : old_instances(instances) {}

element::no_new_instances_guard::~no_new_instances_guard() {
  EXPECT_EQ(old_instances, instances);
}

void element::no_new_instances_guard::expect_no_instances() {
  EXPECT_EQ(old_instances, instances);
}
