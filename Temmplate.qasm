OPENQASM 2.0;
include "qelib1.inc";

qreg q[3];
qreg ancilla[1];
creg c[3];
creg flag[1];

gate cp(theta) control, target {
    u1(theta/2) control;
    cx control, target;
    u1(-theta/2) target;
    cx control, target;
    u1(theta/2) target;
}

gate qft3 q0, q1, q2 {
    h q0;
    cp(pi/2) q1, q0;
    cp(pi/4) q2, q0;
    
    h q1;
    cp(pi/2) q2, q1;
    
    h q2;
    
    cx q0, q2;
    cx q2, q0;
    cx q0, q2;
}

h q[0];
cx q[0], q[1];
cx q[1], q[2];

reset ancilla[0];
h ancilla[0];
cx ancilla[0], q[2];

barrier q[0], q[1], q[2], ancilla[0];

qft3 q[0], q[1], q[2];

rz(2*pi/3 + pi/6) q[0];
rx(pi/4) q[1];

barrier q[0], q[1], q[2], ancilla[0];

measure ancilla[0] -> flag[0];

if (flag == 1) x q[0];
if (flag == 1) z q[2];

measure q[0] -> c[0];
measure q[1] -> c[1];
measure q[2] -> c[2];
