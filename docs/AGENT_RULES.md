- Language: C++17, no external deps except the STL. Build with CMake.
- Read docs/SPEC.md before any task. Do not change the Job struct or the
  Scheduler interface without asking me.
- Each policy lives in its own file under src/policies/ and implements
  the Scheduler interface only. Never touch simulator core from a policy.
- Determinism: all randomness goes through one seeded RNG.
- Every task must end with: it compiles with -Wall -Wextra, tests pass,
  and a short summary of what changed. Do not commit; I commit.
- Don't refactor files unrelated to the current task.