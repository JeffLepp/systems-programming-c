# cpu-scheduling-sim

A discrete-event simulator for common CPU scheduling algorithms.

The simulator models process execution on a cycle-by-cycle basis and reports
per-process and summary statistics.

Build:
make

Run:
./schedsim <input-file>

Usage:
The input file describes process arrival times and CPU/IO parameters.

Implemented algorithms:
First Come First Serve (FCFS)
Shortest Job First (SJF)
Round Robin (fixed quantum)
