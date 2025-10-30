#!/usr/bin/env python3
"""DiSketch 并行实验框架"""

from __future__ import annotations

import argparse
import asyncio
import os
import re
from dataclasses import dataclass
from pathlib import Path
from typing import List, Optional

from rich.console import Console
from rich.live import Live
from rich.table import Table

from runner_core import (
    EXPERIMENTS_DIR,
    SimulatorRunResult,
    ensure_simulator,
    parse_metrics,
    relative_config_path,
    run_simulator,
    update_config_with_results,
)


def collect_configs(group_names: List[str]):
    available_groups = {
        path.name: path for path in sorted(EXPERIMENTS_DIR.iterdir()) if path.is_dir()
    }

    if group_names:
        missing = [name for name in group_names if name not in available_groups]
        if missing:
            raise ValueError(f"未知实验组: {', '.join(missing)}")
        selected = [available_groups[name] for name in group_names]
    else:
        selected = list(available_groups.values())

    configs: List[Path] = []
    for group_dir in selected:
        for cfg in sorted(group_dir.glob("*.ini")):
            configs.append(cfg)
    return configs


def write_log(log_path: Path, result: SimulatorRunResult) -> None:
    stdout_text = "\n".join(result.stdout_lines)
    stderr_text = "\n".join(result.stderr_lines)
    log_path.write_text(
        f"# 运行时间: {result.runtime:.1f}秒\n"
        f'# 命令: {" ".join(result.command)}\n'
        f"# 返回码: {result.returncode}\n\n"
        f"=== STDOUT ===\n{stdout_text}\n\n"
        f"=== STDERR ===\n{stderr_text}"
    )


@dataclass
class ExecutionOutcome:
    config_path: Path
    runtime: float
    full_line: Optional[str] = None
    disketch_line: Optional[str] = None
    full_f1: Optional[float] = None
    disketch_f1: Optional[float] = None
    error: Optional[str] = None


class ProgressDisplay:
    _progress_re = re.compile(
        r"(?P<percent>\d+)%.*?(?P<current>\d+)/(?:\s*)(?P<total>\d+)"
    )

    def __init__(self, labels: List[str], enabled: bool = True):
        self.enabled = enabled and bool(labels)
        self.labels = labels
        self._bar_width = 24
        self.states = {label: self._make_state() for label in labels}
        self._console = Console()
        self._live: Live | None = None

    @staticmethod
    def _make_state() -> dict:
        return {
            "state": "排队",
            "message": "等待启动",
            "percent": None,
            "current": None,
            "total": None,
            "eta": "",
            "elapsed": "",
            "extra": "",
        }

    def __enter__(self):
        if not self.enabled:
            return self
        self._live = Live(
            self._render_rich(), console=self._console, refresh_per_second=12
        )
        self._live.__enter__()
        return self

    def __exit__(self, exc_type, exc, tb):
        if not self.enabled:
            return False
        if self._live:
            self._live.__exit__(exc_type, exc, tb)
        return False

    def update(
        self,
        label: str,
        message: str,
        state: Optional[str] = None,
        *,
        kind: str = "info",
    ) -> None:
        if not self.enabled:
            return
        info = self.states.setdefault(label, self._make_state())
        if state is not None:
            info["state"] = state

        if kind == "progress":
            progress = self._parse_progress(message)
            if progress:
                info.update(progress)
            else:
                info["message"] = message
        elif kind == "stderr":
            info["extra"] = message
        elif kind == "done":
            # 保留最后一次进度信息，但更新结束提示
            info["extra"] = message
        else:
            info["message"] = message

        if self._live:
            self._live.update(self._render_rich())

    def _parse_progress(self, text: str) -> Optional[dict]:
        match = self._progress_re.search(text)
        if not match:
            return None
        percent = int(match.group("percent"))
        current = int(match.group("current"))
        total = int(match.group("total"))
        eta_match = re.search(r"ETA\s+([0-9:]+)", text)
        elapsed_match = re.search(r"elapsed\s+([0-9:]+)", text)
        eta = eta_match.group(1) if eta_match else ""
        elapsed = elapsed_match.group(1) if elapsed_match else ""
        return {
            "percent": percent,
            "current": current,
            "total": total,
            "eta": eta,
            "elapsed": elapsed,
            "message": text,
            "extra": "",
        }

    def _render_rich(self):
        table = Table(show_edge=False, box=None)
        table.add_column("配置", ratio=3)
        table.add_column("状态", ratio=1)
        table.add_column("进度", ratio=4)
        table.add_column("备注", ratio=3)
        for label in self.labels:
            info = self.states.get(label, self._make_state())
            progress_text = self._format_progress_text(info)
            table.add_row(label, info["state"], progress_text, info["extra"])
        return table

    def _format_progress_text(self, info: dict) -> str:
        if info["percent"] is None or info["total"] in (None, 0):
            return info["message"]
        filled = min(
            self._bar_width,
            max(0, int(round(info["percent"] / 100.0 * self._bar_width))),
        )
        bar = "█" * filled + "-" * (self._bar_width - filled)
        text = f"[{bar}] {info['percent']:>3}% {info['current']}/{info['total']}"
        if info["state"] == "完成" and info["elapsed"]:
            text += f" elapsed {info['elapsed']}"
        elif info["eta"]:
            text += f" ETA {info['eta']}"
        return text


