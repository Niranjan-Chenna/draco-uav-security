# DRACO Development Roadmap

**Current phase: Architecture and protocol design**

The roadmap separates completed documentation work from planned implementation and evaluation. A checked item indicates that the corresponding design artifact exists in this repository; it does not imply implementation or security validation.

## Phase 1 — Architecture and Protocol Design

- [x] Define initial DRACO system architecture.
- [x] Define initial threat model.
- [x] Define conceptual command envelope.
- [x] Define initial UAV state model.
- [ ] Review and refine protocol design.
- [ ] Resolve session and key-management design decisions.
- [ ] Select canonical serialization and cryptographic parameters.
- [ ] Define experiment success criteria and safety constraints.

**Exit criteria:** architecture and protocol decisions are reviewed, security assumptions are explicit, and the implementation interfaces are sufficiently specified.

## Phase 2 — Core Simulation

- [ ] Implement UAV state machine.
- [ ] Implement Ground Control Station simulator.
- [ ] Implement UAV firmware simulator.
- [ ] Implement command communication channel.

**Exit criteria:** deterministic command and state-transition behavior can be exercised without authentication, with no connection to real UAV hardware.

## Phase 3 — Authentication

- [ ] Implement session management.
- [ ] Implement HMAC authentication.
- [ ] Implement nonce generation.
- [ ] Implement timestamp validation.
- [ ] Implement replay protection.
- [ ] Implement state-aware command validation at the gateway.

**Exit criteria:** the simulated gateway applies the specified validation pipeline and exposes observable decisions for controlled tests.

## Phase 4 — Security Testing

- [ ] Implement adversary simulator.
- [ ] Add replay-attack simulation.
- [ ] Add command-spoofing simulation.
- [ ] Add packet-modification simulation.
- [ ] Add unauthorized-session simulation.
- [ ] Add invalid-state-transition simulation.

**Exit criteria:** each modeled threat has a repeatable experiment with documented inputs, expected outcomes, and limitations.

## Phase 5 — Evaluation

- [ ] Add structured security logging.
- [ ] Build repeatable experiments.
- [ ] Measure attack rejection behavior.
- [ ] Analyze performance/security trade-offs.
- [ ] Prepare experimental results.
- [ ] Revisit the threat model using observed limitations.

**Exit criteria:** results are reproducible, claims are supported by collected evidence, and limitations are reported alongside findings.

## Documentation principles

- Keep planned behavior clearly separated from observed results.
- Mark an item complete only when the corresponding artifact or behavior genuinely exists.
- Record protocol changes and the assumptions they affect.
- Avoid claims of prevention, validation, or performance until supported by repeatable experiments.
