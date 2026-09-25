#!/usr/bin/env python3
# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
"""Check that the compiled slot buffers keep the order the models ask for.

test/model/fourslot.py and threeslot.py (explore_weak) find where a writer on one core
and a reader on the other need a DMB. The kernels put _OSMemoryBarrier() there, but what
runs is what the compiler emitted: this reads the disassembly of a kernel object and
checks, on every path through each function, that the accesses the models name come in
their order with a DMB between each and the next:

    4-slot writer   last data byte, DMB, Index[pair], DMB, Latest
    4-slot reader   Latest, DMB, Reading, DMB, Index[pair]
    3-slot writer   last data byte, DMB, Latest, DMB, LL(Reading)
    3-slot reader   DMB, Reading = 3, DMB, LL(Reading)

The reader's first barrier stands for "after the previous copy": the one of the 4-slot
reader after Latest, the one of the 3-slot reader before Reading = 3.

The status of the buffer, which says that a slot has been handed over at all, is not in
the models; the writer sets it after the slot, and where a path makes both accesses:

    writer          Latest (4-slot) or LL(Reading) (3-slot), DMB, Status

The accesses are recognised by their offsets in the buffer structures, followed through
the registers from the descriptor each function receives in r0: descriptor->Buffer, the
buffer's CurrentWriter, and its Data. That covers the code GCC emits for the Cortex-M0+
and the Cortex-M33 at -O2; an access the tracking cannot place is not an error, a missing
or misplaced one is.

The same objects hold sequences of stores that a task makes and the timer interrupt may
find half done, which it completes or relies on. There the processor keeps the order,
one core, but the compiler need not: GCC stored _OSNoSaveContext before a task's zombie
state, and dropped the mark of a promotion it saw overwritten (method.md). On every path,
in compiled order, with no second occurrence of a later store before the one checked:

    OSEndTask            TaskState |= ZOMBIE, _OSNoSaveContext = TRUE, head->Next
    ScheduleNextTask     soft kernel, deadline-monotonic: TaskState |= ZOMBIE, head->Next
                         soft kernel, EDF, a promotion: TaskState |= ACTIVATE, head->Next,
                         tail->Next, task->Next, TaskState = INIT; a drop: tail->Next,
                         TaskState |= ZOMBIE

The variables are recognised by the relocations of the literals that hold their
addresses, the TCBs as what _OSActiveTask, _OSQueueHead and _OSQueueTail point to, and
the values stored from the constants moved or or-ed into a register.

    tools/check_order.py [--tasks] build/EscapementHard.o [...]

--tasks checks the task-level stores only: the Cortex-M4 of the STM32F4 has one core and
no barrier in its buffers. An object without a kernel, one the build did not select, is
skipped; at least one must hold one.
"""
import re
import subprocess
import sys

OBJDUMP = "arm-none-eabi-objdump"
FUNCTIONS = ("OSWriteBuffer", "OSGetCopyBuffer", "OSGetReferenceBuffer",
             "GetReadyBuffer3Slot", "GetReadyBuffer4Slot")
# Offsets in BUFFER_4_SLOT and BUFFER_3_SLOT (EscapementHard.c), on a 32-bit target.
READING4, LATEST4, INDEX4 = 52, 53, 56
READING3, LATEST3 = 40, 41
STATUS = 5                          # in BUFFER_DESCRIPTOR
BUFFER = ("d", 0)                   # descriptor->Buffer
DATA = ("d", 0, 0, 0)               # Buffer->CurrentWriter->Data
MAX_PATHS = 20000
TASK_FUNCTIONS = ("OSEndTask", "OSSuspendSynchronousTask", "ScheduleNextTask")
STATE, ZOMBIE, ACTIVATE = 8, 0x02, 0x10     # TaskState in the TCB, and its bits


