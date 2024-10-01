### 1. Cases with Conditionals
```
case | A | B | C |:
  // Support Conditionals in Cases
```

### 2. Check Non Null When Initialization
(Make Sure "A" is Initialized with a Value)
```
  a: T = <~>
```

### 3. 2 ways of finding unique functions:
```
  1. inferenced by back-end
  2. labeled by user
```

### 4. Some New Symbols
```
<!: "Error">
<?: "Nullable">
<~: "Non-Null">
```

### 5. Iterators
Iterator Sequence
```
  list >> # step 1 > # step 2 

  // step 1 context includes (i)
  // step 2 context includes (i, ret of step 1)
  // and so on ...
```

Iterator After Iterator
```
  list 
    >> (i: tuple) 
    ?? (i.size() == 2)
    >> (j: element) 
    => >_(j)
```

Comment Iterator
```
  list >> (i) => >_(i)
```

### 6. Func Name with Spaces
This works:
```
# do something () => {}

list >> # do something /* equals do_something */
```
And this works:
```
# do_something () => {}
# do_something_else () => {}

list >> # do something /* equals do_something */
  > # do something else /* equals do_something_else */
```
And this works:
```
# do something () => {}

do_something()
```
But this doesn't work:
```
# do something () => {}

object.do something()
```