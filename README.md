
This is a fork of [Wren](https://github.com/wren-lang/wren) used for the Sock game framework (currently private/WIP).


## Changes

* `Num` trig methods now use turns (0..1) instead of radians (0..2π).


## Additions

* Added `step` field to `Range`.
  
  Set using `..` operator on a `Range`.
  
  ```wren
  (3..9).step        // 1
  (3..9..2).step     // 2
  (3..9..2).toList   // [3, 5, 7, 9]
  (9..3..2).toList   // [9, 7, 5, 3]
  3..9..0            // error, step must be positive
  3..9..-1           // error, step must be positive
  "fling"[0..-1..2]  // "fig"
  ```

* Added `Sequence` static methods:
  | Method | Description |
  | -- | -- |
  | `Sequence.empty` | Get an empty sequence (singleton) |
  | `Sequence.of(value)` | Get a sequence that contains just `value` |
  | `Sequence.of(value, count)` | Get a sequence that contains `value` repeated `count` times |

  > These use two new undocumented classes `EmptySequence` and `SingleSequence`.

* Added `Num` methods:
  | Method | Description |
  | -- | -- |
  | `num.tri` | Triangle wave function, repeats 0..1. |
  | `num.pulse` | Pulse wave function, repeats 0..1. |
  | `num.smoothstep` | Smoothstep function 0..1 |
  | `num.repeat` | Wrap `num` on the range 0..1 |
  | `num.repeat(to)` | Wrap `num` on the range 0..`to` |
  | `num.repeat(from, to)` | Wrap `num` on the range `from`..`to` |
  | `num.lerp(to, t)` | Linear interporlate between `num` and `to` using `t` |
  | `num.inverseLerp(to, target)` | Get `t` value such that `num.lerp(to, t) == target` |
  | `num.map(a1, b1, a2, b2)` | Linear remap `num` from range `a1`..`b1` to `a2`..`b2`. |

* Added `String` methods:
  | Method | Description |
  | -- | -- |
  | `string.byteCount` | Equivalent to `string.bytes.count`, previously `string.byteCount_` |
  | `string.byteAt(i)` | Equivalent to `string.bytes[i]`, previously `string.byteAt_(i)` |

* Added `Meta` static methods:
  | Method | Description |
  | -- | -- |
  | `Meta.module` | Get the current module name. |
  | `Meta.module(count)` | Get the name of the module `count` stack frames up (0 or more).<br>Returns `null` if `count` is too far up. |

* For the `Random` class:
  * Added a static singleton `Random.default`.
  * Added equivalent static method for each instance method, which use the `Random.default` singleton.

* Added a C function for implictly importing modules (based on [avivbeeri's implementation](https://github.com/avivbeeri/wren/commit/522a20a77138330a17df060afa29f9f44f614f97)).
  
  ```c
  void wrenAddImplicitImportModule(WrenVM* vm, const char* module);
  ```

* Added a C function to check if object is an instance of a class.
  
  ```c
  bool wrenGetSlotIsInstanceOf(WrenVM* vm, int slot, int classSlot);
  ```

* Added capability to ensure `wrenGargabeCollect` does not re-enter (e.g. if releasing handle in finalizer), [pulled from here](https://github.com/wren-lang/wren/pull/1076).

* Added [ChayimFriedman2's](https://github.com/wren-lang/wren/pull/911) Allocation/GC optimization for foreign classes.
