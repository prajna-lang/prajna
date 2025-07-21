use std::time::Instant;

fn perf(array0: &[i32], array1: &[i32], array2: &mut [i32]) {
    for i in 0..array0.len() {
        array2[i] = array0[i] + array1[i];
    }
}

fn main() {
    println!("enter main");

    let size: usize = 1_000_000_000;

    let array0 = vec![0i32; size];
    let array1 = vec![0i32; size];
    let mut array2 = vec![0i32; size];

    println!("start");
    let t0 = Instant::now();

    perf(&array0, &array1, &mut array2);

    let duration = t0.elapsed().as_secs_f32();
    let gb_per_second = (size as f32 * 4.0) / 1e9 / duration;

    println!("{:.2} GB/s", gb_per_second);
}