def disassemble(path):
    """Yield (function, [(address, mnemonic, operands, target)]), the target a call's
    callee, or ("literal", variable) for a load of the address of a variable."""
    out = subprocess.run([OBJDUMP, "-dr", "--no-show-raw-insn", path], check=True,
                         capture_output=True, text=True).stdout
    functions, literals, name, body = {}, {}, None, None
    for line in out.splitlines():
        m = re.match(r"^[0-9a-f]+ <([^>]+)>:$", line)
        if m:
            name = m.group(1)
            body = functions.setdefault(name, [])
            literals[name] = {}
            continue
        if body is None:
            continue
        m = re.match(r"^\s+([0-9a-f]+):\s+R_ARM_THM_(?:CALL|JUMP24)\s+(\S+)", line)
        if m and body:
            body[-1] = body[-1][:3] + (m.group(2),)
            continue
        # A literal holding the address of a variable: with -fdata-sections a static
        # one is named by its section, .bss.Name or .data.Name.
        m = re.match(r"^\s+([0-9a-f]+):\s+R_ARM_ABS32\s+(\S+)", line)
        if m:
            literals[name][int(m.group(1), 16)] = re.sub(r"^\.(bss|data)\.", "",
                                                        m.group(2))
            continue
        m = re.match(r"^\s+([0-9a-f]+):\s+(\S+)\s*(.*)$", line)
        if m:
            operands = m.group(3).split(";")[0].split("@")[0].strip()
            target = None
            t = re.search(r"<([^>+]+)(\+0x[0-9a-f]+)?>", operands)
            if m.group(2).startswith("bl") and t and not t.group(2):
                target = t.group(1)
            lit = re.search(r"@ \(?([0-9a-f]+) <", m.group(3))
            if m.group(2).startswith("ldr") and "[pc" in operands and lit:
                target = ("literal", int(lit.group(1), 16))
            body.append((int(m.group(1), 16), m.group(2), operands, target))
    # The literals are listed after the code that loads them: resolve them now.
    for name, body in functions.items():
        for i, insn in enumerate(body):
            if isinstance(insn[3], tuple):
                body[i] = insn[:3] + (("literal", literals[name].get(insn[3][1])),)
    return functions


def number(text):
    text = text.lstrip("#")
    return int(text, 16) if text.startswith(("0x", "-0x")) else int(text)


class Value:
    """What a register holds: a constant, or a place reached from the descriptor plus a
    known offset, and whether some unknown amount was added (an index)."""
    __slots__ = ("const", "base", "off", "var", "bits")

    def __init__(self, const=None, base=None, off=0, var=False, bits=None):
        self.const, self.base, self.off, self.var = const, base, off, var
        self.bits = bits            # set by an or of this constant into an unknown value

    def plus(self, other):
        """The sum; a place plus an unknown amount stays that place, indexed."""
        if other is None:
            return Value(base=self.base, off=self.off, var=True) if self.base else None
        if self.const is not None and other.const is not None:
            return Value(const=self.const + other.const)
        if self.base is not None and other.const is not None:
            return Value(base=self.base, off=self.off + other.const, var=self.var)
        if other.base is not None and self.const is not None:
            return other.plus(self)
        if self.base is not None:
            return Value(base=self.base, off=self.off, var=True)
        if other.base is not None:
            return Value(base=other.base, off=other.off, var=True)
        return None


def operand_value(regs, text):
    text = text.strip()
    if text.startswith("#"):
        return Value(const=number(text))
    return regs.get(text)


def address(regs, text):
    """The effective address of [rn], [rn, #imm], [rn, rm], with writeback noted."""
    m = re.match(r"\[([^\]]+)\](!?)(?:,\s*(#-?\w+))?", text)
    parts = [p.strip() for p in m.group(1).split(",")]
    base = regs.get(parts[0])
    if len(parts) > 1 and m.group(3) is None:
        base = base.plus(operand_value(regs, parts[1])) if base else None
    post = m.group(3)
    writeback = (parts[0], base) if m.group(2) else None
    if post:
        writeback = (parts[0], base.plus(Value(const=number(post))) if base else None)
    return base, writeback


