.PHONY: perf perf-bench perf-fps clean

perf: perf-bench perf-fps

perf-bench:
	cd perf && bash bench.sh

perf-fps:
	cd perf && bash run_all.sh

clean:
	rm -f results/*.txt
