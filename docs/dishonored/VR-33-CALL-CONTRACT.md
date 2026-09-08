# VR-33 step 1b.1: the ProcessEvent call contract, for review before implementation

**Nothing here is implemented.** This is the proposal for how the pose report
would call UE3 functions, put up for review because it crosses a line this
codebase has never crossed.

---

## 1. Why this needs review at all

**Every engine interaction in this mod to date is a memory READ or a field
write.** A grep for a UE3 function call across `src/game/dishonored/` returns
nothing. `ProcessEvent` is hooked - a five-byte patch at `kProcessEvent`
(0x00470640) trampolining to `PeHandler` - but the hook only *observes* calls
the game makes. The mod has never made one.

Step 1b needs `MatchRefBone`, `GetBoneName`, `GetParentBone` and
`BoneIsChildOf`. Those are UnrealScript-declared functions, and reaching them
means constructing a parameter frame and calling into the engine. The failure
mode is not a wrong number in a log: a malformed frame corrupts the stack or
the engine's own state, and it will crash rather than refuse. Every other
instrument in this project can be wrong safely. This one cannot.

So the contract goes to review before the code exists.

## 2. What is already known, and what is missing

**Known:**

* `kProcessEvent = 0x00470640`, byte-verified at hook time; the exe has no
  ASLR.
* The hook's own signature: `PeHandler(void* obj, void* a1, void* a2, void* a3)`
  reached from a `__thiscall` prologue, so `this` is the object and the first
  stack argument is the `UFunction*`.
* `FindPropOffset` works and is proven across the whole codebase.
* The pose report has the objects it would call on: the pawn's
  `SkeletalMeshComponent`, resolved on the script lane.

**Missing, and each one is a way to crash:**

1. **The exact `ProcessEvent` signature.** UE3's is conventionally
   `void UObject::ProcessEvent(UFunction* Function, void* Parms, void* Result)`
   as a `__thiscall`. The hook's four-argument handler is consistent with that
   plus the return address, but the mod has only ever read those arguments -
   it has never had to get the call convention, argument order or stack cleanup
   right in the outbound direction.
2. **The parameter frame layout.** UnrealScript parameters are packed into a
   `Parms` block by property offset, not by C struct rules. `GetParentBone(int
   BoneIndex) -> int` is not necessarily `struct { int in; int out; }`. The
   layout must come from the `UFunction`'s own property chain, walked by
   reflection, rather than from a hand-written struct.
3. **`FindFunctionObj` does not check the owner.** It returns the FIRST
   `UFunction` in `GObjects` with a matching name (`ue3/uobject.cpp:182-205`).
   `GetBoneName` almost certainly exists on more than one class, and calling
   the wrong class's function on a `SkeletalMeshComponent` is exactly the shape
   of a crash.
4. **FName handling.** `MatchRefBone` takes a name. An `FName` is an index plus
   a NUMBER component, and this codebase has so far only ever compared the
   index. Passing a name with the wrong number is a silent mismatch at best.
5. **Return value location.** Whether the result lands in the `Parms` block at
   the return property's offset, or via the separate `Result` pointer, differs
   by how the function is invoked. Reading the wrong one yields a plausible
   integer.

## 3. The proposed contract

Stated so a reviewer can reject any single element.

### 3.1 Resolve the function on the owning class, not by name alone

Add an owner-checked lookup rather than changing `FindFunctionObj`, which other
code depends on. Walk the class's own `Children` chain from the component's
class up through its supers, matching both the name and the fact that the outer
is a class in that hierarchy. A function found on an unrelated class is a
REFUSAL, not a fallback.

**Gate:** the resolved `UFunction`'s outer chain is printed and is one of the
classes the object actually derives from.

### 3.2 Derive the parameter frame from the function's own properties

Walk the `UFunction`'s property list. For each property record its offset, its
size, and its flags - specifically parm, out-parm, return-parm. Build the frame
from those offsets into a zeroed buffer sized by the function's `ParmsSize`.

Refuse and log if:

* any parameter is not a type this code knows how to place (int, name, bool,
  float, and out-int for the queries above);
