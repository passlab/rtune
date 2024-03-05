## RTune - Runtime Tuning Library
RTune enables automated live analysis and tuning of application and system parameters amidst the simulation process to
meet user-defined objectives. The RTune API enables users to program computing and computational objectives in a
parallel iterative program, and the RTune runtime takes care of the measurement, modeling and analysis according to the
defined objective during the computation. 

### Build and install the library

1. Clone this repo

		git clone https://github.com/passlab/rtune.git
		
2. Build and install the RTune library. You can build and install in the directory you want. Commands in the following are for
   building and installing in the `build` and `install` folder of the source tree. 

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
		./jacobi-rtune_num_threads
		
##### LULESH for automatically tuning OpenMP num_threads for parallel region

		cd ../LULESH-num_threads
		make
		./LULESH-rtune_num_threads
		
##### Jacobi for automatically tuning computational precision according to the number of computing iteration

		cd ../examples/jacobi-precision
		make
		./jacobi-rtune_precision
		
		
##### LULESH for simulation boundary 

		cd ../LULESH-boundary
		make
		./LULESH-boundary

