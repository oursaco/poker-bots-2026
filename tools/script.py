#!/usr/bin/env python3
"""
Scrimmage challenger helper
==========================
This script:
1) Parses a leaderboard export (paste from the website, or provide a file).
2) Using a simple Elo model, estimates how many consecutive *wins* are needed
   to reach #1 by repeatedly challenging a team above you.
3) Prints a browser-console JavaScript snippet that can auto-submit those
   challenges by clicking the website UI (endpoint-independent).

Notes / assumptions:
- We assume you WIN the challenges you submit (we're planning "how many wins
  needed", based only on Elo differences, as requested).
- Elo update uses a configurable K-factor (default 32).
- Only your rating and the challenged opponent's rating are updated in the
  simulation; other teams stay fixed.

Usage:
  python tools/script.py --help

Typical:
  # 1) Copy the leaderboard table (or a CSV/JSON export) into a file:
  python tools/script.py --in leaderboard.txt

  # 2) Or paste directly (finish with Ctrl-D):
  python tools/script.py
"""

from __future__ import annotations

import argparse
import json
import math
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, List, Optional, Tuple


OUR_TEAM_DEFAULT = "Chasers 1/2 Regs"


@dataclass(frozen=True)
class TeamEntry:
    rank: Optional[int]
    name: str
    elo: float


def _try_parse_json(text: str) -> Optional[List[TeamEntry]]:
    try:
        obj = json.loads(text)
    except Exception:
        return None

    # Accept:
    # - list[dict] with {team|name, elo|rating, rank?}
    # - dict with "teams": [...]
    if isinstance(obj, dict) and "teams" in obj:
        obj = obj["teams"]

    if not isinstance(obj, list):
        return None

    out: List[TeamEntry] = []
    for i, item in enumerate(obj):
        if isinstance(item, dict):
            name = item.get("team") or item.get("name")
            elo = item.get("elo") or item.get("rating")
            rank = item.get("rank")
            if name is None or elo is None:
                return None
            out.append(TeamEntry(rank=int(rank) if rank is not None else None, name=str(name), elo=float(elo)))
        elif isinstance(item, (list, tuple)) and len(item) >= 2:
            # [name, elo] or [rank, name, elo]
            if len(item) == 2:
                name, elo = item
                out.append(TeamEntry(rank=None, name=str(name), elo=float(elo)))
            else:
                rank, name, elo = item[0], item[1], item[2]
                out.append(TeamEntry(rank=int(rank), name=str(name), elo=float(elo)))
        else:
            return None

    return out if out else None


def _split_csv_line(line: str) -> List[str]:
    # Minimal CSV splitting; good enough for "rank,team,elo" without quoted commas in team names.
    return [x.strip() for x in line.split(",")]


def _try_parse_csv(text: str) -> Optional[List[TeamEntry]]:
    lines = [ln.strip() for ln in text.splitlines() if ln.strip()]
    if not lines:
        return None

    header = _split_csv_line(lines[0])
    lower = [h.lower() for h in header]
    if not (("elo" in lower or "rating" in lower) and ("team" in lower or "name" in lower)):
        return None

    def idx_of(*names: str) -> Optional[int]:
        for n in names:
            if n in lower:
                return lower.index(n)
        return None

    i_rank = idx_of("rank", "position", "place")
    i_team = idx_of("team", "name")
    i_elo = idx_of("elo", "rating")
    if i_team is None or i_elo is None:
        return None

    out: List[TeamEntry] = []
    for ln in lines[1:]:
        cols = _split_csv_line(ln)
        if len(cols) <= max(i for i in (i_team, i_elo, i_rank or 0)):
            continue
        rank = None
        if i_rank is not None and cols[i_rank]:
            try:
                rank = int(re.sub(r"[^\d]", "", cols[i_rank]))
            except Exception:
                rank = None
        name = cols[i_team]
        try:
            elo = float(cols[i_elo])
        except Exception:
            continue
        out.append(TeamEntry(rank=rank, name=name, elo=elo))
    return out if out else None


_NUM_RE = re.compile(r"(-?\d+(?:\.\d+)?)")


