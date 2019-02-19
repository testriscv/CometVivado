benchmarks = matmul qsort dijkstra

all:
	$(foreach bench, $(benchmarks), $(MAKE) -C $(bench) all;)

clean:
	$(foreach bench, $(benchmarks), $(MAKE) -C $(bench) clean;)
