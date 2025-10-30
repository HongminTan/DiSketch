#!/usr/bin/env python3
"""运行 DiSketch Simulator"""

from __future__ import annotations

import asyncio
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Optional


SCRIPTS_DIR = Path(__file__).resolve().parent
EXPERIMENTS_DIR = SCRIPTS_DIR.parent
CONFIGS_DIR = EXPERIMENTS_DIR.parent
PROJECT_ROOT = CONFIGS_DIR.parent
SIMULATOR_PATH = (PROJECT_ROOT / "build" / "disketch_simulator").resolve()
DEFAULT_TIMEOUT = 24 * 60 * 60  # 24 小时


def ensure_simulator() -> None:
    if not SIMULATOR_PATH.exists():
        raise FileNotFoundError(f"Simulator 不存在，请先编译项目: {SIMULATOR_PATH}")


def relative_config_path(config_path: Path) -> str:
    return str(config_path.relative_to(EXPERIMENTS_DIR))


def parse_metrics(csv_line: str):
    parts = csv_line.strip().split(",")
    if len(parts) < 9:
        return None
    try:
        return {
            "precision": float(parts[1]),
            "recall": float(parts[2]),
            "f1": float(parts[3]),
            "accuracy": float(parts[4]),
            "tp": int(parts[5]),
            "fp": int(parts[6]),
            "fn": int(parts[7]),
            "tn": int(parts[8]),
        }
    except ValueError:
        return None


def update_config_with_results(config_path: Path, full_line: str, ds_line: str):
    full_metrics = parse_metrics(full_line)
    ds_metrics = parse_metrics(ds_line)
    if not full_metrics or not ds_metrics:
        return False

    lines = config_path.read_text().splitlines(keepends=True)

    global_index = None
    for idx, line in enumerate(lines):
        if line.startswith("[global]"):
            global_index = idx
            break

    if global_index is None:
        header_lines = lines
        body_lines: list[str] = []
    else:
        header_lines = lines[:global_index]
        body_lines = lines[global_index:]

    cleaned_header = [
        line
        for line in header_lines
        if line.strip() and not line.lstrip().startswith("#")
    ]

    relative_path = relative_config_path(config_path)
    result_block = [
        f"# 结果: {relative_path}\n",
        (
            f"#   Full Sketch: Precision={full_metrics['precision']:.3f}, "
            f"Recall={full_metrics['recall']:.3f}, F1={full_metrics['f1']:.3f}, "
            f"Accuracy={full_metrics['accuracy']:.3f}, TP={full_metrics['tp']}, "
            f"FP={full_metrics['fp']}, FN={full_metrics['fn']}, TN={full_metrics['tn']}\n"
        ),
        (
            f"#   DiSketch: Precision={ds_metrics['precision']:.3f}, "
            f"Recall={ds_metrics['recall']:.3f}, F1={ds_metrics['f1']:.3f}, "
            f"Accuracy={ds_metrics['accuracy']:.3f}, TP={ds_metrics['tp']}, "
            f"FP={ds_metrics['fp']}, FN={ds_metrics['fn']}, TN={ds_metrics['tn']}\n"
        ),
    ]

    if cleaned_header and cleaned_header[-1].strip() != "":
        cleaned_header.append("\n")
    cleaned_header.extend(result_block)
    if not cleaned_header[-1].endswith("\n"):
        cleaned_header[-1] = cleaned_header[-1] + "\n"
    cleaned_header.append("\n")

    config_path.write_text("".join(cleaned_header + body_lines))
    return True


def _is_progress_line(text: str) -> bool:
    stripped = text.strip()
    if not stripped:
        return False
    return stripped.startswith("DiSketch ") and "[" in stripped and "]" in stripped


ProgressCallback = Callable[[str, str], None]


@dataclass
class SimulatorRunResult:
    config_path: Path
    command: list[str]
    stdout_lines: list[str]
    stderr_lines: list[str]
    returncode: int
    runtime: float
    timed_out: bool

    def find_metric_lines(self):
        full_line = None
        disketch_line = None
        for line in self.stdout_lines:
            if line.startswith("FullSketch,"):
                full_line = line
            elif line.startswith("DiSketch,"):
                disketch_line = line
        return full_line, disketch_line


async def run_simulator(
    config_path: Path,
    *,
    quiet: bool = False,
    timeout: int = DEFAULT_TIMEOUT,
    progress_callback: Optional[ProgressCallback] = None,
) -> SimulatorRunResult:
    ensure_simulator()

    if not config_path.exists():
        raise FileNotFoundError(f"配置不存在: {config_path}")

    cmd = [str(SIMULATOR_PATH), "--config", str(config_path)]
    if quiet:
        cmd.append("--quiet")

    start_time = time.time()
    process = await asyncio.create_subprocess_exec(
        *cmd,
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE,
    )

    if progress_callback:
        progress_callback("start", "启动中")

    stdout_lines: list[str] = []
    stderr_lines: list[str] = []

    async def consume_stream(stream, is_stdout: bool):
        buffer = ""
        assert stream is not None
        while True:
            chunk = await stream.read(1024)
            if not chunk:
                if buffer:
                    _handle_segment(buffer, is_stdout)
                break
            text = chunk.decode("utf-8", errors="replace")
            for ch in text:
                if ch in ("\r", "\n"):
                    _handle_segment(buffer, is_stdout)
                    buffer = ""
                else:
                    buffer += ch

    def _handle_segment(segment: str, is_stdout: bool) -> None:
        if not segment:
            return
        if _is_progress_line(segment):
            if progress_callback:
                progress_callback("progress", segment.strip())
            return
        cleaned = segment.rstrip()
        if not cleaned:
            return
        if is_stdout:
            stdout_lines.append(cleaned)
        else:
            stderr_lines.append(cleaned)
            if progress_callback:
                progress_callback("stderr", cleaned)

    stdout_task = asyncio.create_task(consume_stream(process.stdout, True))
    stderr_task = asyncio.create_task(consume_stream(process.stderr, False))

    timed_out = False
    try:
        await asyncio.wait_for(process.wait(), timeout=timeout)
    except asyncio.TimeoutError:
        timed_out = True
        process.kill()
        if progress_callback:
            progress_callback("stderr", "执行超时，已终止进程")
        await process.wait()

    await asyncio.gather(stdout_task, stderr_task, return_exceptions=True)

    runtime = time.time() - start_time

    if progress_callback:
        if timed_out:
            progress_callback("done", "超时")
        elif process.returncode == 0:
            progress_callback("done", f"完成 ({runtime:.1f}s)")
        else:
            progress_callback("done", f"失败 (返回码 {process.returncode})")

    return SimulatorRunResult(
        config_path=config_path,
        command=cmd,
        stdout_lines=stdout_lines,
        stderr_lines=stderr_lines,
        returncode=process.returncode or 0,
        runtime=runtime,
        timed_out=timed_out,
    )