def _try_parse_plain_text(text: str) -> Optional[List[TeamEntry]]:
    """
    Heuristic parser for copied leaderboard text.
    Looks for lines that contain a name and a trailing number (Elo).
    Accepts optional leading rank.
    """
    lines = [ln.rstrip() for ln in text.splitlines() if ln.strip()]
    if not lines:
        return None

    out: List[TeamEntry] = []
    for ln in lines:
        # Remove repeated whitespace
        s = re.sub(r"\s+", " ", ln).strip()

        # Try: "<rank> <team name> <elo>"
        # We'll treat the LAST number in the line as elo.
        nums = list(_NUM_RE.finditer(s))
        if not nums:
            continue
        elo_match = nums[-1]
        elo_str = elo_match.group(1)
        try:
            elo = float(elo_str)
        except Exception:
            continue

        left = s[: elo_match.start()].strip()
        if not left:
            continue

        rank = None
        # optional leading rank like "1" or "1."
        m = re.match(r"^(\d+)\.?\s+(.*)$", left)
        if m:
            rank = int(m.group(1))
            name = m.group(2).strip()
        else:
            name = left

        # Skip obvious headers
        if name.lower() in {"rank", "team", "elo", "rating"}:
            continue
        out.append(TeamEntry(rank=rank, name=name, elo=elo))

    return out if out else None


def parse_leaderboard(text: str) -> List[TeamEntry]:
    text = text.strip()
    if not text:
        raise ValueError("Empty input. Paste the leaderboard text/CSV/JSON or pass --in FILE.")

    for parser in (_try_parse_json, _try_parse_csv, _try_parse_plain_text):
        parsed = parser(text)
        if parsed is not None:
            # If ranks missing, infer from order
            if all(e.rank is None for e in parsed):
                parsed = [TeamEntry(rank=i + 1, name=e.name, elo=e.elo) for i, e in enumerate(parsed)]
            return parsed

    raise ValueError(
        "Could not parse input as JSON, CSV, or plain text.\n"
        "Tip: easiest is CSV with headers: rank,team,elo"
    )


def elo_expected_score(r_a: float, r_b: float) -> float:
    # Expected score of A vs B.
    return 1.0 / (1.0 + 10 ** ((r_b - r_a) / 400.0))


def elo_update(r_a: float, r_b: float, s_a: float, k: float) -> Tuple[float, float]:
    e_a = elo_expected_score(r_a, r_b)
    e_b = 1.0 - e_a
    r_a2 = r_a + k * (s_a - e_a)
    r_b2 = r_b + k * ((1.0 - s_a) - e_b)
    return r_a2, r_b2


@dataclass
class PlanResult:
    opponent: TeamEntry
    wins_needed: int
    our_elo_final: float
    opp_elo_final: float
    top_elo_target: float


def wins_needed_to_take_first(
    leaderboard: List[TeamEntry],
    our_team: str,
    opponent_name: str,
    k: float,
    max_wins: int,
) -> Optional[PlanResult]:
    by_name = {e.name: e for e in leaderboard}
    if our_team not in by_name:
        return None
    if opponent_name not in by_name:
        return None

    our0 = by_name[our_team]
    opp0 = by_name[opponent_name]

    # Determine "current first-place Elo" based on max Elo (rank can be stale).
    top0 = max(leaderboard, key=lambda e: e.elo)
    top_elo_target = top0.elo

    # Clone ratings for simulation
    our = our0.elo
    opp = opp0.elo

    # Other teams stay fixed (including top if it's not opponent)
    fixed_other_elos = {e.name: e.elo for e in leaderboard if e.name not in {our_team, opponent_name}}

    for wins in range(0, max_wins + 1):
        # Condition: we are >= everyone else (including possibly-updated opponent)
        current_max_other = max([opp] + list(fixed_other_elos.values())) if fixed_other_elos else opp
        if our >= current_max_other:
            return PlanResult(
                opponent=opp0,
                wins_needed=wins,
                our_elo_final=our,
                opp_elo_final=opp,
                top_elo_target=top_elo_target,
            )
        # Take another win vs opponent
        our, opp = elo_update(our, opp, s_a=1.0, k=k)

    return None


