---
title: Contributing
nav_order: 11
---

# Contributing

## Branch Strategy

| Branch | Purpose |
|---|---|
| `main` | Stable source code |
| `Documentation` | Docs site (`docs/` folder) |

## Commit Convention

Use [Conventional Commits](https://www.conventionalcommits.org/):

```
feat(firmware): add CM4 HSEM interrupt handler
fix(microk3): deduplicate failure entries on rapid alerts
docs(microros): add colcon.meta reference
test(e2e): add TAP flap resilience scenario
```

## Running Tests Before PR

```bash
# Unit tests
cd microrosWs/tests/host && make && ./build/test_runner

# All Docker tests
cd microrosWs/tests && ./run_docker_tests.sh
```

## Pull Request Checklist

- [ ] `make` passes for both CM7 and CM4
- [ ] Unit tests pass (`tests/host`)
- [ ] No new cppcheck / clang-tidy warnings
- [ ] Docker tests pass if touching integration/E2E
- [ ] Docs updated if changing API, topics, or config
