# Threads

## Stack layout

Assume the program call func(arg0, arg1, ..., argN); When entering the func, the stack layout should be

<pre>
    +-------------+
    | argN        |
c   +-------------+
a   | ...         |
l   +-------------+
l   | arg1        |
e   +-------------+
r   | arg0        |
    +-------------+
    | return addr |
    +-------------+
    | old ebp     |
-.-.+-------------+ <-- ebp, esp
c
a
l
l
e
e

</pre>
