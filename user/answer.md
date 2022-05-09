# 1. Which registers contain arguments to functions? For example, which register holds 13 in main's call to printf?
a0/a1/a2
# 2. Where is the call to function f in the assembly code for main? Where is the call to g? (Hint: the compiler may inline functions.)
f's call start form address 2a, and g in inline of f()
# 3. At what address is the function printf located?
642
# 4. What value is in the register ra just after the jalr to printf in main?
4a
# 5. If the RISC-V were instead big-endian what would you set i to in order to yield the same output? Would you need to change 57616 to a different value?
first, i keep same. 2nd, yes, and should be 0x726c64
# 6. In the following code, what is going to be printed after 'y='? (note: the answer is not a specific value.) Why does this happen?
```c
    printf("x=%d y=%d", 3);
```
y is $a2 value