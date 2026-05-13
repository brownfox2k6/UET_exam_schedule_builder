import sys
import os

current_dir = os.path.dirname(os.path.abspath(__file__))
solver_dir = os.path.join(current_dir, "solver")
sys.path.append(solver_dir)

try:
  from .aco_solver import *
except ImportError:
  print("Error: C++ library file (.so/.pyd) not found in the 'solver' directory.")
  print("Please run 'cmake --build solver/build' before executing the Python script.")
  print("Add, flag '--config Release' if you want a release version.")
