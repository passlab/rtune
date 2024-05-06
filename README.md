## RTune - Runtime Tuning Library
RTune enables automated live analysis and tuning of application and system parameters amidst the simulation process to
meet user-defined objectives. The RTune API enables users to program computing and computational objectives in a
parallel iterative program, and the RTune runtime takes care of the measurement, modeling and analysis according to the
defined objective during the computation. 

### Build and install the library

1. Clone this repo

		git clone https://github.com/passlab/rtune.git
		
2. Build and install the RTune library. You can build and install in the directory you want. Commands in the following are for
   building and installing in the `build` and `install` folder under the source tree. 

		cd rtune
		mkdir build install
		cd build
		cmake -DCMAKE_INSTALL_PREFIX=../install ..
		make
		make install

### Run examples

##### Jacobi for automatically tuning OpenMP num_threads for parallel region

		cd ../examples/jacobi-num_threads
		make
		export LD_LIBRARY_PATH=../../install/lib
		./jacobi-rtune_num_threads
		
##### LULESH for automatically tuning OpenMP num_threads for parallel region

		cd ../LULESH-num_threads
		make
		export LD_LIBRARY_PATH=../../install/lib
		./LULESH-rtune_num_threads
		
##### Jacobi for automatically tuning computational precision according to the number of computing iteration

		cd ../examples/jacobi-precision
		make
		export LD_LIBRARY_PATH=../../install/lib
		./jacobi-rtune_precision
		
		
##### LULESH for simulation boundary 

		cd ../LULESH-boundary
		make
		export LD_LIBRARY_PATH=../../install/lib
		./LULESH-boundary

### Acknowledgement and Citation
Funding for this research and development was provided by the National Science Foundation 
under award No. 2001580 and 2015254. 

The work is published at in the follwing paper: ISPASS-2024

Yonghong Yan, Kewei Yan, and Anjia Wang, RTune: Towards Automated and Coordinated Optimization of Computing and 
Computational Objectives of Parallel Iterative Applications, ISPASS-2024, 2024 IEEE International Symposium 
on Performance Analysis of Systems and Software May 5-7, 2024, Indianapolis, Indiana