def event_of(kind, addr):
    """Name the access if it is one the models care about."""
    if addr is None or addr.base is None:
        return None
    if addr.base == BUFFER and not addr.var:
        return {("load", LATEST4): "Latest4 load", ("store", READING4): "Reading4 store",
                ("store", LATEST4): "Latest4 store", ("store", LATEST3): "Latest3 store",
                ("store", READING3): "Reading3 store"}.get((kind, addr.off))
    if addr.base == BUFFER and addr.var and addr.off == INDEX4:
        return {"load": "Index4 load", "store": "Index4 store"}[kind]
    if addr.base == DATA and kind == "store":
        return "data store"
    if addr.base == ("d",) and not addr.var and addr.off == STATUS and kind == "store":
        return "Status store"
    return None


def task_event(addr, value):
    """Name a store to a variable, or to what one of them points to, with the value
    stored: "_OSNoSaveContext+0=1", "*_OSActiveTask+8|2", "*_OSQueueHead+0?"."""
    if addr is None or addr.base is None or addr.base[0] != "S" or addr.var:
        return None
    if len(addr.base) == 2:
        place = f"{addr.base[1]}+{addr.off}"
    elif len(addr.base) == 3 and addr.base[2] == 0:
        place = f"*{addr.base[1]}+{addr.off}"
    else:
        return None
    if value is not None and value.const is not None:
        return f"{place}={value.const}"
    if value is not None and value.bits is not None:
        return f"{place}|{value.bits}"
    return f"{place}?"


def state_bits(bit):
    return lambda e: bool(re.match(r"\*_OSActiveTask\+%d\|(\d+)$" % STATE, e)) and \
        int(e.rsplit("|", 1)[1]) & bit != 0


def store_to(place, value=None):
    if value is None:
        return lambda e: e.startswith(place) and e[len(place):len(place) + 1] in "=|?"
    return lambda e: e == f"{place}={value}"


ZOMBIE_STORE = ("TaskState |= ZOMBIE", state_bits(ZOMBIE))
ACTIVATE_STORE = ("TaskState |= ACTIVATE", state_bits(ACTIVATE))
INIT_STORE = ("TaskState = INIT", store_to(f"*_OSActiveTask+{STATE}", 0))
NOSAVE_STORE = ("_OSNoSaveContext = TRUE", store_to("_OSNoSaveContext+0", 1))
HEAD_STORE = ("head->Next", store_to("*_OSQueueHead+0"))
TAIL_STORE = ("tail->Next", store_to("*_OSQueueTail+0"))
NEXT_STORE = ("task->Next", store_to("*_OSActiveTask+0"))

# (functions, sequence, first only): on each path, each store of the last kind comes
# after the others in their order, with no second store of a later kind in between.
TASK_RULES = {
    "task end": (("OSEndTask",), (ZOMBIE_STORE, NOSAVE_STORE, HEAD_STORE), True),
    "drop, deadline-monotonic": (("ScheduleNextTask",), (ZOMBIE_STORE, HEAD_STORE), False),
    "promotion": (("ScheduleNextTask",),
                  (ACTIVATE_STORE, HEAD_STORE, TAIL_STORE, NEXT_STORE, INIT_STORE), False),
    "drop, EDF": (("ScheduleNextTask",), (TAIL_STORE, ZOMBIE_STORE), False),
}


def check_sequence(events, sequence, first_only):
    """Returns an error, None if fine, or "absent" if the path never makes the last
    store of the sequence."""
    last = sequence[-1][1]
    found = False
    for k, e in enumerate(events):
        if not last(e):
            continue
        found = True
        later, end = sequence[-1], k
        for name, match in reversed(sequence[:-1]):
            j = next((i for i in range(end - 1, -1, -1)
                      if match(events[i]) or later[1](events[i])), None)
            if j is None or not match(events[j]):
                return f"{later[0]} with no {name} before it"
            later, end = (name, match), j
        if first_only:
            break
    return None if found else "absent"


