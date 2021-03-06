// vim:filetype=cpp:textwidth=120:shiftwidth=2:softtabstop=2:expandtab
// Copyright 2016-2019 Christoph Schwering
// Licensed under the MIT license. See LICENSE file in the project root.
//
// DenseMap, DenseMinHeap classes, which are based on representing keys or
// entries, respectively, as non-negative integers close to zero.

#ifndef LIMBO_INTERNAL_DENSE_H_
#define LIMBO_INTERNAL_DENSE_H_

#include <algorithm>
#include <vector>

namespace limbo {
namespace internal {

struct NoBoundCheck {
  template<typename T, typename Index>
  void operator()(T*, const Index&) const {}
};

struct SlowAdjustBoundCheck {
  template<typename T, typename Index>
  void operator()(T* c, const Index& i) const {
    c->FitForIndex(i);
  }
};

struct FastAdjustBoundCheck {
  template<typename T, typename Index>
  void operator()(T* c, const Index& i) const {
    c->FitForIndex(next_power_of_two(i));
  }
};

template<typename Key,
         typename Val,
         typename Index,
         Index kOffset,
         typename KeyToIndex,
         typename IndexToKey,
         typename CheckBound>
class DenseMap {
 public:
  using Vec             = std::vector<Val>;
  using value_type      = typename Vec::value_type;
  using reference       = typename Vec::reference;
  using const_reference = typename Vec::const_reference;
  using iterator        = typename Vec::iterator;
  using const_iterator  = typename Vec::const_iterator;

  // Wrapper for operator*() and operator++(int).
  template<typename InputIt>
  class IteratorProxy {
   public:
    using value_type = typename InputIt::value_type;
    using reference  = typename InputIt::reference;
    using pointer    = typename InputIt::pointer;

    explicit IteratorProxy(reference v) : v_(v) {}
    reference operator*() const { return v_; }
    pointer operator->() const { return &v_; }

   private:
    mutable typename std::remove_const<value_type>::type v_{};
  };

  struct KeyIterator {
    using difference_type = Index;
    using value_type = Key;
    using pointer = const value_type*;
    using reference = value_type;
    using iterator_category = std::random_access_iterator_tag;

    using proxy = IteratorProxy<KeyIterator>;

    explicit KeyIterator(Index i, KeyToIndex k2i = KeyToIndex(), IndexToKey i2k = IndexToKey())
        : k2i(k2i), i2k(i2k), index(i) {}

    bool operator==(KeyIterator it) const { return index == it.index; }
    bool operator!=(KeyIterator it) const { return !(*this == it); }

    reference operator*() const { return i2k(index); }
    KeyIterator& operator++() { ++index; return *this; }
    KeyIterator& operator--() { --index; return *this; }

    proxy operator->() const { return proxy(operator*()); }
    proxy operator++(int) { proxy p(operator*()); operator++(); return p; }
    proxy operator--(int) { proxy p(operator*()); operator--(); return p; }

    KeyIterator& operator+=(difference_type n) { index += n; return *this; }
    KeyIterator& operator-=(difference_type n) { index -= n; return *this; }
    friend KeyIterator operator+(KeyIterator it, difference_type n) { it += n; return it; }
    friend KeyIterator operator+(difference_type n, KeyIterator it) { it += n; return it; }
    friend KeyIterator operator-(KeyIterator it, difference_type n) { it -= n; return it; }
    friend difference_type operator-(KeyIterator a, KeyIterator b) { return a.index - b.index; }
    reference operator[](difference_type n) const { return *(*this + n); }
    bool operator<(KeyIterator it) const { return index < it.index; }
    bool operator>(KeyIterator it) const { return index > it.index; }
    bool operator<=(KeyIterator it) const { return !(*this > it); }
    bool operator>=(KeyIterator it) const { return !(*this < it); }

   private:
    KeyToIndex k2i{};
    IndexToKey i2k{};
    Index index{};
  };

  template<typename T>
  struct Range {
    Range(T begin, T end) : begin_(begin), end_(end) {}
    T begin() const { return begin_; }
    T end()   const { return end_; }
   private:
    T begin_{};
    T end_{};
  };

  explicit DenseMap(KeyToIndex k2i = KeyToIndex(), IndexToKey i2k = IndexToKey()) : k2i_(k2i), i2k_(i2k) {}

  DenseMap(const DenseMap&)              = default;
  DenseMap& operator=(const DenseMap& c) = default;
  DenseMap(DenseMap&&)                   = default;
  DenseMap& operator=(DenseMap&& c)      = default;

  void FitForKey(Key k)            { FitForIndex(k2i_(k)); }
  void FitForKey(Key k, Val v)     { FitForIndex(k2i_(k), v); }
  void FitForIndex(Index i)        { i -= kOffset; if (i >= Index(vec_.size())) { vec_.resize(i + 1); } }
  void FitForIndex(Index i, Val v) { i -= kOffset; if (i >= Index(vec_.size())) { vec_.resize(i + 1, v); } }

  bool empty() const { return vec_.empty(); }

  void Clear() { vec_.clear(); }

  bool key_in_range(Key k)     const { return index_in_range(k2i_(k)); }
  bool index_in_range(Index i) const { i -= kOffset; return 0 <= i && i < Index(vec_.size()); }

  Index upper_bound_index() const { return Index(vec_.size()) - 1 + kOffset; }
  Key   upper_bound_key()   const { return i2k_(upper_bound_index()); }

