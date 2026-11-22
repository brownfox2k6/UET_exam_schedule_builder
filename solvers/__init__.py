try:
    from .cetp_solvers import *
except ImportError as exc:
    raise ImportError(
        "Could not load the cetp C++ extension. Configure and build it with "
        "'cmake -S solvers -B solvers/build' and "
        "'cmake --build solvers/build --config Release'."
    ) from exc