def choose_best_plan(
    leaderboard: List[TeamEntry],
    our_team: str,
    k: float,
    max_wins: int,
) -> Tuple[TeamEntry, List[PlanResult]]:
    by_name = {e.name: e for e in leaderboard}
    if our_team not in by_name:
        raise ValueError(f"Our team '{our_team}' not found in leaderboard.")

    our_entry = by_name[our_team]
    above = [e for e in leaderboard if e.elo > our_entry.elo and e.name != our_team]
    if not above:
        raise ValueError(f"'{our_team}' already appears to be #1 by Elo (no teams above you).")

    results: List[PlanResult] = []
    for opp in above:
        res = wins_needed_to_take_first(
            leaderboard=leaderboard,
            our_team=our_team,
            opponent_name=opp.name,
            k=k,
            max_wins=max_wins,
        )
        if res is not None:
            results.append(res)

    if not results:
        # Fallback: pick the closest above team if we couldn't reach within max_wins
        best = min(above, key=lambda e: e.elo - our_entry.elo)
        return best, []

    # Prefer fewest wins; tie-breaker: highest final Elo
    results.sort(key=lambda r: (r.wins_needed, -r.our_elo_final))
    return results[0].opponent, results


def make_console_snippet(opponent_name: str, times: int, delay_ms: int = 1400) -> str:
    # Endpoint-independent UI clicker. It tries:
    # - Find leaderboard row containing opponent name
    # - Click "Challenge" button in that row, else click the row
    # - In the modal/dialog, click the primary "Challenge"/"Submit" button
    # It logs and stops on failure.
    safe_name = opponent_name.replace("\\", "\\\\").replace('"', '\\"')
    return f"""// Paste into DevTools console on scrimmage.pokerbots.org (leaderboard page).
// It will attempt to submit {times} challenges vs: "{safe_name}"
// Stop anytime with: window.__pb_stop = true

window.__pb_stop = false;

function sleep(ms) {{
  return new Promise(r => setTimeout(r, ms));
}}

function norm(s) {{
  return (s || "").replace(/\\s+/g, " ").trim().toLowerCase();
}}

function findButtonByText(root, textSubstr) {{
  const want = norm(textSubstr);
  const btns = Array.from(root.querySelectorAll("button"));
  return btns.find(b => norm(b.innerText).includes(want)) || null;
}}

function findRowByTeamName(teamName) {{
  const want = norm(teamName);
  const rows = Array.from(document.querySelectorAll("tr"));
  return rows.find(tr => norm(tr.innerText).includes(want)) || null;
}}

function findDialog() {{
  // Try common patterns
  return (
    document.querySelector('[role="dialog"]') ||
    document.querySelector("dialog") ||
    document.querySelector(".modal, .Modal, [data-modal]")
  );
}}

async function openChallenge(teamName) {{
  const row = findRowByTeamName(teamName);
  if (!row) throw new Error(`Could not find table row for team: ${{teamName}}`);

  const challengeBtn =
    findButtonByText(row, "challenge") ||
    findButtonByText(row, "play") ||
    null;

  (challengeBtn || row).click();

  // Wait for dialog to appear
  for (let i = 0; i < 30; i++) {{
    const dlg = findDialog();
    if (dlg) return dlg;
    await sleep(100);
  }}
  throw new Error("Challenge dialog did not appear.");
}}

async function submitDialog(dlg) {{
  // Try to set "games" / "count" if present (non-fatal if missing)
  const num = dlg.querySelector('input[type="number"]');
  if (num) {{
    num.focus();
    num.value = "1";
    num.dispatchEvent(new Event("input", {{ bubbles: true }}));
    num.dispatchEvent(new Event("change", {{ bubbles: true }}));
  }}

  const submit =
    findButtonByText(dlg, "challenge") ||
    findButtonByText(dlg, "submit") ||
    findButtonByText(dlg, "start") ||
    findButtonByText(dlg, "confirm");

  if (!submit) {{
    throw new Error("Could not find submit button in dialog (looked for Challenge/Submit/Start/Confirm).");
  }}

  submit.click();
}}

(async () => {{
  const opponent = "{safe_name}";
  const times = {times};
  const delayMs = {delay_ms};

  console.log("[pb] starting auto-challenges:", {{ opponent, times, delayMs }});

  for (let i = 1; i <= times; i++) {{
    if (window.__pb_stop) {{
      console.warn("[pb] stopped by user at i =", i);
      break;
    }}
    console.log(`[pb] challenge ${{i}} / ${{times}} vs ${{opponent}}`);
    try {{
      const dlg = await openChallenge(opponent);
      await sleep(150);
      await submitDialog(dlg);
    }} catch (e) {{
      console.error("[pb] failed:", e);
      break;
    }}
    await sleep(delayMs);
  }}

  console.log("[pb] done");
}})();
"""


