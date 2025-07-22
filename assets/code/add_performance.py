import time

def perf(array0, array1, array2):
    for i in range(len(array0)):
        array2[i] = array0[i] + array1[i]

def main():
    print("enter main")

    size = 10_000_000
    array0 = [0] * size
    array1 = [0] * size
    array2 = [0] * size

    print("start")
    t0 = time.time()

    perf(array0, array1, array2)

    t1 = time.time()
    duration = t1 - t0
    gb_per_second = (size * 4) / 1e9 / duration

    print(f"{gb_per_second:.2f} GB/s")

if __name__ == "__main__":
    main()