async def execute_config(
    config_path: Path,
    label: str,
    semaphore: asyncio.Semaphore,
    display: ProgressDisplay,
    quiet: bool,
) -> ExecutionOutcome:
    log_dir = config_path.parent / "logs"
    log_dir.mkdir(exist_ok=True)
    log_file = log_dir / f"{config_path.stem}.log"

    display.update(label, "等待调度", "排队")

    async with semaphore:
        display.update(label, "启动中", "运行中")

        def handle_progress(kind: str, message: str) -> None:
            if kind == "progress":
                display.update(label, message, "运行中", kind="progress")
            elif kind == "stderr":
                display.update(label, f"⚠ {message}", "运行中", kind="stderr")
            elif kind == "done":
                display.update(label, message, "完成", kind="done")

        try:
            result = await run_simulator(
                config_path,
                quiet=quiet,
                progress_callback=None if quiet else handle_progress,
            )
        except FileNotFoundError as exc:
            display.update(label, str(exc), "失败")
            return ExecutionOutcome(
                config_path=config_path, runtime=0.0, error="missing-config"
            )
        except Exception as exc:
            display.update(label, f"异常: {exc}", "失败")
            return ExecutionOutcome(
                config_path=config_path, runtime=0.0, error=f"error: {exc}"
            )

        write_log(log_file, result)

        if result.timed_out:
            display.update(label, "超时 (>30分钟)", "失败")
            return ExecutionOutcome(
                config_path=config_path, runtime=result.runtime, error="timeout"
            )

        if result.returncode != 0:
            display.update(label, f"失败 (返回码 {result.returncode})", "失败")
            return ExecutionOutcome(
                config_path=config_path,
                runtime=result.runtime,
                error=f"returncode={result.returncode}",
            )

        full_line, ds_line = result.find_metric_lines()
        if not full_line or not ds_line:
            display.update(label, "输出解析失败", "失败")
            return ExecutionOutcome(
                config_path=config_path,
                runtime=result.runtime,
                error="parse-error",
            )

        full_metrics = parse_metrics(full_line)
        ds_metrics = parse_metrics(ds_line)
        if not full_metrics or not ds_metrics:
            display.update(label, "指标解析失败", "失败")
            return ExecutionOutcome(
                config_path=config_path,
                runtime=result.runtime,
                error="metric-parse-error",
            )

        if not update_config_with_results(config_path, full_line, ds_line):
            display.update(label, "写回配置失败", "失败")
            return ExecutionOutcome(
                config_path=config_path,
                runtime=result.runtime,
                error="update-failed",
            )

        delta_f1 = full_metrics["f1"] - ds_metrics["f1"]
        display.update(
            label,
            f"ΔF1={delta_f1:.3f} 用时 {result.runtime:.1f}s",
            "完成",
            kind="done",
        )

        return ExecutionOutcome(
            config_path=config_path,
            runtime=result.runtime,
            full_line=full_line,
            disketch_line=ds_line,
            full_f1=full_metrics["f1"],
            disketch_f1=ds_metrics["f1"],
        )


async def run_parallel_async(
    configs: List[Path], workers: int, quiet: bool
) -> List[ExecutionOutcome]:
    ensure_simulator()

    if not configs:
        print("没有匹配的配置文件，已退出")
        return []

    labels = [relative_config_path(cfg) for cfg in configs]
    semaphore = asyncio.Semaphore(max(1, workers))
    outcomes: List[ExecutionOutcome] = []

    display = ProgressDisplay(labels, enabled=not quiet)
    with display:
        tasks = [
            asyncio.create_task(execute_config(cfg, label, semaphore, display, quiet))
            for cfg, label in zip(configs, labels)
        ]
        for task in asyncio.as_completed(tasks):
            outcome = await task
            outcomes.append(outcome)

    return outcomes


def summarize(outcomes: List[ExecutionOutcome]) -> None:
    if not outcomes:
        return

    successes = [o for o in outcomes if o.error is None]
    failures = [o for o in outcomes if o.error is not None]

    print("\n并行执行完成")
    print(f"成功: {len(successes)} / {len(outcomes)}")

    if successes:
        print("\n成功任务:")
        print(
            f"{'配置':<50} {'Full F1':>8} {'DiSketch F1':>12} {'ΔF1':>8} {'用时':>10}"
        )
        print("-" * 96)
        for outcome in successes:
            label = relative_config_path(outcome.config_path)
            print(
                f"{label:<50} {outcome.full_f1:>8.3f} {outcome.disketch_f1:>12.3f} "
                f"{(outcome.full_f1 - outcome.disketch_f1):>8.3f} {outcome.runtime:>9.1f}s"
            )

    if failures:
        print("\n失败任务:")
        for outcome in failures:
            label = relative_config_path(outcome.config_path)
            print(f"  - {label} -> {outcome.error}")


def main():
    parser = argparse.ArgumentParser(description="并行运行DiSketch受控实验")
    parser.add_argument("--groups", help="指定要运行的实验组，逗号分隔（默认全部）")
    parser.add_argument("--workers", type=int, help="最大并行数，默认根据CPU自动计算")
    parser.add_argument("--quiet", action="store_true", help="禁用进度条显示")
    args = parser.parse_args()

    groups: List[str] = []
    if args.groups:
        groups = [name.strip() for name in args.groups.split(",") if name.strip()]

    try:
        configs = collect_configs(groups)
    except ValueError as exc:
        parser.error(str(exc))
        return

    cpu_count = os.cpu_count() or 1
    default_workers = max(1, min(len(configs), cpu_count))
    workers = max(1, args.workers or default_workers)

    print(f"CPU 数: {cpu_count}, 实际使用: {workers}")

    outcomes = asyncio.run(run_parallel_async(configs, workers, quiet=args.quiet))
    summarize(outcomes)


if __name__ == "__main__":
    main()
