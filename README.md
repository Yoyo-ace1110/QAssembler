# OpenQASM Assembler

## Project Overview
This project is currently experimenting with converting OPENQASM 2.0 code into an Abstract Syntax Tree (AST). Future directions and goals are still being explored.

## Current Progress
- [x] **Lexing / Tokenizing**: Completed.
- [ ] **Parsing**: In progress (aiming to produce an AST).
- [ ] **Semantic Analysis**: Not yet started.

## Future Directions
Once the parsing and analysis stages are complete, the project will likely branch into one of two directions:
1. **Transpilation**: Translating OpenQASM 2.0 into Python code targeting IBM's Qiskit API.
2. **Simulation**: Simulating quantum circuit execution directly on classical computers.
