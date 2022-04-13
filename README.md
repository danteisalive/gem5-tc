# gem5-tc


This is a GEM5 implementation of TyCHE project. In order to to see the output of the Type Tracker use these flags with Gem5: 

Capability,TypeTracker,Allocator,SquashMech,AliasCache,PointerDepGraph,TypeCheck,TypeMetadata,StackTypeMetadata


/u/as3mx/gem5-tc/build/X86/gem5.opt --debug-flags=Exec --debug-file=Type.exec  /u/as3mx/gem5-tc/configs/example/se.py -c  ./perlbench_base.TEST -o "-I./lib checkspam.pl 2500 5 25 11 150 1 1 1 1" --caches --l2cache --cpu-type=O3_X86_skylake_1 --mem-type=DDR4_2400_16x4 --mem-size=8GB --mem-channels=2  --enable-capability --heapAllocationPointFile=./allocation_points.hash