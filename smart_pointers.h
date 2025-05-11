#include <memory>

// comment to check this shared ptr

template <typename U>
class SharedPtr;

template <typename T>
class WeakPtr;

template <typename T>
class EnableSharedFromThis;

class BaseControlBlock {
 protected:
  size_t shared_count = 0;
  size_t weak_count = 0;

 public:
  BaseControlBlock(void* ptr)
      : shared_count(static_cast<size_t>(ptr != nullptr)), weak_count(0) {}

  BaseControlBlock() : BaseControlBlock(nullptr) {}

  size_t get_shared() { return shared_count; }

  size_t get_weak() { return weak_count; }

  void add_shared() { ++shared_count; }

  void add_weak() { ++weak_count; }

  void remove_shared() { --shared_count; }

  void remove_weak() { --weak_count; }

  bool expired() { return shared_count == 0; }

  virtual void destroy() = 0;

  virtual void deallocate() = 0;

  virtual BaseControlBlock* AllocateNew() = 0;

  virtual ~BaseControlBlock() = default;
};

template <typename U, typename Deleter = std::default_delete<U>,
          typename Alloc = std::allocator<U>>
class ControlBlockReqular : BaseControlBlock {
  template <typename T>
  friend class SharedPtr;

  template <typename T>
  friend class WeakPtr;

  U* ptr;
  Deleter del;
  Alloc alloc;
  using U_traits = std::allocator_traits<Alloc>;

  using BlockAlloc = typename U_traits::template rebind_alloc<
      ControlBlockReqular<U, Deleter, Alloc>>;  // TODO

  using BlockTraits = std::allocator_traits<BlockAlloc>;

  void deallocate() override {
    BlockAlloc block_alloc(alloc);
    // BlockTraits::destroy(block_alloc, this);
    BlockTraits::deallocate(block_alloc, this, 1);
  }

  void destroy() override {
    del(ptr);
    ptr = nullptr;
    if (get_weak() == 0) {
      deallocate();  // TODO
    }
  }

  BaseControlBlock* AllocateNew() override {
    BlockAlloc block_alloc(alloc);
    ControlBlockReqular* new_block = BlockTraits::allocate(block_alloc, 1);
    BlockTraits::construct(block_alloc, new_block);
    return new_block;
  }

 public:
  ControlBlockReqular(U* ptr)
      : BaseControlBlock(ptr), ptr(ptr), del(Deleter()), alloc(Alloc()) {}

  ControlBlockReqular() : ControlBlockReqular(nullptr) {}

  template <typename... Args>
  void constructValue(Args&&... args) {
    U_traits::construct(alloc, ptr, std::forward<Args>(args)...);
  }

  ControlBlockReqular(U* ptr, Deleter del, Alloc alloc)
      : BaseControlBlock(ptr), ptr(ptr), del(del), alloc(alloc) {}

  /*ControlBlockReqular(U* ptr, Deleter del, BlockAlloc alloc)
      : BaseControlBlock(ptr), ptr(ptr), del(del), alloc(alloc) {}*/
};

template <typename U, typename Alloc = std::allocator<U>>
class ControlBlockMakeShared : BaseControlBlock {
  template <typename T>
  friend class SharedPtr;

  template <typename T>
  friend class WeakPtr;

  template <typename V, typename Allocator, typename... Args>
  friend SharedPtr<V> allocateShared(const Allocator& alloc, Args&&... args);

  template <typename V, typename... Args>
  friend SharedPtr<V> makeShared(Args&&... args);

  U value;
  Alloc alloc;

  using U_traits = std::allocator_traits<Alloc>;

  using BlockAlloc = typename U_traits::template rebind_alloc<
      ControlBlockMakeShared<U, Alloc>>;  // TODO

  using BlockTraits = std::allocator_traits<BlockAlloc>;

  void deallocate() override {
    BlockAlloc block_alloc(alloc);
    // BlockTraits::destroy(block_alloc, this);
    BlockTraits::deallocate(block_alloc, this, 1);
  }

  void destroy() override {
    U_traits::destroy(alloc, &value);
    // delete &value;
    if (get_weak() == 0) {
      deallocate();
    }  // TODO
  }

  BaseControlBlock* AllocateNew() override {
    BlockAlloc block_alloc(alloc);
    ControlBlockMakeShared* new_block = BlockTraits::allocate(block_alloc, 1);
    // BlockTraits::construct(block_alloc, new_block, alloc);
    return new_block;
  }