        reference at_index(Index i)       { cb_(this, i);                        i -= kOffset; return vec_[i]; }
  const_reference at_index(Index i) const { cb_(const_cast<DenseMap*>(this), i); i -= kOffset; return vec_[i]; }

        reference at_key(Key key)       { return at_index(k2i_(key)); }
  const_reference at_key(Key key) const { return at_index(k2i_(key)); }

        reference operator[](Key key)       { return at_key(key); }
  const_reference operator[](Key key) const { return at_key(key); }

        reference head()       { return at_index(kOffset); }
  const_reference head() const { return at_index(kOffset); }

  Range<KeyIterator> keys() const {
    return Range<KeyIterator>(KeyIterator(kOffset, k2i_, i2k_), KeyIterator(Index(vec_.size()) + kOffset, k2i_, i2k_));
  }
  Range<const_iterator> values() const { return Range<const_iterator>(vec_.begin(), vec_.end()); }
  Range<      iterator> values()       { return Range<      iterator>(vec_.begin(), vec_.end()); }

 private:
  CheckBound cb_{};
  KeyToIndex k2i_{};
  IndexToKey i2k_{};
  Vec vec_{};
};

template<typename T,
         typename Less,
         typename Index,
         Index kOffset,
         typename KeyToIndex,
         typename IndexToKey,
         typename CheckBound>
class DenseMinHeap {
 public:
  explicit DenseMinHeap(Less less = Less()) : less_(less) { heap_.emplace_back(); }

  DenseMinHeap(const DenseMinHeap&)            = default;
  DenseMinHeap& operator=(const DenseMinHeap&) = default;
  DenseMinHeap(DenseMinHeap&&)                 = default;
  DenseMinHeap& operator=(DenseMinHeap&&)      = default;

  void set_less(Less less) { less_ = less; }
        Less& less()       { return less; }
  const Less& less() const { return less; }

  void FitForElement(const T x) { index_.FitForKey(x); }
  void FitForIndex(const Index i) { index_.FitForIndex(i); }

  void Clear() { heap_.clear(); index_.Clear(); heap_.emplace_back(); }

  int size()  const { return heap_.size() - 1; }
  bool empty() const { return heap_.size() == 1; }
  const T& operator[](Index i) const { return heap_[i + 1]; }

  bool contains(const T& x) const { return index_[x] != 0; }

  T top() const { return heap_[bool(size())]; }

  void Increase(const T& x) {
    assert(contains(x));
    SiftUp(index_[x]);
    assert(contains(x));
    assert(std::min_element(heap_.begin() + 1, heap_.end(), less_) == heap_.begin() + 1);
  }

  void Decrease(const T& x) {
    Remove(x);
    Insert(x);
  }

  void Insert(const T& x) {
    assert(!contains(x));
    const Index i = heap_.size();
    heap_.push_back(x);
    index_[x] = i;
    SiftUp(i);
    assert(std::min_element(heap_.begin() + 1, heap_.end(), less_) == heap_.begin() + 1);
  }

  void Remove(const T& x) {
    assert(contains(x));
    const Index i = index_[x];
    heap_[i] = heap_.back();
    index_[heap_[i]] = i;
    heap_.pop_back();
    index_[x] = 0;
    if (Index(heap_.size()) > i) {
      SiftDown(i);
      SiftUp(i);
    }
    assert(!contains(x));
    assert(std::min_element(heap_.begin() + 1, heap_.end(), less_) == heap_.begin() + 1);
  }

  typename std::vector<T>::const_iterator begin() const { return heap_.begin() + 1; }
  typename std::vector<T>::const_iterator end()   const { return heap_.end(); }

 private:
  using Map = DenseMap<T, Index, Index, kOffset, KeyToIndex, IndexToKey, CheckBound>;

  static Index left(const Index i)   { return 2 * i; }
  static Index right(const Index i)  { return 2 * i + 1; }
  static Index parent(const Index i) { return i / 2; }

  void SiftUp(Index i) {
    assert(i > 0 && i < heap_.size());
    const T x = heap_[i];
    Index p;
    while ((p = parent(i)) != 0 && less_(x, heap_[p])) {
      heap_[i] = heap_[p];
      index_[heap_[i]] = i;
      i = p;
    }
    heap_[i] = x;
    index_[x] = i;
    assert(std::all_of(heap_.begin() + 1, heap_.end(), [this](T x) { return heap_[index_[x]] == x; }));
  }

  void SiftDown(Index i) {
    assert(i > 0 && i < heap_.size());
    const T x = heap_[i];
    while (left(i) < Index(heap_.size())) {
      const Index min_child =
          right(i) < Index(heap_.size()) && less_(heap_[right(i)], heap_[left(i)])
          ? right(i) : left(i);
      if (!less_(heap_[min_child], x)) {
        break;
      }
      heap_[i] = heap_[min_child];
      index_[heap_[i]] = i;
      i = min_child;
    }
    heap_[i] = x;
    index_[x] = i;
    assert(std::all_of(heap_.begin() + 1, heap_.end(), [this](T x) { return heap_[index_[x]] == x; }));
  }

  Less less_{};
  std::vector<T> heap_{};
  Map index_{};
};

}  // namespace internal
}  // namespace limbo

#endif  // LIMBO_INTERNAL_DENSE_H_