def task_rules(functions):
    """The rules that apply to this kernel, as the object shows it."""
    events = " ".join(" ".join(p) for name in TASK_FUNCTIONS if name in functions
                      for p in paths(functions[name], {}, 1))
    rules = ["task end"]
    if "ScheduleNextTask" in functions and "|%d" % ZOMBIE in events:
        rules += ["promotion", "drop, EDF"] if "*_OSQueueTail+0" in events else \
            ["drop, deadline-monotonic"]
    return rules


def check_task_order(path, functions):
    errors, covered = [], set()
    rules = task_rules(functions)
    for name in TASK_FUNCTIONS:
        if name not in functions:
            continue
        # Once through each loop: every iteration is a path of its own from the start.
        for events in paths(functions[name], {}, 1):
            for rule_name in rules:
                where, sequence, first_only = TASK_RULES[rule_name]
                if name not in where:
                    continue
                result = check_sequence(events, sequence, first_only)
                if result == "absent":
                    continue
                covered.add(rule_name)
                if result:
                    errors.append(f"{path}: {name}, {rule_name}: {result}")
    for rule_name in rules:
        if rule_name not in covered:
            errors.append(f"{path}: the {rule_name} was not found")
    return errors


def step(insn, regs):
    """Apply one instruction to the register state; return the events it makes."""
    _, op, args, target = insn
    op = op.split(".")[0]
    events = []
    parts = [a.strip() for a in re.split(r",\s*(?![^\[]*\])", args)] if args else []
    if op == "dmb":
        return ["DMB"]
    if op in ("bl", "blx"):
        r0 = regs.get("r0")
        if target == "OSUINT8_LL" and r0 and r0.base == BUFFER and r0.off == READING3 \
                and not r0.var:
            events.append("LL Reading3")

        for r in ("r0", "r1", "r2", "r3", "r12", "lr"):
            regs.pop(r, None)
        return events
    if op == "ldr" and isinstance(target, tuple):
        if target[1]:
            regs[parts[0]] = Value(base=("S", target[1]))   # the address of a variable
        else:
            regs.pop(parts[0], None)
        return events
    if op in ("orr", "orrs") and len(parts) in (2, 3):
        v = operand_value(regs, parts[-1])
        if v is not None and v.const is not None:
            regs[parts[0]] = Value(bits=v.const)
        else:
            regs.pop(parts[0], None)
        return events
    m = re.match(r"(ldr|str)(b|h|sb|sh|d|ex|exb|exh)?$", op)
    # A register spilled to the stack and loaded back keeps what it held.
    spill = re.match(r"\[sp(?:,\s*#(\d+))?\]$", args[args.index("["):]) if m and parts \
        and "[" in args else None
    if spill and m.group(2) is None:
        slot = "sp+" + (spill.group(1) or "0")
        if m.group(1) == "str":
            if regs.get(parts[0]) is not None:
                regs[slot] = regs[parts[0]]
            else:
                regs.pop(slot, None)
        elif regs.get(slot) is not None:
            regs[parts[0]] = regs[slot]
        else:
            regs.pop(parts[0], None)
        return events
    if m and parts and "[" in args:
        addr, writeback = address(regs, args[args.index("["):])
        kind = "load" if m.group(1) == "ldr" else "store"
        if m.group(2) == "exb" and kind == "load" and addr and addr.base == BUFFER \
                and addr.off == READING3:
            events.append("LL Reading3")
        else:
            e = event_of(kind, addr)
            if e:
                events.append(e)
            if kind == "store":
                e = task_event(addr, regs.get(parts[0]))
                if e:
                    events.append(e)
                # The register now holds what the variable holds.
                if addr and addr.base and addr.base[0] == "S" and len(addr.base) == 2 \
                        and addr.off == 0 and m.group(2) is None:
                    regs[parts[0]] = Value(base=addr.base + (0,))
        if kind == "load":
            if m.group(2) is None and addr and addr.base is not None and not addr.var:
                regs[parts[0]] = Value(base=addr.base + (addr.off,))
            else:
                regs.pop(parts[0], None)
        if writeback:
            regs[writeback[0]] = writeback[1]
        return events
    if op in ("mov", "movs", "movw") and len(parts) == 2:
        v = operand_value(regs, parts[1])
        regs[parts[0]] = v
        if v is None:
            regs.pop(parts[0])
        return events
    if op in ("add", "adds", "addw", "sub", "subs", "subw") and len(parts) in (2, 3):
        a, b = (parts[0], parts[1]) if len(parts) == 2 else (parts[1], parts[2])
        va, vb = regs.get(a), operand_value(regs, b)
        if op.startswith("sub"):
            vb = Value(const=-vb.const) if vb and vb.const is not None else None
        if va is None and vb is not None and vb.base is not None and not op.startswith("sub"):
            va, vb = vb, None
        v = va.plus(vb) if va else None
        if v is None:
            regs.pop(parts[0], None)
        else:
            regs[parts[0]] = v
        return events
    if op == "cmp" and len(parts) == 2 and parts[1].startswith("#"):
        regs["compared"] = (parts[0], number(parts[1]))   # for the branch that follows
        return events
    if op in ("push", "pop", "stmdb", "ldmia") or (parts and parts[0] == "sp"):
        for k in [k for k in regs if k.startswith("sp+")]:
            del regs[k]
    if op in ("push", "pop", "cmp", "cmn", "tst", "nop") or op.startswith("b") or \
            op.startswith("it") or op.startswith("cb"):
        return events
    if parts:
        regs.pop(parts[0], None)
    return events


