#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Host regressions for the pi32v2 loop cost estimator."""
import unittest

from target_budget import cost


def instructions(count, branches, extra=None):
    """One instruction per address, with representative objdump branch text."""
    text = dict(extra or {})
    for end, head in branches:
        text[end] = f"goto {head - end} <render: {head:x} >"
    return [(a, text.get(a, "nop")) for a in range(count)]


class LoopCostTest(unittest.TestCase):
    def test_overlapping_spans_count_once(self):
        # The branches enter the same loop at two different addresses.
        result = cost(instructions(10, [(6, 1), (8, 3)], {4: "r0 = r1 / r2"}))
        self.assertEqual(result, {"insns": 10, "loop": 8, "div": 1, "call": 0, "cost": 16})

    def test_contained_span_keeps_nesting_weight(self):
        result = cost(instructions(10, [(8, 1), (6, 3)], {4: "call decode"}))
        self.assertEqual(result, {"insns": 10, "loop": 8, "div": 0, "call": 1, "cost": 20})

    def test_merge_preserves_nested_and_separate_loops(self):
        # Merging [1, 6] and [5, 8] still leaves [2, 4] strictly inside.
        result = cost(instructions(12, [(4, 2), (6, 1), (8, 5), (11, 10)]))
        self.assertEqual(result["loop"], 10)
        self.assertEqual(result["cost"], 19)


if __name__ == "__main__":
    unittest.main()
