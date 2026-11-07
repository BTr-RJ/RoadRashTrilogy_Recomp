# Contributing

Read [the build guide](docs/BUILDING.md) and [video guide](docs/VIDEO.md) first.
Keep changes focused and explain the concrete problem and resulting behavior.
Run relevant native tests and `python -m unittest discover -s tests -p "test_*.py"`.

Never commit ROMs, translated game C, `.local`, `.tools`, build outputs, saves,
account credentials, private screenshots or remake audio. Use synthetic fixtures
for source tests. Rendering regressions need the game revision, reproducible
input/scene, original/enhanced comparison and relevant geometry counters.

Engine changes belong in project integration code or reproducible patch scripts
against the pinned SDK. Do not edit generated C. Do not update dependency versions
as a side effect of another change. Retain component license notices and media
provenance. Network play and complete campaign/static coverage are not current
release claims.
