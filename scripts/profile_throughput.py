"""
Throughput profiler for scrabble_bot move generation.

Runs full game episodes (empty board → no valid moves) using random move selection,
measures states (get_all_valid_moves calls) per second over a fixed duration.

Usage:
    uv run python scripts/profile_throughput.py [--duration 30] [--wordlist dictionary/nwl_2023.txt]
"""

import argparse
import random
import time

from scrabble_bot._cpp_ext import Board, Gaddag, Letter

# Standard 100-tile Scrabble bag
TILE_BAG: list[Letter] = (
    [Letter.A] * 9
    + [Letter.B] * 2
    + [Letter.C] * 2
    + [Letter.D] * 4
    + [Letter.E] * 12
    + [Letter.F] * 2
    + [Letter.G] * 3
    + [Letter.H] * 2
    + [Letter.I] * 9
    + [Letter.J] * 1
    + [Letter.K] * 1
    + [Letter.L] * 4
    + [Letter.M] * 2
    + [Letter.N] * 6
    + [Letter.O] * 8
    + [Letter.P] * 2
    + [Letter.Q] * 1
    + [Letter.R] * 6
    + [Letter.S] * 4
    + [Letter.T] * 6
    + [Letter.U] * 4
    + [Letter.V] * 2
    + [Letter.W] * 2
    + [Letter.X] * 1
    + [Letter.Y] * 2
    + [Letter.Z] * 1
    + [Letter.BLANK] * 2
)


def run_episode(gaddag: Gaddag) -> int:
    """Simulate one full game episode. Returns the number of states visited."""
    board = Board(gaddag)
    bag = TILE_BAG[:]
    random.shuffle(bag)

    # Draw initial rack of 7
    rack: list[Letter] = bag[:7]
    bag = bag[7:]

    # Track which squares are occupied so we know which move positions are new
    occupied: set[tuple[int, int]] = set()

    states = 0

    while True:
        moves = board.get_all_valid_moves(rack)
        states += 1

        if not moves:
            break

        move = random.choice(moves)

        # Determine rack tiles consumed by this move.
        # move.letters covers the full word span (including pre-existing tiles).
        # Only squares not already occupied require a tile from the rack.
        vertical = move.start.col == move.end.col and move.start.row != move.end.row
        dr = 1 if vertical else 0
        dc = 0 if vertical else 1

        tiles_consumed: list[Letter] = []
        for i, (letter, is_blank) in enumerate(zip(move.letters, move.is_blank)):
            pos = (move.start.row + dr * i, move.start.col + dc * i)
            if pos not in occupied:
                tiles_consumed.append(Letter.BLANK if is_blank else letter)
                occupied.add(pos)

        board.place_word(move)

        # Remove consumed tiles from rack
        for tile in tiles_consumed:
            rack.remove(tile)

        # Refill rack from bag
        draw = min(7 - len(rack), len(bag))
        rack.extend(bag[:draw])
        bag = bag[draw:]

    return states


def main() -> None:
    parser = argparse.ArgumentParser(description="Profile scrabble_bot move generation throughput")
    parser.add_argument("--duration", type=float, default=30.0, metavar="SEC",
                        help="Profiling duration in seconds (default: 30)")
    parser.add_argument("--wordlist", default="dictionary/nwl_2023.txt", metavar="PATH",
                        help="Path to wordlist file (default: dictionary/nwl_2023.txt)")
    args = parser.parse_args()

    # Load GADDAG
    print(f"Loading dictionary from {args.wordlist}...", end=" ", flush=True)
    t0 = time.perf_counter()
    gaddag = Gaddag()
    gaddag.build_from_file(wordlist_path=args.wordlist, use_cache=True)
    load_time = time.perf_counter() - t0
    print(f"done ({load_time:.2f}s)")

    # Warmup
    print("Warming up (1 episode)...", end=" ", flush=True)
    run_episode(gaddag)
    print("done")

    # Profile
    print(f"Profiling for {args.duration:.1f} seconds...")
    total_states = 0
    total_episodes = 0
    deadline = time.perf_counter() + args.duration

    while time.perf_counter() < deadline:
        total_states += run_episode(gaddag)
        total_episodes += 1

    elapsed = args.duration  # We ran for (approximately) the requested duration

    avg_states = total_states / total_episodes if total_episodes else 0.0
    states_per_sec = total_states / elapsed

    print()
    print("Results")
    print("-------")
    print(f"Duration:              {elapsed:.2f}s")
    print(f"Episodes:              {total_episodes:,}")
    print(f"Total states:          {total_states:,}")
    print(f"States / second:       {states_per_sec:,.1f}")
    print(f"Avg states / episode:  {avg_states:.1f}")


if __name__ == "__main__":
    main()