 public:
  template <typename... Args>
  ControlBlockMakeShared(Alloc alloc, Args&&... args)
      : value(std::forward<Args>(args)...), alloc(alloc) {
    add_shared();
  }
  /*ControlBlockMakeShared(U* ptr)
      : BaseControlBlock(ptr), ptr(ptr), del(Deleter()), alloc(Alloc()) {}*/
};

template <typename T>
class SharedPtr {
 private:
  T* ptr_;
  BaseControlBlock* block_;

  template <typename U, typename Alloc, typename... Args>
  friend SharedPtr<U> allocateShared(const Alloc& alloc, Args&&... args);

  template <typename U, typename... Args>
  friend SharedPtr<U> makeShared(Args&&... args);

  template <typename U>
  friend class WeakPtr;

  template <typename U>
  friend class SharedPtr;

  SharedPtr(T& ptr, BaseControlBlock* block) : ptr_(&ptr), block_(block) {}

  // SharedPtr(T* ptr, BaseControlBlock* block) : ptr_(ptr), block_(block) {}

 public:
  template <typename U, typename Deleter = std::default_delete<U>,
            typename Alloc = std::allocator<U>>
  SharedPtr(U* ptr, Deleter del, Alloc alloc) : ptr_(ptr), block_(nullptr) {

    using BlockAlloc = typename std::allocator_traits<
        Alloc>::template rebind_alloc<ControlBlockReqular<U, Deleter, Alloc>>;
    using BlockTraits = std::allocator_traits<BlockAlloc>;
    BlockAlloc block_alloc;
    block_ = BlockTraits::allocate(block_alloc, 1);
    new (block_) ControlBlockReqular<U, Deleter, Alloc>(ptr, del, alloc);
    if constexpr (std::is_base_of_v<EnableSharedFromThis<T>, T>) {
      ptr->weak_ptr_ = *this;
    }
  }

  template <typename U, typename Deleter = std::default_delete<U>>
  SharedPtr(U* ptr, Deleter del) : SharedPtr(ptr, del, std::allocator<U>()) {}

  template <typename U>
  SharedPtr(U* ptr)
      : SharedPtr(ptr, std::default_delete<U>(), std::allocator<U>()) {}

  SharedPtr() : ptr_(nullptr), block_(nullptr) {}

  SharedPtr(const SharedPtr& other) : ptr_(other.ptr_), block_(other.block_) {
    if (ptr_) {
      block_->add_shared();
    }
  }  // TODO

  template <typename U>
  SharedPtr(const WeakPtr<U>& wptr) : ptr_(wptr.ptr_), block_(wptr.block_) {
    if (block_ == nullptr) {
      block_ = new ControlBlockReqular<U>;
    }
    block_->add_shared();
    if (block_->get_shared() == 1) {
      static_cast<ControlBlockReqular<T>*>(block_)->constructValue();
    }
  }  // TODO

  template <typename U>
  SharedPtr(const SharedPtr<U>& other)
      : ptr_(other.ptr_), block_(other.block_) {
    block_->add_shared();
  }

  SharedPtr(SharedPtr&& other) : ptr_(other.ptr_), block_(other.block_) {
    other.ptr_ = nullptr;
    other.block_ = nullptr;
  }

  template <typename U>
  SharedPtr(SharedPtr<U>&& other) : ptr_(other.ptr_), block_(other.block_) {
    other.ptr_ = nullptr;
    other.block_ = nullptr /*other.block_->AllocateNew()*/;
  }

  T& operator*() { return *ptr_; }

  const T& operator*() const { return *ptr_; }

  T* operator->() const { return ptr_; }

  T* get() { return ptr_; }

  const T* get() const { return ptr_; }

  void swap(SharedPtr& other) {
    std::swap(block_, other.block_);
    std::swap(ptr_, other.ptr_);
  }

  void reset() { SharedPtr().swap(*this); }

  template <typename U>
  void reset(U* new_ptr) {
    SharedPtr<T>(new_ptr).swap(*this);
  }

  size_t use_count() const { return block_->get_shared(); }

  SharedPtr& operator=(const SharedPtr& other) {
    SharedPtr(other).swap(*this);
    return *this;
  }

  template <typename U>
  SharedPtr& operator=(const SharedPtr<U>& other) {
    SharedPtr(other).swap(*this);
    return *this;
  }

