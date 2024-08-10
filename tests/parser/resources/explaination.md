### Verbatim String Literal
```
@("\ |String| /")
```

### Date
```
@date("1998/12/01")
@date("1998-12-01")
```

#### Date Operations
```
@date.year(1998) & @date.month(12) & @date.day(1)
```

```
@date("1998-12-01") - @date.day(90)
```
