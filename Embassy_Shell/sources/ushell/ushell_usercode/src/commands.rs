#![allow(non_snake_case)]

use ushell2::log_info;

pub fn init() {
    log_info!("init | no-args");
}

pub fn ianit() {
    log_info!("ianit | no-args");
}

pub fn iaanit() {
    log_info!("iaanit | no-args");
}

pub fn ibnit() {
    log_info!("ibnit | no-args");
}

pub fn ibbnit() {
    log_info!("ibbnit | no-args");
}

pub fn ibbbnit() {
    log_info!("ibbbnit | no-args");
}

pub fn read(descr: i8, nbytes: u32) {
    log_info!("read | descriptor: {}, bytes:{}", descr, nbytes);
}

pub fn write(filename: &str, nbytes: u64, val: u8) {
    log_info!(
        "write | filename: {}, bytes:{}, value:{:X}/{:o}/{:b}",
        filename,
        nbytes,
        val,
        val,
        val
    );
}

pub fn led(onoff: bool) {
    if onoff {
        log_info!("led | ON");
    } else {
        log_info!("led | OFF");
    }
}

pub fn greeting(s1: &str, s2: &str) {
    log_info!("greeting | [{}] : [{}]", s1, s2);
}

pub fn send(port: &str, baud: u32, data: &[u8]) {
    log_info!("send | port: {} baudrate: {}, data:{:?}", port, baud, data);
}

pub fn astring(s: &str) {
    log_info!("astring | {}", s);
}

pub fn bstring(s: &str) {
    log_info!("bstring | {}", s);
}

pub fn cstring(s: &str) {
    log_info!("cstring | {}", s);
}