  template <typename U>
  SharedPtr& operator=(SharedPtr<U>&& other) {
    SharedPtr(std::move(other)).swap(*this);
    return *this;
  }

  SharedPtr& operator=(SharedPtr&& other) {
    SharedPtr(std::move(other)).swap(*this);
    return *this;
  }

  // bool operator==(const SharedPtr& other) const { return ptr_ == other.ptr_;
  // }

  ~SharedPtr() {
    if (block_ == nullptr) return;
    block_->remove_shared();
    if (block_->get_shared() == 0) {
      block_->destroy();
    }
  }
};

template <typename T>
class WeakPtr {
 private:
  T* ptr_;
  BaseControlBlock* block_;

  template <typename U>
  friend class WeakPtr;

  template <typename U>
  friend class SharedPtr;

 public:
  template <typename U>
  WeakPtr(const SharedPtr<U>& sptr) : ptr_(sptr.ptr_), block_(sptr.block_) {
    if (block_) {
      block_->add_weak();
    }
  }

  WeakPtr() : ptr_(nullptr) { block_ = new ControlBlockReqular<T>; }

  WeakPtr(const WeakPtr& other) : ptr_(other.ptr_), block_(other.block_) {
    if (block_) {
      block_->add_weak();
    }
  }

  template <typename U>
  WeakPtr(const WeakPtr<U>& other) : ptr_(other.ptr_), block_(other.block_) {
    block_->add_weak();
  }

  WeakPtr(WeakPtr&& other) : ptr_(other.ptr_), block_(other.block_) {
    other.ptr_ = nullptr;
    other.block_ = new ControlBlockReqular<T>;
  }

  template <typename U>
  WeakPtr(WeakPtr<U>&& other) : ptr_(other.ptr_), block_(other.block_) {
    other.ptr_ = nullptr;
    other.block_ = new ControlBlockReqular<U>;
  }

  bool expired() const { return block_->expired(); }

  SharedPtr<T> lock() const {
    SharedPtr<T> answer = SharedPtr<T>(*ptr_, block_);
    block_->add_shared();
    return answer;
  }

  size_t use_count() { return block_->get_shared(); }

  void swap(WeakPtr& other) {
    std::swap(ptr_, other.ptr_);
    std::swap(block_, other.block_);
  }

  WeakPtr& operator=(WeakPtr&& other) {
    WeakPtr<T>(std::move(other)).swap(*this);
    return *this;
  }

  template <typename U>
  WeakPtr& operator=(WeakPtr<U>&& other) {
    WeakPtr<T>(std::move(other)).swap(*this);
    return *this;
  }

  template <typename U>
  WeakPtr& operator=(const SharedPtr<U>& sptr) {
    WeakPtr<T>(sptr).swap(*this);
    return *this;
  }

  WeakPtr& operator=(const WeakPtr& other) {
    WeakPtr<T>(other).swap(*this);
    return *this;
  }

  template <typename U>
  WeakPtr& operator=(const WeakPtr<U>& other) {
    WeakPtr<T>(other).swap(*this);
    return *this;
  }

  T* operator->() { return ptr_; }

  ~WeakPtr() {
    if (block_ == nullptr) return;
    if (block_->get_weak() > 0) {
      block_->remove_weak();
    }
    if (block_->get_weak() == 0 && block_->get_shared() == 0) {
      block_->deallocate();
    }
  }
};

template <typename T>
class EnableSharedFromThis {
 private:
  WeakPtr<T> weak_ptr_;

 public:
  EnableSharedFromThis() {}
  SharedPtr<T> shared_from_this() { return weak_ptr_; }
  template <typename U>
  friend class SharedPtr;
};

template <typename T, typename Alloc, typename... Args>
SharedPtr<T> allocateShared(const Alloc& alloc, Args&&... args) {
  using BlockAlloc = typename std::allocator_traits<
      Alloc>::template rebind_alloc<ControlBlockMakeShared<T, Alloc>>;
  using BlockTraits = std::allocator_traits<BlockAlloc>;
  BlockAlloc block_alloc(alloc);
  ControlBlockMakeShared<T, Alloc>* block =
      BlockTraits::allocate(block_alloc, 1);
  BlockTraits::construct(block_alloc, block, alloc,
                         std::forward<Args>(args)...);
  return SharedPtr<T>(block->value, block);
}

template <typename T, typename... Args>
SharedPtr<T> makeShared(Args&&... args) {
  return allocateShared<T>(std::allocator<T>(), std::forward<Args>(args)...);
}
