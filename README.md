# SharedPtr, WeakPtr, EnableSharedFromThis

C++11-style smart pointers with custom deleters and allocators.

## SharedPtr<T>
- **Core Features**:
  - Construct from raw pointer, custom `Deleter`, `Allocator`.
  - Aliasing constructor for casting (e.g., `static_pointer_cast`).
- **Factories**:
  - `makeShared()`: Constructs objects without explicit `new`.
  - `allocateShared()`: Uses custom allocator.

## WeakPtr<T>
- **Methods**:
  - `expired()`: Checks if the managed object exists.
  - `lock()`: Returns `SharedPtr` if object is alive.

## EnableSharedFromThis<T>
- **Usage**:
  - Inherit to enable `shared_from_this()` in class methods.

## Example
```cpp
#include "shared_ptr.h"

class MyObject : public EnableSharedFromThis<MyObject> {};

auto obj = makeShared<MyObject>();
WeakPtr<MyObject> weak = obj;
if (auto ptr = weak.lock()) { /*...*/ }
