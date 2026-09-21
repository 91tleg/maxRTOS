#!/usr/bin/env python3
"""Requirements traceability for the SIL suites.

Reads the JUnit XML reports written by the suites (each test case carries a
`requirement` property) and REQUIREMENTS.md, then

  * prints a requirement -> test case matrix with pass/fail status,
  * writes it to <reports>/traceability.md,
  * exits non-zero if a requirement has no passing test, or a test cites a
    requirement that REQUIREMENTS.md does not define.

Usage: traceability.py <reports-dir> [--requirements FILE]
"""

import argparse
import pathlib
import re
import sys
import xml.etree.ElementTree as ET

HERE = pathlib.Path(__file__).resolve().parent
DEFAULT_REQUIREMENTS = HERE.parent / "REQUIREMENTS.md"


def load_requirements(path):
    requirements = {}
    for line in path.read_text().splitlines():
        m = re.match(r"\|\s*((?:REQ-[A-Z]+|SIM)-\d+)\s*\|\s*(.+?)\s*\|\s*$", line)
        if m:
            requirements[m.group(1)] = m.group(2)
    return requirements


def load_cases(reports):
    cases = []
    for report in sorted(reports.glob("*.xml")):
        suite = ET.parse(report).getroot()
        for case in suite.iter("testcase"):
            props = {p.get("name"): p.get("value") for p in case.iter("property")}
            cases.append({
                "suite": suite.get("name"),
                "name": case.get("name"),
                "requirement": props.get("requirement", ""),
                "title": props.get("title", ""),
                "passed": case.find("failure") is None,
            })
    return cases


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("reports")
    parser.add_argument("--requirements", default=str(DEFAULT_REQUIREMENTS))
    args = parser.parse_args()

    reports = pathlib.Path(args.reports)
    requirements = load_requirements(pathlib.Path(args.requirements))
    cases = load_cases(reports)

    if not cases:
        print(f"no test reports found in {reports}")
        return 2

    by_requirement = {r: [] for r in requirements}
    unknown = set()

    for case in cases:
        if case["requirement"] in by_requirement:
            by_requirement[case["requirement"]].append(case)
        else:
            unknown.add(case["requirement"] or "(none)")

    lines = ["# SIL requirements traceability", "",
             "| Requirement | Verified by | Status |", "|---|---|---|"]
    uncovered = []

    for req in sorted(requirements):
        verifying = by_requirement[req]
        passing = [c for c in verifying if c["passed"]]
        status = "PASS" if verifying and len(passing) == len(verifying) else \
                 "FAIL" if verifying else "NOT COVERED"
        if status != "PASS":
            uncovered.append((req, status))
        names = "<br>".join(f"`{c['suite']}::{c['name']}`" for c in verifying) or "-"
        lines.append(f"| {req} — {requirements[req]} | {names} | {status} |")

    total = len(requirements)
    covered = total - len(uncovered)
    lines += ["", f"{covered}/{total} requirements verified by passing tests, "
                  f"{len(cases)} test cases."]

    text = "\n".join(lines) + "\n"
    (reports / "traceability.md").write_text(text)

    print(f"{covered}/{total} requirements verified, {len(cases)} test cases")
    for req, status in uncovered:
        print(f"  {status}: {req}")
    for req in sorted(unknown):
        print(f"  UNKNOWN requirement cited by a test: {req}")

    return 0 if not uncovered and not unknown else 1


if __name__ == "__main__":
    sys.exit(main())
