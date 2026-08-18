# Validation performed for the consolidated package

Validation updated for the 0.2.1 GeminiAstro EAF increment on 17 August 2026.

The consolidation environment does not contain Qt 6 or OpenCV development packages, so a complete application link/build cannot be executed here.

The following checks were completed:

1. `python3 tools/project_smoke_check.py` passes after adding the Gemini EAF backend.
2. Project structure and brace-balance smoke checks pass for all `.cpp` and `.h` files.
3. Every project-local quoted include resolves to a file under `src/` or `include/`.
4. Every source/header path referenced by `CMakeLists.txt` exists, including `gemini_eaf_focuser.*`.
5. `docs/openapi.yaml` parses as OpenAPI 3.1 and currently contains the existing API surface; the P0 operations/capabilities migration has **not** yet been applied to the implementation.
6. The standalone native OAL example plug-in remains part of the package.
7. CMake configuration detects the compiler and reaches `find_package(Qt6)`; it stops only because Qt 6 is not installed in this validation container.
8. Gemini EAF support is structurally wired into `ApplicationController`, CMake and GUI backend selection.
9. No hardware-in-the-loop Gemini test has been performed in this environment. The backend is therefore labelled a **compatibility profile**, not hardware-validated or native-serial production support.

Before using real equipment, build on the target platform and first test with simulated devices. Then follow `docs/GEMINI_EAF.md`, starting with small safe moves and the hardware validation checklist.
