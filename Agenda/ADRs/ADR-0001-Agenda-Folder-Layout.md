# ADR 0001: Add Agenda folder for governance and agent artifacts

Date: 2025-12-27

Decision
--------
Create an `Agenda/` folder to centralize agent manifests, project policies, ADRs, and meeting notes to make the repository compatible with Agenda+ orchestration and governance tools.

Context
-------
- Agent-driven pipelines (Agenda+) expect a predictable location to find agent manifests and governance artifacts.
- Projects historically suffered from scattered agent instructions and inconsistent policy enforcement.

Consequences
------------
- Agent manifests and governance docs are discoverable under `Agenda/`.
- Policies will be added to `Agenda/policies` and a small set of scripts will help enforce project-only Git rules.

Status
------
Accepted.
