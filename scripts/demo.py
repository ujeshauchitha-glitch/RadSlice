"""A five-minute tour of RadSlice, for showing it to someone.

Runs four small experiments with the real simulator and prints plain-language results. Nothing here is a
measurement of a real chip: every number comes from the simulation and its hypothetical parameters
(docs/assumptions.md).

    python scripts/demo.py [--quick]

Needs the binaries (cmake --build build) and the test programs (bash scripts/build-workloads.sh).
"""
import argparse
import csv
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXE = ".exe" if os.name == "nt" else ""
BIN = ROOT / "build"
PROGRAM = ROOT / "build" / "workloads" / "chacha20.elf"


def say(text=""):
    print(text, flush=True)


def heading(n, title):
    say()
    say("=" * 78)
    say(f" {n}. {title}")
    say("=" * 78)


def bar(frac, width=40):
    n = round(frac * width)
    return "#" * n + "." * (width - n)


def run(cmd, env=None):
    e = dict(os.environ)
    e.update(env or {})
    return subprocess.run([str(c) for c in cmd], capture_output=True, text=True, env=e)


def part1():
    heading(1, "The same program on four different 'chips'")
    say("A program that encrypts data (ChaCha20) runs on the trusted reference emulator, then on the")
    say("sliced chip at three slice widths. A correct sliced chip must give the same answer; it just takes")
    say("more steps because it handles fewer bits at a time.\n")
    say(f"{'machine':36} {'result':8} {'instructions':>13} {'estimated cycles':>17}")
    rows = [("reference (full 64-bit, no slicing)", {"RADSLICE_ALU": "plain", "RADSLICE_RF": "plain"}),
            ("sliced, 32 bits at a time", {"RADSLICE_K": "32"}),
            ("sliced, 16 bits at a time", {"RADSLICE_K": "16"}),
            ("sliced, 8 bits at a time", {"RADSLICE_K": "8"})]
    base = None
    for name, env in rows:
        out = run([BIN / ("radslice_run" + EXE), PROGRAM], env)
        text = out.stdout + out.stderr
        ok = "PASS" if "PASS" in text else "FAIL"
        stats = {}
        for line in text.splitlines():
            if line.startswith("RADSLICE_STATS"):
                stats = dict(kv.split("=") for kv in line.split()[1:])
        instret = int(stats.get("instret", 0))
        beats = int(stats.get("beats", 0)) if stats.get("alu") == "sliced" else int(stats.get("alu_ops", instret))
        cycles = instret + beats
        base = base or cycles
        say(f"{name:36} {ok:8} {instret:>13,} {cycles:>12,} ({cycles / base:.1f}x)")
    say("\nSame answer every time. The price of using fewer bits at once is time. The reason to do it is that")
    say("a narrow slice is a smaller piece of material, and small pieces are the ones that get made correctly.")


def part2(trials):
    heading(2, "Manufacturing: how many chips come out working?")
    say("In the Nature paper from Fudan, a small block of this material works 71% of the time. Here the")
    say(f"simulator 'manufactures' {trials:,} random chips, each with its own random dead parts, runs the")
    say("self-test at start-up, and swaps in spares where it can.\n")
    say(f"{'spares added':34} {'chips that work':>16}")
    cases = [("no spares at all", 0, 0, False), ("96 spare register slices", 96, 0, False),
             ("128 spare register slices", 128, 0, False),
             ("128 register spares + 2 spare ALUs", 128, 2, True)]
    for name, rf, alu, single in cases:
        cmd = [BIN / ("radslice_yield" + EXE), "--mode", "yield", "--trials", trials, "--seed", 1, "--k", 16,
               "--rf", rf, "--alu", alu]
        if single:
            cmd.append("--alu-single")
        out = run(cmd)
        r = next(csv.DictReader(out.stdout.splitlines()))
        frac = int(r["both_ok"]) / int(r["trials"])
        say(f"{name:34} {100 * frac:>14.1f}%  {bar(frac, 30)}")
    say("\nWithout spares essentially no chip works. Sparing the registers alone is not enough, because the")
    say("arithmetic unit is made of the same unreliable material; sparing both makes most chips work.")
    say("The catch, which the study measures, is that spares and the wiring that switches them in cost a")
    say("lot of extra chip area.")


def part3(runs):
    heading(3, "Space: what radiation does, and what the protections buy")
    say(f"The same program is run {runs} times. Each time random bits are flipped (in the registers, the")
    say("arithmetic unit and the program counter), like radiation would. Every run is compared with the")
    say("reference answer and put in exactly one box.\n")
    schemes = [
        ("no protection", ["--rf", "none", "--alu", "none"]),
        ("registers: triple copies + vote", ["--rf", "tmr", "--alu", "none", "--scrub", 32]),
        ("arithmetic: do it twice, compare", ["--rf", "none", "--alu", "dwc"]),
        ("both of the above", ["--rf", "tmr", "--alu", "dwc", "--scrub", 32]),
        ("both + program counter voted", ["--rf", "tmr", "--alu", "dwc", "--scrub", 32, "--control-tmr"]),
    ]
    say(f"{'protection':36} {'right':>6} {'silently wrong':>15} {'crash/hang':>11} {'stopped safely':>15}")
    for name, flags in schemes:
        cmd = [BIN / ("radslice_inject" + EXE), "--elf", PROGRAM, "--runs", runs, "--seed", 7,
               "--lambda-rf", 3e-9, "--lambda-alu", 2e-6, "--lambda-pc", 3e-8] + flags
        out = run(cmd)
        r = next(csv.DictReader(out.stdout.splitlines()))
        n = int(r["runs"])
        right = (int(r["correct"]) + int(r["detected_recovered"])) / n
        say(f"{name:36} {100 * right:>5.0f}% {100 * int(r['sdc']) / n:>14.0f}% "
            f"{100 * int(r['hang_crash']) / n:>10.0f}% {100 * int(r['detected_unrecovered']) / n:>14.0f}%")
    say("\n'Silently wrong' is the dangerous one: the chip gives a wrong answer and nobody knows. Protecting")
    say("only one part barely helps, because the unprotected parts still fail. All of them together remove")
    say("every failure in this model, at a cost in area and time.")


def part4():
    heading(4, "Where the numbers come from, and what they are not")
    say("Real (from the Fudan paper): a small block works 71% of the time; a 64-bit register about 7%;")
    say("the chip is n-type transistors only, runs at a few kHz, and has no spares and no radiation testing.")
    say("Everything else in this tour is a hypothetical parameter, written down and labelled as such in")
    say("docs/assumptions.md. Nothing here proves a real chip would behave this way.")
    say("\nFurther reading in the repository:")
    say("  docs/results.md   conclusions C1 to C17, each with its evidence and what would overturn it")
    say("  docs/gaps.md      the claims, written before the code, each with how it could be proved wrong")
    say("  results/          the tables and figures behind every claim")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--quick", action="store_true", help="fewer runs; about 15 seconds")
    args = ap.parse_args()
    for exe in ("radslice_run", "radslice_yield", "radslice_inject"):
        if not (BIN / (exe + EXE)).exists():
            sys.exit(f"missing build/{exe}{EXE}: run  cmake -S . -B build && cmake --build build -j")
    if not PROGRAM.exists():
        sys.exit("missing the test programs: run  bash scripts/build-workloads.sh")
    say("RadSlice: could a chip made of an atom-thin material survive manufacturing and space radiation?")
    say("(a simulation study; see docs/results.md)")
    part1()
    part2(500 if args.quick else 3000)
    part3(100 if args.quick else 400)
    part4()


if __name__ == "__main__":
    main()