* the computed extent exceeds `ParmsSize`;
* the function takes a dynamic array or a struct by value.

That last one is what keeps `GetBoneNames(out array<name>)` out of scope for
now: the engine would allocate into a caller-owned array and the mod's CRT
would have to free it. It is explicitly not attempted.

**Gate:** for each of the four functions, the derived frame layout is logged -
offsets, sizes, flags, total size - and compared against the declaration in
`tools/uscript/dishonored/Engine/SkeletalMeshComponent.uc` before any call is
made. A disagreement stops the step.

### 3.3 Make the first call the smallest possible one

`GetParentBone(int) -> int` is the candidate: two integers, no names, no
structs, and a result whose plausibility can be checked immediately. The first
call is made on a bone index the report already knows is valid, and the answer
must satisfy `parent < index` for a UE3 reference skeleton and terminate at a
root. **A single call whose result fails that check stops the step** rather
than proceeding to the name-taking functions.

Only after that: `GetBoneName(int) -> name`, then `MatchRefBone(name) -> int`
round-tripped against it, then `BoneIsChildOf`.

**Gate:** every index round-trips - `MatchRefBone(GetBoneName(i)) == i` - across
the whole skeleton, or the mismatches are reported individually.

### 3.4 Lane, re-entrancy and blast radius

* Called only from `PeHandler`'s own script-lane pass, where the objects are
  coherent - the lane the report now runs on.
* A recursion guard: our call re-enters `ProcessEvent` and therefore re-enters
  `PeHandler`. That guard is mandatory and is the second most likely way to
  hang the game after a bad frame.
* Calls happen **once**, on resolution, not per tick. The results are cached
  into an immutable table that the rest of the mod reads.
* Default OFF behind its own ini key, separate from `PoseReport`, so the
  read-only report keeps working if the call path is disabled.
* Wrapped in a structured-exception guard so a fault produces a log line and a
  permanent stand-down rather than a crash dump, with the intent that it must
  never be reached.

## 4. The alternative, if this is judged too risky

The information wanted from these four calls is bone names, parent indices and
ancestry. There is a read-only route to the same answers: decode
`SkeletalMesh.RefSkeleton` (+0xEC, already resolved) directly.

The previous review rejected *leading* with that, correctly, because the stride
and element layout would be guessed. But it is not guessed if it is derived:
the array header gives a count and capacity, and a candidate stride is
confirmed rather than assumed when the name index of every element resolves to
a plausible FName **and** every parent index is less than its own index **and**
the chain terminates at a single root. Three independent constraints that
random data does not satisfy together.

That route cannot crash the game. It reads memory the mod already reads
elsewhere. Its weakness is that a validated-looking stride could still be
wrong in a way all three checks tolerate, and it gives no access to live
composed transforms - only the reference pose.

**A defensible order might therefore be the reverse of what was proposed:**
derive `RefSkeleton` read-only first, since it answers the ancestry question -
the one that decides the whole architecture - at zero risk. Then use the
ProcessEvent calls only for what the reference pose genuinely cannot give,
which is the live animated transforms, and by then the bone table is known
independently so a wrong call result is detectable rather than believed.

## 5. Questions for the reviewer

1. Is the ProcessEvent contract in section 3 sufficient, or does it miss a
   failure mode - particularly around stack cleanup, the `Result` pointer, or
   re-entrancy through our own hook?
2. Is section 4's argument right that `RefSkeleton` decoding should come FIRST
   because it is zero-risk and answers the architectural question, with the
   calls reserved for live transforms? That inverts the previous review's
   ordering, so it needs explicit agreement or rejection.
3. Is the three-constraint validation in section 4 (plausible FNames, parent <
   index, single root) strong enough to call a stride derived rather than
   guessed?
4. Is `GetParentBone` the right first call, or is there a safer one - something
   with no parameters at all whose return is independently checkable?

## 6. State

Branch `claude/vr-33-hands-and-weapons-at-the-controllers`. The pose report is
on the script lane, retries until it succeeds, reports unknowns as unknown, and
prints full socket frames. No placement code is active and no UE3 function has
been called.
