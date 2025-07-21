import numpy as np
import time

def perf(array0, array1, array2):
    array2[:] = array0 + array1

def main():
    print("enter main")

    size = 1_000_000_000
    array0 = np.zeros(size, dtype=np.int32)
    array1 = np.zeros(size, dtype=np.int32)
    array2 = np.empty(size, dtype=np.int32)

    print("start")
    t0 = time.time()

    perf(array0, array1, array2)

    t1 = time.time()
    duration = t1 - t0
    gb_per_second = (size * 4) / 1e9 / duration

    print(f"{gb_per_second:.2f} GB/s")

if __name__ == "__main__":
    main()
