---
title: Testing Overview
parent: Testing
nav_order: 1
---

# Test Pyramid

The project has seven test layers covering firmware through full end-to-end pipeline.

## Test Layers

| Layer | Location | Framework | What it tests |
|---|---|---|---|
| Unit | `microrosWs/tests/host/` | Unity (C) | Memory, IP config, UDP transport, allocators |
| Fuzzing | `microrosWs/tests/fuzzing/` | LibFuzzer | UDP framing, Ethernet frames, allocators |
| Integration | `microrosWs/tests/integration/` | pytest | micro-ROS agent comms, ROS 2 topics, UDP layer |
| E2E (macOS) | `microrosWs/tests/e2e/` | pytest | Full pipeline: firmware → agent → dashboard |
| E2E (Linux) | `microrosWs/tests/e2eLinux/` | pytest | TAP-based pipeline on Linux |
| Simulation | `microrosWs/tests/simulation/` | Robot Framework | Renode platform validation |
| Dashboard | `microrosWs/microk3/tests/` | pytest | Flask API and ROS interface |

## Running Tests

### Unit tests (host, no hardware)
```bash
cd microrosWs/tests/host
make && ./build/test_runner
```

### Integration tests
```bash
cd microrosWs/tests/integration
pip install -r requirements.txt
pytest -v
```

### All Docker tests (recommended for CI)
```bash
cd microrosWs/tests
chmod +x run_docker_tests.sh
./run_docker_tests.sh
```

### Fuzzing (sanitizers required)
```bash
cd microrosWs/tests/fuzzing
make fuzz
```
