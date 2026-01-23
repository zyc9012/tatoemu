#pragma once

#include "../types.h"
#include <cstring>

// SOCD (Simultaneous Opposite Cardinal Directions) processing
// Based on FBNeo joyprocess.h implementation

template <int N, typename T = u8>
struct ClearOpposite {
    mutable T prev[N], prev_ud[N], prev_lr[N];

    void reset() {
        memset(&prev,    0, sizeof(prev));
        memset(&prev_ud, 0, sizeof(prev_ud));
        memset(&prev_lr, 0, sizeof(prev_lr));
    }

    // Insert the positive direction into the two fast-switching oblique directions
    void interp(u8 n, T& inp, const T val_ud, const T val_lr) const {
        if (((inp | prev[n]) & val_ud) == val_ud) inp &= ~val_ud;
        if (((inp | prev[n]) & val_lr) == val_lr) inp &= ~val_lr;

        prev[n] = inp;
    }

    // Oppose simultaneous input from left and right or up and down
    void oppoxy(u8 /*n*/, T& inp, const T val_ud, const T val_lr) const {
        if ((inp & val_ud) == val_ud) inp &= ~val_ud;
        if ((inp & val_lr) == val_lr) inp &= ~val_lr;
    }

    // SOCD - Simultaneous Neutral
    void neu(u8 n, T& inp, const T val_u, const T val_d, const T val_l, const T val_r) const {
        const T val_ud = val_u | val_d, val_lr = val_l | val_r;
        oppoxy(n, inp, val_ud, val_lr);
        interp(n, inp, val_ud, val_lr);
    }

    // SOCD - Last Input Priority (4 Way)
    void lif(u8 n, T& inp, const T val_u, const T val_d, const T val_l, const T val_r) const {
        const T val_ud = val_u | val_d, val_lr = val_l | val_r;
        const T inp_lr = inp & val_lr;
        if (inp_lr == val_lr) inp &= ~prev_lr[n];
        else if (inp_lr) prev_lr[n] = inp_lr;

        const T inp_ud = inp & val_ud;
        if (inp_ud == val_ud) inp &= ~prev_ud[n];
        else if (inp_ud) prev_ud[n] = inp_ud;

        neu(n, inp, val_u, val_d, val_l, val_r);
    }

    // SOCD - Last Input Priority (8 Way)
    void lie(u8 n, T& inp, const T val_u, const T val_d, const T val_l, const T val_r) const {
        const T prev_state = prev[n];
        lif(n, inp, val_u, val_d, val_l, val_r);

        const T val_ud = (val_u | val_d), val_lr = (val_l | val_r);
        const T inp_e  = (inp & (val_ud | val_lr)), prev_e = (prev_state & (val_ud | val_lr));
        if (((inp_e == (val_d | val_l)) && (prev_e == (val_d | val_r))) ||
            ((inp_e == (val_d | val_r)) && (prev_e == (val_d | val_l))) ||
            ((inp_e == (val_u | val_l)) && (prev_e == (val_u | val_r))) ||
            ((inp_e == (val_u | val_r)) && (prev_e == (val_u | val_l)))) {
            inp &= ~val_lr;
        }
        if (((inp_e == (val_d | val_l)) && (prev_e == (val_u | val_l))) ||
            ((inp_e == (val_d | val_r)) && (prev_e == (val_u | val_r))) ||
            ((inp_e == (val_u | val_l)) && (prev_e == (val_d | val_l))) ||
            ((inp_e == (val_u | val_r)) && (prev_e == (val_d | val_r)))) {
            inp &= ~val_ud;
        }
        prev[n] = inp;
    }

    // SOCD - First Input Priority
    void fip(u8 n, T& inp, const T val_u, const T val_d, const T val_l, const T val_r) const {
        const T val_ud = (val_u | val_d), val_lr = (val_l | val_r);
        const T inp_lr = inp & val_lr;
        if (inp_lr == val_lr) inp &= (~val_lr) | prev_lr[n];
        else if (inp_lr) prev_lr[n] = inp_lr;

        const T inp_ud = inp & val_ud;
        if (inp_ud == val_ud) inp &= (~val_ud) | prev_ud[n];
        else if (inp_ud) prev_ud[n] = inp_ud;

        neu(n, inp, val_u, val_d, val_l, val_r);
    }

    // SOCD - Up Priority (Up-override Down)
    void uod(u8 n, T& inp, const T val_u, const T val_d, const T val_l, const T val_r) const {
        const T val_ud = (val_u | val_d), val_lr = (val_l | val_r), inp_u = inp & val_u;
        oppoxy(n, inp, val_ud, val_lr);
        if (inp_u) inp |= inp_u;

        neu(n, inp, val_u, val_d, val_l, val_r);
    }

    // SOCD - Down Priority (Left/Right Last Input Priority)
    void dlr(u8 n, T& inp, const T val_u, const T val_d, const T val_l, const T val_r) const {
        const T val_ud = (val_u | val_d), val_lr = (val_l | val_r), inp_d = (inp & val_d);
        const T inp_lr = inp & val_lr;
        if (inp_lr == val_lr) inp &= ~prev_lr[n];
        else if (inp_lr) prev_lr[n] = inp_lr;

        const T inp_ud = inp & val_ud;
        if (inp_ud == val_ud) inp &= (~val_ud) | inp_d;

        neu(n, inp, val_u, val_d, val_l, val_r);
    }
};
