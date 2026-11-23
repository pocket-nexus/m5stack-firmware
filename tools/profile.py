#!/usr/bin/env python3
"""Host side of the frame profiler (CONFIG_M5_POCKET_APP_PROFILE).

Run with the ESP-IDF Python environment (pyserial, the target's addr2line):

  profile.py run PORT LOG [--reset] STEP...
      Capture the device log into LOG and run console steps:
        "5s"      wait 5 seconds
        "@name"   start a section (a marker line "### name" in the log)
        other     one console line, see platforms/esp-idf/m5_pocket_app/
                  src/profile_tools.c (t, u, p, w, k, r, c)
  profile.py table LOG [--slow]
      Average and peak stage times per section, in milliseconds.
  profile.py symbols LOG ELF [--capture K] [--top N] [--stage NAME]...
      Resolve a program counter capture ("c <ms>") against the firmware ELF.
"""
import re
import subprocess
import sys
import threading
import time
from collections import Counter, OrderedDict, defaultdict

STAGES = ["pre", "input", "bridge", "translate", "appin", "model", "view", "reqs",
          "draw", "damage", "raster", "flush", "wait", "post"]


# ---------------------------------- run ------------------------------------

def run(port, out, steps):
    import serial

    reset = "--reset" in steps
    steps = [s for s in steps if s != "--reset"]
    # USB Serial/JTAG resets the chip while RTS is set and DTR is clear. The
    # OS sets both when the port opens and pyserial then applies DTR before
    # RTS: keep DTR set until RTS is clear.
    ser = serial.Serial()
    ser.port = port
    ser.baudrate = 115200
    ser.timeout = 0.05
    ser.dtr = True
    ser.rts = False
    ser.open()
    ser.dtr = False
    if reset:
        ser.rts = True
        time.sleep(0.2)
        ser.rts = False

    lock = threading.Lock()
    log = open(out, "w", buffering=1)
    start = time.time()
    stop = threading.Event()

    def stamp():
        return f"{time.time() - start:8.3f} "

    def reader():
        pending = b""
        while not stop.is_set():
            try:
                data = ser.read(4096)
            except Exception as error:  # the port went away: reset or crash
                with lock:
                    log.write(stamp() + f"### serial error {error}\n")
                time.sleep(0.5)
                continue
            if not data:
                continue
            pending += data
            *lines, pending = pending.split(b"\n")
            with lock:
                for line in lines:
                    log.write(stamp() + line.decode("utf-8", "replace").rstrip("\r") + "\n")

    thread = threading.Thread(target=reader, daemon=True)
    thread.start()
    for step in steps:
        if re.fullmatch(r"\d+(\.\d+)?s", step):
            time.sleep(float(step[:-1]))
        elif step.startswith("@"):
            with lock:
                log.write(stamp() + "### " + step[1:] + "\n")
        else:
            with lock:
                log.write(stamp() + "### > " + step + "\n")
            ser.write((step + "\n").encode())
            ser.flush()
    stop.set()
    thread.join(1)
    # Leave the lines as they are: clearing them in the other order resets.
    ser.close()
    log.close()


# --------------------------------- table -----------------------------------

def fields(text):
    return dict(re.findall(r"(\w+)=([\d./]+)", text))


def sections_of(path):
    sections, slow, notes = OrderedDict(), [], defaultdict(list)
    name = "start"
    need = ["fps", "frames", "skipped", "slow", "tick", "hook", "rows", "strips",
            "allocs", "largeB"] + STAGES
    for line in open(path, errors="replace"):
        marker = re.match(r"\s*[\d.]+ ### (?!>)(.*)", line)
        if marker:
            name = marker.group(1).strip()
        elif "m5_prof: P " in line:
            row = fields(line.split("m5_prof: P ", 1)[1])
            # A line cut by a reset or mixed with a dump is dropped.
            if all(k in row for k in need) and all("/" in row[k] for k in ["tick", "hook"] + STAGES):
                sections.setdefault(name, []).append(row)
        elif "m5_prof: S " in line:
            slow.append((name, line.split("m5_prof: S ", 1)[1].strip()))
        elif "m5_prof: N " in line:
            notes[name].append(line.split("m5_prof: N ", 1)[1].strip())
    return sections, slow, notes


