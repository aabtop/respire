#ifndef __EBB_LINKED_LIST_H__
#define __EBB_LINKED_LIST_H__

#include <cassert>

namespace ebb {

template <typename T>
class LinkedList {
 public:
  class Node {
   public:
    Node(T* item) : next_(nullptr), prev_(nullptr), item_(item) {}
    void set_item(T* item) { item_ = item; }
    T* item() { return item_; }

   private:
    friend class LinkedList;

    Node* next_;
    Node* prev_;
    T* item_;
  };

  LinkedList() : back_(nullptr), front_(nullptr) {}

  bool empty() const { return back_ == nullptr; }

  void push_back(Node* node) {
    assert(node->next_ == nullptr);
    assert(node->prev_ == nullptr);
    node->next_ = back_;
    if (node->next_) {
      node->next_->prev_ = node;
    }
    node->prev_ = nullptr;
    back_ = node;
    if (front_ == nullptr) {
      front_ = back_;
    }
  }

  Node* front() {
    return front_;
  }

  void pop_front() {
    if (front_) {
      Node* new_front = front_->prev_;
      if (new_front) {
        new_front->next_ = nullptr;
      }
      front_->next_ = nullptr;
      front_->prev_ = nullptr;
      if (back_ == front_) {
        back_ = nullptr;
      }
      front_ = new_front;
    }
  }

  Node* back() {
    return back_;
  }

  void pop_back() {
    if (back_) {
      Node* new_back = back_->next_;
      if (new_back) {
        new_back->prev_ = nullptr;
      }
      back_->next_ = nullptr;
      back_->prev_ = nullptr;
      if (back_ == front_) {
        front_ = nullptr;
      }
      back_ = new_back;
    }      
  }
 private:
  Node* back_;
  Node* front_;
};

}  // namespace ebb

#endif  // __EBB_LINKED_LIST_H__