def paths(body, start=None, visits=2):
    """Every path through the function, as its list of events, each instruction taken
    at most visits times. The registers start as given, by default r0 the
    descriptor."""
    index = {a: i for i, (a, *_) in enumerate(body)}
    start = {"r0": Value(base=("d",))} if start is None else start
    result, stack = [], [(0, dict(start), [], {})]
    while stack:
        if len(result) > MAX_PATHS:
            raise SystemExit(f"more than {MAX_PATHS} paths")
        i, regs, events, seen = stack.pop()
        while i < len(body):
            seen[i] = seen.get(i, 0) + 1
            if seen[i] > visits:
                break
            insn = body[i]
            regs.pop("compared", None) if i and body[i - 1][1] != "cmp" else None
            events = events + step(insn, regs)
            op = insn[1].split(".")[0]
            t = re.search(r"^([0-9a-f]+) <", insn[2])
            branch = op.startswith("b") and op not in ("bl", "blx", "bic", "bics") and t
            if op in ("pop",) and "pc" in insn[2] or op == "bx":
                i = len(body)
                break
            if branch or op.startswith("cb"):
                t = re.search(r"([0-9a-f]+) <", insn[2])
                j = index.get(int(t.group(1), 16))
                conditional = op not in ("b",)
                # What the branch tells of a register: equal to what it was compared
                # with on one side (cmp then beq or bne; cbz, cbnz against zero).
                taken, fallthrough = dict(regs), regs
                known = regs.get("compared")
                if op.startswith("cb"):
                    known = (insn[2].split(",")[0].strip(), 0)
                if known and op in ("beq", "cbz"):
                    taken[known[0]] = Value(const=known[1])
                elif known and op in ("bne", "cbnz"):
                    fallthrough[known[0]] = Value(const=known[1])
                if j is not None:
                    if conditional:
                        stack.append((j, taken, list(events), dict(seen)))
                    else:
                        i = j
                        continue
                elif not conditional:
                    i = len(body)
                    break
            i += 1
        result.append(events)
    return result


