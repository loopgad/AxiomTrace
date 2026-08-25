from pathlib import Path
from types import SimpleNamespace

import pytest

from scripts import benchmark_gate
from scripts.benchmark_gate import parse_budget_failures, parse_summary, regressions


def test_benchmark_summary_parser_extracts_p999_column():
    output = """
B1:FOC_30kHz                  0.06  0.10  0.14  0.04  0 PASS
B7:encode_pipe                0.08  0.10  0.24  0.14  0 PASS
"""
    assert parse_summary(output) == {
        "B1:FOC_30kHz": 0.14,
        "B7:encode_pipe": 0.24,
    }


def test_benchmark_gate_rejects_more_than_twenty_percent_regression():
    baseline = {"B1:FOC_30kHz": 1.0, "B7:encode_pipe": 2.0}
    assert regressions(baseline, {"B1:FOC_30kHz": 1.2, "B7:encode_pipe": 2.4}, 20.0) == []
    failures = regressions(
        baseline, {"B1:FOC_30kHz": 1.21, "B7:encode_pipe": 2.0}, 20.0
    )
    assert len(failures) == 1
    assert failures[0].startswith("B1:FOC_30kHz:")


def test_benchmark_gate_rejects_absolute_budget_failures():
    output = """
B1:FOC_30kHz                                    0.06     0.10     4.20     3.10      0 FAIL
B2:FOC_30kHz_maxload                             0.07     0.10     0.14     0.04      0 PASS
    """
    assert parse_budget_failures(output) == ["B1:FOC_30kHz"]


def test_benchmark_gate_reports_budget_before_process_failure(monkeypatch):
    calls = {}

    def fake_run(command, **kwargs):
        calls["command"] = command
        calls.update(kwargs)
        return SimpleNamespace(
            returncode=1,
            stdout=(
                "B1:FOC_30kHz 0.06 0.10 4.20 3.10 0 FAIL\n"
            ),
            stderr="benchmark failed",
        )

    monkeypatch.setattr(benchmark_gate.subprocess, "run", fake_run)
    with pytest.raises(ValueError, match="benchmark budget failed"):
        benchmark_gate._run(Path("benchmark"))
    assert calls["check"] is False


def test_benchmark_gate_reports_non_budget_process_failure(monkeypatch):
    def fake_run(command, **kwargs):
        return SimpleNamespace(
            returncode=2,
            stdout="",
            stderr="benchmark crashed",
        )

    monkeypatch.setattr(benchmark_gate.subprocess, "run", fake_run)
    with pytest.raises(RuntimeError, match="exit code 2: benchmark crashed"):
        benchmark_gate._run(Path("benchmark"))
