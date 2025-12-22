# kernel-cpu-tracker

A Linux kernel module that tracks userspace CPU time for registered processes.

Processes register by writing their PID to a file in the /proc filesystem. The
module maintains a kernel linked list of registered PIDs and periodically
updates CPU usage using a timer and workqueue.

Build:
make

Load module:
sudo insmod kmlab.ko

Register a process:
echo <pid> > /proc/kmlab/status

View tracked processes:
cat /proc/kmlab/status

Unload module:
sudo rmmod kmlab

**Files:**

`Makefile`	      _Compiles your kernel module and userspace application_

`userapp.c/h`		        _Userspace test application_

`kmlab_given.h`		        _Obtain CPU use of a process (no modification needed)_

`kmlab.c`		        _Kernel module that needs to be implemented_

`kmlab_test.sh`		        _A shell script to test your implementation_

`example_output_pid_x_y.txt`			_Sample outputs when running `kmlab_test.sh` Note: your system may have completely different time values_

`dmesg_trace.txt`		        _Outputs from kernel ring buffer. This is for demonstration only to show how the timer/workqueue is used. In your module, you are not required to write to kernel ring buffer_