def main(argv: Optional[List[str]] = None) -> int:
    p = argparse.ArgumentParser(description="Elo-based scrimmage challenge planner + console snippet generator")
    p.add_argument("--in", dest="in_path", type=str, default=None, help="Input file containing leaderboard text/CSV/JSON")
    p.add_argument("--our-team", type=str, default=OUR_TEAM_DEFAULT, help="Our team name (exact match)")
    p.add_argument("--k", type=float, default=32.0, help="Elo K-factor to assume")
    p.add_argument("--max-wins", type=int, default=200, help="Max simulated wins per opponent")
    p.add_argument("--delay-ms", type=int, default=1400, help="Delay between UI submissions in console snippet")
    p.add_argument("--top", type=int, default=20, help="Show top N rows after parsing")
    args = p.parse_args(argv)

    if args.in_path:
        text = Path(args.in_path).read_text(encoding="utf-8", errors="replace")
    else:
        text = sys.stdin.read()

    leaderboard = parse_leaderboard(text)
    # Sort by Elo descending (rank on site should match, but we'll trust Elo)
    leaderboard_sorted = sorted(leaderboard, key=lambda e: (-e.elo, e.rank or 10**9, e.name))

    by_name = {e.name: e for e in leaderboard_sorted}
    our = by_name.get(args.our_team)
    if our is None:
        names_preview = ", ".join(e.name for e in leaderboard_sorted[: min(10, len(leaderboard_sorted))])
        raise SystemExit(
            f"Could not find our team '{args.our_team}'.\n"
            f"First parsed teams: {names_preview}\n"
            f"Tip: pass --our-team with the exact team name as shown on the site."
        )

    top0 = leaderboard_sorted[0]

    print("Parsed leaderboard (top rows by Elo):")
    for e in leaderboard_sorted[: min(args.top, len(leaderboard_sorted))]:
        r = e.rank if e.rank is not None else leaderboard_sorted.index(e) + 1
        marker = " <= us" if e.name == args.our_team else ""
        print(f"  {r:>3}  {e.elo:>8.2f}  {e.name}{marker}")
    print()

    if our.name == top0.name:
        print(f"You're already #1 by Elo: {our.elo:.2f} ({our.name})")
        return 0

    opponent, plans = choose_best_plan(
        leaderboard=leaderboard_sorted,
        our_team=args.our_team,
        k=args.k,
        max_wins=args.max_wins,
    )

    # Select the best plan for that opponent (if computed)
    best_plan = None
    for pl in plans:
        if pl.opponent.name == opponent.name:
            best_plan = pl
            break

    print(f"Our team: {our.name}")
    print(f"Our Elo:  {our.elo:.2f}")
    print(f"#1 team:  {top0.name} ({top0.elo:.2f})")
    print(f"Assumed K: {args.k:.2f}")
    print()

    if best_plan is None:
        print("Could not compute a 'reach #1' plan within max-wins for any opponent above you.")
        print(f"Fallback recommendation: challenge the closest-above team: {opponent.name} ({opponent.elo:.2f})")
        print("Increase --max-wins if you want the simulation to search further.")
        return 0

    times = best_plan.wins_needed
    print("Recommendation (Elo-difference based):")
    print(f"  Challenge: {opponent.name}")
    print(f"  Wins needed (simulated): {times}")
    print(f"  Our Elo after {times} wins: {best_plan.our_elo_final:.2f}")
    print(f"  Opp Elo after {times} wins: {best_plan.opp_elo_final:.2f}")
    print()

    print("Browser-console snippet (UI clicker):")
    print(make_console_snippet(opponent_name=opponent.name, times=times, delay_ms=args.delay_ms))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

