#![allow(non_snake_case)]

use ushell2::log_info;

// Implement the actual logic for each shortcut
pub fn shortcut_plus_plus(param: &str) {
    log_info!("Executing ++ with param: '{}'", param);
}

pub fn shortcut_plus_l(param: &str) {
    log_info!("Executing +l with param: '{}'", param);
}

pub fn shortcut_plus_m(param: &str) {
    log_info!("Executing +m with param: '{}'", param);
}

pub fn shortcut_plus_question_mark(param: &str) {
    log_info!("Executing +? with param: '{}'", param);
}

pub fn shortcut_plus_tilde(param: &str) {
    log_info!("Executing +~ with param: '{}'", param);
}

pub fn shortcut_dot_dot(param: &str) {
    log_info!("Executing .. with param: '{}'", param);
}

pub fn shortcut_dot_z(param: &str) {
    log_info!("Executing .z with param: '{}'", param);
}

pub fn shortcut_dot_k(param: &str) {
    log_info!("Executing .k with param: '{}'", param);
}

pub fn shortcut_minus_dot(param: &str) {
    log_info!("Executing -. with param: '{}'", param);
}

pub fn shortcut_minus_t(param: &str) {
    log_info!("Executing -t with param: '{}'", param);
}

pub fn shortcut_minus_u(param: &str) {
    log_info!("Executing -u with param: '{}'", param);
}

pub fn shortcut_minus_w(param: &str) {
    log_info!("Executing -w with param: '{}'", param);
}
