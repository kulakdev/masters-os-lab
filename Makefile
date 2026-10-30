BUILD_DIR := build

.PHONY: native docker bench lab3 clean

native:                      ## build + run lab1 natively (macOS/Linux)
	cmake -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release
	cmake --build $(BUILD_DIR) -j
	$(BUILD_DIR)/lab1/socket_benchmark --all

docker:                      ## build inside container, drop into shell
	docker compose run --rm bench

bench:                       ## full benchmark matrix inside container
	docker compose run --rm bench python3 lab1/scripts/run_benchmarks.py

lab3:                        ## build + run lab3 cache benchmarks, write results/lab3_results.csv
	cmake -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release
	cmake --build $(BUILD_DIR) --target cache_benchmark -j
	python3 lab3/scripts/run_benchmarks.py

bench3:                      ## full lab3 cache benchmark matrix inside container
	docker compose run --rm bench python3 lab3/scripts/run_benchmarks.py

clean:
	rm -rf $(BUILD_DIR)