RULES = {
    "4-slot writer": ("data store", "Index4 store", "Latest4 store"),
    "4-slot reader": ("Latest4 load", "Reading4 store", "Index4 load"),
    "3-slot writer": ("data store", "Latest3 store", "LL Reading3"),
    "3-slot reader": (None, "Reading3 store", "LL Reading3"),
}
# Pairs, checked only on the paths that make both: (earlier accesses, later accesses).
PAIRS = {
    "4-slot writer, status": (("Latest4 store",), ("Status store",)),
    "3-slot writer, status": (("LL Reading3",), ("Status store",)),
}


def check_pair(events, pair):
    """On one path: a DMB between the last earlier access and each later one after it.
    Returns an error, None if fine, or "absent" if the path does not make both."""
    earlier, later = pair
    found = False
    for k, e in enumerate(events):
        if e not in later:
            continue
        j = max((i for i in range(k) if events[i] in earlier), default=None)
        if j is None:
            continue
        found = True
        if "DMB" not in events[j + 1:k]:
            return f"no DMB between {events[j]} and {e}"
    return None if found else "absent"


def check_path(events, rule):
    """On one path: before each occurrence of the rule's last access, the ones before it
    in their order, a DMB between each and the next. None stands for the start of the
    path. Returns an error, or None if fine, or "absent" if the path never makes the last
    access."""
    first, second, last = rule
    found = False
    for k, e in enumerate(events):
        if e != last:
            continue
        found = True
        j = max((i for i in range(k) if events[i] == second), default=None)
        if j is None:
            return f"{last} with no {second} before it"
        if "DMB" not in events[j + 1:k]:
            return f"no DMB between {second} and {last}"
        if first is None:
            if "DMB" not in events[:j]:
                return f"no DMB before {second}"
            continue
        i = max((i for i in range(j) if events[i] == first), default=None)
        if i is None:
            # The writer's last data byte may have been written by an earlier call.
            if first != "data store":
                return f"{second} with no {first} before it"
            if "DMB" not in events[:j]:
                return f"no DMB before {second}"
        elif "DMB" not in events[i + 1:j]:
            return f"no DMB between {first} and {second}"
    return None if found else "absent"


def check(path, tasks_only=False):
    functions = disassemble(path)
    if "OSEndTask" not in functions:
        return None       # a kernel the build did not select compiles to nothing
    if tasks_only:
        return sorted(set(check_task_order(path, functions)))
    present = [f for f in FUNCTIONS if f in functions]
    errors, covered = [], set()
    for name in present:
        for events in paths(functions[name]):
            for rule_name, rule in list(RULES.items()) + list(PAIRS.items()):
                if ("writer" in rule_name) != (name == "OSWriteBuffer"):
                    continue
                result = check_pair(events, rule) if rule_name in PAIRS else \
                    check_path(events, rule)
                if result == "absent":
                    continue
                covered.add(rule_name)
                if result:
                    errors.append(f"{path}: {name}, {rule_name}: {result}")
    for rule_name in list(RULES) + list(PAIRS):
        if rule_name not in covered:
            errors.append(f"{path}: the {rule_name} was not found")
    errors += check_task_order(path, functions)
    return sorted(set(errors))


def main():
    args = sys.argv[1:]
    tasks_only = "--tasks" in args
    args = [a for a in args if a != "--tasks"]
    if not args:
        raise SystemExit(__doc__)
    errors, checked = [], 0
    for path in args:
        found = check(path, tasks_only)
        if found is None:
            print(f"{path}: no kernel, skipped")
            continue
        checked += 1
        errors += found
        print(f"{path}: {'order kept' if not found else 'ORDER BROKEN'}")
    for e in errors:
        print("  " + e)
    if not checked:
        print("no object holds a kernel")
    sys.exit(1 if errors or not checked else 0)


if __name__ == "__main__":
    main()
