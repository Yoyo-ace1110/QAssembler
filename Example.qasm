OPENQASM 2.0;

include "qelib1.inc";

qreg q[2];

creg c[2];

gate custom_gate(lambda) a, b {
    u3(0, 0, lambda) a;
    cx a, b;
}

opaque my_opaque(theta) a;

custom_gate(pi/2+sin(0)) q[0], q[1];

my_opaque(pi) q[0];

reset q[0];

barrier q[0], q[1];

measure q[0] -> c[0];

if(c==1) x q[1];
