try:
  from .aco_solver import *
except ImportError:
  print("Error: C++ library file (.so/.pyd) not found in the 'solver' directory.")
  print("Please run 'cmake --build solver/build' before executing the Python script.")
  print("Or, run 'cmake --build solver/build --config Release' if you want a release version.")