def table(path, show_slow):
    sections, slow, notes = sections_of(path)

    def average(rows, key):
        total = frames = peak = 0
        for row in rows:
            n = int(row["frames"])
            mean, _, top = row[key].partition("/")
            total += float(mean) * n
            frames += n
            peak = max(peak, int(top or 0))
        return (total / frames if frames else 0), peak

    columns = ["fps", "tick", "hook"] + STAGES + ["rows/f", "allocs/f", "largeB/f"]
    print(f"{'section':26s} " + " ".join(f"{c:>8s}" for c in columns))
    for name, rows in sections.items():
        frames = sum(int(r["frames"]) for r in rows)
        if not frames:
            continue
        span = sum(int(r["frames"]) / float(r["fps"]) for r in rows if float(r["fps"]) > 0)
        cells = [f"{frames / span:8.1f}" if span else f"{'-':>8s}"]
        peaks = [f"{'':>8s}"]
        for key in ["tick", "hook"] + STAGES:
            mean, peak = average(rows, key)
            cells.append(f"{mean / 1000:8.2f}")
            peaks.append(f"{peak / 1000:8.2f}")
        for key in ["rows", "allocs", "largeB"]:
            cells.append(f"{sum(int(r[key]) for r in rows) / frames:8.1f}")
        skipped = sum(int(r["skipped"]) for r in rows)
        slow_frames = sum(int(r["slow"]) for r in rows)
        failed = sum(1 for n in notes.get(name, []) if n.startswith("flush "))
        print(f"{name[:26]:26s} " + " ".join(cells))
        print(f"{'  peak':26s} " + " ".join(peaks)
              + f"  frames={frames} skipped={skipped} slow={slow_frames} flush_failed={failed}")
    if show_slow:
        print()
        for name, text in slow:
            print(f"[{name}] {text}")


# -------------------------------- symbols ----------------------------------

def symbols(path, elf, which, top, only):
    names = STAGES + ["outside"]
    captures, current, header = [], None, None
    for line in open(path, errors="replace"):
        if "m5_prof: C begin" in line:
            current, header = [], line.split("m5_prof: C begin", 1)[1].strip()
        elif "m5_prof: C end" in line and current is not None:
            captures.append((header, current))
            current = None
        elif current is not None:
            m = re.search(r"Z (\d+) ([0-9a-f]{8}) ([0-9a-f]{8}) (\d+)\s*$", line)
            if m:
                current.append((int(m.group(1)), m.group(2), m.group(3), int(m.group(4))))
    if not captures:
        sys.exit("no capture in the log")
    header, rows = captures[which]
    index = which if which >= 0 else len(captures) + which
    print(f"capture {index} of {len(captures)}: {header}")

    tool = "riscv32-esp-elf-addr2line"
    machine = subprocess.run(["file", "-b", elf], capture_output=True, text=True).stdout
    if "Tensilica" in machine or "Xtensa" in machine:
        tool = "xtensa-esp32s3-elf-addr2line"
    addresses = sorted({pc for _, pc, _, _ in rows} | {ra for _, _, ra, _ in rows})
    out = subprocess.run([tool, "-f", "-C", "-e", elf] + ["0x" + a for a in addresses],
                         capture_output=True, text=True, check=True).stdout.splitlines()
    function = {a: out[2 * i] for i, a in enumerate(addresses)}

    def short(name):
        name = re.sub(r"::h[0-9a-f]{16}$", "", name)
        return name if len(name) <= 110 else name[:50] + " ... " + name[-55:]

    total = sum(n for _, _, _, n in rows)
    by_stage = Counter()
    functions = defaultdict(Counter)
    callers = defaultdict(Counter)
    for stage, pc, ra, n in rows:
        by_stage[stage] += n
        functions[stage][function[pc]] += n
        callers[(stage, function[pc])][function[ra]] += n
    print(f"samples on the app task: {total}")
    for stage, n in by_stage.most_common():
        print(f"\n== {names[stage]}: {n} samples, {100 * n / total:.1f}% ==")
        if only and names[stage] not in only:
            continue
        for name, count in functions[stage].most_common(top):
            if count * 200 < n:
                break
            via = "; ".join(f"{short(c)} {k}" for c, k in callers[(stage, name)].most_common(2))
            print(f"  {100 * count / n:5.1f}% {count:6d}  {short(name)}\n{'':18s}<- {via}")


def option(args, name, default):
    if name in args:
        i = args.index(name)
        value = args[i + 1]
        del args[i:i + 2]
        return value
    return default


def main():
    args = sys.argv[1:]
    if len(args) >= 3 and args[0] == "run":
        run(args[1], args[2], args[3:])
    elif len(args) >= 2 and args[0] == "table":
        table(args[1], "--slow" in args)
    elif len(args) >= 3 and args[0] == "symbols":
        rest = args[3:]
        only = []
        while "--stage" in rest:
            only.append(option(rest, "--stage", ""))
        symbols(args[1], args[2], int(option(rest, "--capture", "-1")),
                int(option(rest, "--top", "12")), only)
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
